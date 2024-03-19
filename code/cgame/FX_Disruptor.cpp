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

// Disruptor Weapon

#include "cg_headers.h"

#include "cg_media.h"
#include "FxScheduler.h"

/*
---------------------------
FX_DisruptorMainShot
---------------------------
*/
static vec3_t WHITE = { 1.0f, 1.0f, 1.0f };

void FX_DisruptorMainShot(vec3_t start, vec3_t end)
{
	FX_AddLine(-1, start, end, 0.1f, 4.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		WHITE, WHITE, 0.0f,
		300, cgi_R_RegisterShader("gfx/effects/redLine"),
		0, FX_SIZE_LINEAR | FX_ALPHA_LINEAR);
}

/*
---------------------------
FX_DisruptorAltShot
---------------------------
*/
void FX_DisruptorAltShot(vec3_t start, vec3_t end, const qboolean full_charge)
{
	FX_AddLine(-1, start, end, 0.1f, 10.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		WHITE, WHITE, 0.0f,
		300, cgi_R_RegisterShader("gfx/effects/redLine"),
		0, FX_SIZE_LINEAR | FX_ALPHA_LINEAR);

	if (full_charge)
	{
		vec3_t YELLER = { 0.8f, 0.7f, 0.0f };

		// add some beef
		FX_AddLine(-1, start, end, 0.1f, 7.0f, 0.0f,
			1.0f, 0.0f, 0.0f,
			YELLER, YELLER, 0.0f,
			300, cgi_R_RegisterShader("gfx/misc/whiteline2"),
			0, FX_SIZE_LINEAR | FX_ALPHA_LINEAR);
	}
}

/*
---------------------------
FX_DisruptorAltMiss
---------------------------
*/

void FX_DisruptorAltMiss(vec3_t origin, vec3_t normal)
{
	vec3_t pos, c1, c2;

	VectorMA(origin, 4.0f, normal, c1);
	VectorCopy(c1, c2);
	c1[2] += 4;
	c2[2] += 12;

	VectorAdd(origin, normal, pos);
	pos[2] += 28;

	FX_AddBezier(origin, pos, c1, vec3_origin, c2, vec3_origin, 6.0f, 6.0f, 0.0f, 0.0f, 0.2f, 0.5f, WHITE, WHITE, 0.0f,
		4000, cgi_R_RegisterShader("gfx/effects/smokeTrail"), FX_ALPHA_WAVE);

	theFxScheduler.PlayEffect("disruptor/alt_miss", origin, normal);
}

/*
---------------------------
FX_KothosBeam
---------------------------
*/
void FX_KothosBeam(vec3_t start, vec3_t end)
{
	FX_AddLine(-1, start, end, 0.1f, 10.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		WHITE, WHITE, 0.0f,
		175, cgi_R_RegisterShader("gfx/misc/dr1"),
		0, FX_SIZE_LINEAR | FX_ALPHA_LINEAR);

	vec3_t YELLER = { 0.8f, 0.7f, 0.0f };

	// add some beef
	FX_AddLine(-1, start, end, 0.1f, 7.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		YELLER, YELLER, 0.0f,
		150, cgi_R_RegisterShader("gfx/misc/whiteline2"),
		0, FX_SIZE_LINEAR | FX_ALPHA_LINEAR);
}

void FX_YellowLightningStrike(vec3_t start, vec3_t end)
{
	vec3_t YELLER = { 0.8f, 0.7f, 0.0f };

	// add some beef
	FX_AddLine(-1, start, end, 0.1f, 7.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		YELLER, YELLER, 0.0f,
		750, cgi_R_RegisterShader("gfx/effects/yellowline"),
		0, FX_SIZE_LINEAR | FX_ALPHA_LINEAR);
}

void FX_LightningStrike(vec3_t start, vec3_t end)
{
	G_PlayEffect("env/yellow_lightning", start, end);
}

/*
---------------------------
FX_Strike_Beam
---------------------------
*/
void FX_Strike_Beam(vec3_t start, vec3_t end, vec3_t normal, vec3_t targ1, vec3_t targ2)
{
	vec3_t dir, chaos,
		c1, c2,
		v1, v2;

	VectorSubtract(end, start, dir);
	float len = VectorNormalize(dir);

	// Get the base control points, we'll work from there
	VectorMA(start, 0.3333f * len, dir, c1);
	VectorMA(start, 0.6666f * len, dir, c2);

	// get some chaos values that really aren't very chaotic :)
	float s1 = sin(cg.time * 0.005f) * 2 + Q_flrand(-1.0f, 1.0f) * 0.2f;
	float s2 = sin(cg.time * 0.001f);
	float s3 = sin(cg.time * 0.011f);

	VectorSet(chaos, len * 0.01f * s1,
		len * 0.02f * s2,
		len * 0.04f * (s1 + s2 + s3));

	VectorAdd(c1, chaos, c1);
	VectorScale(chaos, 4.0f, v1);

	VectorSet(chaos, -len * 0.02f * s3,
		len * 0.01f * (s1 * s2),
		-len * 0.02f * (s1 + s2 * s3));

	VectorAdd(c2, chaos, c2);
	VectorScale(chaos, 2.0f, v2);

	VectorSet(chaos, 1.0f, 1.0f, 1.0f);

	FX_AddBezier(start, targ1,
		c1, v1, c2, v2,
		5.0f + s1 * 2, 8.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		chaos, chaos, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowline"), FX_ALPHA_LINEAR);

	FX_AddBezier(start, targ1,
		c2, v2, c1, v1,
		3.0f + s3, 3.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		chaos, chaos, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowline"), FX_ALPHA_LINEAR);

	s1 = sin(cg.time * 0.0005f) + Q_flrand(-1.0f, 1.0f) * 0.1f;
	s2 = sin(cg.time * 0.0025f);
	float cc2 = cos(cg.time * 0.0025f);
	s3 = sin(cg.time * 0.01f) + Q_flrand(-1.0f, 1.0f) * 0.1f;

	VectorSet(chaos, len * 0.08f * s2,
		len * 0.04f * cc2, //s1 * -s3,
		len * 0.06f * s3);

	VectorAdd(c1, chaos, c1);
	VectorScale(chaos, 4.0f, v1);

	VectorSet(chaos, len * 0.02f * s1 * s3,
		len * 0.04f * s2,
		len * 0.03f * s1 * s2);

	VectorAdd(c2, chaos, c2);
	VectorScale(chaos, 3.0f, v2);

	VectorSet(chaos, 1.0f, 1.0f, 1.0f);

	FX_AddBezier(start, targ1,
		c1, v1, c2, v2,
		4.0f + s3, 8.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		chaos, chaos, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowline"), FX_ALPHA_LINEAR);

	FX_AddBezier(start, targ1,
		c2, v1, c1, v2,
		5.0f + s1 * 2, 8.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		chaos, chaos, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowline"), FX_ALPHA_LINEAR);

	VectorMA(start, 14.0f, dir, c1);

	FX_AddSprite(c1, nullptr, nullptr, 12.0f + Q_flrand(-1.0f, 1.0f) * 4, 1.0f, 1.0f, Q_flrand(0.0f, 1.0f) * 360, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowflash"));
	FX_AddSprite(c1, nullptr, nullptr, 6.0f + Q_flrand(-1.0f, 1.0f) * 2, 1.0f, 1.0f, Q_flrand(0.0f, 1.0f) * 360, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowflash"));

	FX_AddSprite(targ1, nullptr, nullptr, 4.0f + Q_flrand(-1.0f, 1.0f), 1.0f, 0.0f, chaos, chaos, Q_flrand(0.0f, 1.0f) * 360,
		0.0f, 10, cgi_R_RegisterShader("gfx/effects/yellowflash"));
	FX_AddSprite(targ1, nullptr, nullptr, 8.0f + Q_flrand(-1.0f, 1.0f) * 2, 1.0f, 0.0f, chaos, chaos, Q_flrand(0.0f, 1.0f) * 360,
		0.0f, 10, cgi_R_RegisterShader("gfx/effects/yellowflash"));

	//--------------------------------------------

	VectorSubtract(targ2, targ1, dir);
	len = VectorNormalize(dir);

	// Get the base control points, we'll work from there
	VectorMA(targ1, 0.3333f * len, dir, c1);
	VectorMA(targ1, 0.6666f * len, dir, c2);

	// get some chaos values that really aren't very chaotic :)
	s1 = sin(cg.time * 0.005f) * 2 + Q_flrand(-1.0f, 1.0f) * 0.2f;
	s2 = sin(cg.time * 0.001f);
	s3 = sin(cg.time * 0.011f);

	VectorSet(chaos, len * 0.01f * s1,
		len * 0.02f * s2,
		len * 0.04f * (s1 + s2 + s3));

	VectorAdd(c1, chaos, c1);
	VectorScale(chaos, 4.0f, v1);

	VectorSet(chaos, -len * 0.02f * s3,
		len * 0.01f * (s1 * s2),
		-len * 0.02f * (s1 + s2 * s3));

	VectorAdd(c2, chaos, c2);
	VectorScale(chaos, 2.0f, v2);

	VectorSet(chaos, 1.0f, 1.0f, 1.0f);

	FX_AddBezier(targ1, targ2,
		c1, v1, c2, v2,
		5.0f + s1 * 2, 8.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		chaos, chaos, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowline"), FX_ALPHA_LINEAR);

	FX_AddBezier(targ1, targ2,
		c2, v2, c1, v1,
		3.0f + s3, 3.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		chaos, chaos, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowline"), FX_ALPHA_LINEAR);

	s1 = sin(cg.time * 0.0005f) + Q_flrand(-1.0f, 1.0f) * 0.1f;
	s2 = sin(cg.time * 0.0025f);
	cc2 = cos(cg.time * 0.0025f);
	s3 = sin(cg.time * 0.01f) + Q_flrand(-1.0f, 1.0f) * 0.1f;

	VectorSet(chaos, len * 0.08f * s2,
		len * 0.04f * cc2, //s1 * -s3,
		len * 0.06f * s3);

	VectorAdd(c1, chaos, c1);
	VectorScale(chaos, 4.0f, v1);

	VectorSet(chaos, len * 0.02f * s1 * s3,
		len * 0.04f * s2,
		len * 0.03f * s1 * s2);

	VectorAdd(c2, chaos, c2);
	VectorScale(chaos, 3.0f, v2);

	VectorSet(chaos, 1.0f, 1.0f, 1.0f);

	FX_AddBezier(targ1, targ2,
		c1, v1, c2, v2,
		4.0f + s3, 8.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		chaos, chaos, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowline"), FX_ALPHA_LINEAR);

	FX_AddBezier(targ1, targ2,
		c2, v1, c1, v2,
		5.0f + s1 * 2, 8.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		chaos, chaos, 0.0f,
		1.0f, cgi_R_RegisterShader("gfx/effects/yellowline"), FX_ALPHA_LINEAR);

	FX_AddSprite(targ2, nullptr, nullptr, 4.0f + Q_flrand(-1.0f, 1.0f), 1.0f, 0.0f, chaos, chaos, Q_flrand(0.0f, 1.0f) * 360,
		0.0f, 10, cgi_R_RegisterShader("gfx/effects/yellowflash"));
	FX_AddSprite(targ2, nullptr, nullptr, 8.0f + Q_flrand(-1.0f, 1.0f) * 2, 1.0f, 0.0f, chaos, chaos, Q_flrand(0.0f, 1.0f) * 360,
		0.0f, 10, cgi_R_RegisterShader("gfx/effects/yellowflash"));
}