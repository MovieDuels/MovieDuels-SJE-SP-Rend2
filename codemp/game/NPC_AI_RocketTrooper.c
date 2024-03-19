//Rocket Trooper AI code
#include "g_local.h"
#include "b_local.h"

extern qboolean PM_FlippingAnim(int anim);
extern void NPC_BSST_Patrol(void);
extern void NPC_BehaviorSet_Stormtrooper(int b_state);

extern void RT_FlyStart(gentity_t* self);
extern void G_SoundOnEnt(gentity_t* ent, soundChannel_t channel, const char* sound_path);

#define VELOCITY_DECAY		0.7f

#define RT_FLYING_STRAFE_VEL	60
#define RT_FLYING_STRAFE_DIS	200
#define RT_FLYING_UPWARD_PUSH	150

#define RT_FLYING_FORWARD_BASE_SPEED	50
#define RT_FLYING_FORWARD_MULTIPLIER	10

void RT_Precache(void)
{
	G_SoundIndex("sound/chars/boba/bf_blast-off.wav");
	G_SoundIndex("sound/chars/boba/bf_jetpack_lp.wav");
	G_SoundIndex("sound/chars/boba/bf_land.wav");
	G_EffectIndex("rockettrooper/flameNEW");
	G_EffectIndex("rockettrooper/light_cone"); //extern this?  At least use a different one
}

void RT_RunStormtrooperAI(void)
{
	int b_state;
	//Execute our b_state
	if (NPCS.NPCInfo->tempBehavior)
	{
		//Overrides normal behavior until cleared
		b_state = NPCS.NPCInfo->tempBehavior;
	}
	else
	{
		if (!NPCS.NPCInfo->behaviorState)
			NPCS.NPCInfo->behaviorState = NPCS.NPCInfo->defaultBehavior;

		b_state = NPCS.NPCInfo->behaviorState;
	}
	NPC_BehaviorSet_Stormtrooper(b_state);
}

void RT_FireDecide(void)
{
	qboolean enemyLOS = qfalse, enemyCS = qfalse, enemyInFOV = qfalse;
	qboolean shoot = qfalse, hitAlly = qfalse;
	vec3_t impactPos, enemyDir, shootDir;

	if (NPCS.NPC->client->ps.groundEntityNum == ENTITYNUM_NONE
		&& NPCS.NPC->client->ps.fd.forceJumpZStart
		&& !PM_FlippingAnim(NPCS.NPC->client->ps.legsAnim)
		&& !Q_irand(0, 10))
	{
		//take off
		RT_FlyStart(NPCS.NPC);
	}

	if (!NPCS.NPC->enemy)
	{
		return;
	}

	VectorClear(impactPos);
	const float enemyDist = DistanceSquared(NPCS.NPC->r.currentOrigin, NPCS.NPC->enemy->r.currentOrigin);

	VectorSubtract(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPC->r.currentOrigin, enemyDir);
	VectorNormalize(enemyDir);
	AngleVectors(NPCS.NPC->client->ps.viewangles, shootDir, NULL, NULL);
	const float dot = DotProduct(enemyDir, shootDir);
	if (dot > 0.5f || enemyDist * (1.0f - dot) < 10000)
	{
		//enemy is in front of me or they're very close and not behind me
		enemyInFOV = qtrue;
	}

	if (enemyDist < MIN_ROCKET_DIST_SQUARED) //128
	{
		//enemy within 128
		if ((NPCS.NPC->client->ps.weapon == WP_FLECHETTE || NPCS.NPC->client->ps.weapon == WP_REPEATER) &&
			NPCS.NPCInfo->scriptFlags & SCF_altFire)
		{
			//shooting an explosive, but enemy too close, switch to primary fire
			NPCS.NPCInfo->scriptFlags &= ~SCF_altFire;
		}
	}
	else if (enemyDist > 65536) //256 squared
	{
		if (NPCS.NPC->client->ps.weapon == WP_DISRUPTOR)
		{
			//sniping... should be assumed
			if (!(NPCS.NPCInfo->scriptFlags & SCF_altFire))
			{
				//use primary fire
				NPCS.NPCInfo->scriptFlags |= SCF_altFire;
				//reset fire-timing variables
				NPC_ChangeWeapon(WP_DISRUPTOR);
				NPC_UpdateAngles(qtrue, qtrue);
				return;
			}
		}
	}

	//can we see our target?
	if (TIMER_Done(NPCS.NPC, "nextAttackDelay") && TIMER_Done(NPCS.NPC, "flameTime"))
	{
		if (NPC_ClearLOS4(NPCS.NPC->enemy))
		{
			NPCS.NPCInfo->enemyLastSeenTime = level.time;
			enemyLOS = qtrue;

			if (NPCS.NPC->client->ps.weapon == WP_NONE)
			{
				enemyCS = qfalse; //not true, but should stop us from firing
			}
			else
			{
				//can we shoot our target?
				if ((NPCS.NPC->client->ps.weapon == WP_ROCKET_LAUNCHER || NPCS.NPC->client->ps.weapon == WP_FLECHETTE &&
					NPCS.NPCInfo->scriptFlags & SCF_altFire) && enemyDist < MIN_ROCKET_DIST_SQUARED) //128*128
				{
					enemyCS = qfalse; //not true, but should stop us from firing
					hitAlly = qtrue; //us!
					//FIXME: if too close, run away!
				}
				else if (enemyInFOV)
				{
					//if enemy is FOV, go ahead and check for shooting
					const int hit = NPC_ShotEntity(NPCS.NPC->enemy, impactPos);
					const gentity_t* hit_ent = &g_entities[hit];

					if (hit == NPCS.NPC->enemy->s.number
						|| hit_ent && hit_ent->client && hit_ent->client->playerTeam == NPCS.NPC->client->enemyTeam
						|| hit_ent && hit_ent->takedamage && (hit_ent->r.svFlags & SVF_GLASS_BRUSH || hit_ent->health < 40
							|| NPCS.NPC->s.weapon == WP_EMPLACED_GUN))
					{
						//can hit enemy or enemy ally or will hit glass or other minor breakable (or in emplaced gun), so shoot anyway
						enemyCS = qtrue;
						//NPC_AimAdjust( 2 );//adjust aim better longer we have clear shot at enemy
						VectorCopy(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPCInfo->enemyLastSeenLocation);
					}
					else
					{
						//Hmm, have to get around this bastard
						//NPC_AimAdjust( 1 );//adjust aim better longer we can see enemy
						if (hit_ent && hit_ent->client && hit_ent->client->playerTeam == NPCS.NPC->client->playerTeam)
						{
							//would hit an ally, don't fire!!!
							hitAlly = qtrue;
						}
						else
						{
							//Check and see where our shot *would* hit... if it's not close to the enemy (within 256?), then don't fire
						}
					}
				}
				else
				{
					enemyCS = qfalse; //not true, but should stop us from firing
				}
			}
		}
		else if (trap->InPVS(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPC->r.currentOrigin))
		{
			NPCS.NPCInfo->enemyLastSeenTime = level.time;
		}

		if (NPCS.NPC->client->ps.weapon == WP_NONE)
		{
			shoot = qfalse;
		}
		else
		{
			if (enemyCS)
			{
				shoot = qtrue;
			}
		}

		if (!enemyCS)
		{
			//if have a clear shot, always try
			//See if we should continue to fire on their last position
			//!TIMER_Done( NPC, "stick" ) ||
			if (!hitAlly //we're not going to hit an ally
				&& enemyInFOV //enemy is in our FOV //FIXME: or we don't have a clear LOS?
				&& NPCS.NPCInfo->enemyLastSeenTime > 0) //we've seen the enemy
			{
				if (level.time - NPCS.NPCInfo->enemyLastSeenTime < 10000)
					//we have seem the enemy in the last 10 seconds
				{
					if (!Q_irand(0, 10))
					{
						//Fire on the last known position
						vec3_t muzzle;
						qboolean tooClose = qfalse;
						qboolean tooFar = qfalse;

						CalcEntitySpot(NPCS.NPC, SPOT_HEAD, muzzle);
						if (VectorCompare(impactPos, vec3_origin))
						{
							//never checked ShotEntity this frame, so must do a trace...
							trace_t tr;
							//vec3_t	mins = {-2,-2,-2}, maxs = {2,2,2};
							vec3_t forward, end;
							AngleVectors(NPCS.NPC->client->ps.viewangles, forward, NULL, NULL);
							VectorMA(muzzle, 8192, forward, end);
							trap->Trace(&tr, muzzle, vec3_origin, vec3_origin, end, NPCS.NPC->s.number, MASK_SHOT,
								qfalse, 0, 0);
							VectorCopy(tr.endpos, impactPos);
						}

						//see if impact would be too close to me
						float distThreshold = 16384/*128*128*/; //default
						switch (NPCS.NPC->s.weapon)
						{
						case WP_ROCKET_LAUNCHER:
						case WP_FLECHETTE:
						case WP_THERMAL:
						case WP_TRIP_MINE:
						case WP_DET_PACK:
							distThreshold = 65536/*256*256*/;
							break;
						case WP_REPEATER:
							if (NPCS.NPCInfo->scriptFlags & SCF_altFire)
							{
								distThreshold = 65536/*256*256*/;
							}
							break;
						default:
							break;
						}

						float dist = DistanceSquared(impactPos, muzzle);

						if (dist < distThreshold)
						{
							//impact would be too close to me
							tooClose = qtrue;
						}
						else if (level.time - NPCS.NPCInfo->enemyLastSeenTime > 5000 ||
							NPCS.NPCInfo->group && level.time - NPCS.NPCInfo->group->lastSeenEnemyTime > 5000)
						{
							//we've haven't seen them in the last 5 seconds
							//see if it's too far from where he is
							distThreshold = 65536/*256*256*/; //default
							switch (NPCS.NPC->s.weapon)
							{
							case WP_ROCKET_LAUNCHER:
							case WP_FLECHETTE:
							case WP_THERMAL:
							case WP_TRIP_MINE:
							case WP_DET_PACK:
								distThreshold = 262144/*512*512*/;
								break;
							case WP_REPEATER:
								if (NPCS.NPCInfo->scriptFlags & SCF_altFire)
								{
									distThreshold = 262144/*512*512*/;
								}
								break;
							default:
								break;
							}
							dist = DistanceSquared(impactPos, NPCS.NPCInfo->enemyLastSeenLocation);
							if (dist > distThreshold)
							{
								//impact would be too far from enemy
								tooFar = qtrue;
							}
						}

						if (!tooClose && !tooFar)
						{
							vec3_t angles;
							vec3_t dir;
							//okay too shoot at last pos
							VectorSubtract(NPCS.NPCInfo->enemyLastSeenLocation, muzzle, dir);
							VectorNormalize(dir);
							vectoangles(dir, angles);

							NPCS.NPCInfo->desiredYaw = angles[YAW];
							NPCS.NPCInfo->desiredPitch = angles[PITCH];

							shoot = qtrue;
						}
					}
				}
			}
		}

		//FIXME: don't shoot right away!
		if (NPCS.NPC->client->ps.weaponTime > 0)
		{
			if (NPCS.NPC->s.weapon == WP_ROCKET_LAUNCHER
				|| NPCS.NPC->s.weapon == WP_CONCUSSION && !(NPCS.NPCInfo->scriptFlags & SCF_altFire))
			{
				if (!enemyLOS || !enemyCS)
				{
					//cancel it
					NPCS.NPC->client->ps.weaponTime = 0;
				}
				else
				{
					//delay our next attempt
					TIMER_Set(NPCS.NPC, "nextAttackDelay", Q_irand(500, 1000));
				}
			}
		}
		else if (shoot)
		{
			//try to shoot if it's time
			if (TIMER_Done(NPCS.NPC, "nextAttackDelay"))
			{
				if (!(NPCS.NPCInfo->scriptFlags & SCF_FIRE_WEAPON)) // we've already fired, no need to do it again here
				{
					WeaponThink();
				}
				//NASTY
				if (NPCS.NPC->s.weapon == WP_ROCKET_LAUNCHER
					&& NPCS.ucmd.buttons & BUTTON_ATTACK
					&& !Q_irand(0, 3))
				{
					//every now and then, shoot a homing rocket
					NPCS.ucmd.buttons &= ~BUTTON_ATTACK;
					NPCS.ucmd.buttons |= BUTTON_ALT_ATTACK;
					NPCS.NPC->client->ps.weaponTime = Q_irand(500, 1500);
				}
				else if (NPCS.NPC->s.weapon == WP_CONCUSSION)
				{
					const int altChance = 6;
					if (NPCS.ucmd.buttons & BUTTON_ATTACK
						&& Q_irand(0, altChance * 5))
					{
						//fire the beam shot
						NPCS.ucmd.buttons &= ~BUTTON_ATTACK;
						NPCS.ucmd.buttons |= BUTTON_ALT_ATTACK;
						TIMER_Set(NPCS.NPC, "nextAttackDelay", Q_irand(1500, 2500)); //FIXME: base on g_spskill
					}
					else
					{
						//fire the rocket-like shot
						TIMER_Set(NPCS.NPC, "nextAttackDelay", Q_irand(3000, 5000)); //FIXME: base on g_spskill
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
	return self->client->ps.eFlags2 & EF2_FLYING;
}

void RT_FlyStart(gentity_t* self)
{
	//switch to seeker AI for a while
	if (TIMER_Done(self, "jetRecharge")
		&& !RT_Flying(self))
	{
		self->client->ps.gravity = 0;
		if (self->NPC)
		{
			self->NPC->aiFlags |= NPCAI_CUSTOM_GRAVITY;
		}
		self->client->ps.eFlags2 |= EF2_FLYING; //moveType = MT_FLYSWIM;

		if (self->NPC)
		{
			self->NPC->aiFlags |= NPCAI_FLY;
			self->lastInAirTime = level.time;
		}

		//start jet effect
		self->client->jetPackTime = Q3_INFINITE;

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
	self->client->ps.gravity = g_gravity.value;
	if (self->NPC)
	{
		self->NPC->aiFlags &= ~NPCAI_CUSTOM_GRAVITY;
	}
	self->client->ps.eFlags2 &= ~EF2_FLYING;
	self->client->jetPackTime = 0;
	self->client->moveType = MT_RUNJUMP;
	//Stop the effect
	/*if (self->genericBolt1 != -1)
	{
		G_StopEffect("rockettrooper/flameNEW", self->playerModel, self->genericBolt1, self->s.number);
	}
	if (self->genericBolt2 != -1)
	{
		G_StopEffect("rockettrooper/flameNEW", self->playerModel, self->genericBolt2, self->s.number);
	}*/
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

void RT_Flying_ApplyFriction(float frictionScale)
{
	if (NPCS.NPC->client->ps.velocity[0])
	{
		NPCS.NPC->client->ps.velocity[0] *= VELOCITY_DECAY; ///frictionScale;

		if (fabs(NPCS.NPC->client->ps.velocity[0]) < 1)
		{
			NPCS.NPC->client->ps.velocity[0] = 0;
		}
	}

	if (NPCS.NPC->client->ps.velocity[1])
	{
		NPCS.NPC->client->ps.velocity[1] *= VELOCITY_DECAY; ///frictionScale;

		if (fabs(NPCS.NPC->client->ps.velocity[1]) < 1)
		{
			NPCS.NPC->client->ps.velocity[1] = 0;
		}
	}
}

void RT_Flying_MaintainHeight(void)
{
	float dif = 0;

	// Update our angles regardless
	NPC_UpdateAngles(qtrue, qtrue);

	if (NPCS.NPC->client->ps.pm_flags & PMF_TIME_KNOCKBACK)
	{
		//don't slow down for a bit
		if (NPCS.NPC->client->ps.pm_time > 0)
		{
			VectorScale(NPCS.NPC->client->ps.velocity, 0.9f, NPCS.NPC->client->ps.velocity);
			return;
		}
	}
	// If we have an enemy, we should try to hover at or a little below enemy eye level
	if (NPCS.NPC->enemy)
	{
		if (TIMER_Done(NPCS.NPC, "heightChange"))
		{
			TIMER_Set(NPCS.NPC, "heightChange", Q_irand(1000, 3000));

			float enemyZHeight = NPCS.NPC->enemy->r.currentOrigin[2];
			if (NPCS.NPC->enemy->client
				&& NPCS.NPC->enemy->client->ps.groundEntityNum == ENTITYNUM_NONE
				&& NPCS.NPC->enemy->client->ps.fd.forcePowersActive & 1 << FP_LEVITATION)
			{
				//so we don't go up when they force jump up at us
				enemyZHeight = NPCS.NPC->enemy->client->ps.fd.forceJumpZStart;
			}

			// Find the height difference
			dif = enemyZHeight + Q_flrand(NPCS.NPC->enemy->maxs[2] / 2, NPCS.NPC->enemy->maxs[2] + 8) - NPCS.NPC->r.
				currentOrigin[2];

			const float dif_factor = 10.0f;

			// cap to prevent dramatic height shifts
			if (fabs(dif) > 2 * dif_factor)
			{
				if (fabs(dif) > 20 * dif_factor)
				{
					dif = dif < 0 ? -20 * dif_factor : 20 * dif_factor;
				}

				NPCS.NPC->client->ps.velocity[2] = (NPCS.NPC->client->ps.velocity[2] + dif) / 2;
			}
			NPCS.NPC->client->ps.velocity[2] *= Q_flrand(0.85f, 1.25f);
		}
		else
		{
			//don't get too far away from height of enemy...
			float enemyZHeight = NPCS.NPC->enemy->r.currentOrigin[2];
			if (NPCS.NPC->enemy->client
				&& NPCS.NPC->enemy->client->ps.groundEntityNum == ENTITYNUM_NONE
				&& NPCS.NPC->enemy->client->ps.fd.forcePowersActive & 1 << FP_LEVITATION)
			{
				//so we don't go up when they force jump up at us
				enemyZHeight = NPCS.NPC->enemy->client->ps.fd.forceJumpZStart;
			}
			dif = NPCS.NPC->r.currentOrigin[2] - (enemyZHeight + 64);
			float maxHeight = 200;
			const float hDist = DistanceHorizontal(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPC->r.currentOrigin);
			if (hDist < 512)
			{
				maxHeight *= hDist / 512;
			}
			if (dif > maxHeight)
			{
				if (NPCS.NPC->client->ps.velocity[2] > 0) //FIXME: or: we can't see him anymore
				{
					//slow down
					if (NPCS.NPC->client->ps.velocity[2])
					{
						NPCS.NPC->client->ps.velocity[2] *= VELOCITY_DECAY;

						if (fabs(NPCS.NPC->client->ps.velocity[2]) < 2)
						{
							NPCS.NPC->client->ps.velocity[2] = 0;
						}
					}
				}
				else
				{
					//start coming back down
					NPCS.NPC->client->ps.velocity[2] -= 4;
				}
			}
			else if (dif < -200 && NPCS.NPC->client->ps.velocity[2] < 0) //we're way below him
			{
				if (NPCS.NPC->client->ps.velocity[2] < 0) //FIXME: or: we can't see him anymore
				{
					//slow down
					if (NPCS.NPC->client->ps.velocity[2])
					{
						NPCS.NPC->client->ps.velocity[2] *= VELOCITY_DECAY;

						if (fabs(NPCS.NPC->client->ps.velocity[2]) > -2)
						{
							NPCS.NPC->client->ps.velocity[2] = 0;
						}
					}
				}
				else
				{
					//start going back up
					NPCS.NPC->client->ps.velocity[2] += 4;
				}
			}
		}
	}
	else
	{
		const gentity_t* goal;

		if (NPCS.NPCInfo->goalEntity) // Is there a goal?
		{
			goal = NPCS.NPCInfo->goalEntity;
		}
		else
		{
			goal = NPCS.NPCInfo->lastGoalEntity;
		}
		if (goal)
		{
			dif = goal->r.currentOrigin[2] - NPCS.NPC->r.currentOrigin[2];
		}
		else if (VectorCompare(NPCS.NPC->pos1, vec3_origin))
		{
			//have a starting position as a reference point
			dif = NPCS.NPC->pos1[2] - NPCS.NPC->r.currentOrigin[2];
		}

		if (fabs(dif) > 24)
		{
			NPCS.ucmd.upmove = NPCS.ucmd.upmove < 0 ? -4 : 4;
		}
		else
		{
			if (NPCS.NPC->client->ps.velocity[2])
			{
				NPCS.NPC->client->ps.velocity[2] *= VELOCITY_DECAY;

				if (fabs(NPCS.NPC->client->ps.velocity[2]) < 2)
				{
					NPCS.NPC->client->ps.velocity[2] = 0;
				}
			}
		}
	}

	// Apply friction
	RT_Flying_ApplyFriction(1.0f);
}

void RT_Flying_Strafe(void)
{
	int side;
	vec3_t end, right;
	trace_t tr;

	if (Q_flrand(0.0f, 1.0f) > 0.7f
		|| !NPCS.NPC->enemy
		|| !NPCS.NPC->enemy->client)
	{
		// Do a regular style strafe
		AngleVectors(NPCS.NPC->client->renderInfo.eyeAngles, NULL, right, NULL);

		// Pick a random strafe direction, then check to see if doing a strafe would be
		//	reasonably valid
		side = rand() & 1 ? -1 : 1;
		VectorMA(NPCS.NPC->r.currentOrigin, RT_FLYING_STRAFE_DIS * side, right, end);

		trap->Trace(&tr, NPCS.NPC->r.currentOrigin, NULL, NULL, end, NPCS.NPC->s.number, MASK_SOLID, qfalse, 0, 0);

		// Close enough
		if (tr.fraction > 0.9f)
		{
			const float vel = RT_FLYING_STRAFE_VEL + Q_flrand(-20, 20);
			VectorMA(NPCS.NPC->client->ps.velocity, vel * side, right, NPCS.NPC->client->ps.velocity);
			if (!Q_irand(0, 3))
			{
				// Add a slight upward push
				if (NPCS.NPC->client->ps.velocity[2] < 300)
				{
					const float upPush = RT_FLYING_UPWARD_PUSH;
					if (NPCS.NPC->client->ps.velocity[2] < 300 + upPush)
					{
						NPCS.NPC->client->ps.velocity[2] += upPush;
					}
					else
					{
						NPCS.NPC->client->ps.velocity[2] = 300;
					}
				}
			}

			NPCS.NPCInfo->standTime = level.time + 1000 + Q_flrand(0.0f, 1.0f) * 500;
		}
	}
	else
	{
		vec3_t dir;
		// Do a strafe to try and keep on the side of their enemy
		AngleVectors(NPCS.NPC->enemy->client->renderInfo.eyeAngles, dir, right, NULL);

		// Pick a random side
		side = rand() & 1 ? -1 : 1;
		const float stDis = RT_FLYING_STRAFE_DIS * 2.0f;
		VectorMA(NPCS.NPC->enemy->r.currentOrigin, stDis * side, right, end);

		// then add a very small bit of random in front of/behind the player action
		VectorMA(end, Q_flrand(-1.0f, 1.0f) * 25, dir, end);

		trap->Trace(&tr, NPCS.NPC->r.currentOrigin, NULL, NULL, end, NPCS.NPC->s.number, MASK_SOLID, qfalse, 0, 0);

		// Close enough
		if (tr.fraction > 0.9f)
		{
			const float vel = RT_FLYING_STRAFE_VEL * 4 + Q_flrand(-20, 20);
			VectorSubtract(tr.endpos, NPCS.NPC->r.currentOrigin, dir);
			dir[2] *= 0.25; // do less upward change
			float dis = VectorNormalize(dir);
			if (dis > vel)
			{
				dis = vel;
			}
			// Try to move the desired enemy side
			VectorMA(NPCS.NPC->client->ps.velocity, dis, dir, NPCS.NPC->client->ps.velocity);

			if (!Q_irand(0, 3))
			{
				// Add a slight upward push
				if (NPCS.NPC->client->ps.velocity[2] < 300)
				{
					const float upPush = RT_FLYING_UPWARD_PUSH;
					if (NPCS.NPC->client->ps.velocity[2] < 300 + upPush)
					{
						NPCS.NPC->client->ps.velocity[2] += upPush;
					}
					else
					{
						NPCS.NPC->client->ps.velocity[2] = 300;
					}
				}
				else if (NPCS.NPC->client->ps.velocity[2] > 300)
				{
					NPCS.NPC->client->ps.velocity[2] = 300;
				}
			}

			NPCS.NPCInfo->standTime = level.time + 2500 + Q_flrand(0.0f, 1.0f) * 500;
		}
	}
}

void RT_Flying_Hunt(const qboolean visible, const qboolean advance)
{
	vec3_t forward;

	NPC_FaceEnemy(qtrue);

	// If we're not supposed to stand still, pursue the player
	if (NPCS.NPCInfo->standTime < level.time)
	{
		// Only strafe when we can see the player
		if (visible)
		{
			NPCS.NPC->delay = 0;
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
			NPCS.NPCInfo->goalEntity = NPCS.NPC->enemy;
			NPCS.NPCInfo->goalRadius = 24;

			NPCS.NPC->delay = 0;
			NPC_MoveToGoal(qtrue);
			return;
		}
	}
	//else move straight at/away from him
	VectorSubtract(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPC->r.currentOrigin, forward);
	forward[2] *= 0.1f;
	const float distance = VectorNormalize(forward);

	const float speed = RT_FLYING_FORWARD_BASE_SPEED + RT_FLYING_FORWARD_MULTIPLIER * g_npcspskill.integer;
	if (advance && distance < Q_flrand(256, 3096))
	{
		NPCS.NPC->delay = 0;
		VectorMA(NPCS.NPC->client->ps.velocity, speed, forward, NPCS.NPC->client->ps.velocity);
	}
	else if (distance < Q_flrand(0, 128))
	{
		if (NPCS.NPC->health <= 50)
		{
			//always back off
			NPCS.NPC->delay = 0;
		}
		else if (!TIMER_Done(NPCS.NPC, "backoffTime"))
		{
			//still backing off from end of last delay
			NPCS.NPC->delay = 0;
		}
		else if (!NPCS.NPC->delay)
		{
			//start a new delay
			NPCS.NPC->delay = Q_irand(0, 10 + 20 * (2 - g_npcspskill.integer));
		}
		else
		{
			//continue the current delay
			NPCS.NPC->delay--;
		}
		if (!NPCS.NPC->delay)
		{
			//delay done, now back off for a few seconds!
			TIMER_Set(NPCS.NPC, "backoffTime", Q_irand(2000, 5000));
			VectorMA(NPCS.NPC->client->ps.velocity, speed * -2, forward, NPCS.NPC->client->ps.velocity);
		}
	}
	else
	{
		NPCS.NPC->delay = 0;
	}
}

void RT_Flying_Ranged(const qboolean visible, const qboolean advance)
{
	if (NPCS.NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
	{
		RT_Flying_Hunt(visible, advance);
	}
}

void RT_Flying_Attack(void)
{
	// Always keep a good height off the ground
	RT_Flying_MaintainHeight();

	// Rate our distance to the target, and our visibilty
	const float distance = DistanceHorizontalSquared(NPCS.NPC->r.currentOrigin, NPCS.NPC->enemy->r.currentOrigin);
	const qboolean visible = NPC_ClearLOS4(NPCS.NPC->enemy);
	const qboolean advance = distance > 256.0f * 256.0f;

	// If we cannot see our target, move to see it
	if (visible == qfalse)
	{
		if (NPCS.NPCInfo->scriptFlags & SCF_CHASE_ENEMIES)
		{
			RT_Flying_Hunt(visible, advance);
			return;
		}
	}

	RT_Flying_Ranged(visible, advance);
}

void RT_Flying_Think(void)
{
	if (UpdateGoal())
	{
		//being scripted to go to a certain spot, don't maintain height
		if (NPC_MoveToGoal(qtrue))
		{
			//we could macro-nav to our goal
			if (NPCS.NPC->enemy && NPCS.NPC->enemy->health && NPCS.NPC->enemy->inuse)
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

	if (NPCS.NPC->random == 0.0f)
	{
		// used to offset seekers around a circle so they don't occupy the same spot.  This is not a fool-proof method.
		NPCS.NPC->random = Q_flrand(0.0f, 1.0f) * 6.3f; // roughly 2pi
	}

	if (NPCS.NPC->enemy && NPCS.NPC->enemy->health && NPCS.NPC->enemy->inuse)
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
extern void RT_CheckJump(void);

void NPC_BSRT_Default(void)
{
	if (NPCS.NPC->client->ps.groundEntityNum != ENTITYNUM_NONE)
	{
		if (NPCS.NPCInfo->rank >= RANK_LT)
		{
			//officers always stay in the air
			NPCS.NPC->client->ps.velocity[2] = Q_irand(50, 125);
			NPCS.NPC->NPC->aiFlags |= NPCAI_FLY; //fixme also, Inform NPC_HandleAIFlags we want to fly
		}
	}

	if (RT_Flying(NPCS.NPC))
	{
		//FIXME: only officers need do this, right?
		RT_Flying_Think();
	}
	else if (NPCS.NPC->enemy != NULL)
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