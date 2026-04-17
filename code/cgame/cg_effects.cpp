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

// cg_effects.c -- these functions generate localentities

#include "cg_media.h"

#if !defined(FX_SCHEDULER_H_INC)
#include "FxScheduler.h"
#endif

/*
====================
CG_MakeExplosion
====================
*/
/*
localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir,
								qhandle_t hModel, int numFrames, qhandle_t shader,
								int msec, qboolean isSprite, float scale, int flags )
{
	float			ang = 0;
	localEntity_t	*ex;
	int				offset;
	vec3_t			tmpVec, newOrigin;

	if ( msec <= 0 ) {
		CG_Error( "CG_MakeExplosion: msec = %i", msec );
	}

	// skew the time a bit so they aren't all in sync
	offset = rand() & 63;

	ex = CG_AllocLocalEntity();
	if ( isSprite ) {
		ex->leType = LE_SPRITE_EXPLOSION;
		ex->refEntity.rotation = rand() % 360;
		ex->radius = scale;
		VectorScale( dir, 16, tmpVec );
		VectorAdd( tmpVec, origin, newOrigin );
	} else {
		ex->leType = LE_EXPLOSION;
		VectorCopy( origin, newOrigin );

		// set axis with random rotate when necessary
		if ( !dir )
		{
			AxisClear( ex->refEntity.axis );
		}
		else
		{
			if ( !(flags & LEF_NO_RANDOM_ROTATE) )
				ang = rand() % 360;
			VectorCopy( dir, ex->refEntity.axis[0] );
			RotateAroundDirection( ex->refEntity.axis, ang );
		}
	}

	ex->startTime = cg.time - offset;
	ex->endTime = ex->startTime + msec;

	// bias the time so all shader effects start correctly
	ex->refEntity.shaderTime = ex->startTime / 1000.0f;

	ex->refEntity.hModel = hModel;
	ex->refEntity.customShader = shader;
	ex->lifeRate = (float)numFrames / msec;
	ex->leFlags = flags;

	//Scale the explosion
	if (scale != 1) {
		ex->refEntity.nonNormalizedAxes = qtrue;

		VectorScale( ex->refEntity.axis[0], scale, ex->refEntity.axis[0] );
		VectorScale( ex->refEntity.axis[1], scale, ex->refEntity.axis[1] );
		VectorScale( ex->refEntity.axis[2], scale, ex->refEntity.axis[2] );
	}
	// set origin
	VectorCopy ( newOrigin, ex->refEntity.origin);
	VectorCopy ( newOrigin, ex->refEntity.oldorigin );

	ex->color[0] = ex->color[1] = ex->color[2] = 1.0;

	return ex;
}
*/
// When calling this version, just pass in a zero for the flags
//localEntity_t *CG_MakeExplosion( vec3_t origin, vec3_t dir,
//								qhandle_t hModel, int numFrames, qhandle_t shader,
//								int msec, qboolean isSprite, float scale ) {
//	return CG_MakeExplosion( origin, dir, hModel, numFrames, shader, msec, isSprite, scale, 0 );
//}

/*
====================
CG_AddTempLight
====================
*/
localEntity_t* CG_AddTempLight(vec3_t origin, const float scale, vec3_t color, const int msec)
{
	if (msec <= 0)
	{
		CG_Error("CG_AddTempLight: msec = %i", msec);
	}

	localEntity_t* ex = CG_AllocLocalEntity();

	ex->leType = LE_LIGHT;

	ex->startTime = cg.time;
	ex->endTime = ex->startTime + msec;

	// set origin
	VectorCopy(origin, ex->refEntity.origin);
	VectorCopy(origin, ex->refEntity.oldorigin);

	VectorCopy(color, ex->lightColor);
	ex->light = scale;

	return ex;
}

/*
-------------------------
CG_ExplosionEffects

Used to find the player and shake the camera if close enough
intensity ranges from 1 (minor tremble) to 16 (major quake)
-------------------------
*/

void CG_ExplosionEffects(vec3_t origin, const float intensity, const int radius, const int time)
{
	//FIXME: When exactly is the vieworg calculated in relation to the rest of the frame?s

	vec3_t dir;

	VectorSubtract(cg.refdef.vieworg, origin, dir);
	const float dist = VectorNormalize(dir);

	//Use the dir to add kick to the explosion

	if (dist > radius)
		return;

	const float intensity_scale = 1 - dist / static_cast<float>(radius);
	const float real_intensity = intensity * intensity_scale;

	CGCam_Shake(real_intensity, time);
}

/*
-------------------------
CG_MiscModelExplosion

Adds an explosion to a misc model breakables
-------------------------
*/

void CG_MiscModelExplosion(vec3_t mins, vec3_t maxs, const int size, const material_t chunk_type)
{
	int ct = 13;
	vec3_t org{}, mid;
	const char* effect = nullptr, * effect2 = nullptr;

	VectorAdd(mins, maxs, mid);
	VectorScale(mid, 0.5f, mid);

	switch (chunk_type)
	{
	case MAT_GLASS:
		effect = "chunks/glassbreak";
		ct = 5;
		break;
	case MAT_GLASS_METAL:
		effect = "chunks/glassbreak";
		effect2 = "chunks/metalexplode";
		ct = 5;
		break;
	case MAT_ELECTRICAL:
	case MAT_ELEC_METAL:
		effect = "chunks/sparkexplode";
		ct = 5;
		break;
	case MAT_METAL:
	case MAT_METAL2:
	case MAT_METAL3:
	case MAT_CRATE1:
	case MAT_CRATE2:
		effect = "chunks/metalexplode";
		ct = 2;
		break;
	case MAT_GRATE1:
		effect = "chunks/grateexplode";
		ct = 8;
		break;
	case MAT_ROPE:
		ct = 20;
		effect = "chunks/ropebreak";
		break;
	case MAT_WHITE_METAL: //not sure what this crap is really supposed to be..
	case MAT_DRK_STONE:
	case MAT_LT_STONE:
	case MAT_GREY_STONE:
		switch (size)
		{
		case 2:
			effect = "chunks/rockbreaklg";
			break;
		case 1:
		default:
			effect = "chunks/rockbreakmed";
			break;
		}
	default:
		break;
	}

	if (!effect)
	{
		return;
	}

	ct += 7 * size;

	// FIXME: real precache .. VERify that these need to be here...don't think they would because the effects should be registered in g_breakable
	theFxScheduler.RegisterEffect(effect);

	if (effect2)
	{
		// FIXME: real precache
		theFxScheduler.RegisterEffect(effect2);
	}

	// spawn chunk roughly in the bbox of the thing..
	for (int i = 0; i < ct; i++)
	{
		vec3_t dir;
		for (int j = 0; j < 3; j++)
		{
			const float r = Q_flrand(0.0f, 1.0f) * 0.8f + 0.1f;
			org[j] = r * mins[j] + (1 - r) * maxs[j];
		}

		// shoot effect away from center
		VectorSubtract(org, mid, dir);
		VectorNormalize(dir);

		if (effect2 && rand() & 1)
		{
			theFxScheduler.PlayEffect(effect2, org, dir);
		}
		else
		{
			theFxScheduler.PlayEffect(effect, org, dir);
		}
	}
}

/*
-------------------------
CG_Chunks

Fun chunk spewer
-------------------------
*/

void CG_Chunks(const int owner, vec3_t origin, const vec3_t mins, const vec3_t maxs,
	const float speed, const int num_chunks, const material_t chunk_type, const int custom_chunk,
	float base_scale, const int custom_sound = 0)
{
	int chunk_model = 0;
	leBounceSound_t bounce = LEBS_NONE;
	float speed_mod = 1.0f;
	qboolean chunk = qfalse;

	if (chunk_type == MAT_NONE)
	{
		// Well, we should do nothing
		return;
	}

	if (custom_sound)
	{
		if (cgs.sound_precache[custom_sound])
		{
			cgi_S_StartSound(nullptr, owner, CHAN_BODY, cgs.sound_precache[custom_sound]);
		}
	}
	// Set up our chunk sound info...breaking sounds are done here so they are done once on breaking..some return instantly because the chunks are done with effects instead of models
	switch (chunk_type)
	{
	case MAT_GLASS:
		if (!custom_sound)
		{
			cgi_S_StartSound(nullptr, owner, CHAN_BODY, cgs.media.glassChunkSound);
		}
		return;
	case MAT_GRATE1:
		if (!custom_sound)
		{
			cgi_S_StartSound(nullptr, owner, CHAN_BODY, cgs.media.grateSound);
		}
		return;
	case MAT_ELECTRICAL: // (sparks)
		if (!custom_sound)
		{
			cgi_S_StartSound(nullptr, owner, CHAN_BODY,
				cgi_S_RegisterSound(va("sound/ambience/spark%d.wav", Q_irand(1, 6))));
		}
		return;
	case MAT_DRK_STONE:
	case MAT_LT_STONE:
	case MAT_GREY_STONE:
	case MAT_WHITE_METAL: // not quite sure what this stuff is supposed to be...it's for Stu
		if (!custom_sound)
		{
			cgi_S_StartSound(nullptr, owner, CHAN_BODY, cgs.media.rockBreakSound);
			bounce = LEBS_ROCK;
		}
		speed_mod = 0.5f; // rock blows up less
		break;
	case MAT_GLASS_METAL:
		if (!custom_sound)
		{
			cgi_S_StartSound(nullptr, owner, CHAN_BODY, cgs.media.glassChunkSound);
			// FIXME: should probably have a custom sound
			bounce = LEBS_METAL;
		}
		break;
	case MAT_CRATE1:
	case MAT_CRATE2:
		if (!custom_sound)
		{
			cgi_S_StartSound(nullptr, owner, CHAN_BODY, cgs.media.crateBreakSound[Q_irand(0, 1)]);
		}
		break;
	case MAT_METAL:
	case MAT_METAL2:
	case MAT_METAL3:
	case MAT_ELEC_METAL: // FIXME: maybe have its own sound?
		if (!custom_sound)
		{
			cgi_S_StartSound(nullptr, owner, CHAN_BODY, cgs.media.chunkSound);
			bounce = LEBS_METAL;
		}
		speed_mod = 0.8f; // metal blows up a bit more
		break;
	case MAT_ROPE:
		/*
		if ( !customSound )
		{
			cgi_S_StartSound( NULL, owner, CHAN_BODY, cgi_S_RegisterSound( "" ));  FIXME:  needs a sound
		}
		*/
		return;
	default:
		break;
	}

	if (base_scale <= 0.0f)
	{
		base_scale = 1.0f;
	}

	// Chunks
	for (int i = 0; i < num_chunks; i++)
	{
		if (custom_chunk > 0)
		{
			// Try to use a custom chunk.
			if (cgs.model_draw[custom_chunk])
			{
				chunk = qtrue;
				chunk_model = cgs.model_draw[custom_chunk];
			}
		}

		if (!chunk)
		{
			// No custom chunk.  Pick a random chunk type at run-time so we don't get the same chunks
			switch (chunk_type)
			{
			case MAT_METAL2: //bluegrey
				chunk_model = cgs.media.chunkModels[CHUNK_METAL2][Q_irand(0, 3)];
				break;
			case MAT_GREY_STONE: //gray
				chunk_model = cgs.media.chunkModels[CHUNK_ROCK1][Q_irand(0, 3)];
				break;
			case MAT_LT_STONE: //tan
				chunk_model = cgs.media.chunkModels[CHUNK_ROCK2][Q_irand(0, 3)];
				break;
			case MAT_DRK_STONE: //brown
				chunk_model = cgs.media.chunkModels[CHUNK_ROCK3][Q_irand(0, 3)];
				break;
			case MAT_WHITE_METAL:
				chunk_model = cgs.media.chunkModels[CHUNK_WHITE_METAL][Q_irand(0, 3)];
				break;
			case MAT_CRATE1: //yellow multi-colored crate chunks
				chunk_model = cgs.media.chunkModels[CHUNK_CRATE1][Q_irand(0, 3)];
				break;
			case MAT_CRATE2: //red multi-colored crate chunks
				chunk_model = cgs.media.chunkModels[CHUNK_CRATE2][Q_irand(0, 3)];
				break;
			case MAT_ELEC_METAL:
			case MAT_GLASS_METAL:
			case MAT_METAL: //grey
				chunk_model = cgs.media.chunkModels[CHUNK_METAL1][Q_irand(0, 3)];
				break;
			case MAT_METAL3:
				if (rand() & 1)
				{
					chunk_model = cgs.media.chunkModels[CHUNK_METAL1][Q_irand(0, 3)];
				}
				else
				{
					chunk_model = cgs.media.chunkModels[CHUNK_METAL2][Q_irand(0, 3)];
				}
				break;
			default:
				break;
			}
		}

		// It wouldn't look good to throw a bunch of RGB axis models...so make sure we have something to work with.
		if (chunk_model)
		{
			vec3_t dir;
			localEntity_t* le = CG_AllocLocalEntity();
			refEntity_t* re = &le->refEntity;

			re->hModel = chunk_model;
			le->leType = LE_FRAGMENT;
			le->endTime = cg.time + 1300 + Q_flrand(0.0f, 1.0f) * 900;

			// spawn chunk roughly in the bbox of the thing...bias towards center in case thing blowing up doesn't complete fill its bbox.
			for (int j = 0; j < 3; j++)
			{
				const float r = Q_flrand(0.0f, 1.0f) * 0.8f + 0.1f;
				re->origin[j] = r * mins[j] + (1 - r) * maxs[j];
			}
			VectorCopy(re->origin, le->pos.trBase);

			// Move out from center of thing, otherwise you can end up things moving across the brush in an undesirable direction.  Visually looks wrong
			VectorSubtract(re->origin, origin, dir);
			VectorNormalize(dir);
			VectorScale(dir, Q_flrand(speed * 0.5f, speed * 1.25f) * speed_mod, le->pos.trDelta);

			// Angular Velocity
			VectorSet(le->angles.trBase, Q_flrand(0.0f, 1.0f) * 360, Q_flrand(0.0f, 1.0f) * 360,
				Q_flrand(0.0f, 1.0f) * 360);

			le->angles.trDelta[0] = Q_flrand(-1.0f, 1.0f);
			le->angles.trDelta[1] = Q_flrand(-1.0f, 1.0f);
			le->angles.trDelta[2] = 0; // don't do roll

			VectorScale(le->angles.trDelta, Q_flrand(0.0f, 1.0f) * 600.0f + 200.0f, le->angles.trDelta);

			le->pos.trType = TR_GRAVITY;
			le->angles.trType = TR_LINEAR;
			le->pos.trTime = le->angles.trTime = cg.time;
			le->bounceFactor = 0.2f + Q_flrand(0.0f, 1.0f) * 0.2f;
			le->leFlags |= LEF_TUMBLE;
			le->ownerGentNum = owner;
			le->leBounceSoundType = bounce;

			// Make sure that we have the desired start size set
			le->radius = Q_flrand(base_scale * 0.75f, base_scale * 1.25f);
			re->nonNormalizedAxes = qtrue;
			AxisCopy(axisDefault, re->axis); // could do an angles to axis, but this is cheaper and works ok
			for (auto& axi : re->axis)
			{
				VectorScale(axi, le->radius, axi);
			}
		}
	}
}

void CG_TestLine(vec3_t start, vec3_t end, const int time, unsigned int color, const int radius)
{
	localEntity_t* le = CG_AllocLocalEntity();
	le->leType = LE_LINE;
	le->startTime = cg.time;
	le->endTime = cg.time + time;
	le->lifeRate = 1.0 / (le->endTime - le->startTime);

	refEntity_t* re = &le->refEntity;
	VectorCopy(start, re->origin);
	VectorCopy(end, re->oldorigin);
	re->shaderTime = cg.time / 1000.0f;

	re->reType = RT_LINE;
	re->radius = 0.5 * radius;
	re->customShader = cgs.media.whiteShader;

	re->shaderTexCoord[0] = re->shaderTexCoord[1] = 1.0f;

	if (color == 0)
	{
		re->shaderRGBA[0] = re->shaderRGBA[1] = re->shaderRGBA[2] = re->shaderRGBA[3] = 0xff;
	}
	else
	{
		re->shaderRGBA[0] = color & 0xff;
		color >>= 8;
		re->shaderRGBA[1] = color & 0xff;
		color >>= 8;
		re->shaderRGBA[2] = color & 0xff;
		//		color >>= 8;
		//		re->shaderRGBA[3] = color & 0xff;
		re->shaderRGBA[3] = 0xff;
	}

	le->color[3] = 1.0;
}

void CG_BlockLine(vec3_t start, vec3_t end, const int time, unsigned int color, const int radius)
{
	localEntity_t* le = CG_AllocLocalEntity();
	le->leType = LE_LINE;
	le->startTime = cg.time;
	le->endTime = cg.time + time;
	le->lifeRate = 1.0 / (le->endTime - le->startTime);

	refEntity_t* re = &le->refEntity;
	VectorCopy(start, re->origin);
	VectorCopy(end, re->oldorigin);
	re->shaderTime = cg.time / 1000.0f;

	re->reType = RT_LINE;
	re->radius = 0.1 * radius;
	re->customShader = cgs.media.whiteShader;

	re->shaderTexCoord[0] = re->shaderTexCoord[1] = 1.0f;

	if (color == 0)
	{
		re->shaderRGBA[0] = re->shaderRGBA[1] = re->shaderRGBA[2] = re->shaderRGBA[3] = 0xff;
	}
	else
	{
		re->shaderRGBA[0] = color & 0xff;
		color >>= 8;
		re->shaderRGBA[1] = color & 0xff;
		color >>= 8;
		re->shaderRGBA[2] = color & 0xff;
		re->shaderRGBA[3] = 0xff;
	}

	le->color[3] = 1.0;
}

void CG_StunStartpoint(vec3_t start_pos)
{
	refEntity_t model = {};

	model.reType = RT_SABER_GLOW;
	VectorCopy(start_pos, model.lightingOrigin);
	VectorCopy(start_pos, model.origin);

	model.customShader = cgs.media.blueSaberGlowShader;
	model.shaderRGBA[0] = model.shaderRGBA[1] = model.shaderRGBA[2] = model.shaderRGBA[3] = 0xff;

	cgi_R_AddRefEntityToScene(&model);
}

void CG_GrappleStartpoint(vec3_t start_pos)
{
	refEntity_t model = {};

	model.reType = RT_SABER_GLOW;
	VectorCopy(start_pos, model.lightingOrigin);
	VectorCopy(start_pos, model.origin);

	model.customShader = cgs.media.rgbSaberGlowShader;
	model.shaderRGBA[0] = model.shaderRGBA[1] = model.shaderRGBA[2] = model.shaderRGBA[3] = 0xff;

	cgi_R_AddRefEntityToScene(&model);
}

void CG_GrappleLine(vec3_t start, vec3_t end, const int time, unsigned int color, const int radius)
{
	localEntity_t* le = CG_AllocLocalEntity();
	le->leType = LE_LINE;
	le->startTime = cg.time;
	le->endTime = cg.time + time;
	le->lifeRate = 1.0 / (le->endTime - le->startTime);

	refEntity_t* re = &le->refEntity;
	VectorCopy(start, re->origin);
	VectorCopy(end, re->oldorigin);
	re->shaderTime = cg.time / 1000.0f;

	re->reType = RT_LINE;
	re->radius = 0.5 * radius;
	re->customShader = cgs.media.electricBodyShader;

	re->shaderTexCoord[0] = re->shaderTexCoord[1] = 1.0f;

	if (color == 0)
	{
		re->shaderRGBA[0] = re->shaderRGBA[1] = re->shaderRGBA[2] = re->shaderRGBA[3] = 0xff;
	}
	else
	{
		re->shaderRGBA[0] = color & 0xff;
		color >>= 8;
		re->shaderRGBA[1] = color & 0xff;
		color >>= 8;
		re->shaderRGBA[2] = color & 0xff;
		re->shaderRGBA[3] = 0xff;
	}

	le->color[3] = 1.0;
}

//----------------------------
//
// Breaking Glass Technology
//
//----------------------------

// Since we have shared verts when we tesselate the glass sheet, it helps to have a
//	random offset table set up up front...so that we can have more random looking breaks.

static float offX[20][20],
offZ[20][20];

static void CG_DoGlassQuad(vec3_t p[4], vec2_t uv[4], const bool stick, const int time, vec3_t dmg_dir)
{
	vec3_t rot_delta;
	vec3_t vel, accel;
	vec3_t rgb1;

	VectorSet(vel, Q_flrand(-1.0f, 1.0f) * 12, Q_flrand(-1.0f, 1.0f) * 12, -1);

	if (!stick)
	{
		// We aren't a motion delayed chunk, so let us move quickly
		VectorMA(vel, 0.3f, dmg_dir, vel);
	}

	// Set up acceleration due to gravity, 800 is standard QuakeIII gravity, so let's use something close
	VectorSet(accel, 0.0f, 0.0f, -(600.0f + Q_flrand(0.0f, 1.0f) * 100.0f));

	VectorSet(rgb1, 1.0f, 1.0f, 1.0f);

	// Being glass, we don't want to bounce much
	const float bounce = Q_flrand(0.0f, 1.0f) * 0.2f + 0.15f;

	// Set up our random rotate, we only do PITCH and YAW, not ROLL.  This is something like degrees per second
	VectorSet(rot_delta, Q_flrand(-1.0f, 1.0f) * 40.0f, Q_flrand(-1.0f, 1.0f) * 40.0f, 0.0f);

	CPoly* pol = FX_AddPoly(p, uv, 4, // verts, ST, vertCount
		vel, accel, // motion
		0.15f, 0.0f, 85.0f,
		// alpha start, alpha end, alpha parm ( begin alpha fade when 85% of life is complete )
		rgb1, rgb1, 0.0f, // rgb start, rgb end, rgb parm ( not used )
		rot_delta, bounce, time,
		// rotation amount, bounce, and time to delay motion for ( zero if no delay );
		3500 + Q_flrand(0.0f, 1.0f) * 1000, // life
		cgi_R_RegisterShader("gfx/misc/test_crackle"),
		FX_APPLY_PHYSICS | FX_ALPHA_NONLINEAR | FX_USE_ALPHA);

	if (Q_flrand(0.0f, 1.0f) > 0.95f && pol)
	{
		pol->AddFlags(FX_IMPACT_RUNS_FX | FX_KILL_ON_IMPACT);
		pol->SetImpactFxID(theFxScheduler.RegisterEffect("misc/glass_impact"));
	}
}

static void CG_CalcBiLerp(vec3_t verts[4], vec3_t sub_verts[4], vec2_t uv[4])
{
	vec3_t temp;

	// Nasty crap
	VectorScale(verts[0], 1.0f - uv[0][0], sub_verts[0]);
	VectorMA(sub_verts[0], uv[0][0], verts[1], sub_verts[0]);
	VectorScale(sub_verts[0], 1.0f - uv[0][1], temp);
	VectorScale(verts[3], 1.0f - uv[0][0], sub_verts[0]);
	VectorMA(sub_verts[0], uv[0][0], verts[2], sub_verts[0]);
	VectorMA(temp, uv[0][1], sub_verts[0], sub_verts[0]);

	VectorScale(verts[0], 1.0f - uv[1][0], sub_verts[1]);
	VectorMA(sub_verts[1], uv[1][0], verts[1], sub_verts[1]);
	VectorScale(sub_verts[1], 1.0f - uv[1][1], temp);
	VectorScale(verts[3], 1.0f - uv[1][0], sub_verts[1]);
	VectorMA(sub_verts[1], uv[1][0], verts[2], sub_verts[1]);
	VectorMA(temp, uv[1][1], sub_verts[1], sub_verts[1]);

	VectorScale(verts[0], 1.0f - uv[2][0], sub_verts[2]);
	VectorMA(sub_verts[2], uv[2][0], verts[1], sub_verts[2]);
	VectorScale(sub_verts[2], 1.0f - uv[2][1], temp);
	VectorScale(verts[3], 1.0f - uv[2][0], sub_verts[2]);
	VectorMA(sub_verts[2], uv[2][0], verts[2], sub_verts[2]);
	VectorMA(temp, uv[2][1], sub_verts[2], sub_verts[2]);

	VectorScale(verts[0], 1.0f - uv[3][0], sub_verts[3]);
	VectorMA(sub_verts[3], uv[3][0], verts[1], sub_verts[3]);
	VectorScale(sub_verts[3], 1.0f - uv[3][1], temp);
	VectorScale(verts[3], 1.0f - uv[3][0], sub_verts[3]);
	VectorMA(sub_verts[3], uv[3][0], verts[2], sub_verts[3]);
	VectorMA(temp, uv[3][1], sub_verts[3], sub_verts[3]);
}

// bilinear
//f(p',q') = (1 - y) × {[(1 - x) × f(p,q)] + [x × f(p,q+1)]} + y × {[(1 - x) × f(p+1,q)] + [x × f(p+1,q+1)]}.

static void CG_CalcHeightWidth(vec3_t verts[4], float* height, float* width)
{
	vec3_t dir1, dir2, cross;

	VectorSubtract(verts[3], verts[0], dir1); // v
	VectorSubtract(verts[1], verts[0], dir2); // p-a
	CrossProduct(dir1, dir2, cross);
	*width = VectorNormalize(cross) / VectorNormalize(dir1); // v
	VectorSubtract(verts[2], verts[0], dir2); // p-a
	CrossProduct(dir1, dir2, cross);
	*width += VectorNormalize(cross) / VectorNormalize(dir1); // v
	*width *= 0.5f;

	VectorSubtract(verts[1], verts[0], dir1); // v
	VectorSubtract(verts[2], verts[0], dir2); // p-a
	CrossProduct(dir1, dir2, cross);
	*height = VectorNormalize(cross) / VectorNormalize(dir1); // v
	VectorSubtract(verts[3], verts[0], dir2); // p-a
	CrossProduct(dir1, dir2, cross);
	*height += VectorNormalize(cross) / VectorNormalize(dir1); // v
	*height *= 0.5f;
}

//Consider a line in 3D with position vector "a" and direction vector "v" and
// let "p" be the position vector of an arbitrary point in 3D
//dist = len( crossprod(p-a,v) ) / len(v);

void CG_InitGlass()
{
	// Build a table first, so that we can do a more unpredictable crack scheme
	//	do it once, up front to save a bit of time.
	for (int i = 0; i < 20; i++)
	{
		for (int t = 0; t < 20; t++)
		{
			offX[t][i] = Q_flrand(-1.0f, 1.0f) * 0.03f;
			offZ[i][t] = Q_flrand(-1.0f, 1.0f) * 0.03f;
		}
	}
}

constexpr auto TIME_DECAY_SLOW = 0.1f;
constexpr auto TIME_DECAY_MED = 0.04f;
constexpr auto TIME_DECAY_FAST = 0.009f;

void CG_DoGlass(vec3_t verts[], vec3_t dmg_pt, vec3_t dmg_dir, const float dmg_radius)
{
	int i, t;
	int mx_height, mx_width;
	float height, width;
	float step_width, step_height;
	float time_decay;
	float x, z;
	float xx, zz;
	int time;
	bool stick;

	// To do a smarter tesselation, we should figure out the relative height and width of the brush face,
	//	then use this to pick a lod value from 1-3 in each axis.  This will give us 1-9 lod levels, which will
	//	hopefully be sufficient.
	CG_CalcHeightWidth(verts, &height, &width);

	cgi_S_StartSound(dmg_pt, -1, CHAN_AUTO, cgi_S_RegisterSound("sound/effects/glassbreak1.wav"));

	// Pick "LOD" for height
	if (height < 100)
	{
		step_height = 0.2f;
		mx_height = 5;
		time_decay = TIME_DECAY_SLOW;
	}
	else
	{
		step_height = 0.1f;
		mx_height = 10;
		time_decay = TIME_DECAY_MED;
	}

	// Pick "LOD" for width
	if (width < 100)
	{
		step_width = 0.2f;
		mx_width = 5;
		time_decay = (time_decay + TIME_DECAY_SLOW) * 0.5f;
	}
	else
	{
		step_width = 0.1f;
		mx_width = 10;
		time_decay = (time_decay + TIME_DECAY_MED) * 0.5f;
	}

	for (z = 0.0f, i = 0; z < 1.0f; z += step_height, i++)
	{
		for (x = 0.0f, t = 0; x < 1.0f; x += step_width, t++)
		{
			vec2_t bi_points[4]{};
			vec3_t sub_verts[4];
			// This is nasty..we do this because we don't want to add a random offset on the edge of the glass brush
			//	...but we do in the center, otherwise the breaking scheme looks way too orderly
			if (t > 0 && t < mx_width)
			{
				xx = x - offX[i][t];
			}
			else
			{
				xx = x;
			}

			if (i > 0 && i < mx_height)
			{
				zz = z - offZ[t][i];
			}
			else
			{
				zz = z;
			}

			VectorSet2(bi_points[0], xx, zz);

			if (t + 1 > 0 && t + 1 < mx_width)
			{
				xx = x - offX[i][t + 1];
			}
			else
			{
				xx = x;
			}

			if (i > 0 && i < mx_height)
			{
				zz = z - offZ[t + 1][i];
			}
			else
			{
				zz = z;
			}

			VectorSet2(bi_points[1], xx + step_width, zz);

			if (t + 1 > 0 && t + 1 < mx_width)
			{
				xx = x - offX[i + 1][t + 1];
			}
			else
			{
				xx = x;
			}

			if (i + 1 > 0 && i + 1 < mx_height)
			{
				zz = z - offZ[t + 1][i + 1];
			}
			else
			{
				zz = z;
			}

			VectorSet2(bi_points[2], xx + step_width, zz + step_height);

			if (t > 0 && t < mx_width)
			{
				xx = x - offX[i + 1][t];
			}
			else
			{
				xx = x;
			}

			if (i + 1 > 0 && i + 1 < mx_height)
			{
				zz = z - offZ[t][i + 1];
			}
			else
			{
				zz = z;
			}

			VectorSet2(bi_points[3], xx, zz + step_height);

			CG_CalcBiLerp(verts, sub_verts, bi_points);

			float dif = DistanceSquared(sub_verts[0], dmg_pt) * time_decay - Q_flrand(0.0f, 1.0f) * 32;

			// If we decrease dif, we are increasing the impact area, making it more likely to blow out large holes
			dif -= dmg_radius * dmg_radius;

			if (dif > 1)
			{
				stick = true;
				time = dif + Q_flrand(0.0f, 1.0f) * 200;
			}
			else
			{
				stick = false;
				time = 0;
			}

			CG_DoGlassQuad(sub_verts, bi_points, stick, time, dmg_dir);
		}
	}
}

//------------------------------------------------------------------------------------------
void CG_DrawTargetBeam(vec3_t start, vec3_t end, vec3_t norm, const char* beam_fx, const char* impact_fx)
{
	int handle = 0;
	vec3_t dir;

	// overriding the effect, so give us a copy first
	const SEffectTemplate* temp = theFxScheduler.GetEffectCopy(beam_fx, &handle);

	VectorSubtract(start, end, dir);
	VectorNormalize(dir);

	if (temp)
	{
		// have a copy, so get the line element out of there
		CPrimitiveTemplate* prim = theFxScheduler.GetPrimitiveCopy(temp, "beam");

		if (prim)
		{
			// we have the primitive, so modify the endpoint
			prim->mOrigin2X.SetRange(end[0], end[0]);
			prim->mOrigin2Y.SetRange(end[1], end[1]);
			prim->mOrigin2Z.SetRange(end[2], end[2]);

			// have a copy, so get the line element out of there
			CPrimitiveTemplate* primitive_template = theFxScheduler.GetPrimitiveCopy(temp, "glow");

			// glow is not required
			if (primitive_template)
			{
				// we have the primitive, so modify the endpoint
				primitive_template->mOrigin2X.SetRange(end[0], end[0]);
				primitive_template->mOrigin2Y.SetRange(end[1], end[1]);
				primitive_template->mOrigin2Z.SetRange(end[2], end[2]);
			}

			// play the modified effect
			theFxScheduler.PlayEffect(handle, start, dir);
		}
	}

	if (impact_fx)
	{
		theFxScheduler.PlayEffect(impact_fx, end, norm);
	}
}

void CG_PlayEffectBolted(const char* fx_name, const int modelIndex, const int bolt_index, const int entNum,
	vec3_t origin,
	const int i_loop_time, const bool is_relative)
{
	vec3_t axis[3];
	//FIXME: shouldn't this be initialized to something?  It isn't in the EV_PLAY_EFFECT call... irrelevant?
	int boltInfo;

	//pack the data into boltInfo as if we were going to send it over the network
	gi.G2API_AttachEnt(&boltInfo,
		&g_entities[entNum].ghoul2[modelIndex],
		bolt_index,
		entNum,
		modelIndex);
	//send direcly to FX scheduler
	theFxScheduler.PlayEffect(fx_name,
		origin,
		axis,
		boltInfo,
		-1,
		false,
		i_loop_time,
		is_relative); //i_loop_time 0 = not looping, 1 for infinite, else duration
}

void CG_PlayEffectIDBolted(const int fx_id, const int modelIndex, const int bolt_index, const int entNum,
	vec3_t origin,
	const int i_loop_time, const bool is_relative)
{
	const char* fx_name = CG_ConfigString(CS_EFFECTS + fx_id);
	CG_PlayEffectBolted(fx_name, modelIndex, bolt_index, entNum, origin, i_loop_time, is_relative);
}

void CG_PlayEffectOnEnt(const char* fx_name, const int clientNum, vec3_t origin, const vec3_t fwd)
{
	vec3_t temp, axis[3]{};

	// Assume angles, we'll do a cross product to finish up
	VectorCopy(fwd, axis[0]);
	MakeNormalVectors(fwd, axis[1], temp);
	CrossProduct(axis[0], axis[1], axis[2]);
	//call FX scheduler directly
	theFxScheduler.PlayEffect(fx_name, origin, axis, -1, clientNum, false);
}

void CG_PlayEffectIDOnEnt(const int fx_id, const int clientNum, vec3_t origin, const vec3_t fwd)
{
	const char* fx_name = CG_ConfigString(CS_EFFECTS + fx_id);
	CG_PlayEffectOnEnt(fx_name, clientNum, origin, fwd);
}

void CG_PlayEffect(const char* fx_name, vec3_t origin, const vec3_t fwd)
{
	vec3_t temp, axis[3]{};

	// Assume angles, we'll do a cross product to finish up
	VectorCopy(fwd, axis[0]);
	MakeNormalVectors(fwd, axis[1], temp);
	CrossProduct(axis[0], axis[1], axis[2]);
	//call FX scheduler directly
	theFxScheduler.PlayEffect(fx_name, origin, axis, -1, -1, false);
}

void CG_PlayEffectID(const int fx_id, vec3_t origin, const vec3_t fwd)
{
	const char* fx_name = CG_ConfigString(CS_EFFECTS + fx_id);
	CG_PlayEffect(fx_name, origin, fwd);
}

inline float random()
{
	return rand() / static_cast<float>(0x7fff);
}

inline float crandom()
{
	return 2.0F * (random() - 0.5F);
}

/*
===================
CG_GibPlayer

Generated a bunch of gibs launching out from the bodies location
===================
*/
constexpr auto GIB_VELOCITY = 250;
constexpr auto GIB_JUMP = 250;

void CG_GibPlayer(vec3_t player_origin)
{
	vec3_t origin, velocity{};

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * GIB_VELOCITY;
	velocity[1] = crandom() * GIB_VELOCITY;
	velocity[2] = GIB_JUMP + crandom() * GIB_VELOCITY;
}

constexpr auto DECAPITATE_VELOCITY = 3000;
constexpr auto HEAD_JUMP = 3000;

void CG_GibPlayerHeadshot(vec3_t player_origin)
{
	vec3_t origin, velocity{};

	VectorCopy(player_origin, origin);
	origin[2] += 25;
	velocity[0] = crandom() * DECAPITATE_VELOCITY;
	velocity[1] = crandom() * DECAPITATE_VELOCITY;
	velocity[2] = HEAD_JUMP + crandom() * DECAPITATE_VELOCITY;
	//delete from here
	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * DECAPITATE_VELOCITY;
	velocity[1] = crandom() * DECAPITATE_VELOCITY;
	velocity[2] = HEAD_JUMP + crandom() * DECAPITATE_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * DECAPITATE_VELOCITY;
	velocity[1] = crandom() * DECAPITATE_VELOCITY;
	velocity[2] = HEAD_JUMP + crandom() * DECAPITATE_VELOCITY;

	VectorCopy(player_origin, origin);
	velocity[0] = crandom() * DECAPITATE_VELOCITY;
	velocity[1] = crandom() * DECAPITATE_VELOCITY;
	velocity[2] = HEAD_JUMP + crandom() * DECAPITATE_VELOCITY;
}