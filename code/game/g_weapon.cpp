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

// g_weapon.c
// perform the server side effects of a weapon firing

#include "g_local.h"
#include "g_functions.h"
#include "anims.h"
#include "b_local.h"
#include "wp_saber.h"
#include "g_vehicles.h"
#include "w_local.h"
#include "../cgame/cg_local.h"

vec3_t forward_vec, vright_vec, up;
vec3_t muzzle;
vec3_t muzzle2;
gentity_t* ent_list[MAX_GENTITIES];
extern cvar_t* g_SerenityJediEngineMode;

// some naughty little things that are used cg side
int g_rocketLockEntNum = ENTITYNUM_NONE;
int g_rocketLockTime = 0;
int g_rocketSlackTime = 0;

// Weapon Helper Functions
float weaponSpeed[WP_NUM_WEAPONS][2] =
{
	{0, 0}, //WP_NONE,
	{0, 0},
	//WP_SABER,				 // NOTE: lots of code assumes this is the first weapon (... which is crap) so be careful -Ste.
	{BRYAR_PISTOL_VEL,BRYAR_PISTOL_VEL}, //WP_BLASTER_PISTOL,
	{BLASTER_VELOCITY,BLASTER_VELOCITY}, //WP_BLASTER,
	{Q3_INFINITE,Q3_INFINITE}, //WP_DISRUPTOR,
	{BOWCASTER_VELOCITY,BOWCASTER_VELOCITY}, //WP_BOWCASTER,
	{REPEATER_VELOCITY,REPEATER_ALT_VELOCITY}, //WP_REPEATER,
	{DEMP2_VELOCITY,DEMP2_ALT_RANGE}, //WP_DEMP2,
	{FLECHETTE_VEL,FLECHETTE_MINE_VEL}, //WP_FLECHETTE,
	{ROCKET_VELOCITY,ROCKET_ALT_VELOCITY}, //WP_ROCKET_LAUNCHER,
	{TD_VELOCITY,TD_ALT_VELOCITY}, //WP_THERMAL,
	{0, 0}, //WP_TRIP_MINE,
	{0, 0}, //WP_DET_PACK,
	{CONC_VELOCITY,Q3_INFINITE}, //WP_CONCUSSION,
	{0, 0}, //WP_MELEE,			// Any ol' melee attack
	{0, 0}, //WP_STUN_BATON,
	{BRYAR_PISTOL_VEL,BRYAR_PISTOL_VEL}, //WP_BRYAR_PISTOL,
	{EMPLACED_VEL,EMPLACED_VEL}, //WP_EMPLACED_GUN,
	{BLASTER_VELOCITY, BLASTER_VELOCITY}, // WP_DROIDEKA
	{BRYAR_PISTOL_VEL,BRYAR_PISTOL_VEL}, //WP_SBD_BLASTER,
	{CLONECOMMANDO_VELOCITY, CLONECOMMANDO_VELOCITY}, // WP_WRIST_BLASTER
	{JANGO_VELOCITY, JANGO_VELOCITY}, // WP_DUAL_PISTOL
	{CLONEPISTOL_VEL, CLONEPISTOL_VEL}, // WP_DUAL_CLONEPISTOL
	{BRYAR_PISTOL_VEL,BRYAR_PISTOL_VEL}, //WP_BOT_LASER,		// Probe droid	- Laser blast
	{0, 0}, //WP_TURRET,			// turret guns
	{ATST_MAIN_VEL,ATST_MAIN_VEL}, //WP_ATST_MAIN,
	{ATST_SIDE_MAIN_VELOCITY,ATST_SIDE_ALT_NPC_VELOCITY}, //WP_ATST_SIDE,
	{EMPLACED_VEL,EMPLACED_VEL}, //WP_TIE_FIGHTER,
	{EMPLACED_VEL,REPEATER_ALT_VELOCITY}, //WP_RAPID_FIRE_CONC,
	{BRYAR_PISTOL_VEL,BRYAR_PISTOL_VEL}, //WP_JAWA,
	{TUSKEN_RIFLE_VEL,TUSKEN_RIFLE_VEL}, //WP_TUSKEN_RIFLE,
	{0, 0}, //WP_TUSKEN_STAFF,
	{0, 0}, //WP_SCEPTER,
	{0, 0}, //WP_NOGHRI_STICK,
	{BLASTER_VELOCITY, BLASTER_VELOCITY}, // WP_BATTLEDROID,
	{BLASTER_VELOCITY, BLASTER_VELOCITY}, // WP_THEFIRSTORDER,
	{BLASTER_VELOCITY, BLASTER_VELOCITY}, // WP_CLONECARBINE,
	{CLONERIFLE_VELOCITY, CLONERIFLE_VELOCITY}, // WP_CLONERIFLE
	{REBELBLASTER_VELOCITY, REBELBLASTER_VELOCITY}, // WP_REBELBLASTER
	{CLONECOMMANDO_VELOCITY, CLONECOMMANDO_VELOCITY}, // WP_CLONECOMMANDO
	{Z6_ROTARY_CANNON_VELOCITY, Z6_ROTARY_CANNON_VELOCITY}, // WP_Z6_ROTARY_CANNON
	{REBELRIFLE_VELOCITY, REBELRIFLE_VELOCITY}, // WP_REBELRIFLE
	{REY_VEL,REY_VEL}, //WP_REY,
	{JANGO_VELOCITY, JANGO_VELOCITY}, // WP_JANGO
	{BOBA_VELOCITY, BOBA_VELOCITY}, // WP_BOBA
	{CLONEPISTOL_VEL,CLONEPISTOL_VEL}, //WP_CLONEPISTOL,
};

float WP_SpeedOfMissileForWeapon(const int wp, const qboolean alt_fire)
{
	if (alt_fire)
	{
		return weaponSpeed[wp][1];
	}
	return weaponSpeed[wp][0];
}

//-----------------------------------------------------------------------------
void WP_TraceSetStart(const gentity_t* ent, vec3_t start)
//-----------------------------------------------------------------------------
{
	//make sure our start point isn't on the other side of a wall
	trace_t tr;
	vec3_t entMins, newstart;
	vec3_t entMaxs;

	VectorSet(entMaxs, 5, 5, 5);
	VectorScale(entMaxs, -1, entMins);

	if (!ent->client)
	{
		return;
	}

	VectorCopy(ent->currentOrigin, newstart);
	newstart[2] = start[2]; // force newstart to be on the same plane as the muzzle ( start )

	gi.trace(&tr, newstart, entMins, entMaxs, start, ent->s.number, MASK_SOLID | CONTENTS_SHOTCLIP,
		static_cast<EG2_Collision>(0), 0);

	if (tr.startsolid || tr.allsolid)
	{
		// there is a problem here..
		return;
	}

	if (tr.fraction < 1.0f)
	{
		VectorCopy(tr.endpos, start);
	}
}

extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);
//-----------------------------------------------------------------------------
gentity_t* create_missile(vec3_t org, vec3_t dir, const float vel, const int life, gentity_t* owner,
	const qboolean alt_fire)
	//-----------------------------------------------------------------------------
{
	gentity_t* missile = G_Spawn();

	missile->nextthink = level.time + life;
	missile->e_ThinkFunc = thinkF_G_FreeEntity;
	missile->s.eType = ET_MISSILE;
	missile->owner = owner;

	const Vehicle_t* p_veh = G_IsRidingVehicle(owner);

	missile->alt_fire = alt_fire;

	missile->s.pos.trType = TR_LINEAR;
	missile->s.pos.trTime = level.time; // - 10;	// move a bit on the very first frame
	VectorCopy(org, missile->s.pos.trBase);
	VectorScale(dir, vel, missile->s.pos.trDelta);
	if (p_veh)
	{
		missile->s.eFlags |= EF_USE_ANGLEDELTA;
		vectoangles(missile->s.pos.trDelta, missile->s.angles);
		VectorMA(missile->s.pos.trDelta, 2.0f, p_veh->m_pParentEntity->client->ps.velocity, missile->s.pos.trDelta);
	}

	VectorCopy(org, missile->currentOrigin);
	gi.linkentity(missile);

	return missile;
}

//-----------------------------------------------------------------------------
void WP_Stick(gentity_t* missile, const trace_t* trace, const float fudge_distance)
//-----------------------------------------------------------------------------
{
	vec3_t org, ang;

	// not moving or rotating
	missile->s.pos.trType = TR_STATIONARY;
	VectorClear(missile->s.pos.trDelta);
	VectorClear(missile->s.apos.trDelta);

	// so we don't stick into the wall
	VectorMA(trace->endpos, fudge_distance, trace->plane.normal, org);
	G_SetOrigin(missile, org);

	vectoangles(trace->plane.normal, ang);
	G_SetAngles(missile, ang);

	// I guess explode death wants me as the normal?
	//	VectorCopy( trace->plane.normal, missile->pos1 );
	gi.linkentity(missile);
}

// This version shares is in the thinkFunc format
//-----------------------------------------------------------------------------
void WP_Explode(gentity_t* self)
//-----------------------------------------------------------------------------
{
	gentity_t* attacker = self;
	vec3_t forward_vec = { 0, 0, 1 };

	// stop chain reaction runaway loops
	self->takedamage = qfalse;

	self->s.loopSound = 0;

	//	VectorCopy( self->currentOrigin, self->s.pos.trBase );
	if (!self->client)
	{
		AngleVectors(self->s.angles, forward_vec, nullptr, nullptr);
	}

	if (self->fxID > 0)
	{
		G_PlayEffect(self->fxID, self->currentOrigin, forward_vec);
	}

	if (self->owner)
	{
		attacker = self->owner;
	}
	else if (self->activator)
	{
		attacker = self->activator;
	}

	if (self->splashDamage > 0 && self->splashRadius > 0)
	{
		G_RadiusDamage(self->currentOrigin, attacker, self->splashDamage, self->splashRadius,
			nullptr/*don't ignore attacker*/, MOD_EXPLOSIVE_SPLASH);
	}

	if (self->target)
	{
		G_UseTargets(self, attacker);
	}

	G_SetOrigin(self, self->currentOrigin);

	self->nextthink = level.time + 50;
	self->e_ThinkFunc = thinkF_G_FreeEntity;
}

// We need to have a dieFunc, otherwise G_Damage won't actually make us die.  I could modify G_Damage, but that entails too many changes
//-----------------------------------------------------------------------------
void WP_ExplosiveDie(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, int means_of_death,
	int d_flags, int hit_loc)
	//-----------------------------------------------------------------------------
{
	self->enemy = attacker;

	if (attacker && !attacker->s.number)
	{
		// less damage when shot by player
		self->splashDamage /= 3;
		self->splashRadius /= 3;
	}

	self->s.eFlags &= ~EF_FIRING; // don't draw beam if we are dead

	WP_Explode(self);
}

bool WP_MissileTargetHint(gentity_t* shooter, vec3_t start, vec3_t out)
{
	return false;
}

int G_GetHitLocFromTrace(trace_t* trace, const int mod)
{
	int hit_loc = HL_NONE;
	for (auto& i : trace->G2CollisionMap)
	{
		if (i.mEntityNum == -1)
		{
			break;
		}

		CCollisionRecord& coll = i;
		if (coll.mFlags & G2_FRONTFACE)
		{
			G_GetHitLocFromSurfName(&g_entities[coll.mEntityNum],
				gi.G2API_GetSurfaceName(&g_entities[coll.mEntityNum].ghoul2[coll.mModelIndex],
					coll.mSurfaceIndex), &hit_loc, coll.mCollisionPosition,
				nullptr, nullptr, mod);
			//we only want the first "entrance wound", so break
			break;
		}
	}
	return hit_loc;
}

//---------------------------------------------------------
void AddLeanOfs(const gentity_t* const ent, vec3_t point)
//---------------------------------------------------------
{
	if (ent->client)
	{
		if (ent->client->ps.leanofs)
		{
			vec3_t right;
			//add leaning offset
			AngleVectors(ent->client->ps.viewangles, nullptr, right, nullptr);
			VectorMA(point, static_cast<float>(ent->client->ps.leanofs), right, point);
		}
	}
}

//---------------------------------------------------------
void subtract_lean_ofs(const gentity_t* ent, vec3_t point)
//---------------------------------------------------------
{
	if (ent->client)
	{
		if (ent->client->ps.leanofs)
		{
			vec3_t right;
			//add leaning offset
			AngleVectors(ent->client->ps.viewangles, nullptr, right, nullptr);
			VectorMA(point, ent->client->ps.leanofs * -1, right, point);
		}
	}
}

//---------------------------------------------------------
void ViewHeightFix(const gentity_t* const ent)
//---------------------------------------------------------
{
	//FIXME: this is hacky and doesn't need to be here.  Was only put here to make up
	//for the times a crouch anim would be used but not actually crouching.
	//When we start calcing eyepos (SPOT_HEAD) from the tag_eyes, we won't need
	//this (or viewheight at all?)
	if (!ent)
		return;

	if (!ent->client || !ent->NPC)
		return;

	if (ent->client->ps.stats[STAT_HEALTH] <= 0)
		return; //dead

	if (ent->client->ps.legsAnim == BOTH_CROUCH1IDLE || ent->client->ps.legsAnim == BOTH_CROUCH1 || ent->client->ps.
		legsAnim == BOTH_CROUCH1WALK)
	{
		if (ent->client->ps.viewheight != ent->client->crouchheight + STANDARD_VIEWHEIGHT_OFFSET)
			ent->client->ps.viewheight = ent->client->crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
	}
	else
	{
		if (ent->client->ps.viewheight != ent->client->standheight + STANDARD_VIEWHEIGHT_OFFSET)
			ent->client->ps.viewheight = ent->client->standheight + STANDARD_VIEWHEIGHT_OFFSET;
	}
}

qboolean W_AccuracyLoggableWeapon(const int weapon, const qboolean alt_fire, const int mod)
{
	if (mod != MOD_UNKNOWN)
	{
		switch (mod)
		{
			//standard weapons
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
		case MOD_JANGO:
		case MOD_JANGO_ALT:
		case MOD_BOBA:
		case MOD_BOBA_ALT:
		case MOD_CLONEPISTOL:
		case MOD_CLONEPISTOL_ALT:
		case MOD_DISRUPTOR:
		case MOD_SNIPER:
		case MOD_BOWCASTER:
		case MOD_BOWCASTER_ALT:
		case MOD_ROCKET:
		case MOD_ROCKET_ALT:
		case MOD_CONC:
		case MOD_CONC_ALT:
			return qtrue;
			//non-alt standard
		case MOD_REPEATER:
		case MOD_DEMP2:
		case MOD_FLECHETTE:
			return qtrue;
			//emplaced gun
		case MOD_EMPLACED:
			return qtrue;
			//atst
		case MOD_ENERGY:
		case MOD_EXPLOSIVE:
			if (weapon == WP_ATST_MAIN || weapon == WP_ATST_SIDE)
			{
				return qtrue;
			}
			break;
		default:;
		}
	}
	else if (weapon != WP_NONE)
	{
		switch (weapon)
		{
		case WP_BRYAR_PISTOL:
		case WP_BLASTER_PISTOL:
		case WP_BLASTER:
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
		case WP_DISRUPTOR:
		case WP_BOWCASTER:
		case WP_ROCKET_LAUNCHER:
		case WP_CONCUSSION:
		case WP_SBD_BLASTER:
		case WP_WRIST_BLASTER:
		case WP_JAWA:
		case WP_DUAL_PISTOL:
		case WP_DUAL_CLONEPISTOL:
		case WP_DROIDEKA:
			return qtrue;
			//non-alt standard
		case WP_REPEATER:
		case WP_DEMP2:
		case WP_FLECHETTE:
			if (!alt_fire)
			{
				return qtrue;
			}
			break;
			//emplaced gun
		case WP_EMPLACED_GUN:
			return qtrue;
			//atst
		case WP_ATST_MAIN:
		case WP_ATST_SIDE:
			return qtrue;
		default:;
		}
	}
	return qfalse;
}

/*
===============
LogAccuracyHit
===============
*/
qboolean LogAccuracyHit(const gentity_t* target, const gentity_t* attacker)
{
	if (!target->takedamage)
	{
		return qfalse;
	}

	if (target == attacker)
	{
		return qfalse;
	}

	if (!target->client)
	{
		return qfalse;
	}

	if (!attacker->client)
	{
		return qfalse;
	}

	if (target->client->ps.stats[STAT_HEALTH] <= 0)
	{
		return qfalse;
	}

	if (OnSameTeam(target, attacker))
	{
		return qfalse;
	}

	return qtrue;
}

static void CalcMuzzlePoint2(const gentity_t* const ent, vec3_t muzzle_point, const float lead_in)
{
	if (!lead_in) //&& ent->s.number != 0
	{
		//Not players or melee
		if (ent->client)
		{
			if (ent->client->renderInfo.mPCalcTime >= level.time - FRAMETIME * 2)
			{
				//Our muzz point was calced no more than 2 frames ago
				VectorCopy(ent->client->renderInfo.muzzlePointOld, muzzle_point);
			}
		}
	}
}

//---------------------------------------------------------
void CalcMuzzlePoint(gentity_t* const ent, vec3_t forward_vec, vec3_t muzzle_point, const float lead_in)
//---------------------------------------------------------
{
	vec3_t org;
	mdxaBone_t bolt_matrix;

	if (!lead_in) //&& ent->s.number != 0
	{
		//Not players or melee
		if (ent->client)
		{
			if (ent->client->renderInfo.mPCalcTime >= level.time - FRAMETIME * 2)
			{
				//Our muzz point was calced no more than 2 frames ago
				VectorCopy(ent->client->renderInfo.muzzlePoint, muzzle_point);
				return;
			}
		}
	}

	VectorCopy(ent->currentOrigin, muzzle_point);

	switch (ent->s.weapon)
	{
	case WP_BRYAR_PISTOL:
	case WP_BLASTER_PISTOL:
	case WP_SBD_BLASTER:
	case WP_JAWA:
	case WP_JANGO:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 16;
		VectorMA(muzzle_point, 28, forward_vec, muzzle_point);
		VectorMA(muzzle_point, 6, vright_vec, muzzle_point);
		break;

	case WP_ROCKET_LAUNCHER:
	case WP_CONCUSSION:
	case WP_THERMAL:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 2;
		break;

	case WP_BLASTER:
	case WP_THEFIRSTORDER:
	case WP_CLONECARBINE:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 1;
		if (ent->s.number == 0)
			VectorMA(muzzle_point, 12, forward_vec, muzzle_point);
		// player, don't set this any lower otherwise the projectile will impact immediately when your back is to a wall
		else
			VectorMA(muzzle_point, 2, forward_vec, muzzle_point);
		// NPC, don't set too far forwardVec otherwise the projectile can go through doors

		VectorMA(muzzle_point, 1, vright_vec, muzzle_point);
		break;

	case WP_BATTLEDROID:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 1;
		if (ent->s.number == 0)
			VectorMA(muzzle_point, 12, forward_vec, muzzle_point);
		// player, don't set this any lower otherwise the projectile will impact immediately when your back is to a wall
		else
			VectorMA(muzzle_point, 2, forward_vec, muzzle_point);
		// NPC, don't set too far forwardVec otherwise the projectile can go through doors

		VectorMA(muzzle_point, 1, vright_vec, muzzle_point);
		break;

	case WP_REBELBLASTER:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 1;
		if (ent->s.number == 0)
			VectorMA(muzzle_point, 12, forward_vec, muzzle_point);
		// player, don't set this any lower otherwise the projectile will impact immediately when your back is to a wall
		else
			VectorMA(muzzle_point, 2, forward_vec, muzzle_point);
		// NPC, don't set too far forwardVec otherwise the projectile can go through doors

		VectorMA(muzzle_point, 1, vright_vec, muzzle_point);
		break;

	case WP_CLONERIFLE:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 1;
		if (ent->s.number == 0)
			VectorMA(muzzle_point, 12, forward_vec, muzzle_point);
		// player, don't set this any lower otherwise the projectile will impact immediately when your back is to a wall
		else
			VectorMA(muzzle_point, 2, forward_vec, muzzle_point);
		// NPC, don't set too far forwardVec otherwise the projectile can go through doors

		VectorMA(muzzle_point, 1, vright_vec, muzzle_point);
		break;

	case WP_CLONECOMMANDO:
	case WP_WRIST_BLASTER:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 1;
		if (ent->s.number == 0)
			VectorMA(muzzle_point, 12, forward_vec, muzzle_point);
		// player, don't set this any lower otherwise the projectile will impact immediately when your back is to a wall
		else
			VectorMA(muzzle_point, 2, forward_vec, muzzle_point);
		// NPC, don't set too far forwardVec otherwise the projectile can go through doors

		VectorMA(muzzle_point, 1, vright_vec, muzzle_point);
		break;

	case WP_Z6_ROTARY_CANNON:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 1;
		if (ent->s.number == 0)
			VectorMA(muzzle_point, 12, forward_vec, muzzle_point);
		// player, don't set this any lower otherwise the projectile will impact immediately when your back is to a wall
		else
			VectorMA(muzzle_point, 2, forward_vec, muzzle_point);
		// NPC, don't set too far forwardVec otherwise the projectile can go through doors
		VectorMA(muzzle_point, 1, vright_vec, muzzle_point);
		break;

	case WP_REBELRIFLE:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 1;
		if (ent->s.number == 0)
			VectorMA(muzzle_point, 12, forward_vec, muzzle_point);
		// player, don't set this any lower otherwise the projectile will impact immediately when your back is to a wall
		else
			VectorMA(muzzle_point, 2, forward_vec, muzzle_point);
		// NPC, don't set too far forwardVec otherwise the projectile can go through doors

		VectorMA(muzzle_point, 1, vright_vec, muzzle_point);
		break;

	case WP_REY:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 16;
		VectorMA(muzzle_point, 28, forward_vec, muzzle_point);
		VectorMA(muzzle_point, 6, vright_vec, muzzle_point);
		break;

	case WP_DUAL_PISTOL:
	case WP_DUAL_CLONEPISTOL:
	case WP_DROIDEKA:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 1;
		if (ent->s.number == 0)
			VectorMA(muzzle_point, 12, forward_vec, muzzle_point);
		// player, don't set this any lower otherwise the projectile will impact immediately when your back is to a wall
		else
			VectorMA(muzzle_point, 2, forward_vec, muzzle_point);
		// NPC, don't set too far forwardVec otherwise the projectile can go through doors

		VectorMA(muzzle_point, 1, vright_vec, muzzle_point);
		break;

	case WP_BOBA:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 1;
		if (ent->s.number == 0)
			VectorMA(muzzle_point, 12, forward_vec, muzzle_point);
		// player, don't set this any lower otherwise the projectile will impact immediately when your back is to a wall
		else
			VectorMA(muzzle_point, 2, forward_vec, muzzle_point);
		// NPC, don't set too far forwardVec otherwise the projectile can go through doors

		VectorMA(muzzle_point, 1, vright_vec, muzzle_point);
		break;

	case WP_CLONEPISTOL:
		ViewHeightFix(ent);
		muzzle_point[2] += ent->client->ps.viewheight; //By eyes
		muzzle_point[2] -= 16;
		VectorMA(muzzle_point, 28, forward_vec, muzzle_point);
		VectorMA(muzzle_point, 6, vright_vec, muzzle_point);
		break;

	case WP_SABER:
		if (ent->NPC != nullptr &&
			(ent->client->ps.torsoAnim == TORSO_WEAPONREADY2 ||
				ent->client->ps.torsoAnim == BOTH_ATTACK2)) //Sniper pose
		{
			ViewHeightFix(ent);
			muzzle[2] += ent->client->ps.viewheight; //By eyes
		}
		else
		{
			muzzle_point[2] += 16;
		}
		VectorMA(muzzle_point, 8, forward_vec, muzzle_point);
		VectorMA(muzzle_point, 16, vright_vec, muzzle_point);
		break;

	case WP_BOT_LASER:
		muzzle_point[2] -= 16; //
		break;
	case WP_ATST_MAIN:

		if (ent->count > 0)
		{
			ent->count = 0;
			gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel,
				ent->handLBolt,
				&bolt_matrix, ent->s.angles, ent->s.origin, cg.time ? cg.time : level.time,
				nullptr, ent->s.modelScale);
		}
		else
		{
			ent->count = 1;
			gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel,
				ent->handRBolt,
				&bolt_matrix, ent->s.angles, ent->s.origin, cg.time ? cg.time : level.time,
				nullptr, ent->s.modelScale);
		}

		gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, org);

		VectorCopy(org, muzzle_point);

		break;
	default:;
	}

	AddLeanOfs(ent, muzzle_point);
}

// Muzzle point table...
vec3_t WP_MuzzlePoint[WP_NUM_WEAPONS] =
{
	//	Fwd,	right,	up.
	{0, 0, 0}, // WP_NONE,
	{8, 16, 0}, // WP_SABER,
	{12, 6, -6}, // WP_BLASTER_PISTOL,
	{12, 6, -6}, // WP_BLASTER,
	{12, 6, -6}, // WP_DISRUPTOR,
	{12, 2, -6}, // WP_BOWCASTER,
	{12, 4.5, -6}, // WP_REPEATER,
	{12, 6, -6}, // WP_DEMP2,
	{12, 6, -6}, // WP_FLECHETTE,
	{12, 8, -4}, // WP_ROCKET_LAUNCHER,
	{12, 0, -4}, // WP_THERMAL,
	{12, 0, -10}, // WP_TRIP_MINE,
	{12, 0, -4}, // WP_DET_PACK,
	{12, 8, -4}, // WP_CONCUSSION,
	{0, 8, 0}, // WP_MELEE,
	{0, 0, 0}, // WP_ATST_MAIN,
	{0, 0, 0}, // WP_ATST_SIDE,
	{0, 8, 0}, // WP_STUN_BATON,
	{12, 6, -6}, // WP_BRYAR_PISTOL,
	{12, 6, -6}, // WP_DROIDEKA,
	{12, 6, -6}, // WP_SBD_BLASTER,
	{12, 6, -6}, // WP_WRIST_BLASTER,
	{12, 6, -6}, // WP_DUAL_PISTOL,
	{12, 6, -6}, // WP_DUAL_CLONEPISTOL,
	{12, 6, -6}, // WP_BATTLEDROID,
	{12, 6, -6}, // WP_THEFIRSTORDER,
	{12, 6, -6}, // WP_CLONECARBINE,
	{12, 6, -6}, // WP_REBELBLASTER,
	{12, 6, -6}, // WP_CLONERIFLE,
	{12, 6, -6}, // WP_CLONECOMMANDO,
	{12, 6, -6}, // WP_Z6_ROTARY_CANNON,
	{12, 6, -6}, // WP_REBELRIFLE,
	{12, 6, -6}, // WP_REY,
	{12, 6, -6}, // WP_JANGO,
	{12, 6, -6}, // WP_BOBA,
	{12, 6, -6}, // WP_CLONEPISTOL,
};

static void WP_RocketLock(const gentity_t* ent, const float lock_dist)
{
	// Not really a charge weapon, but we still want to delay fire until the button comes up so that we can
	//	implement our alt-fire locking stuff
	vec3_t ang;
	trace_t tr;

	vec3_t muzzle_off_point, muzzle_point, forward_vec, right, up;

	AngleVectors(ent->client->ps.viewangles, forward_vec, right, up);

	AngleVectors(ent->client->ps.viewangles, ang, nullptr, nullptr);

	VectorCopy(ent->client->ps.origin, muzzle_point);
	VectorCopy(WP_MuzzlePoint[WP_ROCKET_LAUNCHER], muzzle_off_point);

	VectorMA(muzzle_point, muzzle_off_point[0], forward_vec, muzzle_point);
	VectorMA(muzzle_point, muzzle_off_point[1], right, muzzle_point);
	muzzle_point[2] += ent->client->ps.viewheight + muzzle_off_point[2];

	ang[0] = muzzle_point[0] + ang[0] * lock_dist;
	ang[1] = muzzle_point[1] + ang[1] * lock_dist;
	ang[2] = muzzle_point[2] + ang[2] * lock_dist;

	gi.trace(&tr, muzzle_point, nullptr, nullptr, ang, ent->client->ps.client_num, MASK_PLAYERSOLID,
		static_cast<EG2_Collision>(0), 0);

	if (tr.fraction != 1 && tr.entityNum < ENTITYNUM_NONE && tr.entityNum != ent->client->ps.client_num)
	{
		const gentity_t* bg_ent = &g_entities[tr.entityNum];
		if (bg_ent && bg_ent->s.powerups & PW_CLOAKED)
		{
			ent->client->rocketLockIndex = ENTITYNUM_NONE;
			ent->client->rocketLockTime = 0;
		}
		else if (bg_ent && bg_ent->s.eType == ET_PLAYER)
		{
			if (ent->client->rocketLockIndex == ENTITYNUM_NONE)
			{
				ent->client->rocketLockIndex = tr.entityNum;
				ent->client->rocketLockTime = level.time;
			}
			else if (ent->client->rocketLockIndex != tr.entityNum && ent->client->rocketTargetTime < level.time)
			{
				ent->client->rocketLockIndex = tr.entityNum;
				ent->client->rocketLockTime = level.time;
			}
			else if (ent->client->rocketLockIndex == tr.entityNum)
			{
				if (ent->client->rocketLockTime == -1)
				{
					ent->client->rocketLockTime = ent->client->rocketLastValidTime;
				}
			}

			if (ent->client->rocketLockIndex == tr.entityNum)
			{
				ent->client->rocketTargetTime = level.time + 500;
			}
		}
	}
	else if (ent->client->rocketTargetTime < level.time)
	{
		ent->client->rocketLockIndex = ENTITYNUM_NONE;
		ent->client->rocketLockTime = 0;
	}
	else
	{
		if (ent->client->rocketLockTime != -1)
		{
			ent->client->rocketLastValidTime = ent->client->rocketLockTime;
		}
		ent->client->rocketLockTime = -1;
	}
}

constexpr auto VEH_HOMING_MISSILE_THINK_TIME = 100;

static void WP_FireVehicleWeapon(gentity_t* ent, vec3_t start, vec3_t dir, const vehWeaponInfo_t* veh_weapon)
{
	if (!veh_weapon)
	{
		//invalid vehicle weapon
		return;
	}
	if (veh_weapon->bIsProjectile)
	{
		//projectile entity
		vec3_t mins, maxs;

		VectorSet(maxs, veh_weapon->fWidth / 2.0f, veh_weapon->fWidth / 2.0f, veh_weapon->fHeight / 2.0f);
		VectorScale(maxs, -1, mins);

		//make sure our start point isn't on the other side of a wall
		WP_TraceSetStart(ent, start);

		//QUERY: alt_fire true or not?  Does it matter?
		gentity_t* missile = create_missile(start, dir, veh_weapon->fSpeed, 10000, ent, qfalse);
		if (veh_weapon->bHasGravity)
		{
			//TESTME: is this all we need to do?
			missile->s.pos.trType = TR_GRAVITY;
		}

		missile->classname = "vehicle_proj";

		missile->damage = veh_weapon->iDamage;
		missile->splashDamage = veh_weapon->iSplashDamage;
		missile->splashRadius = veh_weapon->fSplashRadius;

		// HUGE HORRIBLE HACK
		if (ent->owner && ent->owner->s.number == 0)
		{
			//Should only be for speeders - mainly for t2_trip
			if (ent->m_pVehicle->m_pVehicleInfo && ent->m_pVehicle->m_pVehicleInfo->type == VH_SPEEDER)
			{
				missile->damage *= 20.0f;
				missile->splashDamage *= 20.0f;
				missile->splashRadius *= 20.0f;
			}
		}

		//FIXME: externalize some of these properties?
		if (g_SerenityJediEngineMode->integer == 2)
		{
			missile->dflags = DAMAGE_DEATH_KNOCKBACK | DAMAGE_EXTRA_KNOCKBACK;
		}
		else
		{
			missile->dflags = DAMAGE_DEATH_KNOCKBACK;
		}
		missile->clipmask = MASK_SHOT;
		//Maybe by checking flags...?
		if (veh_weapon->bSaberBlockable)
		{
			missile->clipmask |= CONTENTS_LIGHTSABER;
		}

		missile->s.weapon = WP_BLASTER; //does this really matter?

		// Make it easier to hit things
		VectorCopy(mins, missile->mins);
		VectorCopy(maxs, missile->maxs);
		//some slightly different stuff for things with bboxes
		if (veh_weapon->fWidth || veh_weapon->fHeight)
		{
			//we assume it's a rocket-like thing
			missile->methodOfDeath = MOD_ROCKET;
			missile->splashMethodOfDeath = MOD_ROCKET; // ?SPLASH;

			// we don't want it to ever bounce
			missile->bounceCount = 0;

			missile->mass = 10;
		}
		else
		{
			//a blaster-laser-like thing
			missile->s.weapon = WP_BLASTER; //does this really matter?
			missile->methodOfDeath = MOD_EMPLACED; //MOD_TURBLAST; //count as a heavy weap
			missile->splashMethodOfDeath = MOD_EMPLACED; //MOD_TURBLAST;// ?SPLASH;
			// we don't want it to bounce forever
			missile->bounceCount = 8;
		}

		if (veh_weapon->iHealth)
		{
			//the missile can take damage
			missile->health = veh_weapon->iHealth;
			missile->takedamage = qtrue;
			missile->contents = MASK_SHOT;
			missile->e_DieFunc = dieF_WP_ExplosiveDie; //dieF_RocketDie;
		}

		//set veh as cgame side owner for purpose of fx overrides
		if (ent->m_pVehicle && ent->m_pVehicle->m_pPilot)
		{
			missile->owner = ent->m_pVehicle->m_pPilot;
		}
		else
		{
			missile->owner = ent;
		}
		missile->s.otherentity_num = ent->s.number;
		missile->s.otherentity_num2 = veh_weapon - &g_vehWeaponInfo[0];

		if (veh_weapon->iLifeTime)
		{
			//expire after a time
			if (veh_weapon->bExplodeOnExpire)
			{
				//blow up when your lifetime is up
				missile->e_ThinkFunc = thinkF_WP_Explode; //FIXME: custom func?
			}
			else
			{
				//just remove yourself
				missile->e_ThinkFunc = thinkF_G_FreeEntity; //FIXME: custom func?
			}
			missile->nextthink = level.time + veh_weapon->iLifeTime;
		}
		if (veh_weapon->fHoming)
		{
			//homing missile
			//crap, we need to set up the homing stuff like it is in MP...
			WP_RocketLock(ent, 16384);
			if (ent->client && ent->client->rocketLockIndex != ENTITYNUM_NONE)
			{
				int dif;
				float r_time = ent->client->rocketLockTime;

				if (r_time == -1)
				{
					r_time = ent->client->rocketLastValidTime;
				}

				if (!veh_weapon->iLockOnTime)
				{
					//no minimum lock-on time
					dif = 10; //guaranteed lock-on
				}
				else
				{
					const float lock_time_interval = veh_weapon->iLockOnTime / 16.0f;
					dif = (level.time - r_time) / lock_time_interval;
				}

				if (dif < 0)
				{
					dif = 0;
				}

				//It's 10 even though it locks client-side at 8, because we want them to have a sturdy lock first, and because there's a slight difference in time between server and client
				if (dif >= 10 && r_time != -1)
				{
					missile->enemy = &g_entities[ent->client->rocketLockIndex];

					if (missile->enemy && missile->enemy->client && missile->enemy->health > 0 && !OnSameTeam(
						ent, missile->enemy))
					{
						//if enemy became invalid, died, or is on the same team, then don't seek it
						missile->spawnflags |= 1; //just to let it know it should be faster... FIXME: EXTERNALIZE
						missile->speed = veh_weapon->fSpeed;
						missile->angle = veh_weapon->fHoming;
						if (veh_weapon->iLifeTime)
						{
							//expire after a time
							missile->disconnectDebounceTime = level.time + veh_weapon->iLifeTime;
							missile->lockCount = static_cast<int>(veh_weapon->bExplodeOnExpire);
						}
						missile->e_ThinkFunc = thinkF_rocketThink;
						missile->nextthink = level.time + VEH_HOMING_MISSILE_THINK_TIME;
						missile->s.eFlags2 |= EF2_RADAROBJECT;
					}
				}

				ent->client->rocketLockIndex = ENTITYNUM_NONE;
				ent->client->rocketLockTime = 0;
				ent->client->rocketTargetTime = 0;

				VectorCopy(dir, missile->movedir);
				missile->random = 1.0f; //FIXME: externalize?
			}
		}
	}
	else
	{
		//traceline
		//FIXME: implement
	}
}

static void WP_VehLeadCrosshairVeh(gentity_t* camtrace_ent, vec3_t newEnd, const vec3_t dir, const vec3_t shotStart, vec3_t shotDir)
{
	//FIXME: implement from MP?
}

static qboolean WP_VehCheckTraceFromCamPos(gentity_t* ent, const vec3_t shotStart, vec3_t shotDir)
{
	//FIXME: implement from MP?
	return qfalse;
}

//---------------------------------------------------------
static void FireVehicleWeapon(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	Vehicle_t* p_veh = ent->m_pVehicle;

	if (!p_veh)
	{
		return;
	}

	if (p_veh->m_iRemovedSurfaces)
	{
		//can't fire when the thing is breaking apart
		return;
	}

	if (ent->owner && ent->owner->client && ent->owner->client->ps.weapon != WP_NONE)
	{
		return;
	}

	// TODO?: If possible (probably not enough time), it would be nice if secondary fire was actually a mode switch/toggle
	// so that, for instance, an x-wing can have 4-gun fire, or individual muzzle fire. If you wanted a different weapon, you
	// would actually have to press the 2 key or something like that (I doubt I'd get a graphic for it anyways though). -AReis

	// If this is not the alternate fire, fire a normal blaster shot...
	if (p_veh->m_pVehicleInfo &&
		(p_veh->m_pVehicleInfo->type != VH_FIGHTER || p_veh->m_ulFlags & VEH_WINGSOPEN))
		// NOTE: Wings open also denotes that it has already launched.
	{
		//fighters can only fire when wings are open
		int weapon_num;
		qboolean linked_firing = qfalse;

		if (!alt_fire)
		{
			weapon_num = 0;
		}
		else
		{
			weapon_num = 1;
		}

		const int veh_weapon_index = p_veh->m_pVehicleInfo->weapon[weapon_num].ID;

		if (p_veh->weaponStatus[weapon_num].ammo <= 0)
		{
			//no ammo for this weapon
			if (p_veh->m_pPilot && p_veh->m_pPilot->s.number < MAX_CLIENTS)
			{
				// let the client know he's out of ammo
				//but only if one of the vehicle muzzles is actually ready to fire this weapon
				for (int i = 0; i < MAX_VEHICLE_MUZZLES; i++)
				{
					if (p_veh->m_pVehicleInfo->weapMuzzle[i] != veh_weapon_index)
					{
						//this muzzle doesn't match the weapon we're trying to use
						continue;
					}
					if (p_veh->m_iMuzzleTag[i] != -1
						&& p_veh->m_Muzzles[i].m_iMuzzleWait < level.time)
					{
						//this one would have fired, send the no ammo message
						G_AddEvent(p_veh->m_pPilot, EV_NOAMMO, weapon_num);
						break;
					}
				}
			}
			return;
		}

		const int delay = p_veh->m_pVehicleInfo->weapon[weapon_num].delay;
		const qboolean aim_correct = p_veh->m_pVehicleInfo->weapon[weapon_num].aimCorrect;
		if (p_veh->m_pVehicleInfo->weapon[weapon_num].linkable == 2 //always linked
			|| p_veh->m_pVehicleInfo->weapon[weapon_num].linkable == 1 //optionally linkable
			&& p_veh->weaponStatus[weapon_num].linked) //linked
		{
			//we're linking the primary or alternate weapons, so we'll do *all* the muzzles
			linked_firing = qtrue;
		}

		if (veh_weapon_index <= VEH_WEAPON_BASE || veh_weapon_index >= MAX_VEH_WEAPONS)
		{
			//invalid vehicle weapon
			return;
		}
		int i, num_muzzles = 0, num_muzzles_ready = 0, cumulativeDelay = 0, cumulativeAmmo = 0;
		qboolean sent_ammo_warning = qfalse;

		const vehWeaponInfo_t* veh_weapon = &g_vehWeaponInfo[veh_weapon_index];

		if (p_veh->m_pVehicleInfo->weapon[weapon_num].linkable == 2)
		{
			//always linked weapons don't accumulate delay, just use specified delay
			cumulativeDelay = delay;
		}
		//find out how many we've got for this weapon
		for (i = 0; i < MAX_VEHICLE_MUZZLES; i++)
		{
			if (p_veh->m_pVehicleInfo->weapMuzzle[i] != veh_weapon_index)
			{
				//this muzzle doesn't match the weapon we're trying to use
				continue;
			}
			if (p_veh->m_iMuzzleTag[i] != -1 && p_veh->m_Muzzles[i].m_iMuzzleWait < level.time)
			{
				num_muzzles_ready++;
			}
			if (p_veh->m_pVehicleInfo->weapMuzzle[p_veh->weaponStatus[weapon_num].nextMuzzle] != veh_weapon_index)
			{
				//Our designated next muzzle for this weapon isn't valid for this weapon (happens when ships fire for the first time)
				//set the next to this one
				p_veh->weaponStatus[weapon_num].nextMuzzle = i;
			}
			if (linked_firing)
			{
				cumulativeAmmo += veh_weapon->iAmmoPerShot;
				if (p_veh->m_pVehicleInfo->weapon[weapon_num].linkable != 2)
				{
					//always linked weapons don't accumulate delay, just use specified delay
					cumulativeDelay += delay;
				}
			}
			num_muzzles++;
		}

		if (linked_firing)
		{
			//firing all muzzles at once
			if (num_muzzles_ready != num_muzzles)
			{
				//can't fire all linked muzzles yet
				return;
			}
			//can fire all linked muzzles, check ammo
			if (p_veh->weaponStatus[weapon_num].ammo < cumulativeAmmo)
			{
				//can't fire, not enough ammo
				if (p_veh->m_pPilot && p_veh->m_pPilot->s.number < MAX_CLIENTS)
				{
					// let the client know he's out of ammo
					G_AddEvent(p_veh->m_pPilot, EV_NOAMMO, weapon_num);
				}
				return;
			}
		}

		for (i = 0; i < MAX_VEHICLE_MUZZLES; i++)
		{
			if (p_veh->m_pVehicleInfo->weapMuzzle[i] != veh_weapon_index)
			{
				//this muzzle doesn't match the weapon we're trying to use
				continue;
			}
			if (!linked_firing
				&& i != p_veh->weaponStatus[weapon_num].nextMuzzle)
			{
				//we're only firing one muzzle and this isn't it
				continue;
			}

			// Fire this muzzle.
			if (p_veh->m_iMuzzleTag[i] != -1 && p_veh->m_Muzzles[i].m_iMuzzleWait < level.time)
			{
				if (p_veh->weaponStatus[weapon_num].ammo < veh_weapon->iAmmoPerShot)
				{
					//out of ammo!
					if (!sent_ammo_warning)
					{
						sent_ammo_warning = qtrue;
						if (p_veh->m_pPilot && p_veh->m_pPilot->s.number < MAX_CLIENTS)
						{
							// let the client know he's out of ammo
							G_AddEvent(p_veh->m_pPilot, EV_NOAMMO, weapon_num);
						}
					}
				}
				else
				{
					vec3_t dir;
					vec3_t start;
					//have enough ammo to shoot
					//do the firing
					//WP_CalcVehMuzzle(ent, i);
					VectorCopy(p_veh->m_Muzzles[i].m_vMuzzlePos, start);
					VectorCopy(p_veh->m_Muzzles[i].m_vMuzzleDir, dir);
					if (WP_VehCheckTraceFromCamPos(ent, start, dir))
					{
						//auto-aim at whatever crosshair would be over from camera's point of view (if closer)
					}
					else if (aim_correct)
					{
						//auto-aim the missile at the crosshair if there's anything there
						trace_t trace;
						vec3_t end;
						vec3_t ang;
						vec3_t fixedDir;

						if (p_veh->m_pVehicleInfo->type == VH_SPEEDER)
						{
							VectorSet(ang, 0.0f, p_veh->m_vOrientation[1], 0.0f);
						}
						else
						{
							VectorCopy(p_veh->m_vOrientation, ang);
						}
						AngleVectors(ang, fixedDir, nullptr, nullptr);
						//VectorMA( ent->currentOrigin, 32768, dir, end );
						VectorMA(ent->currentOrigin, 8192, dir, end);
						gi.trace(&trace, ent->currentOrigin, vec3_origin, vec3_origin, end, ent->s.number, MASK_SHOT,
							static_cast<EG2_Collision>(0), 0);
						if (trace.fraction < 1.0f && !trace.allsolid && !trace.startsolid)
						{
							vec3_t new_end;
							VectorCopy(trace.endpos, new_end);
							WP_VehLeadCrosshairVeh(&g_entities[trace.entityNum], new_end, fixedDir, start, dir);
						}
					}

					//play the weapon's muzzle effect if we have one
					if (veh_weapon->iMuzzleFX)
					{
						G_PlayEffect(veh_weapon->iMuzzleFX, p_veh->m_Muzzles[i].m_vMuzzlePos,
							p_veh->m_Muzzles[i].m_vMuzzleDir);
					}
					WP_FireVehicleWeapon(ent, start, dir, veh_weapon);
				}

				if (linked_firing)
				{
					//we're linking the weapon, so continue on and fire all appropriate muzzles
					continue;
				}
				//else just firing one
				//take the ammo, set the next muzzle and set the delay on it
				if (num_muzzles > 1)
				{
					//more than one, look for it
					int next_muzzle = p_veh->weaponStatus[weapon_num].nextMuzzle;
					while (true)
					{
						next_muzzle++;
						if (next_muzzle >= MAX_VEHICLE_MUZZLES)
						{
							next_muzzle = 0;
						}
						if (next_muzzle == p_veh->weaponStatus[weapon_num].nextMuzzle)
						{
							//WTF?  Wrapped without finding another valid one!
							break;
						}
						if (p_veh->m_pVehicleInfo->weapMuzzle[next_muzzle] == veh_weapon_index)
						{
							//this is the next muzzle for this weapon
							p_veh->weaponStatus[weapon_num].nextMuzzle = next_muzzle;
							break;
						}
					}
				} //else, just stay on the one we just fired
				//set the delay on the next muzzle
				p_veh->m_Muzzles[p_veh->weaponStatus[weapon_num].nextMuzzle].m_iMuzzleWait = level.time + delay;
				//take away the ammo
				p_veh->weaponStatus[weapon_num].ammo -= veh_weapon->iAmmoPerShot;
				//NOTE: in order to send the vehicle's ammo info to the client, we copy the ammo into the first 2 ammo slots on the vehicle NPC's client->ps.ammo array
				if (p_veh->m_pParentEntity && p_veh->m_pParentEntity->client)
				{
					p_veh->m_pParentEntity->client->ps.ammo[weapon_num] = p_veh->weaponStatus[weapon_num].ammo;
				}
				//done!
				//we'll get in here again next frame and try the next muzzle...
				//return;
				return;
			}
		}
		//we went through all the muzzles, so apply the cumulative delay and ammo cost
		if (cumulativeAmmo)
		{
			//taking ammo one shot at a time
			//take the ammo
			p_veh->weaponStatus[weapon_num].ammo -= cumulativeAmmo;
			//NOTE: in order to send the vehicle's ammo info to the client, we copy the ammo into the first 2 ammo slots on the vehicle NPC's client->ps.ammo array
			if (p_veh->m_pParentEntity && p_veh->m_pParentEntity->client)
			{
				p_veh->m_pParentEntity->client->ps.ammo[weapon_num] = p_veh->weaponStatus[weapon_num].ammo;
			}
		}
		if (cumulativeDelay)
		{
			//we linked muzzles so we need to apply the cumulative delay now, to each of the linked muzzles
			for (i = 0; i < MAX_VEHICLE_MUZZLES; i++)
			{
				if (p_veh->m_pVehicleInfo->weapMuzzle[i] != veh_weapon_index)
				{
					//this muzzle doesn't match the weapon we're trying to use
					continue;
				}
				//apply the cumulative delay
				p_veh->m_Muzzles[i].m_iMuzzleWait = level.time + cumulativeDelay;
			}
		}
	}
}

static void WP_FireScepter(gentity_t* ent)
{
	//just a straight beam
	vec3_t start, end;
	trace_t tr;
	constexpr float shot_range = 8192;
	qboolean render_impact = qtrue;

	VectorCopy(muzzle, start);
	WP_TraceSetStart(ent, start);

	WP_MissileTargetHint(ent, start, forward_vec);
	VectorMA(start, shot_range, forward_vec, end);

	gi.trace(&tr, start, nullptr, nullptr, end, ent->s.number, MASK_SHOT, G2_RETURNONHIT, 10);
	gentity_t* traceEnt = &g_entities[tr.entityNum];

	if (tr.surfaceFlags & SURF_NOIMPACT)
	{
		render_impact = qfalse;
	}

	// always render a shot beam, doing this the old way because I don't much feel like overriding the effect.
	gentity_t* tent = G_TempEntity(tr.endpos, EV_DISRUPTOR_MAIN_SHOT);
	tent->svFlags |= SVF_BROADCAST;
	VectorCopy(muzzle, tent->s.origin2);

	if (render_impact)
	{
		if (tr.entityNum < ENTITYNUM_WORLD && traceEnt->takedamage)
		{
			constexpr int damage = 1;
			// Create a simple impact type mark that doesn't last long in the world
			G_PlayEffect(G_EffectIndex("disruptor/flesh_impact"), tr.endpos, tr.plane.normal);

			const int hit_loc = G_GetHitLocFromTrace(&tr, MOD_DISRUPTOR);
			G_Damage(traceEnt, ent, ent, forward_vec, tr.endpos, damage, DAMAGE_EXTRA_KNOCKBACK, MOD_DISRUPTOR,
				hit_loc);
		}
		else
		{
			G_PlayEffect(G_EffectIndex("disruptor/wall_impact"), tr.endpos, tr.plane.normal);
		}
	}
}

extern qboolean WalkCheck(const gentity_t* self);
extern qboolean G_ControlledByPlayer(const gentity_t* self);

static qboolean doesnot_drain_mishap(const gentity_t* ent)
{
	switch (ent->s.weapon)
	{
	case WP_STUN_BATON:
	case WP_MELEE:
	case WP_SABER:
	case WP_THERMAL:
	case WP_TRIP_MINE:
	case WP_DET_PACK:
	case WP_EMPLACED_GUN:
	case WP_ATST_MAIN:
	case WP_ATST_SIDE:
	case WP_SBD_BLASTER:
	case WP_WRIST_BLASTER:
	case WP_BOT_LASER:
	case WP_TURRET:
	case WP_TIE_FIGHTER:
	case WP_TUSKEN_STAFF:
	case WP_TUSKEN_RIFLE:
	case WP_SCEPTER:
	case WP_JAWA:
	case WP_NOGHRI_STICK:
	case WP_RAPID_FIRE_CONC:
		return qtrue;
	default:;
	}
	return qfalse;
}

static void G_AddBlasterAttackChainCount(const gentity_t* ent, int amount)
{
	if (!g_SerenityJediEngineMode->integer)
	{
		return;
	}

	if (ent->s.client_num >= MAX_CLIENTS && !G_ControlledByPlayer(ent))
	{
		return;
	}

	if (!WalkCheck(ent))
	{
		if (amount > 0)
		{
			amount *= 2;
		}
		else
		{
			amount = amount * .5f;
		}
	}

	ent->client->ps.BlasterAttackChainCount += amount;

	if (ent->client->ps.BlasterAttackChainCount < BLASTERMISHAPLEVEL_NONE)
	{
		ent->client->ps.BlasterAttackChainCount = BLASTERMISHAPLEVEL_NONE;
	}
	else if (ent->client->ps.BlasterAttackChainCount > BLASTERMISHAPLEVEL_MAX)
	{
		ent->client->ps.BlasterAttackChainCount = BLASTERMISHAPLEVEL_MAX;
	}
}

extern Vehicle_t* G_IsRidingVehicle(const gentity_t* pEnt);
extern qboolean PM_ReloadAnim(int anim);
extern qboolean PM_WeponRestAnim(int anim);
extern qboolean PM_CrouchAnim(int anim);
extern qboolean PM_RunningAnim(int anim);
extern qboolean PM_WalkingAnim(int anim);
extern int fire_deley_time();
extern void CG_ChangeWeapon(int num);
extern qboolean IsHoldingReloadableGun(const gentity_t* ent);

//---------------------------------------------------------
void FireWeapon(gentity_t* ent, const qboolean alt_fire)
//---------------------------------------------------------
{
	float alert = 256;
	const Vehicle_t* p_veh = nullptr;

	// track shots taken for accuracy tracking.
	ent->client->ps.persistant[PERS_ACCURACY_SHOTS]++;

	if (PM_ReloadAnim(ent->client->ps.torsoAnim) ||
		PM_WeponRestAnim(ent->client->ps.torsoAnim))
	{
		return;
	}

	if (ent->weaponfiredelaytime > level.time)
	{
		return;
	}

	if (ent->s.weapon == WP_DROIDEKA && PM_RunningAnim(ent->client->ps.legsAnim))
	{
		return;
	}

	if (ent->client->ps.BlasterAttackChainCount >= BLASTERMISHAPLEVEL_FIFTEEN && IsHoldingReloadableGun(ent))
	{
		if (ent->s.weapon == WP_BRYAR_PISTOL ||
			ent->s.weapon == WP_BLASTER_PISTOL ||
			ent->s.weapon == WP_DUAL_PISTOL ||
			ent->s.weapon == WP_DUAL_CLONEPISTOL ||
			ent->s.weapon == WP_REY ||
			ent->s.weapon == WP_JANGO ||
			ent->s.weapon == WP_CLONEPISTOL ||
			ent->s.weapon == WP_REBELBLASTER)
		{
			NPC_SetAnim(ent, SETANIM_TORSO, BOTH_PISTOLFAIL, SETANIM_AFLAG_BLOCKPACE);
		}
		else if (ent->s.weapon == WP_DROIDEKA)
		{
			NPC_SetAnim(ent, SETANIM_TORSO, BOTH_RELOAD_DEKA, SETANIM_AFLAG_BLOCKPACE);
		}
		/*else if (ent->s.weapon == WP_Z6_ROTARY_CANNON)
		{
			NPC_SetAnim(ent, SETANIM_TORSO, BOTH_RELOAD_FAIL_MINIGUN, SETANIM_AFLAG_BLOCKPACE);
		}*/
		else
		{
			NPC_SetAnim(ent, SETANIM_TORSO, BOTH_RIFLEFAIL, SETANIM_AFLAG_BLOCKPACE);
		}
		G_SoundOnEnt(ent, CHAN_WEAPON, "sound/weapons/reloadfail.mp3");
		G_SoundOnEnt(ent, CHAN_VOICE_ATTEN, "*pain25.wav");
		G_Damage(ent, nullptr, nullptr, nullptr, ent->currentOrigin, 2, DAMAGE_NO_ARMOR, MOD_LAVA);
		ent->reloadTime = level.time + fire_deley_time();
		return;
	}

	// If this is a vehicle, fire it's weapon and we're done.
	if (ent && ent->client && ent->client->NPC_class == CLASS_VEHICLE)
	{
		FireVehicleWeapon(ent, alt_fire);
		return;
	}

	// set aiming directions
	if (ent && ent->client && ent->s.weapon == WP_DISRUPTOR && alt_fire)
	{
		if (ent->NPC)
		{
			//snipers must use the angles they actually did their shot trace with
			AngleVectors(ent->lastAngles, forward_vec, vright_vec, up);
		}
	}
	else if (ent && (ent->s.weapon == WP_ATST_SIDE || ent->s.weapon == WP_ATST_MAIN))
	{
		vec3_t muzzle1;

		VectorCopy(ent->client->renderInfo.muzzlePoint, muzzle1);

		if (!ent->s.number)
		{
			//player driving an AT-ST
			//SIGH... because we can't anticipate alt-fire, must calc muzzle here and now
			mdxaBone_t bolt_matrix;
			int bolt;

			if (ent->client->ps.weapon == WP_ATST_MAIN)
			{
				//FIXME: alt_fire should fire both barrels, but slower?
				if (ent->alt_fire)
				{
					bolt = ent->handRBolt;
				}
				else
				{
					bolt = ent->handLBolt;
				}
			}
			else
			{
				// ATST SIDE weapons
				if (ent->alt_fire)
				{
					if (gi.G2API_GetSurfaceRenderStatus(&ent->ghoul2[ent->playerModel], "head_light_blaster_cann"))
					{
						//don't have it!
						return;
					}
					bolt = ent->genericBolt2;
				}
				else
				{
					if (gi.G2API_GetSurfaceRenderStatus(&ent->ghoul2[ent->playerModel], "head_concussion_charger"))
					{
						//don't have it!
						return;
					}
					bolt = ent->genericBolt1;
				}
			}

			vec3_t yaw_only_angles = { 0, ent->currentAngles[YAW], 0 };
			if (ent->currentAngles[YAW] != ent->client->ps.legsYaw)
			{
				yaw_only_angles[YAW] = ent->client->ps.legsYaw;
			}
			gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, bolt, &bolt_matrix, yaw_only_angles, ent->currentOrigin,
				cg.time ? cg.time : level.time, nullptr, ent->s.modelScale);

			// work the matrix axis stuff into the original axis and origins used.
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, ent->client->renderInfo.muzzlePoint);
			gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, ent->client->renderInfo.muzzleDir);
			ent->client->renderInfo.mPCalcTime = level.time;

			AngleVectors(ent->client->ps.viewangles, forward_vec, vright_vec, up);
		}
		else if (!ent->enemy)
		{
			//an NPC with no enemy to auto-aim at
			VectorCopy(ent->client->renderInfo.muzzleDir, forward_vec);
		}
		else
		{
			vec3_t angle_to_enemy1;
			vec3_t enemy_org1;
			vec3_t delta1;
			//NPC, auto-aim at enemy
			CalcEntitySpot(ent->enemy, SPOT_HEAD, enemy_org1);

			VectorSubtract(enemy_org1, muzzle1, delta1);

			vectoangles(delta1, angle_to_enemy1);
			AngleVectors(angle_to_enemy1, forward_vec, vright_vec, up);
		}
	}
	else if (ent && ent->client && ent->s.weapon == WP_BOT_LASER && ent->enemy)
	{
		vec3_t delta1, enemy_org1, muzzle1;
		vec3_t angle_to_enemy1;

		CalcEntitySpot(ent->enemy, SPOT_HEAD, enemy_org1);
		CalcEntitySpot(ent, SPOT_WEAPON, muzzle1);

		VectorSubtract(enemy_org1, muzzle1, delta1);

		vectoangles(delta1, angle_to_enemy1);
		AngleVectors(angle_to_enemy1, forward_vec, vright_vec, up);
	}
	else
	{
		if ((p_veh = G_IsRidingVehicle(ent)) != nullptr) //riding a vehicle
		{
			//use our muzzleDir, can't use viewangles or vehicle m_vOrientation because we may be animated to shoot left or right...
			if (ent->s.eFlags & EF_NODRAW) //we're inside it
			{
				vec3_t aim_angles;
				VectorCopy(ent->client->renderInfo.muzzleDir, forward_vec);
				vectoangles(forward_vec, aim_angles);
				//we're only keeping the yaw
				aim_angles[PITCH] = ent->client->ps.viewangles[PITCH];
				aim_angles[ROLL] = 0;
				AngleVectors(aim_angles, forward_vec, vright_vec, up);
			}
			else
			{
				vec3_t actor_right;
				vec3_t actor_fwd;

				VectorCopy(ent->client->renderInfo.muzzlePoint, muzzle);
				AngleVectors(ent->currentAngles, actor_fwd, actor_right, nullptr);

				// Aiming Left
				//-------------
				if (ent->client->ps.torsoAnim == BOTH_VT_ATL_G || ent->client->ps.torsoAnim == BOTH_VS_ATL_G)
				{
					VectorScale(actor_right, -1.0f, forward_vec);
				}

				// Aiming Right
				//--------------
				else if (ent->client->ps.torsoAnim == BOTH_VT_ATR_G || ent->client->ps.torsoAnim == BOTH_VS_ATR_G)
				{
					VectorCopy(actor_right, forward_vec);
				}

				// Aiming Forward
				//----------------
				else
				{
					VectorCopy(actor_fwd, forward_vec);
				}

				// If We Have An Enemy, Fudge The Aim To Hit The Enemy
				if (ent->enemy)
				{
					vec3_t to_enemy;
					VectorSubtract(ent->enemy->currentOrigin, ent->currentOrigin, to_enemy);
					VectorNormalize(to_enemy);
					if (DotProduct(to_enemy, forward_vec) > 0.75f &&
						(ent->s.number == 0 && !Q_irand(0, 2) || // the player has a 1 in 3 chance
							ent->s.number != 0 && !Q_irand(0, 5))) // other guys have a 1 in 6 chance
					{
						VectorCopy(to_enemy, forward_vec);
					}
					else
					{
						forward_vec[0] += Q_flrand(-0.1f, 0.1f);
						forward_vec[1] += Q_flrand(-0.1f, 0.1f);
						forward_vec[2] += Q_flrand(-0.1f, 0.1f);
					}
				}
			}
		}
		else
		{
			AngleVectors(ent->client->ps.viewangles, forward_vec, vright_vec, up);
		}
	}

	ent->alt_fire = alt_fire;
	if (!p_veh)
	{
		if (ent->NPC && ent->NPC->scriptFlags & SCF_FIRE_WEAPON_NO_ANIM)
		{
			VectorCopy(ent->client->renderInfo.muzzlePoint, muzzle);
			VectorCopy(ent->client->renderInfo.muzzleDir, forward_vec);
			MakeNormalVectors(forward_vec, vright_vec, up);
		}
		else
		{
			CalcMuzzlePoint(ent, forward_vec, muzzle, 0);

			if (!cg_trueguns.integer && !cg.renderingThirdPerson && (ent->client->ps.eFlags & EF2_JANGO_DUALS || ent->client->ps.eFlags & EF2_DUAL_CLONE_PISTOLS || ent->client->ps.eFlags & EF2_DUAL_PISTOLS))
			{
				CalcMuzzlePoint2(ent, muzzle2, 0);
			}
			if (ent->s.weapon == WP_DROIDEKA)
			{
				CalcMuzzlePoint2(ent, muzzle2, 0);
			}

			if (!doesnot_drain_mishap(ent))
			{
				if (ent->s.weapon == WP_REPEATER)
				{
					ent->client->cloneFired++;

					if (ent->client->cloneFired == 4)
					{
						if (ent->client->pers.cmd.forwardmove == 0 && ent->client->pers.cmd.rightmove == 0)
						{
							G_AddBlasterAttackChainCount(ent, BLASTERMISHAPLEVEL_MIN);
						}
						else
						{
							G_AddBlasterAttackChainCount(ent, BLASTERMISHAPLEVEL_TWO);
						}

						ent->client->cloneFired = 0;
					}
				}
				else if (ent->s.weapon == WP_Z6_ROTARY_CANNON)
				{
					ent->client->cloneFired++;

					if (ent->client->cloneFired == 4)
					{
						G_AddBlasterAttackChainCount(ent, BLASTERMISHAPLEVEL_MIN);

						ent->client->cloneFired = 0;
					}
				}
				else if (ent->s.weapon == WP_DROIDEKA)
				{
					ent->client->DekaFired++;

					if (ent->client->DekaFired == 5)
					{
						G_AddBlasterAttackChainCount(ent, BLASTERMISHAPLEVEL_MIN);

						ent->client->DekaFired = 0;
					}
				}
				else if (ent->s.weapon == WP_DISRUPTOR)
				{
					G_AddBlasterAttackChainCount(ent, BLASTERMISHAPLEVEL_THREE);
				}
				else
				{
					ent->client->BoltsFired++;

					if (ent->client->BoltsFired == 2)
					{
						if (ent->client->pers.cmd.forwardmove == 0 && ent->client->pers.cmd.rightmove == 0)
						{
							G_AddBlasterAttackChainCount(ent, BLASTERMISHAPLEVEL_MIN);
						}
						else
						{
							G_AddBlasterAttackChainCount(ent, Q_irand(BLASTERMISHAPLEVEL_MIN, BLASTERMISHAPLEVEL_TWO)); // 1 was not enough
						}

						ent->client->BoltsFired = 0;
					}
				}
			}
		}
	}

	// fire the specific weapon
	switch (ent->s.weapon)
	{
		// Player weapons
		//-----------------
	case WP_SABER:
		return;

	case WP_BRYAR_PISTOL:
		WP_FireBryarPistol(ent, alt_fire);
		break;
	case WP_BLASTER_PISTOL:
		if (!cg_trueguns.integer && !cg.renderingThirdPerson)
		{
			WP_FireBryarPistolDuals(ent, alt_fire, qfalse);

			if (ent->client->ps.eFlags & EF2_DUAL_PISTOLS)
			{
				WP_FireBryarPistolDuals(ent, alt_fire, qtrue);
			}
		}
		else
		{
			WP_FireBryarPistol(ent, alt_fire);
		}
		break;

	case WP_BLASTER:
		WP_FireBlaster(ent, alt_fire);
		break;

	case WP_TUSKEN_RIFLE:
		if (alt_fire)
		{
			WP_FireTuskenRifle(ent);
		}
		else
		{
			WP_Melee(ent);
		}
		break;

	case WP_DISRUPTOR:
		alert = 50; // if you want it to alert enemies, remove this
		WP_FireDisruptor(ent, alt_fire);
		break;

	case WP_BOWCASTER:
		WP_FireBowcaster(ent, alt_fire);
		break;

	case WP_REPEATER:
		WP_FireRepeater(ent, alt_fire);
		break;

	case WP_DEMP2:
		WP_FireDEMP2(ent, alt_fire);
		break;

	case WP_FLECHETTE:
		WP_FireFlechette(ent, alt_fire);
		break;

	case WP_ROCKET_LAUNCHER:
		WP_FireRocket(ent, alt_fire);
		break;

	case WP_CONCUSSION:
		WP_Concussion(ent, alt_fire);
		break;

	case WP_THERMAL:
		WP_FireThermalDetonator(ent, alt_fire);
		break;

	case WP_TRIP_MINE:
		alert = 0; // if you want it to alert enemies, remove this
		WP_PlaceLaserTrap(ent, alt_fire);
		break;

	case WP_DET_PACK:
		alert = 0; // if you want it to alert enemies, remove this
		WP_FireDetPack(ent, alt_fire);
		break;

	case WP_BOT_LASER:
		WP_BotLaser(ent);
		break;

	case WP_EMPLACED_GUN:
		// doesn't care about whether it's alt-fire or not.  We can do an alt-fire if needed
		WP_EmplacedFire(ent);
		break;

	case WP_MELEE:
		alert = 0; // if you want it to alert enemies, remove this
		if (!alt_fire)
		{
			WP_Melee(ent);
		}
		break;

	case WP_ATST_MAIN:
		WP_ATSTMainFire(ent);
		break;

	case WP_ATST_SIDE:

		// TEMP
		if (alt_fire)
		{
			WP_ATSTSideAltFire(ent);
		}
		else
		{
			WP_ATSTSideFire(ent);
		}
		break;

	case WP_TIE_FIGHTER:
		// TEMP
		WP_EmplacedFire(ent);
		break;

	case WP_RAPID_FIRE_CONC:
		// TEMP
		if (alt_fire)
		{
			WP_FireRepeater(ent, alt_fire);
		}
		else
		{
			WP_EmplacedFire(ent);
		}
		break;

	case WP_STUN_BATON:
		WP_FireStunBaton(ent, alt_fire);
		break;

	case WP_JAWA:
		WP_FireJawaPistol(ent, qfalse); // never an alt-fire?
		break;

	case WP_SCEPTER:
		WP_FireScepter(ent);
		break;

	case WP_NOGHRI_STICK:
		if (!alt_fire)
		{
			WP_FireNoghriStick(ent);
		}
		else
		{
			WP_Melee(ent);
		}
		break;

	case WP_BATTLEDROID:
		WP_FireBattleDroid(ent, alt_fire);
		break;

	case WP_THEFIRSTORDER:
		WP_FireFirstOrder(ent, alt_fire);
		break;

	case WP_CLONECARBINE:
		WP_FireClone(ent, alt_fire);
		break;

	case WP_REBELBLASTER:
		if (!cg_trueguns.integer && !cg.renderingThirdPerson)
		{
			WP_FireRebelBlasterDuals(ent, alt_fire, qfalse);

			if (ent->client->ps.eFlags & EF2_DUAL_PISTOLS)
			{
				WP_FireRebelBlasterDuals(ent, alt_fire, qtrue);
			}
		}
		else
		{
			WP_FireRebelBlaster(ent, alt_fire);
		}
		break;

	case WP_CLONERIFLE:
		WP_FireCloneRifle(ent, alt_fire);
		break;

	case WP_CLONECOMMANDO:
		WP_FireCloneCommando(ent, alt_fire);
		break;

	case WP_Z6_ROTARY_CANNON:
		WP_FireZ6RotaryCannon(ent);
		break;

	case WP_REBELRIFLE:
		WP_FireRebelRifle(ent, alt_fire);
		break;

	case WP_REY:
		if (!cg_trueguns.integer && !cg.renderingThirdPerson)
		{
			WP_FireReyPistolDuals(ent, alt_fire, qfalse);

			if (ent->client->ps.eFlags & EF2_DUAL_PISTOLS)
			{
				WP_FireReyPistolDuals(ent, alt_fire, qtrue);
			}
		}
		else
		{
			WP_FireReyPistol(ent, alt_fire);
		}
		break;

	case WP_JANGO:
		if (!cg_trueguns.integer && !cg.renderingThirdPerson)
		{
			WP_FireJangoPistol(ent, alt_fire, qfalse);

			if (ent->client->ps.eFlags & EF2_DUAL_PISTOLS)
			{
				WP_FireJangoPistol(ent, alt_fire, qtrue);
			}
		}
		else
		{
			WP_FireJangoPistol(ent, alt_fire, qfalse);
		}
		break;
	case WP_DUAL_PISTOL:
		if (!cg_trueguns.integer && !cg.renderingThirdPerson)
		{
			WP_FireJangoFPPistolDuals(ent, alt_fire, qfalse);

			if (ent->client->ps.eFlags & EF2_JANGO_DUALS)
			{
				WP_FireJangoFPPistolDuals(ent, alt_fire, qtrue);
			}
		}
		else
		{
			WP_FireJangoDualPistol(ent, alt_fire);
		}
		break;
	case WP_DUAL_CLONEPISTOL:
		if (!cg_trueguns.integer && !cg.renderingThirdPerson)
		{
			WP_FireMandoClonePistolDuals(ent, alt_fire, qfalse);

			if (ent->client->ps.eFlags & EF2_DUAL_CLONE_PISTOLS)
			{
				WP_FireMandoClonePistolDuals(ent, alt_fire, qtrue);
			}
		}
		else
		{
			WP_FireClonePistol(ent, alt_fire);
		}
		break;

	case WP_DROIDEKA:
		if (!cg_trueguns.integer && !cg.renderingThirdPerson)
		{
			WP_FireDroidekaFPPistolDuals(ent, alt_fire, qtrue); // in first person // should never happen with deka
		}
		else
		{
			WP_FireDroidekaDualPistol(ent, alt_fire);
		}
		break;

	case WP_BOBA:
	{
		if (alt_fire)
		{
			for (int i = 0; i < 3; i++)
			{
				WP_FireBobaRifle(ent, alt_fire);
			}
			break;
		}
		WP_FireBobaRifle(ent, alt_fire);
		break;
	}

	case WP_CLONEPISTOL:
		if (!cg_trueguns.integer && !cg.renderingThirdPerson)
		{
			WP_FireClonePistolDuals(ent, alt_fire, qfalse);

			if (ent->client->ps.eFlags & EF2_DUAL_PISTOLS)
			{
				WP_FireClonePistolDuals(ent, alt_fire, qtrue);
			}
		}
		else
		{
			WP_FireClonePistol(ent, alt_fire);
		}
		break;
	case WP_SBD_BLASTER:
		WP_FireSBDPistol(ent, alt_fire);
		break;
	case WP_WRIST_BLASTER:
		WP_FireWristPistol(ent, alt_fire);
		break;

	case WP_TUSKEN_STAFF:
	default:
		return;
	}

	if (!ent->s.number)
	{
		if (ent->s.weapon == WP_FLECHETTE || ent->s.weapon == WP_BOWCASTER && !alt_fire)
		{
			//these can fire multiple shots, count them individually within the firing functions
		}
		else if (W_AccuracyLoggableWeapon(ent->s.weapon, alt_fire, MOD_UNKNOWN))
		{
			ent->client->sess.missionStats.shotsFired++;
		}
	}
	// We should probably just use this as a default behavior, in special cases, just set alert to false.
	if (ent->s.number == 0 && alert > 0)
	{
		if (ent->client->ps.groundEntityNum == ENTITYNUM_WORLD //FIXME: check for sand contents type?
			&& ent->s.weapon != WP_STUN_BATON
			&& ent->s.weapon != WP_MELEE
			&& ent->s.weapon != WP_TUSKEN_STAFF
			&& ent->s.weapon != WP_THERMAL
			&& ent->s.weapon != WP_TRIP_MINE
			&& ent->s.weapon != WP_DET_PACK)
		{
			//the vibration of the shot carries through your feet into the ground
			AddSoundEvent(ent, muzzle, alert, AEL_DISCOVERED, qfalse, qtrue);
		}
		else
		{
			//an in-air alert
			AddSoundEvent(ent, muzzle, alert, AEL_DISCOVERED);
		}
		AddSightEvent(ent, muzzle, alert * 2, AEL_DISCOVERED, 20);
	}
}

void misc_weapon_shooter_fire(gentity_t* self)
{
	FireWeapon(self, static_cast<qboolean>((self->spawnflags & 1) != 0));

	if (self->spawnflags & 2)
	{
		//repeat
		self->e_ThinkFunc = thinkF_misc_weapon_shooter_fire;
		if (self->random)
		{
			self->nextthink = level.time + self->wait + static_cast<int>(Q_flrand(0.0f, 1.0f) * self->random);
		}
		else
		{
			self->nextthink = level.time + self->wait;
		}
	}
}

void misc_weapon_shooter_use(gentity_t* self, gentity_t* other, gentity_t* activator)
{
	if (self->e_ThinkFunc == thinkF_misc_weapon_shooter_fire)
	{
		//repeating fire, stop
		self->e_ThinkFunc = thinkF_NULL;
		self->nextthink = -1;
		return;
	}
	//otherwise, fire
	misc_weapon_shooter_fire(self);
}

void misc_weapon_shooter_aim(gentity_t* self)
{
	//update my aim
	if (self->target)
	{
		gentity_t* targ = G_Find(nullptr, FOFS(targetname), self->target);
		if (targ)
		{
			self->enemy = targ;
			VectorSubtract(targ->currentOrigin, self->currentOrigin, self->client->renderInfo.muzzleDir);
			VectorCopy(targ->currentOrigin, self->pos1);
			vectoangles(self->client->renderInfo.muzzleDir, self->client->ps.viewangles);
			SetClientViewAngle(self, self->client->ps.viewangles);
			//FIXME: don't keep doing this unless target is a moving target?
			self->nextthink = level.time + FRAMETIME;
		}
		else
		{
			self->enemy = nullptr;
		}
	}
}

extern stringID_table_t WPTable[];

void SP_misc_weapon_shooter(gentity_t* self)
{
	//alloc a client just for the weapon code to use
	self->client = static_cast<gclient_t*>(gi.Malloc(sizeof(gclient_t), TAG_G_ALLOC, qtrue));

	//set weapon
	self->s.weapon = self->client->ps.weapon = WP_BLASTER;
	if (self->paintarget)
	{
		//use a different weapon
		self->s.weapon = self->client->ps.weapon = GetIDForString(WPTable, self->paintarget);
	}

	//set where our muzzle is
	VectorCopy(self->s.origin, self->client->renderInfo.muzzlePoint);
	//permanently updated
	self->client->renderInfo.mPCalcTime = Q3_INFINITE;

	//set up to link
	if (self->target)
	{
		self->e_ThinkFunc = thinkF_misc_weapon_shooter_aim;
		self->nextthink = level.time + START_TIME_LINK_ENTS;
	}
	else
	{
		//just set aim angles
		VectorCopy(self->s.angles, self->client->ps.viewangles);
		AngleVectors(self->s.angles, self->client->renderInfo.muzzleDir, nullptr, nullptr);
	}

	//set up to fire when used
	self->e_UseFunc = useF_misc_weapon_shooter_use;

	if (!self->wait)
	{
		self->wait = 500;
	}
}

extern gentity_t* fire_stun(gentity_t* self, vec3_t start, vec3_t dir);

void Weapon_AltStun_Fire(gentity_t* ent)
{
	vec3_t forward, right, vup;

	AngleVectors(ent->client->ps.viewangles, forward, right, vup);
	CalcMuzzlePoint(ent, forward, muzzle, 0);
	if (!ent->client->stunHeld && !ent->client->stun)
	{
		fire_stun(ent, muzzle, forward);
	}
	ent->client->stunHeld = qtrue;
}

void Weapon_StunFree(gentity_t* ent)
{
	ent->parent->client->stunHeld = qfalse;
	ent->parent->client->stunhasbeenfired = qfalse;
	ent->parent->client->stun = nullptr;
	G_FreeEntity(ent);
}

void Weapon_StunThink(gentity_t* ent)
{
	if (ent->enemy)
	{
		vec3_t v{}, oldorigin;

		VectorCopy(ent->currentOrigin, oldorigin);

		G_SetOrigin(ent, v);
		ent->nextthink = level.time + 50; //continue to think if attached to an enemy!
	}

	VectorCopy(ent->currentOrigin, ent->parent->client->ps.lastHitLoc);
}

extern gentity_t* fire_grapple(gentity_t* self, vec3_t start, vec3_t dir);

void Weapon_GrapplingHook_Fire(gentity_t* ent)
{
	vec3_t forward, right, vup;

	AngleVectors(ent->client->ps.viewangles, forward, right, vup);
	CalcMuzzlePoint(ent, forward, muzzle, 0);
	if (!ent->client->fireHeld && !ent->client->hook)
	{
		fire_grapple(ent, muzzle, forward);
	}
	ent->client->fireHeld = qtrue;
}

void Weapon_MandoGrappleHook_Fire(gentity_t* ent)
{
	vec3_t forward, right, vup;

	AngleVectors(ent->client->ps.viewangles, forward, right, vup);
	CalcMuzzlePoint(ent, forward, muzzle, 0);
	if (!ent->client->hook)
	{
		fire_grapple(ent, muzzle, forward);
	}
	ent->client->fireHeld = qtrue;
}

/*
======================
SnapVectorTowards

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating
into a wall.
======================
*/
void SnapVectorTowards(vec3_t v, vec3_t to)
{
	for (int i = 0; i < 3; i++)
	{
		if (to[i] <= v[i])
		{
			v[i] = floorf(v[i]);
		}
		else
		{
			v[i] = ceilf(v[i]);
		}
	}
}

void Weapon_HookThink(gentity_t* ent)
{
	if (ent->enemy)
	{
		vec3_t v{}, oldorigin;

		VectorCopy(ent->currentOrigin, oldorigin);
		v[0] = ent->enemy->currentOrigin[0] + (ent->enemy->mins[0] + ent->enemy->maxs[0]) * 0.5;
		v[1] = ent->enemy->currentOrigin[1] + (ent->enemy->mins[1] + ent->enemy->maxs[1]) * 0.5;
		v[2] = ent->enemy->currentOrigin[2] + (ent->enemy->mins[2] + ent->enemy->maxs[2]) * 0.5;
		SnapVectorTowards(v, oldorigin); // save net bandwidth

		G_SetOrigin(ent, v);
		ent->nextthink = level.time + 50; //continue to think if attached to an enemy!
	}

	VectorCopy(ent->currentOrigin, ent->parent->client->ps.lastHitLoc);
}

void Weapon_HookFree(gentity_t* ent)
{
	ent->parent->client->fireHeld = qfalse;
	ent->parent->client->hookhasbeenfired = qfalse;
	ent->parent->client->hook = nullptr;
	ent->parent->client->ps.pm_flags &= ~PMF_GRAPPLE_PULL;
	G_FreeEntity(ent);
}