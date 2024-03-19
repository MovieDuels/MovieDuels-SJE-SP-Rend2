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

// tr_mesh.c: triangle model functions

#include "tr_local.h"

float ProjectRadius(float r, vec3_t location)
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
static int R_CullModel(md3Header_t* header, trRefEntity_t* ent) {
	vec3_t		bounds[2];

	// compute frame pointers
	const md3Frame_t* newFrame = (md3Frame_t*)((byte*)header + header->ofsFrames) + ent->e.frame;
	const md3Frame_t* oldFrame = (md3Frame_t*)((byte*)header + header->ofsFrames) + ent->e.oldframe;

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
			}
		}
		else
		{
			int sphereCullB;

			const int sphereCull = R_CullLocalPointAndRadius(newFrame->localOrigin, newFrame->radius);
			if (newFrame == oldFrame) {
				sphereCullB = sphereCull;
			}
			else {
				sphereCullB = R_CullLocalPointAndRadius(oldFrame->localOrigin, oldFrame->radius);
			}

			if (sphereCull == sphereCullB)
			{
				if (sphereCull == CULL_OUT)
				{
					tr.pc.c_sphere_cull_md3_out++;
					return CULL_OUT;
				}
				if (sphereCull == CULL_IN)
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
		bounds[0][i] = oldFrame->bounds[0][i] < newFrame->bounds[0][i] ? oldFrame->bounds[0][i] : newFrame->bounds[0][i];
		bounds[1][i] = oldFrame->bounds[1][i] > newFrame->bounds[1][i] ? oldFrame->bounds[1][i] : newFrame->bounds[1][i];
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
//rwwRMG - added
void RE_GetModelBounds(refEntity_t* ref_ent, vec3_t bounds1, vec3_t bounds2)
{
	assert(ref_ent);

	const model_t* model = R_GetModelByHandle(ref_ent->hModel);
	assert(model);
	md3Header_t* header = model->md3[0];
	assert(header);
	const md3Frame_t* frame = (md3Frame_t*)((byte*)header + header->ofsFrames) + ref_ent->frame;
	assert(frame);

	VectorCopy(frame->bounds[0], bounds1);
	VectorCopy(frame->bounds[1], bounds2);
}

/*
=================
R_ComputeLOD

=================
*/
int R_ComputeLOD(trRefEntity_t* ent) {
	float flod;
	float projectedRadius;
	int lod;

	if (tr.current_model->numLods < 2)
	{
		// model has only 1 LOD level, skip computations and bias
		lod = 0;
	}
	else
	{
		// multiple LODs exist, so compute projected bounding sphere
		// and use that as a criteria for selecting LOD

		md3Frame_t* frame = (md3Frame_t*)(((unsigned char*)tr.current_model->md3[0]) + tr.current_model->md3[0]->ofsFrames);

		frame += ent->e.frame;

		const float radius = RadiusFromBounds(frame->bounds[0], frame->bounds[1]);

		if ((projectedRadius = ProjectRadius(radius, ent->e.origin)) != 0)
		{
			float lodscale = (r_lodscale->value + r_autolodscalevalue->value);
			if (lodscale > 20)
			{
				lodscale = 20;
			}
			else if (lodscale < 0)
			{
				lodscale = 0;
			}
			flod = 1.0f - projectedRadius * lodscale;
		}
		else
		{
			// object intersects near view plane, e.g. view weapon
			flod = 0;
		}

		flod *= tr.current_model->numLods;
		lod = Q_ftol(flod);

		if (lod < 0)
		{
			lod = 0;
		}
		else if (lod >= tr.current_model->numLods)
		{
			lod = tr.current_model->numLods - 1;
		}
	}

	lod += r_lodbias->integer;

	if (lod >= tr.current_model->numLods)
		lod = tr.current_model->numLods - 1;
	if (lod < 0)
		lod = 0;

	return lod;
}

/*
=================
R_ComputeFogNum

=================
*/
int R_ComputeFogNum(md3Header_t* header, trRefEntity_t* ent) {
	int j;
	vec3_t			localOrigin;

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL) {
		return 0;
	}

	// FIXME: non-normalized axis issues
	const md3Frame_t* md3Frame = (md3Frame_t*)((byte*)header + header->ofsFrames) + ent->e.frame;
	VectorAdd(ent->e.origin, md3Frame->localOrigin, localOrigin);
	for (int i = 1; i < tr.world->numfogs; i++) {
		const fog_t* fog = &tr.world->fogs[i];
		for (j = 0; j < 3; j++) {
			if (localOrigin[j] - md3Frame->radius >= fog->bounds[1][j]) {
				break;
			}
			if (localOrigin[j] + md3Frame->radius <= fog->bounds[0][j]) {
				break;
			}
		}
		if (j == 3) {
			return i;
		}
	}

	return 0;
}

/*
=================
R_AddMD3Surfaces

=================
*/
void R_AddMD3Surfaces(trRefEntity_t* ent) {
	shader_t* shader = nullptr;

	// don't add third_person objects if not in a portal
	const qboolean personalModel = static_cast<qboolean>((ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.is_portal);

	if (ent->e.renderfx & RF_WRAP_FRAMES) {
		ent->e.frame %= tr.current_model->md3[0]->num_frames;
		ent->e.oldframe %= tr.current_model->md3[0]->num_frames;
	}

	//
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	//
	if ((ent->e.frame >= tr.current_model->md3[0]->num_frames)
		|| (ent->e.frame < 0)
		|| (ent->e.oldframe >= tr.current_model->md3[0]->num_frames)
		|| (ent->e.oldframe < 0)) {
		ri->Printf(PRINT_DEVELOPER, S_COLOR_RED "R_AddMD3Surfaces: no such frame %d to %d for '%s'\n",
			ent->e.oldframe, ent->e.frame,
			tr.current_model->name);
		ent->e.frame = 0;
		ent->e.oldframe = 0;
	}

	//
	// compute LOD
	//
	const int lod = R_ComputeLOD(ent);

	md3Header_t* header = tr.current_model->md3[lod];

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
	md3Surface_t* surface = (md3Surface_t*)((byte*)header + header->ofsSurfaces);
	for (int i = 0; i < header->numSurfaces; i++) {
		if (ent->e.customShader) {
			shader = R_GetShaderByHandle(ent->e.customShader);
		}
		else if (ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins) {
			skin_t* skin = R_GetSkinByHandle(ent->e.customSkin);

			// match the surface name to something in the skin file
			shader = tr.defaultShader;
			for (int j = 0; j < skin->numSurfaces; j++) {
				// the names have both been lowercased
				if (!strcmp(skin->surfaces[j]->name, surface->name)) {
					shader = static_cast<shader_t*>(skin->surfaces[j]->shader);
					break;
				}
			}
			if (shader == tr.defaultShader) {
				ri->Printf(PRINT_DEVELOPER, S_COLOR_RED "WARNING: no shader for surface %s in skin %s\n", surface->name, skin->name);
			}
			else if (shader->defaultShader) {
				ri->Printf(PRINT_DEVELOPER, S_COLOR_RED "WARNING: shader %s in skin %s not found\n", shader->name, skin->name);
			}
		}
		else if (surface->numShaders <= 0) {
			shader = tr.defaultShader;
		}
		else {
			md3Shader_t* md3Shader = (md3Shader_t*)((byte*)surface + surface->ofsShaders);
			md3Shader += ent->e.skinNum % surface->numShaders;
			shader = tr.shaders[md3Shader->shaderIndex];
		}

		// we will add shadows even if the main object isn't visible in the view

		// stencil shadows can't do personal models unless I polyhedron clip
		if (!personalModel
			&& r_shadows->integer == 2
			&& fogNum == 0
			&& !(ent->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK))
			&& shader->sort == SS_OPAQUE) {
			R_AddDrawSurf((surfaceType_t*)surface, tr.shadowShader, 0, qfalse);
		}

		// projection shadows work fine with personal models
		if (r_shadows->integer == 3
			&& fogNum == 0
			&& (ent->e.renderfx & RF_SHADOW_PLANE)
			&& shader->sort == SS_OPAQUE) {
			R_AddDrawSurf((surfaceType_t*)surface, tr.projectionShadowShader, 0, qfalse);
		}

		// don't add third_person objects if not viewing through a portal
		if (!personalModel) {
			R_AddDrawSurf((surfaceType_t*)surface, shader, fogNum, qfalse);
		}

		surface = (md3Surface_t*)((byte*)surface + surface->ofsEnd);
	}
}