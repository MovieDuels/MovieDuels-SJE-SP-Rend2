/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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

// tr_mesh.c: triangle model functions

#include "../server/exe_headers.h"

#include "tr_local.h"

float ProjectRadius(const float r, vec3_t location)
{
	vec3_t	p;

	const float c = DotProduct(tr.viewParms.ori.axis[0], tr.viewParms.ori.origin);
	const float dist = DotProduct(tr.viewParms.ori.axis[0], location) - c;

	if (dist <= 0)
		return 0;

	p[0] = 0;
	p[1] = Q_fabs(r);
	p[2] = -dist;

	const float width = p[0] * tr.viewParms.projectionMatrix[1] +
		p[1] * tr.viewParms.projectionMatrix[5] +
		p[2] * tr.viewParms.projectionMatrix[9] +
		tr.viewParms.projectionMatrix[13];

	const float depth = p[0] * tr.viewParms.projectionMatrix[3] +
		p[1] * tr.viewParms.projectionMatrix[7] +
		p[2] * tr.viewParms.projectionMatrix[11] +
		tr.viewParms.projectionMatrix[15];

	float pr = width / depth;

	if (pr > 1.0f)
		pr = 1.0f;

	return pr;
}

/*
=============
R_CullModel
=============
*/
static int R_CullModel(md3Header_t* header, const trRefEntity_t* ent) {
	vec3_t		bounds[2]{};

	// compute frame pointers
	const md3Frame_t* newFrame = reinterpret_cast<md3Frame_t*>(reinterpret_cast<byte*>(header) + header->ofsFrames) + ent->e.frame;
	const md3Frame_t* old_frame = reinterpret_cast<md3Frame_t*>(reinterpret_cast<byte*>(header) + header->ofsFrames) + ent->e.oldframe;

	// cull bounding sphere ONLY if this is not an upscaled entity
	if (!ent->e.nonNormalizedAxes)
	{
		if (ent->e.frame == ent->e.oldframe)
		{
			switch (R_CullLocalPointAndRadius(newFrame->localOrigin, newFrame->radius))
			{
			case CULL_OUT:
				tr.pc.c_sphere_cull_md3_out++;
				return CULL_OUT;

			case CULL_IN:
				tr.pc.c_sphere_cull_md3_in++;
				return CULL_IN;

			case CULL_CLIP:
				tr.pc.c_sphere_cull_md3_clip++;
				break;
			default:;
			}
		}
		else
		{
			int sphere_cull_b;

			const int sphere_cull = R_CullLocalPointAndRadius(newFrame->localOrigin, newFrame->radius);
			if (newFrame == old_frame) {
				sphere_cull_b = sphere_cull;
			}
			else {
				sphere_cull_b = R_CullLocalPointAndRadius(old_frame->localOrigin, old_frame->radius);
			}

			if (sphere_cull == sphere_cull_b)
			{
				if (sphere_cull == CULL_OUT)
				{
					tr.pc.c_sphere_cull_md3_out++;
					return CULL_OUT;
				}
				if (sphere_cull == CULL_IN)
				{
					tr.pc.c_sphere_cull_md3_in++;
					return CULL_IN;
				}
				tr.pc.c_sphere_cull_md3_clip++;
			}
		}
	}

	// calculate a bounding box in the current coordinate system
	for (int i = 0; i < 3; i++) {
		bounds[0][i] = old_frame->bounds[0][i] < newFrame->bounds[0][i] ? old_frame->bounds[0][i] : newFrame->bounds[0][i];
		bounds[1][i] = old_frame->bounds[1][i] > newFrame->bounds[1][i] ? old_frame->bounds[1][i] : newFrame->bounds[1][i];
	}

	switch (R_CullLocalBox(bounds))
	{
	case CULL_IN:
		tr.pc.c_box_cull_md3_in++;
		return CULL_IN;
	case CULL_CLIP:
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	case CULL_OUT:
	default:
		tr.pc.c_box_cull_md3_out++;
		return CULL_OUT;
	}
}

/*
=================
RE_GetModelBounds

  Returns the bounds of the current model
  (qhandle_t)hModel and (int)frame need to be set
=================
*/

void RE_GetModelBounds(const refEntity_t* ref_ent, vec3_t bounds1, vec3_t bounds2)
{
	assert(ref_ent);

	const model_t* model = R_GetModelByHandle(ref_ent->hModel);
	assert(model);
	md3Header_t* header = model->md3[0];
	assert(header);
	const md3Frame_t* frame = reinterpret_cast<md3Frame_t*>(reinterpret_cast<byte*>(header) + header->ofsFrames) + ref_ent->frame;
	assert(frame);

	VectorCopy(frame->bounds[0], bounds1);
	VectorCopy(frame->bounds[1], bounds2);
}

/*
=================
R_ComputeLOD

=================
*/
static int R_ComputeLOD(trRefEntity_t* ent) {
	float radius;
	float flod;
	float projected_radius;

	if (tr.currentModel->numLods < 2)
	{	// model has only 1 LOD level, skip computations and bias
		return 0;
	}

	// multiple LODs exist, so compute projected bounding sphere
	// and use that as a criteria for selecting LOD
//	if ( tr.currentModel->md3[0] )
	{	//normal md3
		md3Frame_t* frame = reinterpret_cast<md3Frame_t*>(reinterpret_cast<unsigned char*>(tr.currentModel->md3[0]) + tr.currentModel->md3[0]->ofsFrames);
		frame += ent->e.frame;
		radius = RadiusFromBounds(frame->bounds[0], frame->bounds[1]);
	}

	if ((projected_radius = ProjectRadius(radius, ent->e.origin)) != 0)
	{
		flod = 1.0f - projected_radius * r_lodscale->value;
		flod *= tr.currentModel->numLods;
	}
	else
	{	// object intersects near view plane, e.g. view weapon
		flod = 0;
	}

	int lod = Q_ftol(flod);

	if (lod < 0) {
		lod = 0;
	}
	else if (lod >= tr.currentModel->numLods) {
		lod = tr.currentModel->numLods - 1;
	}

	lod += r_lodbias->integer;
	if (lod >= tr.currentModel->numLods)
		lod = tr.currentModel->numLods - 1;
	if (lod < 0)
		lod = 0;

	return lod;
}

/*
=================
R_ComputeFogNum

=================
*/
static int R_ComputeFogNum(md3Header_t* header, const trRefEntity_t* ent) {
	vec3_t			local_origin;

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
		return 0;
	}

	if (tr.refdef.doLAGoggles)
	{
		return tr.world->numfogs;
	}

	// FIXME: non-normalized axis issues
	const md3Frame_t* md3_frame = reinterpret_cast<md3Frame_t*>(reinterpret_cast<byte*>(header) + header->ofsFrames) + ent->e.frame;
	VectorAdd(ent->e.origin, md3_frame->localOrigin, local_origin);

	int partial_fog = 0;
	for (int i = 1; i < tr.world->numfogs; i++) {
		const fog_t* fog = &tr.world->fogs[i];
		if (local_origin[0] - md3_frame->radius >= fog->bounds[0][0]
			&& local_origin[0] + md3_frame->radius <= fog->bounds[1][0]
			&& local_origin[1] - md3_frame->radius >= fog->bounds[0][1]
			&& local_origin[1] + md3_frame->radius <= fog->bounds[1][1]
			&& local_origin[2] - md3_frame->radius >= fog->bounds[0][2]
			&& local_origin[2] + md3_frame->radius <= fog->bounds[1][2])
		{//totally inside it
			return i;
		}
		if (local_origin[0] - md3_frame->radius >= fog->bounds[0][0] && local_origin[1] - md3_frame->radius >= fog->bounds[0][1] && local_origin[2] - md3_frame->radius >= fog->bounds[0][2] &&
			local_origin[0] - md3_frame->radius <= fog->bounds[1][0] && local_origin[1] - md3_frame->radius <= fog->bounds[1][1] && local_origin[2] - md3_frame->radius <= fog->bounds[1][2] ||
			local_origin[0] + md3_frame->radius >= fog->bounds[0][0] && local_origin[1] + md3_frame->radius >= fog->bounds[0][1] && local_origin[2] + md3_frame->radius >= fog->bounds[0][2] &&
			local_origin[0] + md3_frame->radius <= fog->bounds[1][0] && local_origin[1] + md3_frame->radius <= fog->bounds[1][1] && local_origin[2] + md3_frame->radius <= fog->bounds[1][2])
		{
			//partially inside it
			if (tr.refdef.fogIndex == i || R_FogParmsMatch(tr.refdef.fogIndex, i))
			{//take new one only if it's the same one that the viewpoint is in
				return i;
			}
			if (!partial_fog)
			{//first partialFog
				partial_fog = i;
			}
		}
	}
	//if all else fails, return the first partialFog
	return partial_fog;
}

/*
=================
R_AddMD3Surfaces

=================
*/
void R_AddMD3Surfaces(trRefEntity_t* ent) {
	const shader_t* shader;

	// don't add third_person objects if not in a portal
	const auto personalModel = static_cast<qboolean>(ent->e.renderfx & RF_THIRD_PERSON && !tr.viewParms.is_portal);

	if (ent->e.renderfx & RF_CAP_FRAMES) {
		if (ent->e.frame > tr.currentModel->md3[0]->numFrames - 1)
			ent->e.frame = tr.currentModel->md3[0]->numFrames - 1;
		if (ent->e.oldframe > tr.currentModel->md3[0]->numFrames - 1)
			ent->e.oldframe = tr.currentModel->md3[0]->numFrames - 1;
	}
	else if (ent->e.renderfx & RF_WRAP_FRAMES) {
		ent->e.frame %= tr.currentModel->md3[0]->numFrames;
		ent->e.oldframe %= tr.currentModel->md3[0]->numFrames;
	}

	//
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	//
	if (ent->e.frame >= tr.currentModel->md3[0]->numFrames
		|| ent->e.frame < 0
		|| ent->e.oldframe >= tr.currentModel->md3[0]->numFrames
		|| ent->e.oldframe < 0)
	{
		ri.Printf(PRINT_ALL, "R_AddMD3Surfaces: no such frame %d to %d for '%s'\n",
			ent->e.oldframe, ent->e.frame,
			tr.currentModel->name);
		ent->e.frame = 0;
		ent->e.oldframe = 0;
	}

	//
	// compute LOD
	//
	const int lod = R_ComputeLOD(ent);

	md3Header_t* header = tr.currentModel->md3[lod];

	//
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	//
	const int cull = R_CullModel(header, ent);
	if (cull == CULL_OUT) {
		return;
	}

	//
	// set up lighting now that we know we aren't culled
	//
	if (!personalModel || r_shadows->integer > 1) {
		R_SetupEntityLighting(&tr.refdef, ent);
	}

	//
	// see if we are in a fog volume
	//
	const int fogNum = R_ComputeFogNum(header, ent);

	//
	// draw all surfaces
	//
	const shader_t* main_shader = R_GetShaderByHandle(ent->e.customShader);

	auto surface = reinterpret_cast<md3Surface_t*>(reinterpret_cast<byte*>(header) + header->ofsSurfaces);
	for (int i = 0; i < header->numSurfaces; i++) {
		if (ent->e.customShader) {// a little more efficient
			shader = main_shader;
		}
		else if (ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins) {
			const skin_t* skin = R_GetSkinByHandle(ent->e.customSkin);

			// match the surface name to something in the skin file
			shader = tr.defaultShader;
			for (int j = 0; j < skin->numSurfaces; j++) {
				// the names have both been lowercased
				if (!strcmp(skin->surfaces[j]->name, surface->name)) {
					shader = skin->surfaces[j]->shader;
					break;
				}
			}
		}
		else if (surface->numShaders <= 0) {
			shader = tr.defaultShader;
		}
		else {
			auto md3_shader = reinterpret_cast<md3Shader_t*>(reinterpret_cast<byte*>(surface) + surface->ofsShaders);
			md3_shader += ent->e.skinNum % surface->numShaders;
			shader = tr.shaders[md3_shader->shaderIndex];
		}

		// we will add shadows even if the main object isn't visible in the view

		// stencil shadows can't do personal models unless I polyhedron clip
		if (r_AdvancedsurfaceSprites->integer)
		{
			if (!personalModel
				&& r_shadows->integer == 2
				&& fogNum == 0
				&& !(ent->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
				&& shader->sort == SS_OPAQUE)
			{
				R_AddDrawSurf(reinterpret_cast<surfaceType_t*>(surface), tr.shadowShader, 0, qfalse);
			}
		}
		else
		{
			if (!personalModel
				&& r_shadows->integer == 2
				&& fogNum == 0
				&& (ent->e.renderfx & RF_SHADOW_PLANE)
				&& !(ent->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
				&& shader->sort == SS_OPAQUE)
			{
				R_AddDrawSurf(reinterpret_cast<surfaceType_t*>(surface), tr.shadowShader, 0, qfalse);
			}
		}

		// projection shadows work fine with personal models
		if (r_shadows->integer == 3
			&& fogNum == 0
			&& ent->e.renderfx & RF_SHADOW_PLANE
			&& shader->sort == SS_OPAQUE)
		{
			R_AddDrawSurf(reinterpret_cast<surfaceType_t*>(surface), tr.projectionShadowShader, 0, qfalse);
		}

		// don't add third_person objects if not viewing through a portal
		if (!personalModel)
		{
			R_AddDrawSurf(reinterpret_cast<surfaceType_t*>(surface), shader, fogNum, qfalse);
		}

		surface = reinterpret_cast<md3Surface_t*>(reinterpret_cast<byte*>(surface) + surface->ofsEnd);
	}
}