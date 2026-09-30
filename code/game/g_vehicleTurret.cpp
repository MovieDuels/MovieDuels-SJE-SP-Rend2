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

// The turrets of a vehicle (the two of the YT-1300, the one of the Y-wing): SP's version of codemp/game/g_vehicleTurret.c.
// A turret with "turretNAI" in the .veh finds its own enemies while the ship has a pilot, turns its bones onto them
// and fires. What MP has and this does not: a passenger taking a turret over (SP has no passengers).

#include "g_local.h"
#include "g_functions.h"
#include "g_vehicles.h"
#include "b_local.h"
#include "../cgame/cg_local.h"

extern void G_SetEnemy(gentity_t* self, gentity_t* enemy);
extern void WP_FireVehicleWeapon(gentity_t* ent, vec3_t start, vec3_t dir, const vehWeaponInfo_t* vehWeapon,
	qboolean is_turret_weap);

// Where a muzzle of the ship is and points right now. (The cgame works these out too, once a rendered frame; a turret
// needs them again right after it has turned its bones.)
static void VEH_TurretCalcMuzzle(gentity_t* parent, const int muzzle_num)
{
	Vehicle_t* p_veh = parent->m_pVehicle;
	mdxaBone_t bolt_matrix;

	gi.G2API_GetBoltMatrix(parent->ghoul2, parent->playerModel, p_veh->m_iMuzzleTag[muzzle_num], &bolt_matrix,
		p_veh->m_vOrientation, parent->currentOrigin, cg.time ? cg.time : level.time, nullptr, parent->s.modelScale);
	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, ORIGIN, p_veh->m_Muzzles[muzzle_num].m_vMuzzlePos);
	gi.G2API_GiveMeVectorFromMatrix(bolt_matrix, NEGATIVE_Y, p_veh->m_Muzzles[muzzle_num].m_vMuzzleDir);
}

// Turn a bone of the ship's model (same axes as MP's NPC_SetBoneAngles). The cgame draws the same ghoul2 model.
static void VEH_TurretSetBoneAngles(gentity_t* parent, const char* bone, const vec3_t angles)
{
	gi.G2API_SetBoneAngles(&parent->ghoul2[parent->playerModel], bone, angles, BONE_ANGLES_POSTMULT, POSITIVE_X,
		NEGATIVE_Y, NEGATIVE_Z, nullptr, 100, cg.time ? cg.time : level.time);
}

static void VEH_AnglesSubtract(const vec3_t v1, const vec3_t v2, vec3_t v3)
{
	v3[0] = AngleSubtract(v1[0], v2[0]);
	v3[1] = AngleSubtract(v1[1], v2[1]);
	v3[2] = AngleSubtract(v1[2], v2[2]);
}

// A turret that belongs to a map (misc_turret, misc_turretG2...), not to a ship.
static qboolean VEH_TurretIsMapTurret(const gentity_t* ent)
{
	return static_cast<qboolean>(ent->s.weapon == WP_TURRET && ent->classname
		&& Q_strncmp("misc_turret", ent->classname, 11) == 0);
}

//-----------------------------------------------------
static void VEH_TurretCheckFire(Vehicle_t* p_veh,
	gentity_t* parent,
	const turretStats_t* turret_stats,
	const vehWeaponInfo_t* veh_weapon,
	const int turret_num, const int cur_muzzle)
{
	// if it's time to fire and we have an enemy, then gun 'em down!
	if (p_veh->m_iMuzzleTag[cur_muzzle] == -1)
	{
		//invalid muzzle?
		return;
	}

	if (p_veh->m_Muzzles[cur_muzzle].m_iMuzzleWait >= level.time)
	{
		//can't fire yet
		return;
	}

	if (p_veh->turretStatus[turret_num].ammo < veh_weapon->iAmmoPerShot)
	{
		//no ammo, can't fire
		return;
	}

	VEH_TurretCalcMuzzle(parent, cur_muzzle);

	//play the weapon's muzzle effect if we have one
	if (veh_weapon->iMuzzleFX)
	{
		G_PlayEffect(veh_weapon->iMuzzleFX, p_veh->m_Muzzles[cur_muzzle].m_vMuzzlePos,
			p_veh->m_Muzzles[cur_muzzle].m_vMuzzleDir);
	}
	WP_FireVehicleWeapon(parent, p_veh->m_Muzzles[cur_muzzle].m_vMuzzlePos, p_veh->m_Muzzles[cur_muzzle].m_vMuzzleDir,
		veh_weapon, qtrue);

	//take the ammo away
	p_veh->turretStatus[turret_num].ammo -= veh_weapon->iAmmoPerShot;
	if (parent->client)
	{
		parent->client->ps.ammo[MAX_VEHICLE_WEAPONS + turret_num] = p_veh->turretStatus[turret_num].ammo;
	}
	//toggle to the next muzzle on this turret, if there is one
	const int next_muzzle = cur_muzzle + 1 == p_veh->m_pVehicleInfo->turret[turret_num].iMuzzle[0]
		? p_veh->m_pVehicleInfo->turret[turret_num].iMuzzle[1]
		: p_veh->m_pVehicleInfo->turret[turret_num].iMuzzle[0];
	if (next_muzzle > 0 && next_muzzle <= MAX_VEHICLE_MUZZLES)
	{
		//a valid muzzle to toggle to
		p_veh->turretStatus[turret_num].nextMuzzle = next_muzzle - 1; //-1 because you type muzzles 1-10 in the .veh file
	}
	//add delay to the next muzzle so it doesn't fire right away on the next frame
	p_veh->m_Muzzles[p_veh->turretStatus[turret_num].nextMuzzle].m_iMuzzleWait = level.time + turret_stats->iDelay;
}

static void VEH_TurretAnglesToEnemy(const Vehicle_t* p_veh, const int cur_muzzle, const float f_speed,
	const gentity_t* turret_enemy, const qboolean b_ai_lead, vec3_t desired_angles)
{
	vec3_t enemy_dir, org;
	VectorCopy(turret_enemy->currentOrigin, org);
	if (b_ai_lead && f_speed > 0.0f)
	{
		//we want to lead them a bit
		vec3_t diff, velocity;
		VectorSubtract(org, p_veh->m_Muzzles[cur_muzzle].m_vMuzzlePos, diff);
		const float dist = VectorNormalize(diff);
		if (turret_enemy->client)
		{
			VectorCopy(turret_enemy->client->ps.velocity, velocity);
		}
		else
		{
			VectorCopy(turret_enemy->s.pos.trDelta, velocity);
		}
		VectorMA(org, dist / f_speed, velocity, org);
	}

	//FIXME: this isn't quite right, it's aiming from the muzzle, not the center of the turret...
	VectorSubtract(org, p_veh->m_Muzzles[cur_muzzle].m_vMuzzlePos, enemy_dir);
	//Get the desired absolute, world angles to our target
	vectoangles(enemy_dir, desired_angles);
}

//-----------------------------------------------------
static qboolean VEH_TurretAim(Vehicle_t* p_veh,
	gentity_t* parent,
	const gentity_t* turret_enemy,
	const turretStats_t* turret_stats,
	const vehWeaponInfo_t* veh_weapon,
	const int turret_num, const int cur_muzzle, vec3_t desired_angles)
	//-----------------------------------------------------
{
	vec3_t cur_angles, add_angles, new_angles = { 0 };
	qboolean aim_correct = qfalse;

	VEH_TurretCalcMuzzle(parent, cur_muzzle);
	//get the current absolute angles of the turret right now
	vectoangles(p_veh->m_Muzzles[cur_muzzle].m_vMuzzleDir, cur_angles);
	//subtract out the vehicle's angles to get the relative alignment
	VEH_AnglesSubtract(cur_angles, p_veh->m_vOrientation, cur_angles);

	if (turret_enemy)
	{
		aim_correct = qtrue;
		// ...then we'll calculate what new aim adjustments we should attempt to make this frame
		// Aim at enemy
		VEH_TurretAnglesToEnemy(p_veh, cur_muzzle, veh_weapon->fSpeed, turret_enemy, turret_stats->bAILead,
			desired_angles);
	}
	//subtract out the vehicle's angles to get the relative desired alignment
	VEH_AnglesSubtract(desired_angles, p_veh->m_vOrientation, desired_angles);
	//Now clamp the desired relative angles
	//clamp yaw
	desired_angles[YAW] = AngleNormalize180(desired_angles[YAW]);
	if (p_veh->m_pVehicleInfo->turret[turret_num].yawClampLeft
		&& desired_angles[YAW] > p_veh->m_pVehicleInfo->turret[turret_num].yawClampLeft)
	{
		aim_correct = qfalse;
		desired_angles[YAW] = p_veh->m_pVehicleInfo->turret[turret_num].yawClampLeft;
	}
	if (p_veh->m_pVehicleInfo->turret[turret_num].yawClampRight
		&& desired_angles[YAW] < p_veh->m_pVehicleInfo->turret[turret_num].yawClampRight)
	{
		aim_correct = qfalse;
		desired_angles[YAW] = p_veh->m_pVehicleInfo->turret[turret_num].yawClampRight;
	}
	//clamp pitch
	desired_angles[PITCH] = AngleNormalize180(desired_angles[PITCH]);
	if (p_veh->m_pVehicleInfo->turret[turret_num].pitchClampDown
		&& desired_angles[PITCH] > p_veh->m_pVehicleInfo->turret[turret_num].pitchClampDown)
	{
		aim_correct = qfalse;
		desired_angles[PITCH] = p_veh->m_pVehicleInfo->turret[turret_num].pitchClampDown;
	}
	if (p_veh->m_pVehicleInfo->turret[turret_num].pitchClampUp
		&& desired_angles[PITCH] < p_veh->m_pVehicleInfo->turret[turret_num].pitchClampUp)
	{
		aim_correct = qfalse;
		desired_angles[PITCH] = p_veh->m_pVehicleInfo->turret[turret_num].pitchClampUp;
	}
	//Now get the offset we want from our current relative angles
	VEH_AnglesSubtract(desired_angles, cur_angles, add_angles);
	//Now cap the addAngles for our fTurnSpeed
	if (add_angles[PITCH] > turret_stats->fTurnSpeed)
	{
		add_angles[PITCH] = turret_stats->fTurnSpeed;
	}
	else if (add_angles[PITCH] < -turret_stats->fTurnSpeed)
	{
		add_angles[PITCH] = -turret_stats->fTurnSpeed;
	}
	if (add_angles[YAW] > turret_stats->fTurnSpeed)
	{
		add_angles[YAW] = turret_stats->fTurnSpeed;
	}
	else if (add_angles[YAW] < -turret_stats->fTurnSpeed)
	{
		add_angles[YAW] = -turret_stats->fTurnSpeed;
	}
	//Now add the additional angles back in to our current relative angles
	//FIXME: add some AI aim error randomness...?
	new_angles[PITCH] = AngleNormalize180(cur_angles[PITCH] + add_angles[PITCH]);
	new_angles[YAW] = AngleNormalize180(cur_angles[YAW] + add_angles[YAW]);
	//Now set the bone angles to the new angles
	//set yaw
	if (turret_stats->yawBone && turret_stats->yawAxis >= 0 && turret_stats->yawAxis < 3)
	{
		vec3_t yaw_angles;
		VectorClear(yaw_angles);
		yaw_angles[turret_stats->yawAxis] = new_angles[YAW];
		VEH_TurretSetBoneAngles(parent, turret_stats->yawBone, yaw_angles);
	}
	//set pitch
	if (turret_stats->pitchBone && turret_stats->pitchAxis >= 0 && turret_stats->pitchAxis < 3)
	{
		vec3_t pitch_angles;
		VectorClear(pitch_angles);
		pitch_angles[turret_stats->pitchAxis] = new_angles[PITCH];
		VEH_TurretSetBoneAngles(parent, turret_stats->pitchBone, pitch_angles);
	}

	return aim_correct;
}

// Whose side a target is on, as the turret sees it: a ship counts as its pilot's.
static team_t VEH_TurretTargetTeam(const gentity_t* target)
{
	if (target->m_pVehicle && target->m_pVehicle->m_pPilot && target->m_pVehicle->m_pPilot->client)
	{
		return target->m_pVehicle->m_pPilot->client->playerTeam;
	}
	return target->client->playerTeam;
}

// Something this ship's turrets may shoot at (or go on shooting at).
static qboolean VEH_TurretValidEnemy(const Vehicle_t* p_veh, const gentity_t* parent, const gentity_t* target)
{
	const gentity_t* pilot = p_veh->m_pPilot;

	if (!target || !target->inuse
		|| target == parent
		|| !target->takedamage
		|| target->health <= 0
		|| target->flags & FL_NOTARGET)
	{
		return qfalse;
	}

	// Don't shoot the pilot or anyone (anything) else that is aboard
	if (target == pilot || target->owner == parent || target->s.m_iVehicleNum == parent->s.number)
	{
		return qfalse;
	}

	const team_t my_team = pilot && pilot->client ? pilot->client->playerTeam : TEAM_NEUTRAL;

	if (!target->client)
	{
		//only breakable brushes and the turrets of the map
		if (target->svFlags & SVF_BBRUSH)
		{
			if (target->NPC_targetname && parent->targetname
				&& Q_stricmp(target->NPC_targetname, parent->targetname) != 0)
			{
				//can only be broken by someone else
				return qfalse;
			}
			return qtrue;
		}
		if (!VEH_TurretIsMapTurret(target))
		{
			return qfalse;
		}
		//noDamageTeam is the side a map turret is on (the one it does not shoot at)
		return static_cast<qboolean>(target->noDamageTeam != my_team);
	}

	if (target->m_pVehicle)
	{
		//a ship: only with someone in it (he counts: his team, his notarget)
		const gentity_t* target_pilot = target->m_pVehicle->m_pPilot;
		if (!target_pilot || target_pilot->flags & FL_NOTARGET)
		{
			return qfalse;
		}
	}
	else if (target->s.m_iVehicleNum)
	{
		//someone aboard another ship: if he can't be seen in it, the ship is the target
		const gentity_t* veh = &g_entities[target->s.m_iVehicleNum];
		if (veh->m_pVehicle && veh->m_pVehicle->m_pVehicleInfo && veh->m_pVehicle->m_pVehicleInfo->hideRider)
		{
			return qfalse;
		}
	}

	const team_t target_team = VEH_TurretTargetTeam(target);
	if (target_team == my_team || target_team == TEAM_NEUTRAL)
	{
		//my side, or nobody's
		return qfalse;
	}
	return qtrue;
}

//-----------------------------------------------------
static qboolean VEH_TurretFindEnemies(Vehicle_t* p_veh,
	gentity_t* parent,
	const turretStats_t* turret_stats,
	const int turret_num, const int cur_muzzle)
{
	qboolean found = qfalse;
	float best_dist = turret_stats->fAIRange * turret_stats->fAIRange;
	vec3_t org2;
	qboolean found_client = qfalse;
	static gentity_t* entity_list[MAX_GENTITIES]; // too big for the stack
	const gentity_t* best_target = nullptr;

	if (!p_veh->m_pPilot)
	{
		// only try to find enemies if the vehicle has a pilot
		return qfalse;
	}

	VEH_TurretCalcMuzzle(parent, cur_muzzle);
	VectorCopy(p_veh->m_Muzzles[cur_muzzle].m_vMuzzlePos, org2);

	const int count = G_RadiusList(org2, turret_stats->fAIRange, parent, qtrue, entity_list);

	for (int i = 0; i < count; i++)
	{
		const gentity_t* target = entity_list[i];
		trace_t tr;

		if (!VEH_TurretValidEnemy(p_veh, parent, target))
		{
			continue;
		}

		if (!gi.inPVS(org2, target->currentOrigin))
		{
			continue;
		}

		gi.trace(&tr, org2, nullptr, nullptr, target->currentOrigin, parent->s.number, MASK_SHOT,
			static_cast<EG2_Collision>(0), 0);

		if (tr.entityNum == target->s.number || !tr.allsolid && !tr.startsolid && tr.fraction == 1.0f)
		{
			// Only acquire if have a clear shot, Is it in range and closer than our best?
			vec3_t enemy_dir;
			VectorSubtract(target->currentOrigin, org2, enemy_dir);
			const float enemy_dist = VectorLengthSquared(enemy_dir);

			if (enemy_dist < best_dist || target->client && !found_client)
			{
				//prefer clients over non-clients
				best_target = target;
				best_dist = enemy_dist;
				found = qtrue;
				if (target->client)
				{
					found_client = qtrue;
				}
			}
		}
	}

	if (found)
	{
		p_veh->turretStatus[turret_num].enemyEntNum = best_target->s.number;
	}

	return found;
}

void VEH_TurretThink(Vehicle_t* p_veh, gentity_t* parent, const int turret_num)
//-----------------------------------------------------
{
	qboolean do_aim = qfalse;
	const turretStats_t* turret_stats = &p_veh->m_pVehicleInfo->turret[turret_num];
	const gentity_t* turret_enemy = nullptr;

	if (!turret_stats->iAmmoMax)
	{
		//not a valid turret
		return;
	}

	// Bad .veh data must not index out of bounds: weapon slot (0 = "not found" from the loader, -1 = none) and the
	// current muzzle (from turretNMuzzle1/2 - 1, unchecked at load)
	if (turret_stats->iWeapon <= VEH_WEAPON_BASE || turret_stats->iWeapon >= numVehicleWeapons
		|| p_veh->turretStatus[turret_num].nextMuzzle < 0
		|| p_veh->turretStatus[turret_num].nextMuzzle >= MAX_VEHICLE_MUZZLES
		|| p_veh->m_iMuzzleTag[p_veh->turretStatus[turret_num].nextMuzzle] == -1)
	{
		return;
	}

	if (!parent || !parent->client || parent->health <= 0 || !parent->ghoul2.size() || parent->playerModel < 0)
	{
		return;
	}

	if (!turret_stats->bAI) //try AI
	{
		//this turret does not think on its own.
		return;
	}

	//okay, so it has AI, but still don't think if there's no pilot!
	if (!p_veh->m_pPilot)
	{
		return;
	}

	const vehWeaponInfo_t* veh_weapon = &g_vehWeaponInfo[turret_stats->iWeapon];
	const float range_sq = turret_stats->fAIRange * turret_stats->fAIRange;
	const int cur_muzzle = p_veh->turretStatus[turret_num].nextMuzzle;

	if (p_veh->turretStatus[turret_num].enemyEntNum < ENTITYNUM_WORLD)
	{
		turret_enemy = &g_entities[p_veh->turretStatus[turret_num].enemyEntNum];
		if (!VEH_TurretValidEnemy(p_veh, parent, turret_enemy))
		{
			//don't keep going after the pilot, self, dead people, etc.
			turret_enemy = nullptr;
			p_veh->turretStatus[turret_num].enemyEntNum = ENTITYNUM_NONE;
		}
	}

	if (p_veh->turretStatus[turret_num].enemyHoldTime < level.time)
	{
		if (VEH_TurretFindEnemies(p_veh, parent, turret_stats, turret_num, cur_muzzle))
		{
			turret_enemy = &g_entities[p_veh->turretStatus[turret_num].enemyEntNum];
			do_aim = qtrue;
		}
		else if (parent->enemy && VEH_TurretValidEnemy(p_veh, parent, parent->enemy))
		{
			turret_enemy = parent->enemy;
			do_aim = qtrue;
		}
		if (turret_enemy)
		{
			//found one
			if (turret_enemy->client)
			{
				//hold on to clients for a min of 3 seconds
				p_veh->turretStatus[turret_num].enemyHoldTime = level.time + 3000;
			}
			else
			{
				//hold less
				p_veh->turretStatus[turret_num].enemyHoldTime = level.time + 500;
			}
		}
	}
	if (turret_enemy != nullptr)
	{
		if (turret_enemy->health > 0)
		{
			vec3_t enemy_dir;
			// enemy is alive
			VEH_TurretCalcMuzzle(parent, cur_muzzle);
			VectorSubtract(turret_enemy->currentOrigin, p_veh->m_Muzzles[cur_muzzle].m_vMuzzlePos, enemy_dir);
			const float enemy_dist = VectorLengthSquared(enemy_dir);

			if (enemy_dist < range_sq)
			{
				// was in valid radius
				if (gi.inPVS(p_veh->m_Muzzles[cur_muzzle].m_vMuzzlePos, turret_enemy->currentOrigin))
				{
					// Every now and again, check to see if we can even trace to the enemy
					trace_t tr;
					gi.trace(&tr, p_veh->m_Muzzles[cur_muzzle].m_vMuzzlePos, nullptr, nullptr,
						turret_enemy->currentOrigin, parent->s.number, MASK_SHOT, static_cast<EG2_Collision>(0), 0);

					if (tr.entityNum == turret_enemy->s.number || !tr.allsolid && !tr.startsolid)
					{
						do_aim = qtrue; // Can see our enemy
					}
				}
			}
		}
	}

	if (do_aim)
	{
		vec3_t aim_angles;
		if (VEH_TurretAim(p_veh, parent, turret_enemy, turret_stats, veh_weapon, turret_num, cur_muzzle, aim_angles))
		{
			VEH_TurretCheckFire(p_veh, parent, turret_stats, veh_weapon, turret_num, cur_muzzle);
		}
	}
}
