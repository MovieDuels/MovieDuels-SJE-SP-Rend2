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

// Blaster Weapon

#include "cg_headers.h"

#include "cg_local.h"
#include "cg_media.h"
#include "FxScheduler.h"

/*
-------------------------
FX_BlasterProjectileThink
-------------------------
*/

void FX_BlasterProjectileThink(centity_t* cent, const weaponInfo_s* weapon)
{
	vec3_t forward;

	if (cent->currentState.eFlags & EF_USE_ANGLEDELTA)
	{
		AngleVectors(cent->currentState.angles, forward, nullptr, nullptr);
	}
	else
	{
		if (VectorNormalize2(cent->gent->s.pos.trDelta, forward) == 0.0f)
		{
			if (VectorNormalize2(cent->currentState.pos.trDelta, forward) == 0.0f)
			{
				forward[2] = 1.0f;
			}
		}
	}

	// hack the scale of the forward vector if we were just fired or bounced...this will shorten up the tail for a split second so tails don't clip so harshly
	int dif = cg.time - cent->gent->s.pos.trTime;

	if (dif < 75)
	{
		if (dif < 0)
		{
			dif = 0;
		}

		const float scale = dif / 75.0f * 0.95f + 0.05f;

		VectorScale(forward, scale, forward);
	}

	if (cent->gent && cent->gent->owner && cent->gent->owner->s.number > 0)
	{
		theFxScheduler.PlayEffect("blaster/NPCshot", cent->lerpOrigin, forward);
	}
	else
	{
		theFxScheduler.PlayEffect(cgs.effects.blasterShotEffect, cent->lerpOrigin, forward);
	}
}

/*
-------------------------
FX_BlasterAltFireThink
-------------------------
*/
void FX_BlasterAltFireThink(centity_t* cent, const weaponInfo_s* weapon)
{
	FX_BlasterProjectileThink(cent, weapon);
}

/*
-------------------------
FX_BlasterWeaponHitWall
-------------------------
*/
void FX_BlasterWeaponHitWall(vec3_t origin, vec3_t normal)
{
	theFxScheduler.PlayEffect(cgs.effects.blasterWallImpactEffect, origin, normal);
}

/*
-------------------------
FX_BlasterWeaponHitPlayer
-------------------------
*/
void FX_BlasterWeaponHitPlayer(gentity_t* hit, vec3_t origin, vec3_t normal, const qboolean humanoid)
{
	if (humanoid)
	{
		if (hit && hit->client && hit->ghoul2.size())
		{
			CG_AddGhoul2Mark(cgs.media.bdecal_bodyburn1, flrand(3.5, 4.0), origin, normal, hit->s.number,
				hit->client->ps.origin, hit->client->renderInfo.legsYaw, hit->ghoul2, hit->s.modelScale,
				Q_irand(10000, 13000));
		}
	}
	else
	{
		if (hit && hit->client && hit->ghoul2.size())
		{
			CG_AddGhoul2Mark(cgs.media.bdecal_burnmark1, flrand(3.5, 4.0), origin, normal, hit->s.number,
				hit->client->ps.origin, hit->client->renderInfo.legsYaw, hit->ghoul2, hit->s.modelScale,
				Q_irand(10000, 13000));
		}
	}

	theFxScheduler.PlayEffect(cgs.effects.blasterFleshImpactEffect, origin, normal);
}

//////////////// Droideka ////////////////

void FX_DroidekaProjectileThink(centity_t* cent, const weaponInfo_s* weapon)
{
	vec3_t forward;

	if (cent->currentState.eFlags & EF_USE_ANGLEDELTA)
	{
		AngleVectors(cent->currentState.angles, forward, nullptr, nullptr);
	}
	else
	{
		if (VectorNormalize2(cent->gent->s.pos.trDelta, forward) == 0.0f)
		{
			if (VectorNormalize2(cent->currentState.pos.trDelta, forward) == 0.0f)
			{
				forward[2] = 1.0f;
			}
		}
	}

	// hack the scale of the forward vector if we were just fired or bounced...this will shorten up the tail for a split second so tails don't clip so harshly
	int dif = cg.time - cent->gent->s.pos.trTime;

	if (dif < 75)
	{
		if (dif < 0)
		{
			dif = 0;
		}

		const float scale = dif / 75.0f * 0.95f + 0.05f;

		VectorScale(forward, scale, forward);
	}

	if (cent->gent && cent->gent->owner && cent->gent->owner->s.number > 0)
	{
		theFxScheduler.PlayEffect("droideka/shotnpc", cent->lerpOrigin, forward);
	}
	else
	{
		theFxScheduler.PlayEffect(cgs.effects.DroidekaShotEffect, cent->lerpOrigin, forward);
	}
}