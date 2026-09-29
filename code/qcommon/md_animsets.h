/*
===========================================================================
MovieDuels: master _humanoid animation set (Update 9)
===========================================================================

Every model.glm names the animation set (GLA) it uses, e.g. Vader's model points at
models/players/_humanoid_vader/_humanoid. With g_ActivateAnimationStyle 0 that stays as it is.

With g_ActivateAnimationStyle 1, a model whose GLA belongs to one of the sets below uses the
master set instead: models/players/_humanoid/_humanoid.gla plus the animation.cfg and
animevents.cfg in models/players/_humanoid. The master holds every character's animations,
so the animation style (not the model) decides which ones a character plays.

Used by the game (animation.cfg / animevents.cfg folder) and by both renderers (which .gla
the model loads), so the list only exists here. Models with other skeletons (rancor, wampa,
droideka, SBD, vehicles...) are not in the list and keep their own animations.
*/

#pragma once

#include "q_shared.h"

// The animation style in use for the current map (read-only). The player's setting is
// g_ActivateAnimationStyle (menu/console); the game copies it into this cvar at every map load
// (new map or loaded save), so a change in the middle of a map waits for the next map load and
// the models, animation.cfg and animation code always agree. Game code and renderers read this one.
#define MD_ANIMSTYLE_ACTIVE_CVAR "g_animationStyleActive"

// When the setting changes in the middle of a map (Update 9), the game sends this engine command:
// it saves the game in its own slot and loads it straight back, and the load uses the new style.
#define MD_ANIMSTYLE_RELOAD_CMD "md_animstylereload"
#define MD_ANIMSTYLE_SAVE "animstyle"

// The master animation set (without the .gla extension, as model.glm stores it)
#define MD_MASTER_HUMANOID_GLA "models/players/_humanoid/_humanoid"
#define MD_MASTER_HUMANOID_DIR "_humanoid"

// Animation sets that are replaced by the master set with g_ActivateAnimationStyle 1.
// Folder names only: a GLA matches if it lives directly in one of these folders.
static const char* const md_masterHumanoidSets[] =
{
	"models/players/JK2anims",
	"models/players/_humanoid_ani",
	"models/players/_humanoid_bdroid",
	"models/players/_humanoid_ben",
	"models/players/_humanoid_cal",
	"models/players/_humanoid_clo",
	"models/players/_humanoid_df2",
	"models/players/_humanoid_dooku",
	"models/players/_humanoid_galen",
	"models/players/_humanoid_gon",
	"models/players/_humanoid_grievous",
	"models/players/_humanoid_jabba",
	"models/players/_humanoid_jango",
	"models/players/_humanoid_kotor",
	"models/players/_humanoid_luke",
	"models/players/_humanoid_mace",
	"models/players/_humanoid_maul",
	"models/players/_humanoid_md",
	"models/players/_humanoid_melee",
	"models/players/_humanoid_obi",
	"models/players/_humanoid_obi3",
	"models/players/_humanoid_pal",
	"models/players/_humanoid_reb",
	"models/players/_humanoid_ren",
	"models/players/_humanoid_rey",
	"models/players/_humanoid_vader",
	"models/players/_humanoid_yoda",
};

// True if gla_name (e.g. "models/players/_humanoid_vader/_humanoid") is in one of the sets above.
// Whole folder names only, so "_humanoid_obi" does not also match "_humanoid_obi3".
inline bool MD_IsMasterHumanoidSet(const char* gla_name)
{
	if (!gla_name || !gla_name[0])
	{
		return false;
	}
	for (const char* set : md_masterHumanoidSets)
	{
		const size_t len = strlen(set);
		if (!Q_stricmpn(gla_name, set, static_cast<int>(len)) && (gla_name[len] == '/' || gla_name[len] == '\\'))
		{
			return true;
		}
	}
	return false;
}

// The GLA a model should load: the master set when the style is on and its own set is in the list,
// otherwise its own. anim_style_active = g_ActivateAnimationStyle->integer == 1.
inline const char* MD_AnimSetGLA(const char* gla_name, const bool anim_style_active)
{
	return anim_style_active && MD_IsMasterHumanoidSet(gla_name) ? MD_MASTER_HUMANOID_GLA : gla_name;
}

// Renderers: can a mesh use the master set? Only if it has the master's skeleton, or it is an old JK2
// mesh (72 bones on a _humanoid set) that the engine converts. Otherwise (e.g. kyleJK2 on JK2anims,
// 72 bones) the model would fail to load, so it keeps its own set. The game follows automatically:
// it picks the animation.cfg folder from the GLA the model really loaded (G2API_GetGLAName).
inline bool MD_SkeletonFitsMaster(const int mesh_bones, const int master_bones, const char* own_gla_name)
{
	return mesh_bones == master_bones || (mesh_bones == 72 && strstr(own_gla_name, "_humanoid"));
}
