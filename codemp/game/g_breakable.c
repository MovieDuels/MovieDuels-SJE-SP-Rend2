//SP code for breakables

#include "g_local.h"

#define MDL_OTHER			0
#define MDL_ARMOR_HEALTH	1
#define MDL_AMMO			2

extern void G_MiscModelExplosion(vec3_t mins, vec3_t maxs, int size, material_t chunkType);
extern void G_SoundOnEnt(gentity_t* ent, soundChannel_t channel, const char* sound_path);
extern void G_Chunks(int owner, vec3_t origin, const vec3_t normal, const vec3_t mins, const vec3_t maxs,
	float speed, int numChunks, material_t chunkType, int customChunk, float baseScale);

void misc_model_breakable_die(gentity_t* self, const gentity_t* inflictor, gentity_t* attacker, int damage,
	int meansOfDeath)
{
	float size = 0;
	vec3_t dir, up, dis;

	if (self->die == NULL) //i was probably already killed since my die func was removed
	{
#ifndef FINAL_BUILD
		G_Printf(S_COLOR_YELLOW"Recursive misc_model_breakable_die.  Use targets probably pointing back at self.\n");
#endif
		return; //this happens when you have a cyclic target chain!
	}
	//Stop any scripts that are currently running (FLUSH)... ?
	//Turn off animation
	self->s.frame = self->start_frame = self->end_frame = 0;
	self->r.svFlags &= ~SVF_ANIMATING;

	self->health = 0;
	//Throw some chunks
	AngleVectors(self->s.apos.trBase, dir, NULL, NULL);
	VectorNormalize(dir);

	int numChunks = Q_flrand(0.0f, 1.0f) * 6 + 20;

	VectorSubtract(self->r.absmax, self->r.absmin, dis);

	// This formula really has no logical basis other than the fact that it seemed to be the closest to yielding the results that I wanted.
	// Volume is length * width * height...then break that volume down based on how many chunks we have
	float scale = sqrt(sqrt(dis[0] * dis[1] * dis[2])) * 1.75f;

	if (scale > 48)
	{
		size = 2;
	}
	else if (scale > 24)
	{
		size = 1;
	}

	scale = scale / numChunks;

	if (self->radius > 0.0f)
	{
		// designer wants to scale number of chunks, helpful because the above scale code is far from perfect
		//	I do this after the scale calculation because it seems that the chunk size generally seems to be very close, it's just the number of chunks is a bit weak
		numChunks *= self->radius;
	}

	VectorAdd(self->r.absmax, self->r.absmin, dis);
	VectorScale(dis, 0.5f, dis);

	G_Chunks(self->s.number, dis, dir, self->r.absmin, self->r.absmax, 300, numChunks, self->material, 0, scale);

	self->pain = NULL;
	self->die = NULL;

	self->takedamage = qfalse;

	if (!(self->spawnflags & 4))
	{
		//We don't want to stay solid
		self->s.solid = 0;
		self->r.contents = 0;
		self->clipmask = 0;
		trap->LinkEntity((sharedEntity_t*)self);
	}

	VectorSet(up, 0, 0, 1);

	if (self->target)
	{
		G_UseTargets(self, attacker);
	}

	if (inflictor->client)
	{
		VectorSubtract(self->r.currentOrigin, inflictor->r.currentOrigin, dir);
		VectorNormalize(dir);
	}
	else
	{
		VectorCopy(up, dir);
	}

	if (!(self->spawnflags & 2048)) // NO_EXPLOSION
	{
		// Ok, we are allowed to explode, so do it now!
		if (self->splashDamage > 0 && self->splashRadius > 0)
		{
			//explode
			vec3_t org;
			AddSightEvent(attacker, self->r.currentOrigin, 256, AEL_DISCOVERED, 100);
			AddSoundEvent(attacker, self->r.currentOrigin, 128, AEL_DISCOVERED, qfalse, qtrue);
			//FIXME: am I on ground or not?
			// up the origin a little for the damage check, because several models have their origin on the ground, so they don't alwasy do damage, not the optimal solution...
			VectorCopy(self->r.currentOrigin, org);
			if (self->r.mins[2] > -4)
			{
				//origin is going to be below it or very very low in the model
				//center the origin
				org[2] = self->r.currentOrigin[2] + self->r.mins[2] + (self->r.maxs[2] - self->r.mins[2]) / 2.0f;
			}
			g_radius_damage(org, self, self->splashDamage, self->splashRadius, self, self, MOD_UNKNOWN);

			if (self->model && (Q_stricmp("models/map_objects/ships/tie_fighter.md3", self->model) == 0 ||
				Q_stricmp("models/map_objects/ships/tie_bomber.md3", self->model) == 0))
			{
				//TEMP HACK for Tie Fighters- they're HUGE
				G_PlayEffect(G_EffectIndex("explosions/fighter_explosion2"), self->r.currentOrigin,
					self->r.currentAngles);
				G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/weapons/tie_fighter/TIEexplode.wav"));
				self->s.loopSound = 0;
			}
			else
			{
				G_MiscModelExplosion(self->r.absmin, self->r.absmax, size, self->material);
				G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/weapons/explosions/cargoexplode.wav"));
				self->s.loopSound = 0;
			}
		}
		else
		{
			//just break
			AddSightEvent(attacker, self->r.currentOrigin, 128, AEL_DISCOVERED, 0);
			AddSoundEvent(attacker, self->r.currentOrigin, 64, AEL_SUSPICIOUS, qfalse, qtrue);
			//FIXME: am I on ground or not?
			// This is the default explosion
			G_MiscModelExplosion(self->r.absmin, self->r.absmax, size, self->material);
			G_Sound(self, CHAN_AUTO, G_SoundIndex("sound/weapons/explosions/cargoexplode.wav"));
		}
	}

	self->think = NULL;
	self->nextthink = -1;

	if (self->s.modelindex2 != -1 && !(self->spawnflags & 8))
	{
		//FIXME: modelindex doesn't get set to -1 if the damage model doesn't exist
		self->r.svFlags |= SVF_BROKEN;
		self->s.modelindex = self->s.modelindex2;
		G_ActivateBehavior(self, BSET_DEATH);
	}
	else
	{
		G_FreeEntity(self);
	}
}

void misc_model_breakable_touch(gentity_t* self, gentity_t* other, trace_t* trace)
{
	//touch function for model breakable.  doesn't actually do anything, but we need one to prevent crashs like the one on taspir2
}

void misc_model_throw_at_target4(gentity_t* self, const gentity_t* activator)
{
	vec3_t push_dir, kvel;
	float knockback = 200;
	float mass = self->mass;
	const gentity_t* target = G_Find(NULL, FOFS(targetname), self->target4);
	if (!target)
	{
		//nothing to throw ourselves at...
		return;
	}
	VectorSubtract(target->r.currentOrigin, self->r.currentOrigin, push_dir);
	knockback -= VectorNormalize(push_dir);
	if (knockback < 100)
	{
		knockback = 100;
	}
	VectorCopy(self->r.currentOrigin, self->s.pos.trBase);
	self->s.pos.trTime = level.time; // move a bit on the very first frame
	if (self->s.pos.trType != TR_INTERPOLATE)
	{
		//don't do this to rolling missiles
		self->s.pos.trType = TR_GRAVITY;
	}

	if (mass < 50)
	{
		//???
		mass = 50;
	}

	if (g_gravity.value > 0)
	{
		VectorScale(push_dir, g_knockback.value * knockback / mass * 0.8, kvel);
		kvel[2] = push_dir[2] * g_knockback.value * knockback / mass * 1.5;
	}
	else
	{
		VectorScale(push_dir, g_knockback.value * knockback / mass, kvel);
	}

	VectorAdd(self->s.pos.trDelta, kvel, self->s.pos.trDelta);
	if (g_gravity.value > 0)
	{
		if (self->s.pos.trDelta[2] < knockback)
		{
			self->s.pos.trDelta[2] = knockback;
		}
	}
	//no trDuration?
	if (self->think != G_RunObject)
	{
		//objects spin themselves?
		self->s.apos.trTime = level.time;
		self->s.apos.trType = TR_LINEAR;
		VectorClear(self->s.apos.trDelta);
		self->s.apos.trDelta[1] = Q_irand(-800, 800);
	}
}

void misc_model_use(gentity_t* self, const gentity_t* other, gentity_t* activator)
{
	if (self->target4)
	{
		//throw me at my target!
		misc_model_throw_at_target4(self, activator);
		return;
	}

	if (self->health <= 0 && self->maxHealth > 0)
	{
		//used while broken fired target3
		G_UseTargets2(self, activator, self->target3);
		return;
	}

	// Become solid again.
	if (!self->count)
	{
		self->count = 1;
		self->activator = activator;
		self->r.svFlags &= ~SVF_NOCLIENT;
		self->s.eFlags &= ~EF_NODRAW;
	}

	G_ActivateBehavior(self, BSET_USE);
	//Don't explode if they've requested it to not
	if (self->spawnflags & 64)
	{
		//Usemodels toggling
		if (self->spawnflags & 32)
		{
			if (self->s.modelindex == self->sound1to2)
			{
				self->s.modelindex = self->sound2to1;
			}
			else
			{
				self->s.modelindex = self->sound1to2;
			}
		}

		return;
	}

	self->die = misc_model_breakable_die;
	misc_model_breakable_die(self, other, activator, self->health, MOD_UNKNOWN);
}

//pain function for model_breakables
void misc_model_breakable_pain(gentity_t* self, gentity_t* attacker, int damage)
{
	if (self->health > 0)
	{
		// still alive, react to the pain
		if (self->paintarget)
		{
			G_UseTargets2(self, self->activator, self->paintarget);
		}
		// Don't do script if dead
		G_ActivateBehavior(self, BSET_PAIN);
	}
}

void health_shutdown(gentity_t* self)
{
	if (!(self->s.eFlags & EF_ANIM_ONCE))
	{
		self->s.eFlags &= ~EF_ANIM_ALLFAST;
		self->s.eFlags |= EF_ANIM_ONCE;

		// Switch to and animate its used up model.
		if (!Q_stricmp(self->model, "models/mapobjects/stasis/plugin2.md3"))
		{
			self->s.modelindex = self->s.modelindex2;
		}
		else if (!Q_stricmp(self->model, "models/mapobjects/borg/plugin2.md3"))
		{
			self->s.modelindex = self->s.modelindex2;
		}
		else if (!Q_stricmp(self->model, "models/mapobjects/stasis/plugin2_floor.md3"))
		{
			self->s.modelindex = self->s.modelindex2;
		}
		else if (!Q_stricmp(self->model, "models/mapobjects/forge/panels.md3"))
		{
			self->s.modelindex = self->s.modelindex2;
		}

		trap->LinkEntity((sharedEntity_t*)self);
	}
}

//add amount number of health to given player
qboolean ITM_AddHealth(gentity_t* ent, const int amount)
{
	if (ent->health == ent->client->ps.stats[STAT_MAX_HEALTH])
	{
		//maxed out health as is
		return qfalse;
	}

	ent->health += amount;
	if (ent->health > ent->client->ps.stats[STAT_MAX_HEALTH])
		ent->health = ent->client->ps.stats[STAT_MAX_HEALTH];

	ent->client->ps.stats[STAT_HEALTH] = ent->health;
	return qtrue;
}

qboolean ITM_AddArmor(const gentity_t* ent, const int amount)
{
	if (ent->client->ps.stats[STAT_ARMOR] == ent->client->ps.stats[STAT_MAX_HEALTH])
	{
		//maxed out health as is
		return qfalse;
	}

	ent->client->ps.stats[STAT_ARMOR] += amount;
	if (ent->client->ps.stats[STAT_ARMOR] > ent->client->ps.stats[STAT_MAX_HEALTH])
		ent->client->ps.stats[STAT_ARMOR] = ent->client->ps.stats[STAT_MAX_HEALTH];

	return qtrue;
}

void health_use(gentity_t* self, gentity_t* other, gentity_t* activator);

void health_think(gentity_t* ent)
{
	// He's dead, Jim. Don't give him health
	if (ent->enemy->health < 1)
	{
		ent->count = 0;
		ent->think = NULL;
	}

	// Still has power to give
	if (ent->count > 0)
	{
		// For every 3 points of health, you get 1 point of armor
		// BUT!!! after health is filled up, you get the full energy going to armor

		int dif = ent->enemy->client->ps.stats[STAT_MAX_HEALTH] - ent->enemy->health;

		if (dif > 3)
		{
			dif = 3;
		}
		else if (dif < 0)
		{
			dif = 0;
		}

		if (dif > ent->count) // Can't give more than count
		{
			dif = ent->count;
		}

		if (ITM_AddHealth(ent->enemy, dif) && dif > 0)
		{
			ITM_AddArmor(ent->enemy, 1); // 1 armor for every 3 health

			ent->count -= dif;
			ent->nextthink = level.time + 10;
		}
		else // User has taken all health he can hold, see about giving it all to armor
		{
			dif = ent->enemy->client->ps.stats[STAT_MAX_HEALTH] -
				ent->enemy->client->ps.stats[STAT_ARMOR];

			if (dif > 3)
			{
				dif = 3;
			}
			else if (dif < 0)
			{
				dif = 0;
			}

			if (ent->count < dif) // Can't give more than count
			{
				dif = ent->count;
			}

			if (!ITM_AddArmor(ent->enemy, dif) || dif <= 0)
			{
				ent->use = health_use;
				ent->think = NULL;
			}
			else
			{
				ent->count -= dif;
				ent->nextthink = level.time + 10;
			}
		}
	}

	if (ent->count < 1)
	{
		health_shutdown(ent);
	}
}

void health_use(gentity_t* self, gentity_t* other, gentity_t* activator)
{
	//FIXME: Heal entire team?  Or only those that are undying...?

	G_ActivateBehavior(self, BSET_USE);

	if (self->think != NULL)
	{
		self->think = NULL;
	}
	else
	{
		int dif;
		if (other->client)
		{
			// He's dead, Jim. Don't give him health
			if (other->client->ps.stats[STAT_HEALTH] < 1)
			{
				dif = 1;
				self->count = 0;
			}
			else
			{
				// Health
				dif = other->client->ps.stats[STAT_MAX_HEALTH] - other->client->ps.stats[STAT_HEALTH];
				// Armor
				int dif2 = other->client->ps.stats[STAT_MAX_HEALTH] - other->client->ps.stats[STAT_ARMOR];
				int hold = dif2 - dif;
				if (hold > 0) // Need more armor than health
				{
					// Calculate total amount of station energy needed.
					hold = dif / 3; //	For every 3 points of health, you get 1 point of armor
					dif2 -= hold;
					dif2 += dif;

					dif = dif2;
				}
			}
		}
		else
		{
			// Being triggered to be used up
			dif = 1;
			self->count = 0;
		}

		// Does player already have full health and full armor?
		if (dif > 0)
		{
			if (dif >= self->count || self->count < 1) // use it all up?
			{
				health_shutdown(self);
			}
			// Use target when used
			if (self->spawnflags & 8)
			{
				G_UseTargets(self, activator);
			}

			self->use = NULL;
			self->enemy = other;
			self->think = health_think;
			self->nextthink = level.time + 50;
		}
		else
		{
			//G_Sound(self, G_SoundIndex("sound/weapons/noammo.wav") );
		}
	}
}

void ammo_shutdown(gentity_t* self)
{
	if (!(self->s.eFlags & EF_ANIM_ONCE))
	{
		self->s.eFlags &= ~EF_ANIM_ALLFAST;
		self->s.eFlags |= EF_ANIM_ONCE;

		trap->LinkEntity((sharedEntity_t*)self);
	}
}

qboolean Add_Ammo2(const gentity_t* ent, const int ammotype, const int amount)
{
	if (ent->client->ps.ammo[ammotype] == ammoData[ammotype].max)
	{
		//ammo maxed
		return qfalse;
	}

	ent->client->ps.ammo[ammotype] += amount;

	if (ent->client->ps.ammo[ammotype] > ammoData[ammotype].max)
		ent->client->ps.ammo[ammotype] = ammoData[ammotype].max;

	return qtrue;
}

void ammo_use(gentity_t* self, gentity_t* other, gentity_t* activator);

void ammo_think(gentity_t* ent)
{
	// Still has ammo to give
	if (ent->count > 0 && ent->enemy)
	{
		int dif = ammoData[AMMO_BLASTER].max - ent->enemy->client->ps.ammo[AMMO_BLASTER];

		if (dif > 2)
		{
			dif = 2;
		}
		else if (dif < 0)
		{
			dif = 0;
		}

		if (ent->count < dif) // Can't give more than count
		{
			dif = ent->count;
		}

		// Give player ammo
		if (Add_Ammo2(ent->enemy, AMMO_BLASTER, dif) && dif != 0)
		{
			ent->count -= dif;
			ent->nextthink = level.time + 10;
		}
		else // User has taken all ammo he can hold
		{
			ent->use = ammo_use;
			ent->think = NULL;
		}
	}

	if (ent->count < 1)
	{
		ammo_shutdown(ent);
	}
}

void ammo_use(gentity_t* self, gentity_t* other, gentity_t* activator)
{
	G_ActivateBehavior(self, BSET_USE);

	if (self->think != NULL)
	{
		if (self->use != NULL)
		{
			self->think = NULL;
		}
	}
	else
	{
		int dif;
		if (other->client)
		{
			dif = ammoData[AMMO_BLASTER].max - other->client->ps.ammo[AMMO_BLASTER];
		}
		else
		{
			// Being triggered to be used up
			dif = 1;
			self->count = 0;
		}

		// Does player already have full ammo?
		if (dif > 0)
		{
			if (dif >= self->count || self->count < 1) // use it all up?
			{
				ammo_shutdown(self);
			}
		}
		else
		{
			//G_Sound(self, G_SoundIndex("sound/weapons/noammo.wav") );
		}
		// Use target when used
		if (self->spawnflags & 8)
		{
			G_UseTargets(self, activator);
		}

		self->use = NULL;
		G_SetEnemy(self, other);
		self->think = ammo_think;
		self->nextthink = level.time + 50;
	}
}

//initization for misc_model_breakable
void misc_model_breakable_init(gentity_t* ent)
{
	const int type = MDL_OTHER;

	if (!ent->model)
	{
		trap->Error(ERR_DROP, "no model set on %s at (%.1f %.1f %.1f)\n", ent->classname, ent->s.origin[0],
			ent->s.origin[1], ent->s.origin[2]);
	}
	//Main model
	ent->s.modelindex = ent->sound2to1 = G_ModelIndex(ent->model);

	if (ent->spawnflags & 1)
	{
		//Blocks movement
		ent->r.contents = CONTENTS_SOLID | CONTENTS_OPAQUE | CONTENTS_BODY | CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP;
		//Was CONTENTS_SOLID, but only architecture should be this
		ent->s.solid = 2; //SOLID_BBOX
		ent->clipmask = MASK_PLAYERSOLID;
	}
	else if (ent->health)
	{
		//Can only be shot
		ent->r.contents = CONTENTS_SHOTCLIP;
	}

	if (type == MDL_OTHER)
	{
		ent->use = misc_model_use;
	}

	if (ent->health)
	{
		G_SoundIndex("sound/weapons/explosions/cargoexplode.wav");
		ent->maxHealth = ent->health;
		G_ScaleNetHealth(ent);
		ent->takedamage = qtrue;
		ent->pain = misc_model_breakable_pain;
		ent->die = misc_model_breakable_die;
	}

	ent->touch = misc_model_breakable_touch;
}

void TieFighterThink(gentity_t* self)
{
	const gentity_t* player = &g_entities[0];

	if (self->health <= 0)
	{
		return;
	}

	self->nextthink = level.time + FRAMETIME;
	if (player)
	{
		vec3_t playerDir, fighterDir, fwd, rt;

		//use player eyepoint
		VectorSubtract(player->r.currentOrigin, self->r.currentOrigin, playerDir);
		const float playerDist = VectorNormalize(playerDir);
		VectorSubtract(self->r.currentOrigin, self->r.lastOrigin, fighterDir);
		VectorCopy(self->r.currentOrigin, self->r.lastOrigin);
		float fighterSpeed = VectorNormalize(fighterDir) * 1000;
		AngleVectors(self->r.currentAngles, fwd, rt, NULL);

		if (fighterSpeed)
		{
			// Magic number fun!  Speed is used for banking, so modulate the speed by a sine wave
			fighterSpeed *= sin(100 * 0.003);

			// Clamp to prevent harsh rolling
			if (fighterSpeed > 10)
				fighterSpeed = 10;

			const float side = fighterSpeed * DotProduct(fighterDir, rt);
			self->s.apos.trBase[2] -= side;
		}

		//FIXME: bob up/down, strafe left/right some
		const float dot = DotProduct(playerDir, fighterDir);
		if (dot > 0)
		{
			//heading toward the player
			if (playerDist < 1024)
			{
				if (DotProduct(playerDir, fwd) > 0.7)
				{
					//facing the player
					if (self->attackDebounceTime < level.time)
					{
						gentity_t* bolt = G_Spawn();

						bolt->classname = "tie_proj";
						bolt->nextthink = level.time + 10000;
						bolt->think = G_FreeEntity;
						bolt->s.eType = ET_MISSILE;
						bolt->s.weapon = WP_BLASTER;
						bolt->owner = self;
						bolt->damage = 30;
						bolt->dflags = DAMAGE_NO_KNOCKBACK;
						// Don't push them around, or else we are constantly re-aiming
						bolt->splashDamage = 0;
						bolt->splashRadius = 0;
						bolt->methodOfDeath = MOD_ELECTROCUTE; // ?
						bolt->clipmask = MASK_SHOT;

						bolt->s.pos.trType = TR_LINEAR;
						bolt->s.pos.trTime = level.time; // move a bit on the very first frame
						VectorCopy(self->r.currentOrigin, bolt->s.pos.trBase);
						VectorScale(fwd, 8000, bolt->s.pos.trDelta);
						SnapVector(bolt->s.pos.trDelta); // save net bandwidth
						VectorCopy(self->r.currentOrigin, bolt->r.currentOrigin);

						if (!Q_irand(0, 2))
						{
							G_SoundOnEnt(bolt, CHAN_VOICE, "sound/weapons/tie_fighter/tie_fire.wav");
						}
						else
						{
							G_SoundOnEnt(bolt, CHAN_VOICE,
								va("sound/weapons/tie_fighter/tie_fire%d.wav", Q_irand(2, 3)));
						}
						self->attackDebounceTime = level.time + Q_irand(300, 2000);
					}
				}
			}
		}

		if (playerDist < 1024) //512 )
		{
			//within range to start our sound
			if (dot > 0)
			{
				if (!self->fly_sound_debounce_time)
				{
					//start sound
					G_SoundOnEnt(self, CHAN_VOICE, va("sound/weapons/tie_fighter/tiepass%d.wav", Q_irand(1, 5)));
					self->fly_sound_debounce_time = 2000;
				}
				else
				{
					//sound already started
					self->fly_sound_debounce_time = -1;
				}
			}
		}
		else if (self->fly_sound_debounce_time < level.time)
		{
			self->fly_sound_debounce_time = 0;
		}
	}
}

void misc_model_breakable_gravity_init(gentity_t* ent, const qboolean dropToFloor)
{
	trace_t tr;

	G_EffectIndex("melee/kick_impact");
	G_EffectIndex("melee/kick_impact_silent");
	G_SoundIndex("sound/movers/objects/objectHit.wav");
	G_SoundIndex("sound/movers/objects/objectHitHeavy.wav");
	G_SoundIndex("sound/movers/objects/objectBreak.wav");
	ent->s.eType = ET_GENERAL;
	ent->flags |= FL_BOUNCE_HALF;
	ent->clipmask = MASK_SOLID | CONTENTS_BODY | CONTENTS_MONSTERCLIP | CONTENTS_BOTCLIP; //?
	if (!ent->mass)
	{
		//not overridden by designer
		ent->mass = VectorLength(ent->r.maxs) + VectorLength(ent->r.mins);
	}
	ent->physicsBounce = ent->mass;
	//drop to floor
	if (dropToFloor)
	{
		vec3_t bottom;
		vec3_t top;
		VectorCopy(ent->r.currentOrigin, top);
		top[2] += 1;
		VectorCopy(ent->r.currentOrigin, bottom);
		bottom[2] = MIN_WORLD_COORD;
		trap->Trace(&tr, top, ent->r.mins, ent->r.maxs, bottom, ent->s.number, MASK_NPCSOLID, qfalse, 0, 0);
		if (!tr.allsolid && !tr.startsolid && tr.fraction < 1.0)
		{
			G_SetOrigin(ent, tr.endpos);
			trap->LinkEntity((sharedEntity_t*)ent);
		}
	}
	else
	{
		G_SetOrigin(ent, ent->r.currentOrigin);
		trap->LinkEntity((sharedEntity_t*)ent);
	}
	//set up for object thinking
	if (VectorCompare(ent->s.pos.trDelta, vec3_origin))
	{
		//not moving
		ent->s.pos.trType = TR_STATIONARY;
	}
	else
	{
		ent->s.pos.trType = TR_GRAVITY;
	}
	VectorCopy(ent->r.currentOrigin, ent->s.pos.trBase);
	VectorClear(ent->s.pos.trDelta);
	ent->s.pos.trTime = level.time;
	if (VectorCompare(ent->s.apos.trDelta, vec3_origin))
	{
		//not moving
		ent->s.apos.trType = TR_STATIONARY;
	}
	else
	{
		ent->s.apos.trType = TR_LINEAR;
	}
	VectorCopy(ent->r.currentAngles, ent->s.apos.trBase);
	VectorClear(ent->s.apos.trDelta);
	ent->s.apos.trTime = level.time;
	ent->nextthink = level.time + FRAMETIME;
	ent->think = G_RunObject;
}

extern void CacheChunkEffects(material_t material);
extern stringID_table_t TeamTable[];

void SP_misc_model_breakable(gentity_t* ent)
{
	char damageModel[MAX_QPATH];
	char chunkModel[MAX_QPATH];
	char useModel[MAX_QPATH];

	float grav = 0;
	int forceVisible = 0;
	int redCrosshair = 0;

	G_SpawnInt("material", "8", (int*)&ent->material);
	G_SpawnFloat("radius", "1", &ent->radius); // used to scale chunk code if desired by a designer
	qboolean bHasScale = G_SpawnVector("modelscale_vec", "0 0 0", ent->modelScale);

	if (!bHasScale)
	{
		float temp;
		G_SpawnFloat("modelscale", "0", &temp);
		if (temp != 0.0f)
		{
			ent->modelScale[0] = ent->modelScale[1] = ent->modelScale[2] = temp;
			bHasScale = qtrue;
		}
	}

	if (!ent->model)
	{
		// must have a model
		Com_Printf(S_COLOR_RED"ERROR: misc_model_breakable at %s has no md3 model specified\n", vtos(ent->s.origin));

		ent->think = G_FreeEntity;
		ent->nextthink = level.time + FRAMETIME;

		return;
	}

	const int len = strlen(ent->model) - 4;

	if (ent->model[len] != '.') //we're expecting ".md3"
	{
		// if model is not md3, do not spawn the entity
		Com_Printf(S_COLOR_RED"ERROR: misc_model_breakable at %s has no md3 path specified\n", vtos(ent->s.origin));

		ent->think = G_FreeEntity;
		ent->nextthink = level.time + FRAMETIME;

		return;
	}

	CacheChunkEffects(ent->material);
	misc_model_breakable_init(ent);

	strncpy(damageModel, ent->model, sizeof damageModel);
	damageModel[len] = 0; //chop extension
	strncpy(chunkModel, damageModel, sizeof chunkModel);
	strncpy(useModel, damageModel, sizeof useModel);

	if (ent->takedamage)
	{
		//Dead/damaged model
		if (!(ent->spawnflags & 8))
		{
			//no dmodel
			strcat(damageModel, "_d1.md3");
			ent->s.modelindex2 = G_ModelIndex(damageModel);
		}
	}

	//Use model
	if (ent->spawnflags & 32)
	{
		//has umodel
		strcat(useModel, "_u1.md3");
		ent->sound1to2 = G_ModelIndex(useModel);
	}

	G_SpawnVector("mins", "-16 -16 -16", ent->r.mins);
	G_SpawnVector("maxs", "16 16 16", ent->r.maxs);

	// Scale up the tie-bomber bbox a little.
	if (ent->model && Q_stricmp("models/map_objects/ships/tie_bomber.md3", ent->model) == 0)
	{
		VectorSet(ent->r.mins, -80, -80, -80);
		VectorSet(ent->r.maxs, 80, 80, 80);
	}

	if (bHasScale)
	{
		//scale the x axis of the bbox up.
		ent->r.maxs[0] *= ent->modelScale[0]; //*scaleFactor;
		ent->r.mins[0] *= ent->modelScale[0]; //*scaleFactor;

		//scale the y axis of the bbox up.
		ent->r.maxs[1] *= ent->modelScale[1]; //*scaleFactor;
		ent->r.mins[1] *= ent->modelScale[1]; //*scaleFactor;

		//scale the z axis of the bbox up and adjust origin accordingly
		ent->r.maxs[2] *= ent->modelScale[2];
		const float oldMins2 = ent->r.mins[2];
		ent->r.mins[2] *= ent->modelScale[2];
		ent->s.origin[2] += oldMins2 - ent->r.mins[2];
	}

	if (ent->spawnflags & 2)
	{
		ent->s.eFlags |= EF_ANIM_ALLFAST;
	}

	G_SetOrigin(ent, ent->s.origin);
	G_SetAngles(ent, ent->s.angles);
	trap->LinkEntity((sharedEntity_t*)ent);

	if (ent->spawnflags & 128)
	{
		//Can be used by the player's BUTTON_USE
		ent->r.svFlags |= SVF_PLAYER_USABLE;
	}

	if (ent->team && ent->team[0])
	{
		ent->teamnodmg = (team_t)GetIDForString(TeamTable, ent->team);
		if (ent->teamnodmg == TEAM_FREE)
		{
			trap->Error(ERR_DROP, "team name %s not recognized\n", ent->team);
		}
	}

	ent->team = NULL;

	//HACK
	if (ent->model && Q_stricmp("models/map_objects/ships/x_wing_nogear.md3", ent->model) == 0)
	{
		if (ent->splashDamage > 0 && ent->splashRadius > 0)
		{
			ent->s.loopSound = G_SoundIndex("sound/vehicles/x-wing/loop.wav");
		}
	}
	else if (ent->model && Q_stricmp("models/map_objects/ships/tie_fighter.md3", ent->model) == 0)
	{
		//run a think
		G_EffectIndex("explosions/fighter_explosion2");
		G_SoundIndex("sound/weapons/tie_fighter/tiepass1.wav");
		G_SoundIndex("sound/weapons/tie_fighter/tie_fire.wav");
		G_SoundIndex("sound/weapons/tie_fighter/TIEexplode.wav");

		if (ent->splashDamage > 0 && ent->splashRadius > 0)
		{
			vec3_t color;

			ent->s.loopSound = G_SoundIndex("sound/vehicles/tie-bomber/loop.wav");
			const qboolean lightSet = qtrue;

			color[0] = 1;
			color[1] = 1;
			color[2] = 1;
			if (lightSet)
			{
				const float light = 255;
				int r = color[0] * 255;
				if (r > 255)
				{
					r = 255;
				}
				int g = color[1] * 255;
				if (g > 255)
				{
					g = 255;
				}
				int b = color[2] * 255;
				if (b > 255)
				{
					b = 255;
				}
				int i = light / 4;
				if (i > 255)
				{
					i = 255;
				}
				ent->s.constantLight = r | g << 8 | b << 16 | i << 24;
			}
		}
	}
	else if (ent->model && Q_stricmp("models/map_objects/ships/tie_bomber.md3", ent->model) == 0)
	{
		G_EffectIndex("ships/tiebomber_bomb_falling");
		G_EffectIndex("ships/tiebomber_explosion2");
		G_EffectIndex("explosions/fighter_explosion2");
		G_SoundIndex("sound/weapons/tie_fighter/TIEexplode.wav");
		ent->nextthink = level.time + FRAMETIME;
		ent->attackDebounceTime = level.time + 1000;
		// We only take damage from a heavy weapon class missiles.
		ent->flags |= FL_DMG_BY_HEAVY_WEAP_ONLY;
		ent->s.loopSound = G_SoundIndex("sound/vehicles/tie-bomber/loop.wav");
	}

	G_SpawnFloat("gravity", "0", &grav);
	if (grav)
	{
		//affected by gravity
		G_SetAngles(ent, ent->s.angles);
		G_SetOrigin(ent, ent->r.currentOrigin);
		G_SpawnString("throwtarget", NULL, &ent->target4); // used to throw itself at something
		misc_model_breakable_gravity_init(ent, qtrue);
	}

	// Start off.
	if (ent->spawnflags & 4096)
	{
		ent->s.solid = 0;
		ent->r.contents = 0;
		ent->clipmask = 0;
		ent->r.svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
		ent->count = 0;
	}

	G_SpawnInt("forcevisible", "0", &forceVisible);
	if (forceVisible)
	{
		//can see these through walls with force sight, so must be broadcast
		if (VectorCompare(ent->s.origin, vec3_origin))
		{
			//no origin brush
			ent->r.svFlags |= SVF_BROADCAST;
		}
		ent->s.eFlags |= EF_FORCE_VISIBLE;
	}

	G_SpawnInt("redCrosshair", "0", &redCrosshair);
	if (redCrosshair)
	{
		ent->flags |= FL_RED_CROSSHAIR;
	}
}