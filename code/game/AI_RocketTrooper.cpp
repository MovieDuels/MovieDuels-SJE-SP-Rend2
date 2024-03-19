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

#include "b_local.h"
extern qboolean PM_FlippingAnim(int anim);
extern void NPC_BSST_Patrol();

extern void RT_FlyStart(gentity_t* self);
extern qboolean Q3_TaskIDPending(const gentity_t* ent, taskID_t taskType);

constexpr auto VELOCITY_DECAY = 0.7f;

constexpr auto RT_FLYING_STRAFE_VEL = 60;
constexpr auto RT_FLYING_STRAFE_DIS = 200;
constexpr auto RT_FLYING_UPWARD_PUSH = 150;

constexpr auto RT_FLYING_FORWARD_BASE_SPEED = 50;
constexpr auto RT_FLYING_FORWARD_MULTIPLIER = 10;

void RT_Precache()
{
	G_SoundIndex("sound/chars/boba/bf_blast-off.wav");
	G_SoundIndex("sound/chars/boba/bf_jetpack_lp.wav");
	G_SoundIndex("sound/chars/boba/bf_land.wav");
	G_EffectIndex("rockettrooper/flameNEW");
	G_EffectIndex("rockettrooper/light_cone"); //extern this?  At least use a different one
}

extern void NPC_BehaviorSet_Stormtrooper(int b_state);

static void RT_RunStormtrooperAI()
{
	int b_state;
	//Execute our b_state
	if (NPCInfo->tempBehavior)
	{
		//Overrides normal behavior until cleared
		b_state = NPCInfo->tempBehavior;
	}
	else
	{
		if (!NPCInfo->behaviorState)
			NPCInfo->behaviorState = NPCInfo->defaultBehavior;

		b_state = NPCInfo->behaviorState;
	}
	NPC_BehaviorSet_Stormtrooper(b_state);
}

void RT_FireDecide()
{
	qboolean enemy_los = qfalse;
	qboolean enemy_cs = qfalse;
	qboolean enemy_in_fov = qfalse;
	//qboolean move = qtrue;
	qboolean shoot = qfalse;
	qboolean hit_ally = qfalse;
	vec3_t impact_pos;

	if (NPC->client->ps.groundEntityNum == ENTITYNUM_NONE
		&& NPC->client->ps.forceJumpZStart
		&& !PM_FlippingAnim(NPC->client->ps.legsAnim)
		&& !Q_irand(0, 10))
	{
		//take off
		RT_FlyStart(NPC);
	}

	if (!NPC->enemy)
	{
		return;
	}

	VectorClear(impact_pos);
	const float enemy_dist = DistanceSquared(NPC->currentOrigin, NPC->enemy->currentOrigin);

	vec3_t enemy_dir, shoot_dir;
	VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, enemy_dir);
	VectorNormalize(enemy_dir);
	AngleVectors(NPC->client->ps.viewangles, shoot_dir, nullptr, nullptr);
	const float dot = DotProduct(enemy_dir, shoot_dir);
	if (dot > 0.5f || enemy_dist * (1.0f - dot) < 10000)
	{
		//enemy is in front of me or they're very close and not behind me
		enemy_in_fov = qtrue;
	}

	if (enemy_dist < MIN_ROCKET_DIST_SQUARED) //128
	{
		//enemy within 128
		if ((NPC->client->ps.weapon == WP_FLECHETTE || NPC->client->ps.weapon == WP_REPEATER) &&
			NPCInfo->scriptFlags & SCF_ALT_FIRE)
		{
			//shooting an explosive, but enemy too close, switch to primary fire
			NPCInfo->scriptFlags &= ~SCF_ALT_FIRE;
			//FIXME: we can never go back to alt-fire this way since, after this, we don't know if we were initially supposed to use alt-fire or not...
		}
	}

	//can we see our target?
	if (TIMER_Done(NPC, "nextAttackDelay") && TIMER_Done(NPC, "flameTime"))
	{
		if (NPC_ClearLOS(NPC->enemy))
		{
			NPCInfo->enemyLastSeenTime = level.time;
			enemy_los = qtrue;

			if (NPC->client->ps.weapon == WP_NONE)
			{
				enemy_cs = qfalse; //not true, but should stop us from firing
			}
			else
			{
				//can we shoot our target?
				if ((NPC->client->ps.weapon == WP_ROCKET_LAUNCHER
					|| NPC->client->ps.weapon == WP_CONCUSSION && !(NPCInfo->scriptFlags & SCF_ALT_FIRE)
					|| NPC->client->ps.weapon == WP_FLECHETTE && NPCInfo->scriptFlags & SCF_ALT_FIRE) &&
					enemy_dist <
					MIN_ROCKET_DIST_SQUARED) //128*128
				{
					enemy_cs = qfalse; //not true, but should stop us from firing
					hit_ally = qtrue; //us!
					//FIXME: if too close, run away!
				}
				else if (enemy_in_fov)
				{
					//if enemy is FOV, go ahead and check for shooting
					const int hit = NPC_ShotEntity(NPC->enemy, impact_pos);
					const gentity_t* hit_ent = &g_entities[hit];

					if (hit == NPC->enemy->s.number
						|| hit_ent && hit_ent->client && hit_ent->client->playerTeam == NPC->client->enemyTeam
						|| hit_ent && hit_ent->takedamage && (hit_ent->svFlags & SVF_GLASS_BRUSH || hit_ent->health < 40
							|| NPC->s.weapon == WP_EMPLACED_GUN))
					{
						//can hit enemy or enemy ally or will hit glass or other minor breakable (or in emplaced gun), so shoot anyway
						enemy_cs = qtrue;
						//NPC_AimAdjust( 2 );//adjust aim better longer we have clear shot at enemy
						VectorCopy(NPC->enemy->currentOrigin, NPCInfo->enemyLastSeenLocation);
					}
					else
					{
						//Hmm, have to get around this bastard
						//NPC_AimAdjust( 1 );//adjust aim better longer we can see enemy
						if (hit_ent && hit_ent->client && hit_ent->client->playerTeam == NPC->client->playerTeam)
						{
							//would hit an ally, don't fire!!!
							hit_ally = qtrue;
						}
						else
						{
							//Check and see where our shot *would* hit... if it's not close to the enemy (within 256?), then don't fire
						}
					}
				}
				else
				{
					enemy_cs = qfalse; //not true, but should stop us from firing
				}
			}
		}
		else if (gi.inPVS(NPC->enemy->currentOrigin, NPC->currentOrigin))
		{
			NPCInfo->enemyLastSeenTime = level.time;
			//NPC_AimAdjust( -1 );//adjust aim worse longer we cannot see enemy
		}

		if (NPC->client->ps.weapon == WP_NONE)
		{
			shoot = qfalse;
		}
		else
		{
			if (enemy_cs)
			{
				shoot = qtrue;
			}
		}

		if (!enemy_cs)
		{
			//if have a clear shot, always try
			//See if we should continue to fire on their last position
			//!TIMER_Done( NPC, "stick" ) ||
			if (!hit_ally //we're not going to hit an ally
				&& enemy_in_fov //enemy is in our FOV //FIXME: or we don't have a clear LOS?
				&& NPCInfo->enemyLastSeenTime > 0) //we've seen the enemy
			{
				if (level.time - NPCInfo->enemyLastSeenTime < 10000) //we have seem the enemy in the last 10 seconds
				{
					if (!Q_irand(0, 10))
					{
						//Fire on the last known position
						vec3_t muzzle;
						qboolean too_close = qfalse;
						qboolean too_far = qfalse;

						CalcEntitySpot(NPC, SPOT_HEAD, muzzle);
						if (VectorCompare(impact_pos, vec3_origin))
						{
							//never checked ShotEntity this frame, so must do a trace...
							trace_t tr;
							//vec3_t	mins = {-2,-2,-2}, maxs = {2,2,2};
							vec3_t forward, end;
							AngleVectors(NPC->client->ps.viewangles, forward, nullptr, nullptr);
							VectorMA(muzzle, 8192, forward, end);
							gi.trace(&tr, muzzle, vec3_origin, vec3_origin, end, NPC->s.number, MASK_SHOT,
								static_cast<EG2_Collision>(0), 0);
							VectorCopy(tr.endpos, impact_pos);
						}

						//see if impact would be too close to me
						float dist_threshold = 16384/*128*128*/; //default
						switch (NPC->s.weapon)
						{
						case WP_ROCKET_LAUNCHER:
						case WP_FLECHETTE:
						case WP_THERMAL:
						case WP_TRIP_MINE:
						case WP_DET_PACK:
							dist_threshold = 65536/*256*256*/;
							break;
						case WP_REPEATER:
							if (NPCInfo->scriptFlags & SCF_ALT_FIRE)
							{
								dist_threshold = 65536/*256*256*/;
							}
							break;
						case WP_CONCUSSION:
							if (!(NPCInfo->scriptFlags & SCF_ALT_FIRE))
							{
								dist_threshold = 65536/*256*256*/;
							}
							break;
						default:
							break;
						}

						float dist = DistanceSquared(impact_pos, muzzle);

						if (dist < dist_threshold)
						{
							//impact would be too close to me
							too_close = qtrue;
						}
						else if (level.time - NPCInfo->enemyLastSeenTime > 5000 ||
							NPCInfo->group && level.time - NPCInfo->group->lastSeenEnemyTime > 5000)
						{
							//we've haven't seen them in the last 5 seconds
							//see if it's too far from where he is
							dist_threshold = 65536/*256*256*/; //default
							switch (NPC->s.weapon)
							{
							case WP_ROCKET_LAUNCHER:
							case WP_FLECHETTE:
							case WP_THERMAL:
							case WP_TRIP_MINE:
							case WP_DET_PACK:
								dist_threshold = 262144/*512*512*/;
								break;
							case WP_REPEATER:
								if (NPCInfo->scriptFlags & SCF_ALT_FIRE)
								{
									dist_threshold = 262144/*512*512*/;
								}
								break;
							case WP_CONCUSSION:
								if (!(NPCInfo->scriptFlags & SCF_ALT_FIRE))
								{
									dist_threshold = 262144/*512*512*/;
								}
								break;
							default:
								break;
							}
							dist = DistanceSquared(impact_pos, NPCInfo->enemyLastSeenLocation);
							if (dist > dist_threshold)
							{
								//impact would be too far from enemy
								too_far = qtrue;
							}
						}

						if (!too_close && !too_far)
						{
							vec3_t angles;
							vec3_t dir;
							//okay too shoot at last pos
							VectorSubtract(NPCInfo->enemyLastSeenLocation, muzzle, dir);
							VectorNormalize(dir);
							vectoangles(dir, angles);

							NPCInfo->desiredYaw = angles[YAW];
							NPCInfo->desiredPitch = angles[PITCH];

							shoot = qtrue;
						}
					}
				}
			}
		}

		//FIXME: don't shoot right away!
		if (NPC->client->fireDelay)
		{
			if (NPC->s.weapon == WP_ROCKET_LAUNCHER
				|| NPC->s.weapon == WP_CONCUSSION && !(NPCInfo->scriptFlags & SCF_ALT_FIRE))
			{
				if (!enemy_los || !enemy_cs)
				{
					//cancel it
					NPC->client->fireDelay = 0;
				}
				else
				{
					//delay our next attempt
					TIMER_Set(NPC, "nextAttackDelay", Q_irand(1000, 3000)); //FIXME: base on g_spskill
				}
			}
		}
		else if (shoot)
		{
			//try to shoot if it's time
			if (TIMER_Done(NPC, "nextAttackDelay"))
			{
				if (!(NPCInfo->scriptFlags & SCF_FIRE_WEAPON)) // we've already fired, no need to do it again here
				{
					WeaponThink();
				}
				//NASTY
				constexpr int alt_chance = 6; //FIXME: base on g_spskill
				if (NPC->s.weapon == WP_ROCKET_LAUNCHER)
				{
					if (ucmd.buttons & BUTTON_ATTACK
						&& !Q_irand(0, alt_chance))
					{
						//every now and then, shoot a homing rocket
						ucmd.buttons &= ~BUTTON_ATTACK;
						ucmd.buttons |= BUTTON_ALT_ATTACK;
						NPC->client->fireDelay = Q_irand(1000, 3000); //FIXME: base on g_spskill
					}
				}
				else if (NPC->s.weapon == WP_CONCUSSION)
				{
					if (ucmd.buttons & BUTTON_ATTACK
						&& Q_irand(0, alt_chance * 5))
					{
						//fire the beam shot
						ucmd.buttons &= ~BUTTON_ATTACK;
						ucmd.buttons |= BUTTON_ALT_ATTACK;
						TIMER_Set(NPC, "nextAttackDelay", Q_irand(1500, 2500)); //FIXME: base on g_spskill
					}
					else
					{
						//fire the rocket-like shot
						TIMER_Set(NPC, "nextAttackDelay", Q_irand(3000, 5000)); //FIXME: base on g_spskill
					}
				}
			}
		}
	}
}

//=====================================================================================
//FLYING behavior
//=====================================================================================
qboolean RT_Flying(const gentity_t* self)
{
	return static_cast<qboolean>(self->client->moveType == MT_FLYSWIM);
}

void RT_FlyStart(gentity_t* self)
{
	//switch to seeker AI for a while
	if (TIMER_Done(self, "jetRecharge")
		&& !RT_Flying(self))
	{
		self->client->ps.gravity = 0;
		self->svFlags |= SVF_CUSTOM_GRAVITY;
		self->client->moveType = MT_FLYSWIM;
		//Inform NPC_HandleAIFlags we want to fly

		if (self->NPC)
		{
			self->NPC->aiFlags |= NPCAI_FLY;
			self->lastInAirTime = level.time;
		}

		//start jet effect
		self->client->jetPackTime = Q3_INFINITE;
		if (self->genericBolt1 != -1)
		{
			G_PlayEffect(G_EffectIndex("rockettrooper/flameNEW"), self->playerModel, self->genericBolt1, self->s.number,
				self->currentOrigin, qtrue, qtrue);
		}
		if (self->genericBolt2 != -1)
		{
			G_PlayEffect(G_EffectIndex("rockettrooper/flameNEW"), self->playerModel, self->genericBolt2, self->s.number,
				self->currentOrigin, qtrue, qtrue);
		}

		//take-off sound
		G_SoundOnEnt(self, CHAN_ITEM, "sound/chars/boba/bf_blast-off.wav");
		//jet loop sound
		self->s.loopSound = G_SoundIndex("sound/chars/boba/bf_jetpack_lp.wav");
		if (self->NPC)
		{
			self->count = Q3_INFINITE; // SEEKER shot ammo count
		}
	}
}

void RT_FlyStop(gentity_t* self)
{
	self->client->ps.gravity = g_gravity->value;
	self->svFlags &= ~SVF_CUSTOM_GRAVITY;
	self->client->moveType = MT_RUNJUMP;
	//Stop the effect
	self->client->jetPackTime = 0;
	if (self->genericBolt1 != -1)
	{
		G_StopEffect("rockettrooper/flameNEW", self->playerModel, self->genericBolt1, self->s.number);
	}
	if (self->genericBolt2 != -1)
	{
		G_StopEffect("rockettrooper/flameNEW", self->playerModel, self->genericBolt2, self->s.number);
	}
	//stop jet loop sound
	self->s.loopSound = 0;
	G_SoundOnEnt(self, CHAN_ITEM, "sound/chars/boba/bf_land.wav");

	if (self->NPC)
	{
		self->count = 0; // SEEKER shot ammo count
		TIMER_Set(self, "jetRecharge", Q_irand(1000, 5000));
		TIMER_Set(self, "jumpChaseDebounce", Q_irand(500, 2000));
	}
}

void RT_JetPackEffect(const int duration)
{
	if (NPC->genericBolt1 != -1)
	{
		G_PlayEffect(G_EffectIndex("rockettrooper/flameNEW"), NPC->playerModel, NPC->genericBolt1, NPC->s.number,
			NPC->currentOrigin, duration, qtrue);
	}
	if (NPC->genericBolt2 != -1)
	{
		G_PlayEffect(G_EffectIndex("rockettrooper/flameNEW"), NPC->playerModel, NPC->genericBolt2, NPC->s.number,
			NPC->currentOrigin, duration, qtrue);
	}

	//take-off sound
	G_SoundOnEnt(NPC, CHAN_ITEM, "sound/chars/boba/bf_blast-off.wav");
}

static void RT_Flying_ApplyFriction()
{
	if (NPC->client->ps.velocity[0])
	{
		NPC->client->ps.velocity[0] *= VELOCITY_DECAY; ///frictionScale;

		if (fabs(NPC->client->ps.velocity[0]) < 1)
		{
			NPC->client->ps.velocity[0] = 0;
		}
	}

	if (NPC->client->ps.velocity[1])
	{
		NPC->client->ps.velocity[1] *= VELOCITY_DECAY; ///frictionScale;

		if (fabs(NPC->client->ps.velocity[1]) < 1)
		{
			NPC->client->ps.velocity[1] = 0;
		}
	}
}

static void RT_Flying_MaintainHeight()
{
	float dif = 0;

	// Update our angles regardless
	NPC_UpdateAngles(qtrue, qtrue);

	if (NPC->forcePushTime > level.time)
	{
		//if being pushed, we don't have control over our movement
		return;
	}

	if (NPC->client->ps.pm_flags & PMF_TIME_KNOCKBACK)
	{
		//don't slow down for a bit
		if (NPC->client->ps.pm_time > 0)
		{
			VectorScale(NPC->client->ps.velocity, 0.9f, NPC->client->ps.velocity);
			return;
		}
	}
	// If we have an enemy, we should try to hover at or a little below enemy eye level
	if (NPC->enemy
		&& (!Q3_TaskIDPending(NPC, TID_MOVE_NAV) || !NPCInfo->goalEntity))
	{
		if (TIMER_Done(NPC, "heightChange"))
		{
			TIMER_Set(NPC, "heightChange", Q_irand(1000, 3000));

			float enemy_z_height = NPC->enemy->currentOrigin[2];
			if (NPC->enemy->client
				&& NPC->enemy->client->ps.groundEntityNum == ENTITYNUM_NONE
				&& NPC->enemy->client->ps.forcePowersActive & 1 << FP_LEVITATION)
			{
				//so we don't go up when they force jump up at us
				enemy_z_height = NPC->enemy->client->ps.forceJumpZStart;
			}

			// Find the height difference
			dif = enemy_z_height + Q_flrand(NPC->enemy->maxs[2] / 2, NPC->enemy->maxs[2] + 8) - NPC->currentOrigin[2];

			constexpr float dif_factor = 10.0f;

			// cap to prevent dramatic height shifts
			if (fabs(dif) > 2 * dif_factor)
			{
				if (fabs(dif) > 20 * dif_factor)
				{
					dif = dif < 0 ? -20 * dif_factor : 20 * dif_factor;
				}

				NPC->client->ps.velocity[2] = (NPC->client->ps.velocity[2] + dif) / 2;
			}
			NPC->client->ps.velocity[2] *= Q_flrand(0.85f, 1.25f);
		}
		else
		{
			//don't get too far away from height of enemy...
			float enemy_z_height = NPC->enemy->currentOrigin[2];
			if (NPC->enemy->client
				&& NPC->enemy->client->ps.groundEntityNum == ENTITYNUM_NONE
				&& NPC->enemy->client->ps.forcePowersActive & 1 << FP_LEVITATION)
			{
				//so we don't go up when they force jump up at us
				enemy_z_height = NPC->enemy->client->ps.forceJumpZStart;
			}
			dif = NPC->currentOrigin[2] - (enemy_z_height + 64);
			float max_height = 200;
			const float h_dist = DistanceHorizontal(NPC->enemy->currentOrigin, NPC->currentOrigin);
			if (h_dist < 512)
			{
				max_height *= h_dist / 512;
			}
			if (dif > max_height)
			{
				if (NPC->client->ps.velocity[2] > 0) //FIXME: or: we can't see him anymore
				{
					//slow down
					if (NPC->client->ps.velocity[2])
					{
						NPC->client->ps.velocity[2] *= VELOCITY_DECAY;

						if (fabs(NPC->client->ps.velocity[2]) < 2)
						{
							NPC->client->ps.velocity[2] = 0;
						}
					}
				}
				else
				{
					//start coming back down
					NPC->client->ps.velocity[2] -= 4;
				}
			}
			else if (dif < -200 && NPC->client->ps.velocity[2] < 0) //we're way below him
			{
				if (NPC->client->ps.velocity[2] < 0) //FIXME: or: we can't see him anymore
				{
					//slow down
					if (NPC->client->ps.velocity[2])
					{
						NPC->client->ps.velocity[2] *= VELOCITY_DECAY;

						if (fabs(NPC->client->ps.velocity[2]) > -2)
						{
							NPC->client->ps.velocity[2] = 0;
						}
					}
				}
				else
				{
					//start going back up
					NPC->client->ps.velocity[2] += 4;
				}
			}
		}
	}
	else
	{
		const gentity_t* goal;

		if (NPCInfo->goalEntity) // Is there a goal?
		{
			goal = NPCInfo->goalEntity;
		}
		else
		{
			goal = NPCInfo->lastGoalEntity;
		}
		if (goal)
		{
			dif = goal->currentOrigin[2] - NPC->currentOrigin[2];
		}
		else if (VectorCompare(NPC->pos1, vec3_origin))
		{
			//have a starting position as a reference point
			dif = NPC->pos1[2] - NPC->currentOrigin[2];
		}

		if (fabs(dif) > 24)
		{
			ucmd.upmove = ucmd.upmove < 0 ? -4 : 4;
		}
		else
		{
			if (NPC->client->ps.velocity[2])
			{
				NPC->client->ps.velocity[2] *= VELOCITY_DECAY;

				if (fabs(NPC->client->ps.velocity[2]) < 2)
				{
					NPC->client->ps.velocity[2] = 0;
				}
			}
		}
	}

	// Apply friction
	RT_Flying_ApplyFriction();
}

static void RT_Flying_Strafe()
{
	int side;
	vec3_t end, right;
	trace_t tr;

	if (Q_flrand(0.0f, 1.0f) > 0.7f
		|| !NPC->enemy
		|| !NPC->enemy->client)
	{
		// Do a regular style strafe
		AngleVectors(NPC->client->renderInfo.eyeAngles, nullptr, right, nullptr);

		// Pick a random strafe direction, then check to see if doing a strafe would be
		//	reasonably valid
		side = rand() & 1 ? -1 : 1;
		VectorMA(NPC->currentOrigin, RT_FLYING_STRAFE_DIS * side, right, end);

		gi.trace(&tr, NPC->currentOrigin, nullptr, nullptr, end, NPC->s.number, MASK_SOLID,
			static_cast<EG2_Collision>(0), 0);

		// Close enough
		if (tr.fraction > 0.9f)
		{
			const float vel = RT_FLYING_STRAFE_VEL + Q_flrand(-20, 20);
			VectorMA(NPC->client->ps.velocity, vel * side, right, NPC->client->ps.velocity);
			if (!Q_irand(0, 3))
			{
				// Add a slight upward push
				constexpr float up_push = RT_FLYING_UPWARD_PUSH;
				if (NPC->client->ps.velocity[2] < 300)
				{
					if (NPC->client->ps.velocity[2] < 300 + up_push)
					{
						NPC->client->ps.velocity[2] += up_push;
					}
					else
					{
						NPC->client->ps.velocity[2] = 300;
					}
				}
			}

			NPCInfo->standTime = level.time + 1000 + Q_flrand(0.0f, 1.0f) * 500;
		}
	}
	else
	{
		vec3_t dir;
		// Do a strafe to try and keep on the side of their enemy
		AngleVectors(NPC->enemy->client->renderInfo.eyeAngles, dir, right, nullptr);

		// Pick a random side
		side = rand() & 1 ? -1 : 1;
		constexpr float st_dis = RT_FLYING_STRAFE_DIS * 2.0f;
		VectorMA(NPC->enemy->currentOrigin, st_dis * side, right, end);

		// then add a very small bit of random in front of/behind the player action
		VectorMA(end, Q_flrand(-1.0f, 1.0f) * 25, dir, end);

		gi.trace(&tr, NPC->currentOrigin, nullptr, nullptr, end, NPC->s.number, MASK_SOLID,
			static_cast<EG2_Collision>(0), 0);

		// Close enough
		if (tr.fraction > 0.9f)
		{
			const float vel = RT_FLYING_STRAFE_VEL * 4 + Q_flrand(-20, 20);
			VectorSubtract(tr.endpos, NPC->currentOrigin, dir);
			dir[2] *= 0.25; // do less upward change
			float dis = VectorNormalize(dir);
			if (dis > vel)
			{
				dis = vel;
			}
			// Try to move the desired enemy side
			VectorMA(NPC->client->ps.velocity, dis, dir, NPC->client->ps.velocity);

			if (!Q_irand(0, 3))
			{
				constexpr float up_push = RT_FLYING_UPWARD_PUSH;
				// Add a slight upward push
				if (NPC->client->ps.velocity[2] < 300)
				{
					if (NPC->client->ps.velocity[2] < 300 + up_push)
					{
						NPC->client->ps.velocity[2] += up_push;
					}
					else
					{
						NPC->client->ps.velocity[2] = 300;
					}
				}
				else if (NPC->client->ps.velocity[2] > 300)
				{
					NPC->client->ps.velocity[2] = 300;
				}
			}

			NPCInfo->standTime = level.time + 2500 + Q_flrand(0.0f, 1.0f) * 500;
		}
	}
}

static void RT_Flying_Hunt(const qboolean visible, const qboolean advance)
{
	vec3_t forward;

	if (NPC->forcePushTime >= level.time)
	{
		//if being pushed, we don't have control over our movement
		NPC->delay = 0;
		return;
	}
	NPC_FaceEnemy(qtrue);

	// If we're not supposed to stand still, pursue the player
	if (NPCInfo->standTime < level.time)
	{
		// Only strafe when we can see the player
		if (visible)
		{
			NPC->delay = 0;
			RT_Flying_Strafe();
			return;
		}
	}

	// If we don't want to advance, stop here
	if (advance)
	{
		// Only try and navigate if the player is visible
		if (visible == qfalse)
		{
			// Move towards our goal
			NPCInfo->goalEntity = NPC->enemy;
			NPCInfo->goalRadius = 24;

			NPC->delay = 0;
			NPC_MoveToGoal(qtrue);
			return;
		}
	}
	//else move straight at/away from him
	VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, forward);
	forward[2] *= 0.1f;
	const float distance = VectorNormalize(forward);

	const float speed = RT_FLYING_FORWARD_BASE_SPEED + RT_FLYING_FORWARD_MULTIPLIER * g_spskill->integer;
	if (advance && distance < Q_flrand(256, 3096))
	{
		NPC->delay = 0;
		VectorMA(NPC->client->ps.velocity, speed, forward, NPC->client->ps.velocity);
	}
	else if (distance < Q_flrand(0, 128))
	{
		if (NPC->health <= 50)
		{
			//always back off
			NPC->delay = 0;
		}
		else if (!TIMER_Done(NPC, "backoffTime"))
		{
			//still backing off from end of last delay
			NPC->delay = 0;
		}
		else if (!NPC->delay)
		{
			//start a new delay
			NPC->delay = Q_irand(0, 10 + 20 * (2 - g_spskill->integer));
		}
		else
		{
			//continue the current delay
			NPC->delay--;
		}
		if (!NPC->delay)
		{
			//delay done, now back off for a few seconds!
			TIMER_Set(NPC, "backoffTime", Q_irand(2000, 5000));
			VectorMA(NPC->client->ps.velocity, speed * -2, forward, NPC->client->ps.velocity);
		}
	}
	else
	{
		NPC->delay = 0;
	}
}

static void RT_Flying_Ranged(const qboolean visible, const qboolean advance)
{
	if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
	{
		RT_Flying_Hunt(visible, advance);
	}
}

static void RT_Flying_Attack()
{
	// Always keep a good height off the ground
	RT_Flying_MaintainHeight();

	// Rate our distance to the target, and our visibilty
	const float distance = DistanceHorizontalSquared(NPC->currentOrigin, NPC->enemy->currentOrigin);
	const qboolean visible = NPC_ClearLOS(NPC->enemy);
	const auto advance = static_cast<qboolean>(distance > 256.0f * 256.0f);

	// If we cannot see our target, move to see it
	if (visible == qfalse)
	{
		if (NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
		{
			RT_Flying_Hunt(visible, advance);
			return;
		}
	}

	RT_Flying_Ranged(visible, advance);
}

static void RT_Flying_Think()
{
	if (Q3_TaskIDPending(NPC, TID_MOVE_NAV)
		&& UpdateGoal())
	{
		//being scripted to go to a certain spot, don't maintain height
		if (NPC_MoveToGoal(qtrue))
		{
			//we could macro-nav to our goal
			if (NPC->enemy && NPC->enemy->health && NPC->enemy->inuse)
			{
				NPC_FaceEnemy(qtrue);
				RT_FireDecide();
			}
		}
		else
		{
			//frick, no where to nav to, keep us in the air!
			RT_Flying_MaintainHeight();
		}
		return;
	}

	if (NPC->random == 0.0f)
	{
		// used to offset seekers around a circle so they don't occupy the same spot.  This is not a fool-proof method.
		NPC->random = Q_flrand(0.0f, 1.0f) * 6.3f; // roughly 2pi
	}

	if (NPC->enemy && NPC->enemy->health && NPC->enemy->inuse)
	{
		RT_Flying_Attack();
		RT_FireDecide();
		return;
	}
	RT_Flying_MaintainHeight();
	RT_RunStormtrooperAI();
}

//=====================================================================================
//ON GROUND WITH ENEMY behavior
//=====================================================================================

//=====================================================================================
//DEFAULT behavior
//=====================================================================================
extern void RT_CheckJump();

void NPC_BSRT_Default()
{
	//FIXME: custom pain and death funcs:
	//pain3 is in air
	//die in air is both_falldeath1
	//attack1 is on ground, attack2 is in air

	//FIXME: this doesn't belong here
	if (NPC->client->ps.groundEntityNum != ENTITYNUM_NONE)
	{
		if (NPCInfo->rank >= RANK_LT) //&& !Q_irand( 0, 50 ) )
		{
			//officers always stay in the air
			NPC->client->ps.velocity[2] = Q_irand(50, 125);
			NPC->NPC->aiFlags |= NPCAI_FLY; //fixme also, Inform NPC_HandleAIFlags we want to fly
		}
	}

	if (RT_Flying(NPC))
	{
		//FIXME: only officers need do this, right?
		RT_Flying_Think();
	}
	else if (NPC->enemy != nullptr)
	{
		//rocketrooper on ground with enemy
		UpdateGoal();
		RT_RunStormtrooperAI();
		RT_CheckJump();
	}
	else
	{
		//shouldn't have gotten in here
		RT_RunStormtrooperAI();
	}
}