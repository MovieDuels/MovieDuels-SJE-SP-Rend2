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
#include "b_local.h"
#include "g_functions.h"
#include "wp_saber.h"
#include "w_local.h"

extern wristWeapon_t missileStates[4];
constexpr auto MELEE_SWING_WOOKIE_DAMAGE = 50;
constexpr auto MELEE_SWING_EXTRA_DAMAGE = 15;

void WP_Melee(gentity_t* ent)
//---------------------------------------------------------
{
	trace_t tr;
	vec3_t mins, maxs, end;
	int damage = ent->s.number ? g_spskill->integer * 2 + 1 : 3;
	const float range = ent->s.number ? 64 : 32;
	const qboolean isBobaPlayer = !ent->s.number && ent->client->NPC_class == CLASS_BOBAFETT && ent->client->ps.
		forcePowerDuration[missileStates[BOBA_MISSILE_VIBROBLADE].dummyForcePower]
		? qtrue
		: qfalse;
	const qboolean isMandoPlayer = !ent->s.number && ent->client->NPC_class == CLASS_MANDALORIAN && ent->client->ps.
		forcePowerDuration[missileStates[BOBA_MISSILE_VIBROBLADE].dummyForcePower]
		? qtrue
		: qfalse;
	const qboolean isJangoPlayer = !ent->s.number && ent->client->NPC_class == CLASS_JANGO && ent->client->ps.
		forcePowerDuration[missileStates[BOBA_MISSILE_VIBROBLADE].dummyForcePower]
		? qtrue
		: qfalse;
	const qboolean isJangoDualPlayer = !ent->s.number && ent->client->NPC_class == CLASS_JANGODUAL && ent->client->ps.
		forcePowerDuration[missileStates[BOBA_MISSILE_VIBROBLADE].dummyForcePower]
		? qtrue
		: qfalse;

	VectorMA(muzzle, range, forward_vec, end);

	VectorSet(maxs, 6, 6, 6);
	VectorScale(maxs, -1, mins);

	gi.trace(&tr, muzzle, mins, maxs, end, ent->s.number, MASK_SHOT, static_cast<EG2_Collision>(0), 0);

	if (tr.entityNum >= ENTITYNUM_WORLD)
	{
		if (tr.entityNum == ENTITYNUM_WORLD)
		{
			G_PlayEffect(G_EffectIndex("melee/punch_impact"), tr.endpos, forward_vec);
		}
		return;
	}

	gentity_t* tr_ent = &g_entities[tr.entityNum];

	if (ent->client && !PM_DroidMelee(ent->client->NPC_class))
	{
		if (ent->s.number || ent->alt_fire)
		{
			damage *= Q_irand(2, 3);
		}
		else
		{
			damage *= Q_irand(1, 2);
		}
	}

	if (isBobaPlayer)
	{
		damage = 50;
	}
	else if (isMandoPlayer || isJangoPlayer || isJangoDualPlayer)
	{
		damage = 75;
	}

	if (tr_ent && tr_ent->takedamage)
	{
		int dflags = DAMAGE_NO_KNOCKBACK;
		if (isBobaPlayer)
		{
			G_Sound(ent, G_SoundIndex(va("sound/weapons/sword/stab%d.wav", Q_irand(1, 4))));
		}
		else if (isMandoPlayer || isJangoPlayer || isJangoDualPlayer)
		{
			G_Sound(ent, G_SoundIndex(va("sound/weapons/sword/stab%d.wav", Q_irand(1, 4))));
		}
		else if (ent->s.weapon == WP_NOGHRI_STICK || ent->s.weapon == WP_TUSKEN_RIFLE)
		{
			G_Sound(tr_ent, G_SoundIndex(va("sound/weapons/tusken_staff/stickhit%d.wav", Q_irand(1, 4))));
		}
		else
		{
			G_PlayEffect(G_EffectIndex("melee/punch_impact"), tr.endpos, forward_vec);
		}

		if (ent->NPC && ent->NPC->aiFlags & NPCAI_HEAVY_MELEE)
		{
			//4x damage for heavy melee class
			damage *= 4;
			dflags &= ~DAMAGE_NO_KNOCKBACK;
			dflags |= DAMAGE_DISMEMBER;
		}
		else if (ent->client && ent->NPC &&
			(ent->client->NPC_class == CLASS_WOOKIE || ent->client->NPC_class == CLASS_SBD))
		{
			//do a tad bit more damage IF WOOKIE CLASS // SERENITY
			damage = MELEE_SWING_WOOKIE_DAMAGE;
		}
		else if (ent->client && ent->client->ps.torsoAnim == BOTH_WOOKIE_SLAP)
		{
			//do a tad bit more damage IF WOOKIE CLASS // SERENITY
			damage = MELEE_SWING_WOOKIE_DAMAGE;
		}
		else if (ent->client && ent->client->ps.torsoAnim == BOTH_MELEEUP)
		{
			//do a tad bit more damage IF WOOKIE CLASS // SERENITY
			damage = MELEE_SWING_EXTRA_DAMAGE;
		}

		G_Damage(tr_ent, ent, ent, forward_vec, tr.endpos, damage, dflags, MOD_MELEE);
	}
}