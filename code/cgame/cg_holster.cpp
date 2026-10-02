/*
===========================================================================
Holstered guns (singleplayer)

The multiplayer holster system (codemp/cgame/cg_holster.c and CG_VisualWeaponsUpdate) for the guns:
the guns a character carries but is not holding are drawn on his body. Where each one goes is set up
per player model in models/players/<model>/holster.cfg (kyle's when the model has none), so a model
gets its holster positions without code changes. The same holster.cfg files are used in multiplayer.

Sabers keep the singleplayer holster system (wp_saber.cpp: the holsterPlace of the .sab file, the
scripts, the saved games); the saber entries of holster.cfg are not used here, except that a saber
holstered on the back takes the back place.

Places, each for one gun (at most two guns are holstered):
- right hip, then left hip (HLR_*_R / HLR_*_L), as in multiplayer: the rifles at the blaster rifle's places
  (E-11, E-5, F-11D, DC-15S, DH-17), the pistols at the blaster pistol's (blaster pistol, NN-14, Westar, EE-3,
  DC-17) or the bryar pistol's
- back: rocket launcher, concussion rifle, repeater, flechette, disruptor, bowcaster, DEMP 2, Z-6 (its own
  place, HLR_Z6), DC-17m (at the repeater's place), DC-15A and A280 (at the disruptor's)
The dual pistols, the SBD and wrist blasters are not holstered (multiplayer doesn't either).
A gun keeps its place as long as he has it (the place stays empty while it is in his hand), so guns
don't move around when he switches weapons; a new gun only gets a free place, and only one the model's
holster.cfg doesn't disable (boneIndex HOLSTER_NONE / disabled).

No holstered guns in cutscenes (in_camera): characters sit in chairs and so on.
No gun on the back of a character with a jetpack (Boba Fett, Jango Fett, Mandalorians, rocket troopers).

cg_holsteredweapons: 0 = off, 1 = the player only, 2 = everyone (default)
cg_holsterdebug <holsterType number> with cg_holsterdebug_boneindex <0-4>, cg_holsterdebug_posoffset "x y z"
and cg_holsterdebug_angoffset "pitch yaw roll": places that holster type live, to tune a holster.cfg (for a gun
holster type the player then shows only that gun, in that holster, whether he carries it or not)
===========================================================================
*/

#include "cg_headers.h"
#include "cg_media.h"
#include "../game/ghoul2_shared.h"
#include "cg_holster.h"

// The holster types of holster.cfg (the numbers are the ones cg_holsterdebug takes, as in multiplayer)
enum
{
	HLR_NONE,
	HLR_SINGLESABER_1,
	HLR_SINGLESABER_2,
	HLR_STAFFSABER,
	HLR_PISTOL_L,
	HLR_PISTOL_R,
	HLR_BLASTER_L,
	HLR_BLASTER_R,
	HLR_BRYARPISTOL_L,
	HLR_BRYARPISTOL_R,
	HLR_BOWCASTER,
	HLR_ROCKET_LAUNCHER,
	HLR_DEMP2,
	HLR_CONCUSSION,
	HLR_REPEATER,
	HLR_FLECHETTE,
	HLR_DISRUPTOR,
	HLR_Z6, // the Z6 rotary cannon (MovieDuels)
	MAX_HOLSTER
};

// The body parts a holster position is relative to (holster.cfg: HOLSTER_NONE ... HOLSTER_RIGHTHIP;
// the names differ in the code because singleplayer has HOLSTER_NONE / HOLSTER_BACK for the sabers)
enum
{
	HLB_NONE,
	HLB_UPPERBACK,
	HLB_LOWERBACK,
	HLB_LEFTHIP,
	HLB_RIGHTHIP,
	HLB_NUM
};

static const stringID_table_t hlsTypeTable[] =
{
	{"HLR_NONE", HLR_NONE},
	{"HLR_SINGLESABER_1", HLR_SINGLESABER_1},
	{"HLR_SINGLESABER_2", HLR_SINGLESABER_2},
	{"HLR_STAFFSABER", HLR_STAFFSABER},
	{"HLR_PISTOL_L", HLR_PISTOL_L},
	{"HLR_PISTOL_R", HLR_PISTOL_R},
	{"HLR_BLASTER_L", HLR_BLASTER_L},
	{"HLR_BLASTER_R", HLR_BLASTER_R},
	{"HLR_BRYARPISTOL_L", HLR_BRYARPISTOL_L},
	{"HLR_BRYARPISTOL_R", HLR_BRYARPISTOL_R},
	{"HLR_BOWCASTER", HLR_BOWCASTER},
	{"HLR_ROCKET_LAUNCHER", HLR_ROCKET_LAUNCHER},
	{"HLR_DEMP2", HLR_DEMP2},
	{"HLR_CONCUSSION", HLR_CONCUSSION},
	{"HLR_REPEATER", HLR_REPEATER},
	{"HLR_FLECHETTE", HLR_FLECHETTE},
	{"HLR_DISRUPTOR", HLR_DISRUPTOR},
	{"HLR_Z6", HLR_Z6},
	{nullptr, -1}
};

static const stringID_table_t hlsBoneTable[] =
{
	{"HOLSTER_NONE", HLB_NONE},
	{"HOLSTER_UPPERBACK", HLB_UPPERBACK},
	{"HOLSTER_LOWERBACK", HLB_LOWERBACK},
	{"HOLSTER_LEFTHIP", HLB_LEFTHIP},
	{"HOLSTER_RIGHTHIP", HLB_RIGHTHIP},
	{nullptr, -1}
};

// The bolt (bone or surface tag) of each body part, as in multiplayer
static const char* const hlsBoneBolt[HLB_NUM] = { nullptr, "*chestg", "lower_lumbar", "lfemurYZ", "rfemurYZ" };

struct holster_t
{
	int boneIndex;		// HLB_*
	vec3_t posOffset;	// along the body part's axes
	vec3_t angOffset;	// pitch, yaw, roll from the body part's orientation
};

// holster.cfg of one player model (loaded the first time a character with that model is drawn)
struct holsterModel_t
{
	char name[MAX_QPATH];
	holster_t data[MAX_HOLSTER];
};

constexpr int MAX_HOLSTER_MODELS = 256;
constexpr int MAX_HOLSTER_FILE = 8192;

static holsterModel_t hlsModels[MAX_HOLSTER_MODELS];
static int hlsNumModels = 0;

// The holster places of a character, each with the gun that has it (WP_NONE: free)
enum
{
	HLP_RHIP,
	HLP_LHIP,
	HLP_BACK,
	HLP_NUM
};
constexpr int MAX_HOLSTERED = 2; // at most this many guns have a place

static int hlsPlaces[MAX_GENTITIES][HLP_NUM];

// The guns that can be holstered, the ones that come first get a place first.
// Hip guns: the right hip, then the left (holster types right / left); back guns: the back.
struct hlsGun_t
{
	int weapon;
	qboolean back;
	int right; // the holster type on the right hip, or on the back
	int left;  // the holster type on the left hip
};
static const hlsGun_t hlsGuns[] =
{
	// the same guns at the same places and in the same order as multiplayer (CG_VisualWeaponsUpdate)
	// the rifles
	{WP_BLASTER, qfalse, HLR_BLASTER_R, HLR_BLASTER_L},
	{WP_BATTLEDROID, qfalse, HLR_BLASTER_R, HLR_BLASTER_L},
	{WP_THEFIRSTORDER, qfalse, HLR_BLASTER_R, HLR_BLASTER_L},
	{WP_CLONECARBINE, qfalse, HLR_BLASTER_R, HLR_BLASTER_L},
	{WP_REBELBLASTER, qfalse, HLR_BLASTER_R, HLR_BLASTER_L},
	// the pistols (multiplayer's WP_BRYAR_PISTOL is the blaster pistol, its WP_BRYAR_OLD the bryar pistol)
	{WP_BLASTER_PISTOL, qfalse, HLR_PISTOL_R, HLR_PISTOL_L},
	{WP_REY, qfalse, HLR_PISTOL_R, HLR_PISTOL_L},
	{WP_JANGO, qfalse, HLR_PISTOL_R, HLR_PISTOL_L},
	{WP_BOBA, qfalse, HLR_PISTOL_R, HLR_PISTOL_L},
	{WP_CLONEPISTOL, qfalse, HLR_PISTOL_R, HLR_PISTOL_L},
	{WP_BRYAR_PISTOL, qfalse, HLR_BRYARPISTOL_R, HLR_BRYARPISTOL_L},
	// the back
	{WP_ROCKET_LAUNCHER, qtrue, HLR_ROCKET_LAUNCHER, HLR_ROCKET_LAUNCHER},
	{WP_CONCUSSION, qtrue, HLR_CONCUSSION, HLR_CONCUSSION},
	{WP_REPEATER, qtrue, HLR_REPEATER, HLR_REPEATER},
	{WP_FLECHETTE, qtrue, HLR_FLECHETTE, HLR_FLECHETTE},
	{WP_DISRUPTOR, qtrue, HLR_DISRUPTOR, HLR_DISRUPTOR},
	{WP_BOWCASTER, qtrue, HLR_BOWCASTER, HLR_BOWCASTER},
	{WP_DEMP2, qtrue, HLR_DEMP2, HLR_DEMP2},
	{WP_Z6_ROTARY_CANNON, qtrue, HLR_Z6, HLR_Z6},
	// the long rifles, too long for a hip
	{WP_CLONECOMMANDO, qtrue, HLR_REPEATER, HLR_REPEATER},
	{WP_CLONERIFLE, qtrue, HLR_DISRUPTOR, HLR_DISRUPTOR},
	{WP_REBELRIFLE, qtrue, HLR_DISRUPTOR, HLR_DISRUPTOR},
};

static const hlsGun_t* Hls_Gun(const int weapon)
{
	for (const hlsGun_t& gun : hlsGuns)
	{
		if (gun.weapon == weapon)
		{
			return &gun;
		}
	}
	return nullptr;
}

// The holster type of a gun in a place
static int Hls_PlaceType(const hlsGun_t* gun, const int place)
{
	return place == HLP_LHIP ? gun->left : gun->right;
}

// The gun models drawn in the holsters (the weapon's world model, weapons.dat weaponMdl)
static CGhoul2Info_v* hlsWeaponG2[WP_NUM_WEAPONS];
static qhandle_t hlsWeaponModel[WP_NUM_WEAPONS];
static qboolean hlsWeaponTried[WP_NUM_WEAPONS];

// ------------------------------------------------------------------------------------------------
// holster.cfg
// ------------------------------------------------------------------------------------------------

static qboolean Hls_ParseVector(const char* value, vec3_t out)
{
	if (sscanf(value, "%f , %f , %f", &out[0], &out[1], &out[2]) == 3
		|| sscanf(value, "%f %f %f", &out[0], &out[1], &out[2]) == 3)
	{
		return qtrue;
	}
	return qfalse;
}

// The holster.cfg format: blocks of { holsterType <HLR_*> boneIndex <HOLSTER_* or disabled>
// posOffset "x, y, z" angOffset "p, y, r" }
static void Hls_Parse(holsterModel_t* model, const char* fileName, const char* text)
{
	COM_ParseSession session;
	const char* p = text;

	while (true)
	{
		const char* token = COM_ParseExt(&p, qtrue);
		if (!token[0])
		{
			break;
		}
		if (Q_stricmp(token, "{"))
		{
			continue;
		}

		int type = -1;
		int bone = -1;
		vec3_t pos = { 0, 0, 0 };
		vec3_t ang = { 0, 0, 0 };
		qboolean havePos = qfalse;
		qboolean haveAng = qfalse;

		while (true)
		{
			token = COM_ParseExt(&p, qtrue);
			if (!token[0] || !Q_stricmp(token, "}"))
			{
				break;
			}
			char key[64];
			Q_strncpyz(key, token, sizeof key);
			const char* value = COM_ParseExt(&p, qfalse);

			if (!Q_stricmp(key, "holsterType"))
			{
				type = GetIDForString(hlsTypeTable, value);
			}
			else if (!Q_stricmp(key, "boneIndex"))
			{
				bone = !Q_stricmp(value, "disabled") ? HLB_NONE : GetIDForString(hlsBoneTable, value);
			}
			else if (!Q_stricmp(key, "posOffset"))
			{
				havePos = Hls_ParseVector(value, pos);
				if (!havePos)
				{
					Com_Printf(S_COLOR_YELLOW "%s: bad posOffset '%s'\n", fileName, value);
				}
			}
			else if (!Q_stricmp(key, "angOffset"))
			{
				haveAng = Hls_ParseVector(value, ang);
				if (!haveAng)
				{
					Com_Printf(S_COLOR_YELLOW "%s: bad angOffset '%s'\n", fileName, value);
				}
			}
		}

		if (type <= HLR_NONE || type >= MAX_HOLSTER)
		{
			continue; // no or an unknown holsterType: skip the block
		}
		holster_t& h = model->data[type];
		if (bone >= HLB_NONE && bone < HLB_NUM)
		{
			h.boneIndex = bone;
		}
		if (havePos)
		{
			VectorCopy(pos, h.posOffset);
		}
		if (haveAng)
		{
			VectorCopy(ang, h.angOffset);
		}
	}
}

// The holster data of a player model: its holster.cfg, or kyle's
static const holsterModel_t* Hls_ModelData(const char* modelName)
{
	for (int i = 0; i < hlsNumModels; i++)
	{
		if (!Q_stricmp(hlsModels[i].name, modelName))
		{
			return &hlsModels[i];
		}
	}
	if (hlsNumModels >= MAX_HOLSTER_MODELS)
	{
		return nullptr;
	}

	holsterModel_t* model = &hlsModels[hlsNumModels++];
	memset(model, 0, sizeof * model); // every holster type: HLB_NONE (not drawn) until the file says
	Q_strncpyz(model->name, modelName, sizeof model->name);

	char fileName[MAX_QPATH];
	char* buf = nullptr;
	Com_sprintf(fileName, sizeof fileName, "models/players/%s/holster.cfg", modelName);
	long len = gi.FS_ReadFile(fileName, reinterpret_cast<void**>(&buf));
	if (len <= 0 || !buf)
	{
		Q_strncpyz(fileName, "models/players/kyle/holster.cfg", sizeof fileName);
		len = gi.FS_ReadFile(fileName, reinterpret_cast<void**>(&buf));
	}
	if (len <= 0 || !buf)
	{
		return model; // no holster.cfg at all: nothing drawn
	}
	if (len >= MAX_HOLSTER_FILE)
	{
		Com_Printf(S_COLOR_YELLOW "%s is too big (%ld, max %d)\n", fileName, len, MAX_HOLSTER_FILE - 1);
		gi.FS_FreeFile(buf);
		return model;
	}

	static char text[MAX_HOLSTER_FILE];
	memcpy(text, buf, len);
	text[len] = 0;
	gi.FS_FreeFile(buf);
	Hls_Parse(model, fileName, text);
	return model;
}

// The player model folder of a character: "kyle" from models/players/kyle/model.glm
static qboolean Hls_ModelName(const gentity_t* gent, char* out, const int size)
{
	if (gent->playerModel < 0 || gent->playerModel >= gent->ghoul2.size())
	{
		return qfalse;
	}
	char file[MAX_QPATH];
	Q_strncpyz(file, gent->ghoul2[gent->playerModel].mFileName, sizeof file);
	for (char* c = file; *c; c++)
	{
		*c = *c == '\\' ? '/' : static_cast<char>(tolower(static_cast<unsigned char>(*c)));
	}
	const char* start = strstr(file, "models/players/");
	if (!start)
	{
		return qfalse;
	}
	start += strlen("models/players/");
	const char* end = strchr(start, '/');
	const int len = end ? static_cast<int>(end - start) : static_cast<int>(strlen(start));
	if (len <= 0 || len >= size)
	{
		return qfalse;
	}
	memcpy(out, start, len);
	out[len] = 0;
	return qtrue;
}

// ------------------------------------------------------------------------------------------------
// Drawing
// ------------------------------------------------------------------------------------------------

// The gun's model for the holster (created the first time it is needed)
static CGhoul2Info_v* Hls_WeaponG2(const int weapon)
{
	if (weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS)
	{
		return nullptr;
	}
	if (!hlsWeaponTried[weapon])
	{
		hlsWeaponTried[weapon] = qtrue;
		// weapons.dat has the view model (merr_sonn.md3): the world model is merr_sonn_w.glm, the one
		// G_CreateG2AttachedWeaponModel puts in the hand
		char modelFile[64];
		Q_strncpyz(modelFile, weaponData[weapon].weaponMdl, sizeof modelFile);
		if (char* ext = strstr(modelFile, ".md3"))
		{
			*ext = 0;
			if (!strstr(modelFile, "_w"))
			{
				Q_strcat(modelFile, sizeof modelFile, "_w");
			}
			Q_strcat(modelFile, sizeof modelFile, ".glm");
		}
		if (modelFile[0])
		{
			const qhandle_t model = cgi_R_RegisterModel(modelFile);
			if (model)
			{
				const auto g2 = new CGhoul2Info_v;
				if (gi.G2API_InitGhoul2Model(*g2, modelFile, model, NULL_HANDLE, NULL_HANDLE, 0, 0) >= 0)
				{
					hlsWeaponG2[weapon] = g2;
					hlsWeaponModel[weapon] = model;
				}
				else
				{
					delete g2;
				}
			}
		}
	}
	return hlsWeaponG2[weapon];
}

// Applies a rotation (degrees) about one axis (PITCH, YAW or ROLL) to the axis (multiplayer's ApplyAxisRotation)
static void Hls_RotateAxis(vec3_t axis[3], const int rotType, const float value)
{
	vec3_t result[3];
	vec3_t rotation[3];

	if (value == 0)
	{
		return;
	}
	const float s = sin(DEG2RAD(value));
	const float c = cos(DEG2RAD(value));

	switch (rotType)
	{
	case ROLL:
		VectorSet(rotation[0], 1, 0, 0);
		VectorSet(rotation[1], 0, c, -s);
		VectorSet(rotation[2], 0, s, c);
		break;
	case PITCH:
		VectorSet(rotation[0], c, 0, s);
		VectorSet(rotation[1], 0, 1, 0);
		VectorSet(rotation[2], -s, 0, c);
		break;
	case YAW:
		VectorSet(rotation[0], c, -s, 0);
		VectorSet(rotation[1], s, c, 0);
		VectorSet(rotation[2], 0, 0, 1);
		break;
	default:
		return;
	}

	for (int i = 0; i < 3; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			result[i][j] = rotation[i][0] * axis[0][j] + rotation[i][1] * axis[1][j] + rotation[i][2] * axis[2][j];
		}
	}
	for (int i = 0; i < 3; i++)
	{
		VectorCopy(result[i], axis[i]);
	}
}

// Draws one gun in one holster place (multiplayer's CG_HolsteredWeaponRender)
static void Hls_Render(centity_t* cent, const holsterModel_t* model, const int holsterType, const int weapon,
	const vec3_t g2Angles, const vec3_t origin, const int playerRenderfx)
{
	holster_t h = model->data[holsterType];

	if (cg_holsterdebug.integer == holsterType)
	{
		// tuning a holster.cfg: the debug cvars place this holster type
		h.boneIndex = cg_holsterdebug_boneindex.integer;
		if (!Hls_ParseVector(cg_holsterdebug_posoffset.string, h.posOffset))
		{
			VectorClear(h.posOffset);
		}
		if (!Hls_ParseVector(cg_holsterdebug_angoffset.string, h.angOffset))
		{
			VectorClear(h.angOffset);
		}
	}
	if (h.boneIndex <= HLB_NONE || h.boneIndex >= HLB_NUM)
	{
		return; // not set up (or disabled) for this model
	}

	CGhoul2Info_v* weaponG2 = Hls_WeaponG2(weapon);
	if (!weaponG2)
	{
		return;
	}
	gentity_t* gent = cent->gent;
	const int bolt = gi.G2API_AddBolt(&gent->ghoul2[gent->playerModel], hlsBoneBolt[h.boneIndex]);
	if (bolt < 0)
	{
		return; // the model has no such bone (a droid, a creature)
	}

	mdxaBone_t boltMatrix;
	vec3_t angles, org, axis[3];
	VectorCopy(g2Angles, angles);
	VectorCopy(origin, org);
	gi.G2API_GetBoltMatrix(gent->ghoul2, gent->playerModel, bolt, &boltMatrix, angles, org, cg.time,
		cgs.model_draw, cent->currentState.modelScale);

	vec3_t boltOrg;
	gi.G2API_GiveMeVectorFromMatrix(boltMatrix, ORIGIN, boltOrg);
	// The same three axes multiplayer takes (its POSITIVE_X, NEGATIVE_Z, POSITIVE_Y), so the holster.cfg
	// offsets mean the same in both. Singleplayer's bolt matrices name them differently (measured on every
	// holster bolt, bones and tags alike): multiplayer's POSITIVE_X is singleplayer's NEGATIVE_Y, its
	// POSITIVE_Y is singleplayer's POSITIVE_X.
	gi.G2API_GiveMeVectorFromMatrix(boltMatrix, NEGATIVE_Y, axis[0]); // left / right
	gi.G2API_GiveMeVectorFromMatrix(boltMatrix, NEGATIVE_Z, axis[1]); // forward / back
	gi.G2API_GiveMeVectorFromMatrix(boltMatrix, POSITIVE_X, axis[2]); // up / down

	refEntity_t ent = {};
	VectorMA(boltOrg, h.posOffset[0], axis[0], boltOrg);
	VectorMA(boltOrg, h.posOffset[1], axis[1], boltOrg);
	VectorMA(boltOrg, h.posOffset[2], axis[2], boltOrg);

	VectorCopy(axis[1], ent.axis[0]);
	VectorScale(axis[0], -1.0f, ent.axis[1]); // reversed, right-hand rule
	VectorCopy(axis[2], ent.axis[2]);
	Hls_RotateAxis(ent.axis, PITCH, h.angOffset[PITCH]);
	Hls_RotateAxis(ent.axis, YAW, h.angOffset[YAW]);
	Hls_RotateAxis(ent.axis, ROLL, h.angOffset[ROLL]);


	VectorCopy(boltOrg, ent.origin);
	VectorCopy(boltOrg, ent.oldorigin);
	VectorCopy(origin, ent.lightingOrigin); // lit like the body it hangs on
	ent.renderfx = (playerRenderfx & RF_THIRD_PERSON) | RF_LIGHTING_ORIGIN;
	ent.hModel = hlsWeaponModel[weapon];
	ent.ghoul2 = weaponG2;
	ent.radius = 64.0f;
	if (cent->currentState.powerups & 1 << PW_CLOAKED)
	{
		ent.customShader = cgs.media.cloakedShader;
	}
	cgi_R_AddRefEntityToScene(&ent);
}

void CG_HolsteredWeapons(centity_t* cent, const vec3_t g2Angles, const vec3_t origin, const int playerRenderfx)
{
	gentity_t* gent = cent->gent;

	if (cg_holsteredweapons.integer < 1 || !gent || !gent->client || !gent->ghoul2.size() || !cg.snap)
	{
		return;
	}
	if (in_camera)
	{
		return; // no holstered guns in cutscenes (characters sit in chairs and so on)
	}
	const gclient_t* client = gent->client;
	const qboolean isPlayer = cent->currentState.number == 0 ? qtrue : qfalse;

	if (!isPlayer && cg_holsteredweapons.integer < 2)
	{
		return;
	}
	if (isPlayer && !cg.renderingThirdPerson && !cg_trueguns.integer)
	{
		return; // first person without the body
	}
	if (gent->health <= 0 || client->ps.pm_type == PM_DEAD || client->ps.m_iVehicleNum
		|| client->NPC_class == CLASS_VEHICLE)
	{
		return;
	}

	char modelName[MAX_QPATH];
	if (!Hls_ModelName(gent, modelName, sizeof modelName))
	{
		return;
	}
	const holsterModel_t* model = Hls_ModelData(modelName);
	if (!model)
	{
		return;
	}

	// Tuning a gun's holster type: the player shows only that gun, in that holster, carried or not
	if (isPlayer && cg_holsterdebug.integer >= HLR_PISTOL_L && cg_holsterdebug.integer < MAX_HOLSTER)
	{
		for (const hlsGun_t& gun : hlsGuns)
		{
			if (gun.right == cg_holsterdebug.integer || gun.left == cg_holsterdebug.integer)
			{
				Hls_Render(cent, model, cg_holsterdebug.integer, gun.weapon, g2Angles, origin, playerRenderfx);
				break;
			}
		}
		return;
	}

	// the weapons he has (MovieDuels has more than 32, so ps.weapons[] instead of the STAT_WEAPONS bits)
	auto has = [&](const int weapon)
	{
		return weapon > WP_NONE && weapon < WP_NUM_WEAPONS && client->ps.weapons[weapon] ? true : false;
	};
	const int inHand = client->ps.weapon;
	auto carries = [&](const int weapon)
	{
		return weapon != inHand && has(weapon);
	};

	// A jetpack (the classes that can fly with one) or a saber holstered on the back (the singleplayer saber
	// holsters) keeps a gun off the back
	qboolean backBlocked = client->NPC_class == CLASS_BOBAFETT || client->NPC_class == CLASS_MANDALORIAN
		|| client->NPC_class == CLASS_JANGO || client->NPC_class == CLASS_JANGODUAL
		|| client->NPC_class == CLASS_ROCKETTROOPER ? qtrue : qfalse;
	for (int i = 0; i < MAX_HOLSTER_WEAPONS; i++)
	{
		const holster_locations_t place = client->ps.saber[i].holsterPlace;
		if (gent->holsterModel[i] > 0 && (place == HOLSTER_BACK || place == HOLSTER_BACKU || place == HOLSTER_BACKL))
		{
			backBlocked = qtrue;
		}
	}

	// A holster type this model's holster.cfg disables (boneIndex HOLSTER_NONE / disabled) is no place for a gun
	auto enabled = [&](const int holsterType)
	{
		const int bone = cg_holsterdebug.integer == holsterType ? cg_holsterdebug_boneindex.integer
			: model->data[holsterType].boneIndex;
		return bone > HLB_NONE && bone < HLB_NUM;
	};

	// The places keep their gun: a gun that got a place keeps it as long as he has it (also while it is in
	// his hand, then the place stays empty), no other gun takes it meanwhile. At most MAX_HOLSTERED guns.
	int* places = hlsPlaces[cent->currentState.number];
	int used = 0;
	for (int p = 0; p < HLP_NUM; p++)
	{
		const hlsGun_t* gun = places[p] ? Hls_Gun(places[p]) : nullptr;
		if (places[p] && (!has(places[p]) || !gun || !enabled(Hls_PlaceType(gun, p))))
		{
			places[p] = WP_NONE; // he no longer has it (or his model has no such place)
		}
		if (places[p])
		{
			used++;
		}
	}
	auto placed = [&](const int weapon)
	{
		for (int p = 0; p < HLP_NUM; p++)
		{
			if (places[p] == weapon)
			{
				return true;
			}
		}
		return false;
	};
	for (const hlsGun_t& gun : hlsGuns)
	{
		if (used >= MAX_HOLSTERED)
		{
			break;
		}
		if (!carries(gun.weapon) || placed(gun.weapon))
		{
			continue;
		}
		if (gun.back)
		{
			if (!places[HLP_BACK] && !backBlocked && enabled(gun.right))
			{
				places[HLP_BACK] = gun.weapon;
				used++;
			}
		}
		else if (!places[HLP_RHIP] && enabled(gun.right))
		{
			places[HLP_RHIP] = gun.weapon;
			used++;
		}
		else if (!places[HLP_LHIP] && enabled(gun.left))
		{
			places[HLP_LHIP] = gun.weapon;
			used++;
		}
	}

	for (int p = 0; p < HLP_NUM; p++)
	{
		const int weapon = places[p];
		if (!weapon || weapon == inHand || p == HLP_BACK && backBlocked)
		{
			continue;
		}
		if (const hlsGun_t* gun = Hls_Gun(weapon))
		{
			Hls_Render(cent, model, Hls_PlaceType(gun, p), weapon, g2Angles, origin, playerRenderfx);
		}
	}
}

// Whether a gun is holstered on a hip of this character (wp_saber.cpp then holsters his saber at the front of
// that hip); not while that gun is in his hand
qboolean CG_HolsterHipTaken(const int entNum, const qboolean left)
{
	if (entNum < 0 || entNum >= MAX_GENTITIES || cg_holsteredweapons.integer < 1
		|| entNum != 0 && cg_holsteredweapons.integer < 2 || in_camera)
	{
		return qfalse;
	}
	const int weapon = hlsPlaces[entNum][left ? HLP_LHIP : HLP_RHIP];
	if (weapon == WP_NONE)
	{
		return qfalse;
	}
	const gentity_t* gent = cg_entities[entNum].gent;
	if (gent && gent->client && gent->client->ps.weapon == weapon)
	{
		return qfalse; // the gun of that hip is in his hand, the hip is empty
	}
	return qtrue;
}

void CG_HolsterShutdown()
{
	for (int i = 0; i < WP_NUM_WEAPONS; i++)
	{
		if (hlsWeaponG2[i])
		{
			gi.G2API_CleanGhoul2Models(*hlsWeaponG2[i]);
			delete hlsWeaponG2[i];
			hlsWeaponG2[i] = nullptr;
		}
		hlsWeaponModel[i] = 0;
		hlsWeaponTried[i] = qfalse;
	}
	hlsNumModels = 0; // holster.cfg files are read again (they may have changed)
	memset(hlsPlaces, 0, sizeof hlsPlaces);
}
