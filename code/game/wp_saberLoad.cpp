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

//wp_saberLoad.cpp

#include "../qcommon/q_shared.h"
#include "g_local.h"
#include "wp_saber.h"
#include "../cgame/cg_local.h"

extern qboolean G_ParseLiteral(const char** data, const char* string);
extern saber_colors_t TranslateSaberColor(const char* name);
extern qboolean PM_SaberInStart(int move);
extern qboolean PM_SaberInTransition(int move);
extern qboolean PM_SaberInAttack(int move);

extern stringID_table_t FPTable[];

constexpr auto MAX_SABER_DATA_SIZE = 1024 * 1024 * 16; // 16mb, was 512kb;
char SaberParms[MAX_SABER_DATA_SIZE];

static void Saber_SithSwordPrecache()
{
	//*SIGH* special sounds used by the sith sword
	int i;
	for (i = 1; i < 5; i++)
	{
		G_SoundIndex(va("sound/weapons/sword/stab%d.wav", i));
	}
	for (i = 1; i < 5; i++)
	{
		G_SoundIndex(va("sound/weapons/sword/swing%d.wav", i));
	}
	for (i = 1; i < 7; i++)
	{
		G_SoundIndex(va("sound/weapons/sword/fall%d.wav", i));
	}
	/*
	for ( i = 1; i < 4; i++ )
	{
		G_SoundIndex( va( "sound/weapons/sword/spin%d.wav", i ) );
	}
	*/
}

stringID_table_t SaberTable[] =
{
	ENUM2STRING(SABER_NONE),
	ENUM2STRING(SABER_SINGLE),
	ENUM2STRING(SABER_SINGLE_CLASSIC),
	ENUM2STRING(SABER_STAFF),
	ENUM2STRING(SABER_BROAD),
	ENUM2STRING(SABER_PRONG),
	ENUM2STRING(SABER_DAGGER),
	ENUM2STRING(SABER_ARC),
	ENUM2STRING(SABER_SAI),
	ENUM2STRING(SABER_CLAW),
	ENUM2STRING(SABER_LANCE),
	ENUM2STRING(SABER_STAR),
	ENUM2STRING(SABER_TRIDENT),
	ENUM2STRING(SABER_SITH_SWORD),
	ENUM2STRING(SABER_UNSTABLE),
	ENUM2STRING(SABER_STAFF_UNSTABLE),
	ENUM2STRING(SABER_THIN),
	ENUM2STRING(SABER_STAFF_THIN),
	ENUM2STRING(SABER_SFX),
	ENUM2STRING(SABER_STAFF_SFX),
	ENUM2STRING(SABER_CUSTOMSFX),
	ENUM2STRING(SABER_BACKHAND),
	ENUM2STRING(SABER_YODA),
	ENUM2STRING(SABER_DOOKU),
	ENUM2STRING(SABER_PALP),
	ENUM2STRING(SABER_ANAKIN),
	ENUM2STRING(SABER_GRIE),
	ENUM2STRING(SABER_GRIE4),
	ENUM2STRING(SABER_OBIWAN),
	ENUM2STRING(SABER_ASBACKHAND),
	ENUM2STRING(SABER_STAFF_MAUL),
	ENUM2STRING(SABER_ELECTROSTAFF),
	ENUM2STRING(SABER_WINDU),
	ENUM2STRING(SABER_VADER),
	ENUM2STRING(SABER_KENOBI),
	ENUM2STRING(SABER_REY),
	{"", -1}
};

stringID_table_t saber_moveTable[] =
{
	ENUM2STRING(LS_INVALID),
	ENUM2STRING(LS_NONE),

	// General movements with saber
	ENUM2STRING(LS_READY),
	ENUM2STRING(LS_DRAW),
	ENUM2STRING(LS_PUTAWAY),
	// Attacks
	ENUM2STRING(LS_A_TL2BR),
	ENUM2STRING(LS_A_L2R),
	ENUM2STRING(LS_A_BL2TR),
	ENUM2STRING(LS_A_BR2TL),
	ENUM2STRING(LS_A_R2L),
	ENUM2STRING(LS_A_TR2BL),
	ENUM2STRING(LS_A_T2B),
	ENUM2STRING(LS_A_BACKSTAB),
	ENUM2STRING(LS_A_BACK),
	ENUM2STRING(LS_A_BACK_CR),
	ENUM2STRING(LS_ROLL_STAB),
	ENUM2STRING(LS_A_LUNGE),
	ENUM2STRING(LS_A_JUMP_T__B_),
	ENUM2STRING(LS_A_FLIP_STAB),
	ENUM2STRING(LS_A_FLIP_SLASH),
	ENUM2STRING(LS_JUMPATTACK_DUAL),
	ENUM2STRING(LS_JUMPATTACK_ARIAL_LEFT),
	ENUM2STRING(LS_JUMPATTACK_ARIAL_RIGHT),
	ENUM2STRING(LS_JUMPATTACK_CART_LEFT),
	ENUM2STRING(LS_JUMPATTACK_CART_RIGHT),
	ENUM2STRING(LS_JUMPATTACK_STAFF_LEFT),
	ENUM2STRING(LS_JUMPATTACK_STAFF_RIGHT),
	ENUM2STRING(LS_BUTTERFLY_LEFT),
	ENUM2STRING(LS_BUTTERFLY_RIGHT),
	ENUM2STRING(LS_A_BACKFLIP_ATK),
	ENUM2STRING(LS_SPINATTACK_DUAL),
	ENUM2STRING(LS_SPINATTACK),
	ENUM2STRING(LS_LEAP_ATTACK),
	ENUM2STRING(LS_SWOOP_ATTACK_RIGHT),
	ENUM2STRING(LS_SWOOP_ATTACK_LEFT),
	ENUM2STRING(LS_TAUNTAUN_ATTACK_RIGHT),
	ENUM2STRING(LS_TAUNTAUN_ATTACK_LEFT),
	ENUM2STRING(LS_KICK_F),
	ENUM2STRING(LS_KICK_F_MD),
	ENUM2STRING(LS_KICK_B),
	ENUM2STRING(LS_KICK_SWEEP),
	ENUM2STRING(LS_KICK_R),
	ENUM2STRING(LS_SMACK_R),
	ENUM2STRING(LS_KICK_L),
	ENUM2STRING(LS_SMACK_L),
	ENUM2STRING(LS_KICK_S),
	ENUM2STRING(LS_KICK_BF),
	ENUM2STRING(LS_KICK_RL),
	ENUM2STRING(LS_KICK_F_AIR),
	ENUM2STRING(LS_FLYING_KICK),
	ENUM2STRING(LS_KICK_B_AIR),
	ENUM2STRING(LS_KICK_R_AIR),
	ENUM2STRING(LS_KICK_L_AIR),
	ENUM2STRING(LS_STABDOWN),
	ENUM2STRING(LS_STABDOWN_STAFF),
	ENUM2STRING(LS_STABDOWN_DUAL),
	ENUM2STRING(LS_DUAL_SPIN_PROTECT),
	ENUM2STRING(LS_STAFF_SOULCAL),
	ENUM2STRING(LS_A1_SPECIAL),
	ENUM2STRING(LS_A2_SPECIAL),
	ENUM2STRING(LS_A3_SPECIAL),
	ENUM2STRING(LS_UPSIDE_DOWN_ATTACK),
	ENUM2STRING(LS_PULL_ATTACK_STAB),
	ENUM2STRING(LS_GRAPPLE_FIRE),
	ENUM2STRING(LS_PULL_ATTACK_SWING),
	ENUM2STRING(LS_SPINATTACK_ALORA),
	ENUM2STRING(LS_DUAL_FB),
	ENUM2STRING(LS_DUAL_LR),
	ENUM2STRING(LS_HILT_BASH),
	ENUM2STRING(LS_SLAP_R),
	ENUM2STRING(LS_SLAP_L),

	//starts
	ENUM2STRING(LS_S_TL2BR),
	ENUM2STRING(LS_S_L2R),
	ENUM2STRING(LS_S_BL2TR),
	ENUM2STRING(LS_S_BR2TL),
	ENUM2STRING(LS_S_R2L),
	ENUM2STRING(LS_S_TR2BL),
	ENUM2STRING(LS_S_T2B),

	//returns
	ENUM2STRING(LS_R_TL2BR),
	ENUM2STRING(LS_R_L2R),
	ENUM2STRING(LS_R_BL2TR),
	ENUM2STRING(LS_R_BR2TL),
	ENUM2STRING(LS_R_R2L),
	ENUM2STRING(LS_R_TR2BL),
	ENUM2STRING(LS_R_T2B),

	//transitions
	ENUM2STRING(LS_T1_BR__R), //40
	ENUM2STRING(LS_T1_BR_TR),
	ENUM2STRING(LS_T1_BR_T_),
	ENUM2STRING(LS_T1_BR_TL),
	ENUM2STRING(LS_T1_BR__L),
	ENUM2STRING(LS_T1_BR_BL),
	ENUM2STRING(LS_T1__R_BR), //46
	ENUM2STRING(LS_T1__R_TR),
	ENUM2STRING(LS_T1__R_T_),
	ENUM2STRING(LS_T1__R_TL),
	ENUM2STRING(LS_T1__R__L),
	ENUM2STRING(LS_T1__R_BL),
	ENUM2STRING(LS_T1_TR_BR), //52
	ENUM2STRING(LS_T1_TR__R),
	ENUM2STRING(LS_T1_TR_T_),
	ENUM2STRING(LS_T1_TR_TL),
	ENUM2STRING(LS_T1_TR__L),
	ENUM2STRING(LS_T1_TR_BL),
	ENUM2STRING(LS_T1_T__BR), //58
	ENUM2STRING(LS_T1_T___R),
	ENUM2STRING(LS_T1_T__TR),
	ENUM2STRING(LS_T1_T__TL),
	ENUM2STRING(LS_T1_T___L),
	ENUM2STRING(LS_T1_T__BL),
	ENUM2STRING(LS_T1_TL_BR), //64
	ENUM2STRING(LS_T1_TL__R),
	ENUM2STRING(LS_T1_TL_TR),
	ENUM2STRING(LS_T1_TL_T_),
	ENUM2STRING(LS_T1_TL__L),
	ENUM2STRING(LS_T1_TL_BL),
	ENUM2STRING(LS_T1__L_BR), //70
	ENUM2STRING(LS_T1__L__R),
	ENUM2STRING(LS_T1__L_TR),
	ENUM2STRING(LS_T1__L_T_),
	ENUM2STRING(LS_T1__L_TL),
	ENUM2STRING(LS_T1__L_BL),
	ENUM2STRING(LS_T1_BL_BR), //76
	ENUM2STRING(LS_T1_BL__R),
	ENUM2STRING(LS_T1_BL_TR),
	ENUM2STRING(LS_T1_BL_T_),
	ENUM2STRING(LS_T1_BL_TL),
	ENUM2STRING(LS_T1_BL__L),

	//Bounces
	ENUM2STRING(LS_B1_BR),
	ENUM2STRING(LS_B1__R),
	ENUM2STRING(LS_B1_TR),
	ENUM2STRING(LS_B1_T_),
	ENUM2STRING(LS_B1_TL),
	ENUM2STRING(LS_B1__L),
	ENUM2STRING(LS_B1_BL),

	//Deflected attacks
	ENUM2STRING(LS_D1_BR),
	ENUM2STRING(LS_D1__R),
	ENUM2STRING(LS_D1_TR),
	ENUM2STRING(LS_D1_T_),
	ENUM2STRING(LS_D1_TL),
	ENUM2STRING(LS_D1__L),
	ENUM2STRING(LS_D1_BL),
	ENUM2STRING(LS_D1_B_),

	//Reflected attacks
	ENUM2STRING(LS_V1_BR),
	ENUM2STRING(LS_V1__R),
	ENUM2STRING(LS_V1_TR),
	ENUM2STRING(LS_V1_T_),
	ENUM2STRING(LS_V1_TL),
	ENUM2STRING(LS_V1__L),
	ENUM2STRING(LS_V1_BL),
	ENUM2STRING(LS_V1_B_),

	// Broken parries
	ENUM2STRING(LS_H1_T_), //
	ENUM2STRING(LS_H1_TR),
	ENUM2STRING(LS_H1_TL),
	ENUM2STRING(LS_H1_BR),
	ENUM2STRING(LS_H1_B_),
	ENUM2STRING(LS_H1_BL),

	// Knockaways
	ENUM2STRING(LS_K1_T_), //
	ENUM2STRING(LS_K1_TR),
	ENUM2STRING(LS_K1_TR_MD),
	ENUM2STRING(LS_K1_TR_PB),
	ENUM2STRING(LS_K1_TL),
	ENUM2STRING(LS_K1_TL_MD),
	ENUM2STRING(LS_K1_TL_PB),
	ENUM2STRING(LS_K1_BR),
	ENUM2STRING(LS_K1_BL),

	// Parries
	ENUM2STRING(LS_PARRY_UP), //
	ENUM2STRING(LS_PARRY_UR),
	ENUM2STRING(LS_PARRY_UL),
	ENUM2STRING(LS_PARRY_LR),
	ENUM2STRING(LS_PARRY_LL),
	ENUM2STRING(LS_PARRY_B),

	// Projectile Reflections
	ENUM2STRING(LS_REFLECT_UP), //
	ENUM2STRING(LS_REFLECT_UR),
	ENUM2STRING(LS_REFLECT_UL),
	ENUM2STRING(LS_REFLECT_LR),
	ENUM2STRING(LS_REFLECT_LL),
	ENUM2STRING(LS_REFLECT_B),

	{"", -1}
};

stringID_table_t HolsterTable[] =
{
	ENUM2STRING(HOLSTER_NONE),
	ENUM2STRING(HOLSTER_RHIP),
	ENUM2STRING(HOLSTER_RHIPF),
	ENUM2STRING(HOLSTER_RHIPB),
	ENUM2STRING(HOLSTER_LHIP),
	ENUM2STRING(HOLSTER_LHIPF),
	ENUM2STRING(HOLSTER_LHIPB),
	ENUM2STRING(HOLSTER_BACKU),
	ENUM2STRING(HOLSTER_BACKL),
	ENUM2STRING(HOLSTER_LCHESTL),
	ENUM2STRING(HOLSTER_RCHESTL),
	ENUM2STRING(HOLSTER_LCHESTU),
	ENUM2STRING(HOLSTER_RCHESTU),
	ENUM2STRING(HOLSTER_BACK),

	{"none", HOLSTER_NONE},
	{"rhip", HOLSTER_RHIP},
	{"rhipf", HOLSTER_RHIPF},
	{"rhipb", HOLSTER_RHIPB},
	{"lhip", HOLSTER_LHIP},
	{"lhipf", HOLSTER_LHIPF},
	{"lhipb", HOLSTER_LHIPB},
	{"backu", HOLSTER_BACKU},
	{"backl", HOLSTER_BACKL},
	{"lchestl", HOLSTER_LCHESTL},
	{"rchestl", HOLSTER_RCHESTL},
	{"lchestu", HOLSTER_LCHESTU},
	{"rchestu", HOLSTER_RCHESTU},
	{"back", HOLSTER_BACK},
	{"", -1}
};

static saber_styles_t TranslateSaberStyle(const char* name)
{
	if (!Q_stricmp(name, "fast")) return SS_FAST;
	if (!Q_stricmp(name, "medium")) return SS_MEDIUM;
	if (!Q_stricmp(name, "strong")) return SS_STRONG;
	if (!Q_stricmp(name, "desann")) return SS_DESANN;
	if (!Q_stricmp(name, "tavion")) return SS_TAVION;
	if (!Q_stricmp(name, "dual")) return SS_DUAL;
	if (!Q_stricmp(name, "staff")) return SS_STAFF;

	return SS_NONE;
}

void WP_SaberFreeStrings(saberInfo_t& saber)
{
	if (saber.name && gi.bIsFromZone(saber.name, TAG_G_ALLOC))
	{
		gi.Free(saber.name);
		saber.name = nullptr;
	}
	if (saber.fullName && gi.bIsFromZone(saber.fullName, TAG_G_ALLOC))
	{
		gi.Free(saber.fullName);
		saber.fullName = nullptr;
	}
	if (saber.model && gi.bIsFromZone(saber.model, TAG_G_ALLOC))
	{
		gi.Free(saber.model);
		saber.model = nullptr;
	}
	if (saber.skin && gi.bIsFromZone(saber.skin, TAG_G_ALLOC))
	{
		gi.Free(saber.skin);
		saber.skin = nullptr;
	}
	if (saber.brokenSaber1 && gi.bIsFromZone(saber.brokenSaber1, TAG_G_ALLOC))
	{
		gi.Free(saber.brokenSaber1);
		saber.brokenSaber1 = nullptr;
	}
	if (saber.brokenSaber2 && gi.bIsFromZone(saber.brokenSaber2, TAG_G_ALLOC))
	{
		gi.Free(saber.brokenSaber2);
		saber.brokenSaber2 = nullptr;
	}
}

qboolean WP_SaberBladeUseSecondBladeStyle(const saberInfo_t* saber, const int blade_num)
{
	if (saber
		&& saber->bladeStyle2Start > 0
		&& blade_num >= saber->bladeStyle2Start)
		return qtrue;

	return qfalse;
}

qboolean WP_SaberBladeDoTransitionDamage(const saberInfo_t* saber, const int blade_num)
{
	//use first blade style for this blade
	if (!WP_SaberBladeUseSecondBladeStyle(saber, blade_num) && saber->saberFlags2 & SFL2_TRANSITION_DAMAGE)
		return qtrue;

	//use second blade style for this blade
	if (WP_SaberBladeUseSecondBladeStyle(saber, blade_num) && saber->saberFlags2 & SFL2_TRANSITION_DAMAGE2)
		return qtrue;

	return qfalse;
}

qboolean WP_UseFirstValidSaberStyle(const gentity_t* ent, int* saber_anim_level)
{
	if (ent && ent->client)
	{
		qboolean style_invalid = qfalse;
		int valid_styles = 0, style_num;

		//initially, all styles are valid
		for (style_num = SS_NONE + 1; style_num < SS_NUM_SABER_STYLES; style_num++)
		{
			valid_styles |= 1 << style_num;
		}

		if (ent->client->ps.saber[0].Active()
			&& ent->client->ps.saber[0].stylesForbidden)
		{
			if (ent->client->ps.saber[0].stylesForbidden & 1 << *saber_anim_level)
			{
				//not a valid style for first saber!
				style_invalid = qtrue;
				valid_styles &= ~ent->client->ps.saber[0].stylesForbidden;
			}
		}
		if (ent->client->ps.dualSabers)
		{
			//check second saber, too
			if (ent->client->ps.saber[1].Active()
				&& ent->client->ps.saber[1].stylesForbidden)
			{
				if (ent->client->ps.saber[1].stylesForbidden & 1 << *saber_anim_level)
				{
					//not a valid style for second saber!
					style_invalid = qtrue;
					//only the ones both sabers allow is valid
					valid_styles &= ~ent->client->ps.saber[1].stylesForbidden;
				}
			}
			else
			{
				//can't use dual style if not using 2 sabers
				valid_styles &= ~(1 << SS_DUAL);
			}
		}
		else
		{
			//can't use dual style if not using 2 sabers
			valid_styles &= ~(1 << SS_DUAL);
			if (*saber_anim_level == SS_DUAL) // saber style switch
			{
				style_invalid = qtrue;
			}
		}
		if (style_invalid && valid_styles)
		{
			//using an invalid style and have at least one valid style to use, so switch to it
			for (style_num = SS_NONE + 1; style_num < SS_NUM_SABER_STYLES; style_num++)
			{
				if (valid_styles & 1 << style_num)
				{
					*saber_anim_level = style_num;
					return qtrue;
				}
			}
		}
	}
	return qfalse;
}

qboolean WP_SaberStyleValidForSaber(const gentity_t* ent, const int saber_anim_level)
{
	if (ent && ent->client)
	{
		if (ent->client->ps.saber[0].Active()
			&& ent->client->ps.saber[0].stylesForbidden)
		{
			if (ent->client->ps.saber[0].stylesForbidden & 1 << saber_anim_level)
			{
				//not a valid style for first saber!
				return qfalse;
			}
		}
		if (ent->client->ps.dualSabers)
		{
			//check second saber, too
			if (ent->client->ps.saber[1].Active())
			{
				if (ent->client->ps.saber[1].stylesForbidden)
				{
					if (ent->client->ps.saber[1].stylesForbidden & 1 << saber_anim_level)
					{
						//not a valid style for second saber!
						return qfalse;
					}
				}

				//now: if using dual sabers, only dual and tavion (if given with this saber) are allowed
				if (saber_anim_level != SS_DUAL)
				{
					//dual is okay
					if (saber_anim_level != SS_TAVION)
					{
						//tavion might be okay, all others are not
						return qfalse;
					}
					//see if "tavion" style is okay
					if (ent->client->ps.saber[0].Active()
						&& ent->client->ps.saber[0].stylesLearned & 1 << SS_TAVION)
					{
						//okay to use tavion style, first saber gave it to us
					}
					else if (ent->client->ps.saber[1].stylesLearned & 1 << SS_TAVION)
					{
						//okay to use tavion style, second saber gave it to us
					}
					else
					{
						//tavion style is not allowed because neither of the sabers we're using gave it to us (I know, doesn't quite make sense, but...)
						return qfalse;
					}
				}
			}
			else if (saber_anim_level == SS_DUAL)
			{
				//can't use dual style if not using dualSabers
				return qfalse;
			}
		}
		else if (saber_anim_level == SS_DUAL)
		{
			//can't use dual style if not using dualSabers
			return qfalse;
		}
	}
	return qtrue;
}

qboolean WP_SaberCanTurnOffSomeBlades(const saberInfo_t* saber)
{
	if (saber->bladeStyle2Start > 0
		&& saber->numBlades > saber->bladeStyle2Start)
	{
		if (saber->saberFlags2 & SFL2_NO_MANUAL_DEACTIVATE
			&& saber->saberFlags2 & SFL2_NO_MANUAL_DEACTIVATE2)
		{
			//all blades are always on
			return qfalse;
		}
	}
	else
	{
		if (saber->saberFlags2 & SFL2_NO_MANUAL_DEACTIVATE)
		{
			//all blades are always on
			return qfalse;
		}
	}
	//you can turn some off
	return qtrue;
}

void WP_SaberSetDefaults(saberInfo_t* saber, const qboolean set_colors = qtrue)
{
	//Set defaults so that, if it fails, there's at least something there
	saber->name = DEFAULT_SABER;
	saber->fullName = DEFAULT_SABER_NAME;
	for (auto& i : saber->blade)
	{
		if (set_colors)
		{
			i.color = static_cast<saber_colors_t>(Q_irand(SABER_ORANGE, SABER_PURPLE));
		}
		i.radius = SABER_RADIUS_STANDARD;
		i.lengthMax = 32;
	}
	saber->model = DEFAULT_SABER_MODEL;
	saber->skin = nullptr;
	saber->soundOn = G_SoundIndex("sound/weapons/saber/enemy_saber_on.wav");
	saber->soundLoop = G_SoundIndex("sound/weapons/saber/saberhum3.wav");
	saber->soundOff = G_SoundIndex("sound/weapons/saber/enemy_saber_off.wav");
	saber->numBlades = 1;
	saber->type = SABER_SINGLE;
	saber->stylesLearned = 0;
	saber->stylesForbidden = 0;
	saber->maxChain = 0; //0 = use default behavior
	saber->forceRestrictions = 0;
	saber->lockBonus = 0;
	saber->parryBonus = 0;
	saber->breakParryBonus = 0;
	saber->breakParryBonus2 = 0;
	saber->disarmBonus = 0;
	saber->disarmBonus2 = 0;
	saber->singleBladeStyle = SS_NONE;
	//makes it so that you use a different style if you only have the first blade active
	saber->brokenSaber1 = nullptr;
	//if saber is actually hit by another saber, it can be cut in half/broken and will be replaced with this saber in your right hand
	saber->brokenSaber2 = nullptr;
	//if saber is actually hit by another saber, it can be cut in half/broken and will be replaced with this saber in your left hand
	//===NEW========================================================================================
	//these values are global to the saber, like all of the ones above
	saber->saberFlags = 0; //see all the SFL_ flags
	saber->saberFlags2 = 0; //see all the SFL2_ flags
	saber->spinSound = 0; //none - if set, plays this sound as it spins when thrown
	saber->swingSound[0] = 0;
	//none - if set, plays one of these 3 sounds when swung during an attack - NOTE: must provide all 3!!!
	saber->swingSound[1] = 0;
	//none - if set, plays one of these 3 sounds when swung during an attack - NOTE: must provide all 3!!!
	saber->swingSound[2] = 0;
	//none - if set, plays one of these 3 sounds when swung during an attack - NOTE: must provide all 3!!!
	saber->fallSound[0] = 0;
	//none - if set, plays one of these 3 sounds when weapon falls to the ground - NOTE: must provide all 3!!!
	saber->fallSound[1] = 0;
	//none - if set, plays one of these 3 sounds when weapon falls to the ground - NOTE: must provide all 3!!!
	saber->fallSound[2] = 0;
	//none - if set, plays one of these 3 sounds when weapon falls to the ground - NOTE: must provide all 3!!!

	//done in game (server-side code)
	saber->moveSpeedScale = 1.0f; //1.0 - you move faster/slower when using this saber
	saber->animSpeedScale = 1.0f; //1.0 - plays normal attack animations faster/slower

	saber->kataMove = LS_INVALID;
	//LS_INVALID - if set, player will execute this move when they press both attack buttons at the same time
	saber->lungeAtkMove = LS_INVALID; //LS_INVALID - if set, player will execute this move when they crouch+fwd+attack
	saber->jumpAtkUpMove = LS_INVALID; //LS_INVALID - if set, player will execute this move when they jump+attack
	saber->jumpAtkFwdMove = LS_INVALID; //LS_INVALID - if set, player will execute this move when they jump+fwd+attack
	saber->jumpAtkBackMove = LS_INVALID; //LS_INVALID - if set, player will execute this move when they jump+back+attack
	saber->jumpAtkRightMove = LS_INVALID;
	//LS_INVALID - if set, player will execute this move when they jump+rightattack
	saber->jumpAtkLeftMove = LS_INVALID; //LS_INVALID - if set, player will execute this move when they jump+left+attack
	saber->readyAnim = -1; //-1 - anim to use when standing idle
	saber->drawAnim = -1; //-1 - anim to use when drawing weapon
	saber->putawayAnim = -1; //-1 - anim to use when putting weapon away
	saber->tauntAnim = -1; //-1 - anim to use when hit "taunt"
	saber->bowAnim = -1; //-1 - anim to use when hit "bow"
	saber->meditateAnim = -1; //-1 - anim to use when hit "meditate"
	saber->flourishAnim = -1; //-1 - anim to use when hit "flourish"
	saber->gloatAnim = -1; //-1 - anim to use when hit "gloat"
	saber->surrenderAnim = -1; //-1 - anim to use when hit "surrenderAnim"
	saber->combatstanceAnim = -1; //-1 - anim to use when hit "combatstanceAnim"

	//***NOTE: you can only have a maximum of 2 "styles" of blades, so this next value, "bladeStyle2Start" is the number of the first blade to use these value on... all blades before this use the normal values above, all blades at and after this number use the secondary values below***
	saber->bladeStyle2Start = 0;
	//0 - if set, blades from this number and higher use the following values (otherwise, they use the normal values already set)

	//***The following can be different for the extra blades - not setting them individually defaults them to the value for the whole saber (and first blade)***

	//===PRIMARY BLADES=====================
	//done in cgame (client-side code)
	saber->trailStyle = 0;
	//0 - default (0) is normal, 1 is a motion blur and 2 is no trail at all (good for real-sword type mods)
	saber->g2MarksShader[0] = 0;
	//none - if set, the game will use this shader for marks on enemies instead of the default "gfx/damage/saberglowmark"
	saber->g2WeaponMarkShader[0] = 0;
	//none - if set, the game will ry to project this shader onto the weapon when it damages a person (good for a blood splatter on the weapon)
	// Custom saber glow, blade & dlight color code
	saber->glowshader[0] = 0; //none - if set, overrides the shader used for the saber glow
	saber->bladeshader[0] = 0; //none - if set, overrides the shader used for the saber blade
	saber->saberDLightColor[0] = 1; //none - if set, overrides the default white color used for the saber dlight
	saber->saberDLightColor[1] = 1;
	saber->saberDLightColor[2] = 1;
	//saber->trailShader = 0;				//none - if set, overrides the shader used for the saber trail?
	saber->hitSound[0] = 0;
	//none - if set, plays one of these 3 sounds when saber hits a person - NOTE: must provide all 3!!!
	saber->hitSound[1] = 0;
	//none - if set, plays one of these 3 sounds when saber hits a person - NOTE: must provide all 3!!!
	saber->hitSound[2] = 0;
	//none - if set, plays one of these 3 sounds when saber hits a person - NOTE: must provide all 3!!!
	saber->blockSound[0] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits another saber/sword - NOTE: must provide all 3!!!
	saber->blockSound[1] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits another saber/sword - NOTE: must provide all 3!!!
	saber->blockSound[2] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits another saber/sword - NOTE: must provide all 3!!!
	saber->bounceSound[0] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits a wall and bounces off (must set bounceOnWall to 1 to use these sounds) - NOTE: must provide all 3!!!
	saber->bounceSound[1] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits a wall and bounces off (must set bounceOnWall to 1 to use these sounds) - NOTE: must provide all 3!!!
	saber->bounceSound[2] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits a wall and bounces off (must set bounceOnWall to 1 to use these sounds) - NOTE: must provide all 3!!!
	saber->blockEffect = 0;
	//none - if set, plays this effect when the saber/sword hits another saber/sword (instead of "saber/saber_block.efx")
	saber->hitPersonEffect = 0;
	//none - if set, plays this effect when the saber/sword hits a person (instead of "saber/blood_sparks_mp.efx")
	saber->hitOtherEffect = 0;
	//none - if set, plays this effect when the saber/sword hits something else damagable (instead of "saber/saber_cut.efx")
	saber->bladeEffect = 0; //none - if set, plays this effect at the blade tag

	//done in game (server-side code)
	saber->knockbackScale = 0; //0 - if non-zero, uses damage done to calculate an appropriate amount of knockback
	saber->damageScale = 1.0f; //1 - scale up or down the damage done by the saber
	saber->splashRadius = 0.0f; //0 - radius of splashDamage
	saber->splashDamage = 0; //0 - amount of splashDamage, 100% at a distance of 0, 0% at a distance = splashRadius
	saber->splashKnockback = 0.0f;
	//0 - amount of splashKnockback, 100% at a distance of 0, 0% at a distance = splashRadius

	//===SECONDARY BLADES===================
	//done in cgame (client-side code)
	saber->trailStyle2 = 0;
	//0 - default (0) is normal, 1 is a motion blur and 2 is no trail at all (good for real-sword type mods)
	saber->g2MarksShader2[0] = 0;
	//none - if set, the game will use this shader for marks on enemies instead of the default "gfx/damage/saberglowmark"
	saber->g2WeaponMarkShader2[0] = 0;
	//none - if set, the game will ry to project this shader onto the weapon when it damages a person (good for a blood splatter on the weapon)
	saber->hit2Sound[0] = 0;
	//none - if set, plays one of these 3 sounds when saber hits a person - NOTE: must provide all 3!!!
	saber->hit2Sound[1] = 0;
	//none - if set, plays one of these 3 sounds when saber hits a person - NOTE: must provide all 3!!!
	saber->hit2Sound[2] = 0;
	//none - if set, plays one of these 3 sounds when saber hits a person - NOTE: must provide all 3!!!
	saber->block2Sound[0] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits another saber/sword - NOTE: must provide all 3!!!
	saber->block2Sound[1] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits another saber/sword - NOTE: must provide all 3!!!
	saber->block2Sound[2] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits another saber/sword - NOTE: must provide all 3!!!
	saber->bounce2Sound[0] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits a wall and bounces off (must set bounceOnWall to 1 to use these sounds) - NOTE: must provide all 3!!!
	saber->bounce2Sound[1] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits a wall and bounces off (must set bounceOnWall to 1 to use these sounds) - NOTE: must provide all 3!!!
	saber->bounce2Sound[2] = 0;
	//none - if set, plays one of these 3 sounds when saber/sword hits a wall and bounces off (must set bounceOnWall to 1 to use these sounds) - NOTE: must provide all 3!!!
	saber->blockEffect2 = 0;
	//none - if set, plays this effect when the saber/sword hits another saber/sword (instead of "saber/saber_block.efx")
	saber->hitPersonEffect2 = 0;
	//none - if set, plays this effect when the saber/sword hits a person (instead of "saber/blood_sparks_mp.efx")
	saber->hitOtherEffect2 = 0;
	//none - if set, plays this effect when the saber/sword hits something else damagable (instead of "saber/saber_cut.efx")
	saber->bladeEffect2 = 0; //none - if set, plays this effect at the blade tag

	//done in game (server-side code)
	saber->knockbackScale2 = 0; //0 - if non-zero, uses damage done to calculate an appropriate amount of knockback
	saber->damageScale2 = 1.0f; //1 - scale up or down the damage done by the saber
	saber->splashRadius2 = 0.0f; //0 - radius of splashDamage
	saber->splashDamage2 = 0; //0 - amount of splashDamage, 100% at a distance of 0, 0% at a distance = splashRadius
	saber->splashKnockback2 = 0.0f;
	//0 - amount of splashKnockback, 100% at a distance of 0, 0% at a distance = splashRadius
	//=========================================================================================================================================
	saber->BPregenRate = 300;
	saber->holsterPlace = HOLSTER_INVALID;
}

qboolean forcedRGBColours[MAX_BLADES];

static void Saber_ParseName(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->fullName = G_NewString(value);
}

static void Saber_ParseSaberType(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	int saberType = GetIDForString(SaberTable, value);
	if (saberType >= SABER_SINGLE && saberType < NUM_SABERS)
		saber->type = static_cast<saberType_t>(saberType);
}

static void Saber_ParseSaberModel(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->model = G_NewString(value);
}

static void Saber_ParseCustomSkin(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->skin = G_NewString(value);
}

static void Saber_ParseSoundOn(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->soundOn = G_SoundIndex(value);
}

static void Saber_ParseSoundLoop(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->soundLoop = G_SoundIndex(value);
}

static void Saber_ParseSoundOff(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->soundOff = G_SoundIndex(value);
}

static void Saber_ParseNumBlades(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n < 1 || n > MAX_BLADES)
	{
		Com_Error(ERR_DROP, "WP_SaberParseParms: saber %s has illegal number of blades (%d) max: %d", saber->name, n,
			MAX_BLADES);
	}
	saber->numBlades = n;
}

qboolean Saber_SetColor = qtrue;

static void Saber_ParseSaberColor(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	for (int i = 0; i < MAX_BLADES; i++)
	{
		if (!forcedRGBColours[i])
		{
			saber->blade[i].color = color;
		}
	}
}

static void Saber_ParseSaberColor2(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	if (!forcedRGBColours[1])
	{
		saber->blade[1].color = color;
	}
}

static void Saber_ParseSaberColor3(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	if (!forcedRGBColours[2])
	{
		saber->blade[2].color = color;
	}
}

static void Saber_ParseSaberColor4(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	if (!forcedRGBColours[3])
	{
		saber->blade[3].color = color;
	}
}

static void Saber_ParseSaberColor5(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	if (!forcedRGBColours[4])
	{
		saber->blade[4].color = color;
	}
}

static void Saber_ParseSaberColor6(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	if (!forcedRGBColours[5])
	{
		saber->blade[5].color = color;
	}
}

static void Saber_ParseSaberColor7(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	if (!forcedRGBColours[6])
	{
		saber->blade[6].color = color;
	}
}

static void Saber_ParseSaberRGBColor(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	for (auto& i : saber->blade)
	{
		i.color = color;
	}
	forcedRGBColours[0] = qtrue;
}

static void Saber_ParseSaberRGBColor2(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	saber->blade[1].color = color;
	forcedRGBColours[1] = qtrue;
}

static void Saber_ParseSaberRGBColor3(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	saber->blade[2].color = color;
	forcedRGBColours[2] = qtrue;
}

static void Saber_ParseSaberRGBColor4(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	saber->blade[3].color = color;
	forcedRGBColours[3] = qtrue;
}

static void Saber_ParseSaberRGBColor5(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	saber->blade[4].color = color;
	forcedRGBColours[4] = qtrue;
}

static void Saber_ParseSaberRGBColor6(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	saber->blade[5].color = color;
	forcedRGBColours[5] = qtrue;
}

static void Saber_ParseSaberRGBColor7(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	// don't actually want to set the colors
	// read the color out anyway just to advance the *p pointer
	if (!Saber_SetColor)
		return;

	const saber_colors_t color = TranslateSaberColor(value);
	saber->blade[6].color = color;
	forcedRGBColours[6] = qtrue;
}

static void Saber_ParseSaberLength(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 4.0f)
		f = 4.0f;

	for (auto& i : saber->blade)
		i.lengthMax = f;
}

static void Saber_ParseSaberLength2(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 4.0f)
		f = 4.0f;

	saber->blade[1].lengthMax = f;
}

static void Saber_ParseSaberLength3(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 4.0f)
		f = 4.0f;

	saber->blade[2].lengthMax = f;
}

static void Saber_ParseSaberLength4(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 4.0f)
		f = 4.0f;

	saber->blade[3].lengthMax = f;
}

static void Saber_ParseSaberLength5(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 4.0f)
		f = 4.0f;

	saber->blade[4].lengthMax = f;
}

static void Saber_ParseSaberLength6(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 4.0f)
		f = 4.0f;

	saber->blade[5].lengthMax = f;
}

static void Saber_ParseSaberLength7(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 4.0f)
		f = 4.0f;

	saber->blade[6].lengthMax = f;
}

static void Saber_ParseSaberRadius(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 0.25f)
		f = 0.25f;

	for (auto& i : saber->blade)
		i.radius = f;
}

static void Saber_ParseSaberRadius2(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 0.25f)
		f = 0.25f;

	saber->blade[1].radius = f;
}

static void Saber_ParseSaberRadius3(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 0.25f)
		f = 0.25f;

	saber->blade[2].radius = f;
}

static void Saber_ParseSaberRadius4(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 0.25f)
		f = 0.25f;

	saber->blade[3].radius = f;
}

static void Saber_ParseSaberRadius5(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 0.25f)
		f = 0.25f;

	saber->blade[4].radius = f;
}

static void Saber_ParseSaberRadius6(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 0.25f)
		f = 0.25f;

	saber->blade[5].radius = f;
}

static void Saber_ParseSaberRadius7(saberInfo_t* saber, const char** p)
{
	float f;

	if (COM_ParseFloat(p, &f))
		return;

	if (f < 0.25f)
		f = 0.25f;

	saber->blade[6].radius = f;
}

static void Saber_ParseSaberStyle(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	//OLD WAY: only allowed ONE style
	const int style = TranslateSaberStyle(value);
	//learn only this style
	saber->stylesLearned = 1 << style;
	//forbid all other styles
	saber->stylesForbidden = 0;
	for (int styleNum = SS_NONE + 1; styleNum < SS_NUM_SABER_STYLES; styleNum++)
	{
		if (styleNum != style)
			saber->stylesForbidden |= 1 << styleNum;
	}
}

static void Saber_ParseSaberStyleLearned(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->stylesLearned |= 1 << TranslateSaberStyle(value);
}

static void Saber_ParseSaberStyleForbidden(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->stylesForbidden |= 1 << TranslateSaberStyle(value);
}

static void Saber_ParseMaxChain(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->maxChain = n;
}

static void Saber_ParseLockable(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n == 0)
		saber->saberFlags |= SFL_NOT_LOCKABLE;
}

static void Saber_ParseThrowable(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n == 0)
		saber->saberFlags |= SFL_NOT_THROWABLE;
}

static void Saber_ParseDisarmable(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n == 0)
		saber->saberFlags |= SFL_NOT_DISARMABLE;
}

static void Saber_ParseBlocking(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n == 0)
		saber->saberFlags |= SFL_NOT_ACTIVE_BLOCKING;
}

static void Saber_ParseTwoHanded(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_TWO_HANDED;
}

static void Saber_ParseForceRestrict(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
		return;

	const int fp = GetIDForString(FPTable, value);
	if (fp >= FP_FIRST && fp < NUM_FORCE_POWERS)
		saber->forceRestrictions |= 1 << fp;
}

static void Saber_ParseLockBonus(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->lockBonus = n;
}

static void Saber_ParseParryBonus(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->parryBonus = n;
}

static void Saber_ParseBreakParryBonus(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->breakParryBonus = n;
}

static void Saber_ParseBreakParryBonus2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->breakParryBonus2 = n;
}

static void Saber_ParseDisarmBonus(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->disarmBonus = n;
}

static void Saber_ParseDisarmBonus2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->disarmBonus2 = n;
}

static void Saber_ParseSingleBladeStyle(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->singleBladeStyle = TranslateSaberStyle(value);
}

static void Saber_ParseSingleBladeThrowable(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_SINGLE_BLADE_THROWABLE;
}

static void Saber_ParseBrokenSaber1(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->brokenSaber1 = G_NewString(value);
}

static void Saber_ParseBrokenSaber2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->brokenSaber2 = G_NewString(value);
}

static void Saber_ParseReturnDamage(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_RETURN_DAMAGE;
}

static void Saber_ParseSpinSound(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->spinSound = G_SoundIndex(value);
}

static void Saber_ParseSwingSound1(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->swingSound[0] = G_SoundIndex(value);
}

static void Saber_ParseSwingSound2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->swingSound[1] = G_SoundIndex(value);
}

static void Saber_ParseSwingSound3(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->swingSound[2] = G_SoundIndex(value);
}

static void Saber_ParseFallSound1(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->fallSound[0] = G_SoundIndex(value);
}

static void Saber_ParseFallSound2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->fallSound[1] = G_SoundIndex(value);
}

static void Saber_ParseFallSound3(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->fallSound[2] = G_SoundIndex(value);
}

static void Saber_ParseMoveSpeedScale(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->moveSpeedScale = f;
}

static void Saber_ParseAnimSpeedScale(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->animSpeedScale = f;
}

static void Saber_ParseOnInWater(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_ON_IN_WATER;
}

static void Saber_ParseBounceOnWalls(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_BOUNCE_ON_WALLS;
}

static void Saber_ParseBoltToWrist(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_BOLT_TO_WRIST;
}

static void Saber_ParseKataMove(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int saber_move = GetIDForString(saber_moveTable, value);
	if (saber_move >= LS_INVALID && saber_move < LS_MOVE_MAX)
		saber->kataMove = saber_move;
	//LS_INVALID - if set, player will execute this move when they press both attack buttons at the same time
}

static void Saber_ParseLungeAtkMove(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int saber_move = GetIDForString(saber_moveTable, value);
	if (saber_move >= LS_INVALID && saber_move < LS_MOVE_MAX)
		saber->lungeAtkMove = saber_move;
}

static void Saber_ParseJumpAtkUpMove(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int saber_move = GetIDForString(saber_moveTable, value);
	if (saber_move >= LS_INVALID && saber_move < LS_MOVE_MAX)
		saber->jumpAtkUpMove = saber_move;
}

static void Saber_ParseJumpAtkFwdMove(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int saber_move = GetIDForString(saber_moveTable, value);
	if (saber_move >= LS_INVALID && saber_move < LS_MOVE_MAX)
		saber->jumpAtkFwdMove = saber_move;
}

static void Saber_ParseJumpAtkBackMove(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int saber_move = GetIDForString(saber_moveTable, value);
	if (saber_move >= LS_INVALID && saber_move < LS_MOVE_MAX)
		saber->jumpAtkBackMove = saber_move;
}

static void Saber_ParseJumpAtkRightMove(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int saber_move = GetIDForString(saber_moveTable, value);
	if (saber_move >= LS_INVALID && saber_move < LS_MOVE_MAX)
		saber->jumpAtkRightMove = saber_move;
}

static void Saber_ParseJumpAtkLeftMove(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int saber_move = GetIDForString(saber_moveTable, value);
	if (saber_move >= LS_INVALID && saber_move < LS_MOVE_MAX)
		saber->jumpAtkLeftMove = saber_move;
}

static void Saber_ParseReadyAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->readyAnim = anim;
}

static void Saber_ParseDrawAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->drawAnim = anim;
}

static void Saber_ParsePutawayAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->putawayAnim = anim;
}

static void Saber_ParseTauntAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->tauntAnim = anim;
}

static void Saber_ParseBowAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;

	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->bowAnim = anim;
}

static void Saber_ParseMeditateAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->meditateAnim = anim;
}

static void Saber_ParseFlourishAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->flourishAnim = anim;
}

static void Saber_ParseGloatAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->gloatAnim = anim;
}

static void Saber_ParseSurrenderAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->surrenderAnim = anim;
}

static void Saber_ParsecombatstanceAnim(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	const int anim = GetIDForString(anim_table, value);
	if (anim >= 0 && anim < MAX_ANIMATIONS)
		saber->combatstanceAnim = anim;
}

static void Saber_ParseNoRollStab(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_ROLL_STAB;
}

static void Saber_ParseNoPullAttack(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_PULL_ATTACK;
}

static void Saber_ParseNoBackAttack(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_BACK_ATTACK;
}

static void Saber_ParseNoStabDown(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_STABDOWN;
}

static void Saber_ParseNoWallRuns(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_WALL_RUNS;
}

static void Saber_ParseNoWallFlips(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_WALL_FLIPS;
}

static void Saber_ParseNoWallGrab(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_WALL_GRAB;
}

static void Saber_ParseNoRolls(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_ROLLS;
}

static void Saber_ParseNoFlips(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_FLIPS;
}

static void Saber_ParseNoCartwheels(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_CARTWHEELS;
}

static void Saber_ParseNoKicks(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_KICKS;
}

static void Saber_ParseNoMirrorAttacks(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags |= SFL_NO_MIRROR_ATTACKS;
}

static void Saber_ParseNotInMP(saberInfo_t* saber, const char** p)
{
	SkipRestOfLine(p);
}

static void Saber_ParseBladeStyle2Start(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->bladeStyle2Start = n;
}

static void Saber_ParseNoWallMarks(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_WALL_MARKS;
}

static void Saber_ParseNoDLight(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_DLIGHT;
}

static void Saber_ParseNoBlade(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_BLADE;
}

static void Saber_ParseTrailStyle(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->trailStyle = n;
}

static void Saber_ParseG2MarksShader(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
	{
		SkipRestOfLine(p);
		return;
	}
	Q_strncpyz(saber->g2MarksShader, value, sizeof saber->g2MarksShader);
	//NOTE: registers this on cgame side where it registers all client assets
}

static void Saber_ParseG2WeaponMarkShader(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
	{
		SkipRestOfLine(p);
		return;
	}
	Q_strncpyz(saber->g2WeaponMarkShader, value, sizeof saber->g2WeaponMarkShader);
	//NOTE: registers this on cgame side where it registers all client assets
}

static void Saber_ParseKnockbackScale(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->knockbackScale = f;
}

static void Saber_ParseDamageScale(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->damageScale = f;
}

static void Saber_ParseNoDismemberment(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_DISMEMBERMENT;
}

static void Saber_ParseNoIdleEffect(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_IDLE_EFFECT;
}

static void Saber_ParseAlwaysBlock(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_ALWAYS_BLOCK;
}

static void Saber_ParseNoManualDeactivate(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_MANUAL_DEACTIVATE;
}

static void Saber_ParseTransitionDamage(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_TRANSITION_DAMAGE;
}

static void Saber_ParseSplashRadius(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->splashRadius = f;
}

static void Saber_ParseSplashDamage(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->splashDamage = n;
}

static void Saber_ParseSplashKnockback(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->splashKnockback = f;
}

static void Saber_ParseHitSound1(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hitSound[0] = G_SoundIndex(value);
}

static void Saber_ParseHitSound2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hitSound[1] = G_SoundIndex(value);
}

static void Saber_ParseHitSound3(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hitSound[2] = G_SoundIndex(value);
}

static void Saber_ParseBlockSound1(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->blockSound[0] = G_SoundIndex(value);
}

static void Saber_ParseBlockSound2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->blockSound[1] = G_SoundIndex(value);
}

static void Saber_ParseBlockSound3(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->blockSound[2] = G_SoundIndex(value);
}

static void Saber_ParseBounceSound1(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->bounceSound[0] = G_SoundIndex(value);
}

static void Saber_ParseBounceSound2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->bounceSound[1] = G_SoundIndex(value);
}

static void Saber_ParseBounceSound3(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->bounceSound[2] = G_SoundIndex(value);
}

static void Saber_ParseBlockEffect(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->blockEffect = G_EffectIndex(value);
}

static void Saber_ParseHitPersonEffect(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hitPersonEffect = G_EffectIndex(value);
}

static void Saber_ParseHitOtherEffect(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hitOtherEffect = G_EffectIndex(value);
}

static void Saber_ParseBladeEffect(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->bladeEffect = G_EffectIndex(value);
}

static void Saber_ParseNoClashFlare(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_CLASH_FLARE;
}

static void Saber_ParseNoWallMarks2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_WALL_MARKS2;
}

static void Saber_ParseNoDLight2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_DLIGHT2;
}

static void Saber_ParseNoBlade2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_BLADE2;
}

static void Saber_ParseTrailStyle2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->trailStyle2 = n;
}

static void Saber_ParseG2MarksShader2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
	{
		SkipRestOfLine(p);
		return;
	}
	Q_strncpyz(saber->g2MarksShader2, value, sizeof saber->g2MarksShader2);
	//NOTE: registers this on cgame side where it registers all client assets
}

static void Saber_ParseG2WeaponMarkShader2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
	{
		SkipRestOfLine(p);
		return;
	}
	Q_strncpyz(saber->g2WeaponMarkShader2, value, sizeof saber->g2WeaponMarkShader2);
	//NOTE: registers this on cgame side where it registers all client assets
}

static void Saber_ParseKnockbackScale2(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->knockbackScale2 = f;
}

static void Saber_ParseDamageScale2(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->damageScale2 = f;
}

static void Saber_ParseNoDismemberment2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_DISMEMBERMENT2;
}

static void Saber_ParseNoIdleEffect2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_IDLE_EFFECT2;
}

static void Saber_ParseAlwaysBlock2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_ALWAYS_BLOCK2;
}

static void Saber_ParseNoManualDeactivate2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_MANUAL_DEACTIVATE2;
}

static void Saber_ParseTransitionDamage2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_TRANSITION_DAMAGE2;
}

static void Saber_ParseSplashRadius2(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->splashRadius2 = f;
}

static void Saber_ParseSplashDamage2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->splashDamage2 = n;
}

static void Saber_ParseSplashKnockback2(saberInfo_t* saber, const char** p)
{
	float f;
	if (COM_ParseFloat(p, &f))
	{
		SkipRestOfLine(p);
		return;
	}
	saber->splashKnockback2 = f;
}

static void Saber_ParseHit2Sound1(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hit2Sound[0] = G_SoundIndex(value);
}

static void Saber_ParseHit2Sound2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hit2Sound[1] = G_SoundIndex(value);
}

static void Saber_ParseHit2Sound3(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hit2Sound[2] = G_SoundIndex(value);
}

static void Saber_ParseBlock2Sound1(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->block2Sound[0] = G_SoundIndex(value);
}

static void Saber_ParseBlock2Sound2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->block2Sound[1] = G_SoundIndex(value);
}

static void Saber_ParseBlock2Sound3(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->block2Sound[2] = G_SoundIndex(value);
}

static void Saber_ParseBounce2Sound1(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->bounce2Sound[0] = G_SoundIndex(value);
}

static void Saber_ParseBounce2Sound2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->bounce2Sound[1] = G_SoundIndex(value);
}

static void Saber_ParseBounce2Sound3(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->bounce2Sound[2] = G_SoundIndex(value);
}

static void Saber_ParseBlockEffect2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->blockEffect2 = G_EffectIndex(value);
}

static void Saber_ParseHitPersonEffect2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hitPersonEffect2 = G_EffectIndex(value);
}

static void Saber_ParseHitOtherEffect2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->hitOtherEffect2 = G_EffectIndex(value);
}

static void Saber_ParseBladeEffect2(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	saber->bladeEffect2 = G_EffectIndex(value);
}

static void Saber_ParseNoClashFlare2(saberInfo_t* saber, const char** p)
{
	int n;
	if (COM_ParseInt(p, &n))
	{
		SkipRestOfLine(p);
		return;
	}
	if (n)
		saber->saberFlags2 |= SFL2_NO_CLASH_FLARE2;
}

static void Saber_ParseHolsterPlace(saberInfo_t* saber, const char** p)
{
	const char* value;

	if (COM_ParseString(p, &value))
	{
		return;
	}

	int holsterType = GetIDForString(HolsterTable, value);

	if (holsterType >= HOLSTER_NONE && holsterType <= HOLSTER_BACK)
	{
		saber->holsterPlace = static_cast<holster_locations_t>(holsterType);
	}
}

// Custom glow & blade
static void Saber_ParseGlowShader(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	Q_strncpyz(saber->glowshader, value, sizeof saber->glowshader);
	//NOTE: registers this on cgame side where it registers all client assets
}

static void Saber_ParseBladeShader(saberInfo_t* saber, const char** p)
{
	const char* value;
	if (COM_ParseString(p, &value))
		return;
	Q_strncpyz(saber->bladeshader, value, sizeof saber->bladeshader);
	//NOTE: registers this on cgame side where it registers all client assets
}

static void Saber_ParseSaberDLightColor(saberInfo_t* saber, const char** p)
{
	vec3_t rgbValue{};
	float f;

	for (float& i : rgbValue)
	{
		if (COM_ParseFloat(p, &f))
		{
			return;
		}
		i = f;
	}

	saber->saberDLightColor[0] = rgbValue[0];
	saber->saberDLightColor[1] = rgbValue[1];
	saber->saberDLightColor[2] = rgbValue[2];
}

/*
===============
Keyword Hash
===============
*/

#define KEYWORDHASH_SIZE (512)

using keywordHash_t = struct keywordHash_s
{
	char* keyword;
	void (*func)(saberInfo_t* saber, const char** p);

	keywordHash_s* next;
};

static int KeywordHash_Key(const char* keyword)
{
	int hash = 0;
	for (int i = 0; keyword[i]; i++)
	{
		if (keyword[i] >= 'A' && keyword[i] <= 'Z')
			hash += (keyword[i] + ('a' - 'A')) * (119 + i);
		else
			hash += keyword[i] * (119 + i);
	}

	hash = (hash ^ hash >> 10 ^ hash >> 20) & KEYWORDHASH_SIZE - 1;
	return hash;
}

static void KeywordHash_Add(keywordHash_t* table[], keywordHash_t* key)
{
	const int hash = KeywordHash_Key(key->keyword);

	key->next = table[hash];
	table[hash] = key;
}

static keywordHash_t* KeywordHash_Find(keywordHash_t* table[], const char* keyword)
{
	const int hash = KeywordHash_Key(keyword);

	for (keywordHash_t* key = table[hash]; key; key = key->next)
	{
		if (!Q_stricmp(key->keyword, keyword))
			return key;
	}

	return nullptr;
}

static keywordHash_t saberParseKeywords[] = {
	{"name", Saber_ParseName, nullptr},
	{"saberType", Saber_ParseSaberType, nullptr},
	{"saberModel", Saber_ParseSaberModel, nullptr},
	{"customSkin", Saber_ParseCustomSkin, nullptr},
	{"soundOn", Saber_ParseSoundOn, nullptr},
	{"soundLoop", Saber_ParseSoundLoop, nullptr},
	{"soundOff", Saber_ParseSoundOff, nullptr},
	{"numBlades", Saber_ParseNumBlades, nullptr},
	{"saberColor", Saber_ParseSaberColor, nullptr},
	{"saberColor2", Saber_ParseSaberColor2, nullptr},
	{"saberColor3", Saber_ParseSaberColor3, nullptr},
	{"saberColor4", Saber_ParseSaberColor4, nullptr},
	{"saberColor5", Saber_ParseSaberColor5, nullptr},
	{"saberColor6", Saber_ParseSaberColor6, nullptr},
	{"saberColor7", Saber_ParseSaberColor7, nullptr},
	{"saberColorRGB", Saber_ParseSaberRGBColor, nullptr},
	{"saberColorRGB2", Saber_ParseSaberRGBColor2, nullptr},
	{"saberColorRGB3", Saber_ParseSaberRGBColor3, nullptr},
	{"saberColorRGB4", Saber_ParseSaberRGBColor4, nullptr},
	{"saberColorRGB5", Saber_ParseSaberRGBColor5, nullptr},
	{"saberColorRGB6", Saber_ParseSaberRGBColor6, nullptr},
	{"saberColorRGB7", Saber_ParseSaberRGBColor7, nullptr},
	{"saberLength", Saber_ParseSaberLength, nullptr},
	{"saberLength2", Saber_ParseSaberLength2, nullptr},
	{"saberLength3", Saber_ParseSaberLength3, nullptr},
	{"saberLength4", Saber_ParseSaberLength4, nullptr},
	{"saberLength5", Saber_ParseSaberLength5, nullptr},
	{"saberLength6", Saber_ParseSaberLength6, nullptr},
	{"saberLength7", Saber_ParseSaberLength7, nullptr},
	{"saberRadius", Saber_ParseSaberRadius, nullptr},
	{"saberRadius2", Saber_ParseSaberRadius2, nullptr},
	{"saberRadius3", Saber_ParseSaberRadius3, nullptr},
	{"saberRadius4", Saber_ParseSaberRadius4, nullptr},
	{"saberRadius5", Saber_ParseSaberRadius5, nullptr},
	{"saberRadius6", Saber_ParseSaberRadius6, nullptr},
	{"saberRadius7", Saber_ParseSaberRadius7, nullptr},
	{"saberStyle", Saber_ParseSaberStyle, nullptr},
	{"saberStyleLearned", Saber_ParseSaberStyleLearned, nullptr},
	{"saberStyleForbidden", Saber_ParseSaberStyleForbidden, nullptr},
	{"maxChain", Saber_ParseMaxChain, nullptr},
	{"lockable", Saber_ParseLockable, nullptr},
	{"throwable", Saber_ParseThrowable, nullptr},
	{"disarmable", Saber_ParseDisarmable, nullptr},
	{"blocking", Saber_ParseBlocking, nullptr},
	{"twoHanded", Saber_ParseTwoHanded, nullptr},
	{"forceRestrict", Saber_ParseForceRestrict, nullptr},
	{"lockBonus", Saber_ParseLockBonus, nullptr},
	{"parryBonus", Saber_ParseParryBonus, nullptr},
	{"breakParryBonus", Saber_ParseBreakParryBonus, nullptr},
	{"breakParryBonus2", Saber_ParseBreakParryBonus2, nullptr},
	{"disarmBonus", Saber_ParseDisarmBonus, nullptr},
	{"disarmBonus2", Saber_ParseDisarmBonus2, nullptr},
	{"singleBladeStyle", Saber_ParseSingleBladeStyle, nullptr},
	{"singleBladeThrowable", Saber_ParseSingleBladeThrowable, nullptr},
	{"brokenSaber1", Saber_ParseBrokenSaber1, nullptr},
	{"brokenSaber2", Saber_ParseBrokenSaber2, nullptr},
	{"returnDamage", Saber_ParseReturnDamage, nullptr},
	{"spinSound", Saber_ParseSpinSound, nullptr},
	{"swingSound1", Saber_ParseSwingSound1, nullptr},
	{"swingSound2", Saber_ParseSwingSound2, nullptr},
	{"swingSound3", Saber_ParseSwingSound3, nullptr},
	{"fallSound1", Saber_ParseFallSound1, nullptr},
	{"fallSound2", Saber_ParseFallSound2, nullptr},
	{"fallSound3", Saber_ParseFallSound3, nullptr},
	{"moveSpeedScale", Saber_ParseMoveSpeedScale, nullptr},
	{"animSpeedScale", Saber_ParseAnimSpeedScale, nullptr},
	{"onInWater", Saber_ParseOnInWater, nullptr},
	{"bounceOnWalls", Saber_ParseBounceOnWalls, nullptr},
	{"boltToWrist", Saber_ParseBoltToWrist, nullptr},
	{"kataMove", Saber_ParseKataMove, nullptr},
	{"lungeAtkMove", Saber_ParseLungeAtkMove, nullptr},
	{"jumpAtkUpMove", Saber_ParseJumpAtkUpMove, nullptr},
	{"jumpAtkFwdMove", Saber_ParseJumpAtkFwdMove, nullptr},
	{"jumpAtkBackMove", Saber_ParseJumpAtkBackMove, nullptr},
	{"jumpAtkRightMove", Saber_ParseJumpAtkRightMove, nullptr},
	{"jumpAtkLeftMove", Saber_ParseJumpAtkLeftMove, nullptr},
	{"readyAnim", Saber_ParseReadyAnim, nullptr},
	{"drawAnim", Saber_ParseDrawAnim, nullptr},
	{"putawayAnim", Saber_ParsePutawayAnim, nullptr},
	{"tauntAnim", Saber_ParseTauntAnim, nullptr},
	{"bowAnim", Saber_ParseBowAnim, nullptr},
	{"meditateAnim", Saber_ParseMeditateAnim, nullptr},
	{"flourishAnim", Saber_ParseFlourishAnim, nullptr},
	{"gloatAnim", Saber_ParseGloatAnim, nullptr},
	{"noRollStab", Saber_ParseNoRollStab, nullptr},
	{"noPullAttack", Saber_ParseNoPullAttack, nullptr},
	{"noBackAttack", Saber_ParseNoBackAttack, nullptr},
	{"noStabDown", Saber_ParseNoStabDown, nullptr},
	{"noWallRuns", Saber_ParseNoWallRuns, nullptr},
	{"noWallFlips", Saber_ParseNoWallFlips, nullptr},
	{"noWallGrab", Saber_ParseNoWallGrab, nullptr},
	{"noRolls", Saber_ParseNoRolls, nullptr},
	{"noFlips", Saber_ParseNoFlips, nullptr},
	{"noCartwheels", Saber_ParseNoCartwheels, nullptr},
	{"noKicks", Saber_ParseNoKicks, nullptr},
	{"noMirrorAttacks", Saber_ParseNoMirrorAttacks, nullptr},
	{"notInMP", Saber_ParseNotInMP, nullptr},
	{"bladeStyle2Start", Saber_ParseBladeStyle2Start, nullptr},
	{"noWallMarks", Saber_ParseNoWallMarks, nullptr},
	{"noWallMarks2", Saber_ParseNoWallMarks2, nullptr},
	{"noDlight", Saber_ParseNoDLight, nullptr},
	{"noDlight2", Saber_ParseNoDLight2, nullptr},
	{"noBlade", Saber_ParseNoBlade, nullptr},
	{"noBlade2", Saber_ParseNoBlade2, nullptr},
	{"trailStyle", Saber_ParseTrailStyle, nullptr},
	{"trailStyle2", Saber_ParseTrailStyle2, nullptr},
	{"g2MarksShader", Saber_ParseG2MarksShader, nullptr},
	{"g2MarksShader2", Saber_ParseG2MarksShader2, nullptr},
	{"g2WeaponMarkShader", Saber_ParseG2WeaponMarkShader, nullptr},
	{"g2WeaponMarkShader2", Saber_ParseG2WeaponMarkShader2, nullptr},
	{"knockbackScale", Saber_ParseKnockbackScale, nullptr},
	{"knockbackScale2", Saber_ParseKnockbackScale2, nullptr},
	{"damageScale", Saber_ParseDamageScale, nullptr},
	{"damageScale2", Saber_ParseDamageScale2, nullptr},
	{"noDismemberment", Saber_ParseNoDismemberment, nullptr},
	{"noDismemberment2", Saber_ParseNoDismemberment2, nullptr},
	{"noIdleEffect", Saber_ParseNoIdleEffect, nullptr},
	{"noIdleEffect2", Saber_ParseNoIdleEffect2, nullptr},
	{"alwaysBlock", Saber_ParseAlwaysBlock, nullptr},
	{"alwaysBlock2", Saber_ParseAlwaysBlock2, nullptr},
	{"noManualDeactivate", Saber_ParseNoManualDeactivate, nullptr},
	{"noManualDeactivate2", Saber_ParseNoManualDeactivate2, nullptr},
	{"transitionDamage", Saber_ParseTransitionDamage, nullptr},
	{"transitionDamage2", Saber_ParseTransitionDamage2, nullptr},
	{"splashRadius", Saber_ParseSplashRadius, nullptr},
	{"splashRadius2", Saber_ParseSplashRadius2, nullptr},
	{"splashDamage", Saber_ParseSplashDamage, nullptr},
	{"splashDamage2", Saber_ParseSplashDamage2, nullptr},
	{"splashKnockback", Saber_ParseSplashKnockback, nullptr},
	{"splashKnockback2", Saber_ParseSplashKnockback2, nullptr},
	{"hit_sound1", Saber_ParseHitSound1, nullptr },
	{"hitSound1", Saber_ParseHitSound1, nullptr },
	{"hit2Sound1", Saber_ParseHit2Sound1, nullptr },
	{"hitSound2", Saber_ParseHitSound2, nullptr },
	{"hit_sound2", Saber_ParseHitSound2, nullptr },
	{"hit2Sound2", Saber_ParseHit2Sound2, nullptr },
	{"hit_sound3", Saber_ParseHitSound3, nullptr },
	{"hitSound3", Saber_ParseHitSound3, nullptr },
	{"hit2Sound3", Saber_ParseHit2Sound3, nullptr },
	{"blockSound1", Saber_ParseBlockSound1, nullptr},
	{"block2Sound1", Saber_ParseBlock2Sound1, nullptr},
	{"blockSound2", Saber_ParseBlockSound2, nullptr},
	{"block2Sound2", Saber_ParseBlock2Sound2, nullptr},
	{"blockSound3", Saber_ParseBlockSound3, nullptr},
	{"block2Sound3", Saber_ParseBlock2Sound3, nullptr},
	{"bounceSound1", Saber_ParseBounceSound1, nullptr},
	{"bounce2Sound1", Saber_ParseBounce2Sound1, nullptr},
	{"bounceSound2", Saber_ParseBounceSound2, nullptr},
	{"bounce2Sound2", Saber_ParseBounce2Sound2, nullptr},
	{"bounceSound3", Saber_ParseBounceSound3, nullptr},
	{"bounce2Sound3", Saber_ParseBounce2Sound3, nullptr},
	{"blockEffect", Saber_ParseBlockEffect, nullptr},
	{"blockEffect2", Saber_ParseBlockEffect2, nullptr},
	{"hitPersonEffect", Saber_ParseHitPersonEffect, nullptr},
	{"hitPersonEffect2", Saber_ParseHitPersonEffect2, nullptr},
	{"hitOtherEffect", Saber_ParseHitOtherEffect, nullptr},
	{"hitOtherEffect2", Saber_ParseHitOtherEffect2, nullptr},
	{"bladeEffect", Saber_ParseBladeEffect, nullptr},
	{"bladeEffect2", Saber_ParseBladeEffect2, nullptr},
	{"noClashFlare", Saber_ParseNoClashFlare, nullptr},
	{"noClashFlare2", Saber_ParseNoClashFlare2, nullptr},
	// Custom saber glow, blade & dlight color code
	{"glowshader", Saber_ParseGlowShader, nullptr},
	{"bladeshader", Saber_ParseBladeShader, nullptr},
	{"saberDLightColor", Saber_ParseSaberDLightColor, nullptr},
	{"holsterPlace", Saber_ParseHolsterPlace, nullptr},
	{"surrenderAnim", Saber_ParseSurrenderAnim, nullptr},
	{"combatstanceAnim", Saber_ParsecombatstanceAnim, nullptr},

	{nullptr, nullptr, nullptr}
};
static keywordHash_t* saberParseKeywordHash[KEYWORDHASH_SIZE];
static qboolean hashSetup = qfalse;

static void WP_SaberSetupKeywordHash()
{
	memset(saberParseKeywordHash, 0, sizeof saberParseKeywordHash);
	for (int i = 0; saberParseKeywords[i].keyword; i++)
		KeywordHash_Add(saberParseKeywordHash, &saberParseKeywords[i]);

	hashSetup = qtrue;
}

qboolean WP_SaberParseParms(const char* SaberName, saberInfo_t* saber, const qboolean setColors)
{
	const char* token;
	const char* p;

	for (auto& forced_rgb_colour : forcedRGBColours)
	{
		forced_rgb_colour = qfalse;
	}

	// make sure the hash table has been setup
	if (!hashSetup)
		WP_SaberSetupKeywordHash();

	if (!saber)
		return qfalse;

	//Set defaults so that, if it fails, there's at least something there
	WP_SaberSetDefaults(saber, setColors);

	if (!VALIDSTRING(SaberName))
		return qfalse;

	//check if we want to set the sabercolors or not (for if we're loading a savegame)
	Saber_SetColor = setColors;

	//try to parse it out
	p = SaberParms;
	COM_ParseSession ps;

	// look for the right saber
	while (p)
	{
		token = COM_ParseExt(&p, qtrue);
		if (!token[0])
			return qfalse;

		if (!Q_stricmp(token, SaberName))
			break;

		SkipBracedSection(&p);
	}

	if (!p)
		return qfalse;

	saber->name = G_NewString(SaberName);

	if (G_ParseLiteral(&p, "{"))
		return qfalse;

	// parse the saber info block
	while (true)
	{
		token = COM_ParseExt(&p, qtrue);
		if (!token[0])
		{
			gi.Printf(S_COLOR_RED"ERROR: unexpected EOF while parsing '%s' (WP_SaberParseParms)\n", SaberName);
			return qfalse;
		}

		if (!Q_stricmp(token, "}"))
			break;

		const keywordHash_t* key = KeywordHash_Find(saberParseKeywordHash, token);
		if (key)
		{
			key->func(saber, &p);
			continue;
		}

		gi.Printf("WARNING: unknown keyword '%s' while parsing '%s'\n", token, SaberName);
		SkipRestOfLine(&p);
	}

	//FIXME: precache the saberModel(s)?

	if (saber->type == SABER_SITH_SWORD)
	{
		//precache all the sith sword sounds
		Saber_SithSwordPrecache();
	}

	if (saber->holsterPlace == HOLSTER_INVALID)
	{
		if (saber->type == SABER_SITH_SWORD)
		{
			saber->holsterPlace = HOLSTER_LHIP;
		}
		else
		{
			if (saber->type == SABER_STAFF ||
				saber->type == SABER_STAFF_SFX ||
				saber->type == SABER_STAFF_UNSTABLE ||
				saber->type == SABER_STAFF_THIN)
			{
				saber->holsterPlace = HOLSTER_RHIPB;
			}
			else
			{
				saber->holsterPlace = HOLSTER_LHIPF;
			}
		}
	}
	return qtrue;
}

void WP_RemoveSaber(gentity_t* ent, const int saber_num)
{
	if (!ent || !ent->client)
	{
		return;
	}
	//reset everything for this saber just in case
	WP_SaberSetDefaults(&ent->client->ps.saber[saber_num]);

	ent->client->ps.dualSabers = qfalse;
	ent->client->ps.saber[saber_num].Deactivate();
	ent->client->ps.saber[saber_num].SetLength(0.0f);
	if (ent->weaponModel[saber_num] > 0)
	{
		gi.G2API_SetSkin(&ent->ghoul2[ent->weaponModel[saber_num]], -1, 0);
		gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->weaponModel[saber_num]);
		ent->weaponModel[saber_num] = -1;
	}
	if (ent->client->ps.saber_anim_level == SS_DUAL
		|| ent->client->ps.saber_anim_level == SS_STAFF)
	{
		//change to the style to the default
		for (int i = SS_NONE + 1; i < SS_NUM_SABER_STYLES; i++)
		{
			if (ent->client->ps.saberStylesKnown & 1 << i)
			{
				ent->client->ps.saber_anim_level = i;
				if (ent->s.number < MAX_CLIENTS)
				{
					cg.saberAnimLevelPending = ent->client->ps.saber_anim_level;
				}
				break;
			}
		}
	}
}

void WP_RemoveSecondSaber(gentity_t* ent, const int saber_num)
{
	if (!ent || !ent->client)
	{
		return;
	}
	//reset everything for this saber just in case
	WP_SaberSetDefaults(&ent->client->ps.saber[saber_num]);

	ent->client->ps.dualSabers = qfalse;
	ent->client->ps.saber[saber_num].Deactivate();
	ent->client->ps.saber[saber_num].SetLength(0.0f);
	if (ent->weaponModel[saber_num] > 0)
	{
		gi.G2API_SetSkin(&ent->ghoul2[ent->weaponModel[saber_num]], -1, 0);
		gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->weaponModel[saber_num]);
		ent->weaponModel[saber_num] = -1;
	}
	if (ent->client->ps.saber_anim_level == SS_DUAL
		|| ent->client->ps.saber_anim_level == SS_STAFF)
	{
		//change to the style to the default
		for (int i = SS_NONE + 1; i < SS_NUM_SABER_STYLES; i++)
		{
			if (ent->client->ps.saberStylesKnown & 1 << i)
			{
				ent->client->ps.saber_anim_level = i;
				if (ent->s.number < MAX_CLIENTS)
				{
					cg.saberAnimLevelPending = ent->client->ps.saber_anim_level;
				}
				break;
			}
		}
	}
}

void WP_SetSaber(gentity_t* ent, const int saber_num, const char* saber_name)
{
	if (!ent || !ent->client)
	{
		return;
	}
	if (Q_stricmp("none", saber_name) == 0 || Q_stricmp("remove", saber_name) == 0)
	{
		WP_RemoveSaber(ent, saber_num);
		return;
	}
	if (ent->weaponModel[saber_num] > 0)
	{
		gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->weaponModel[saber_num]);
		ent->weaponModel[saber_num] = -1;
	}
	WP_SaberParseParms(saber_name, &ent->client->ps.saber[saber_num]); //get saber info
	if (ent->client->ps.saber[saber_num].stylesLearned)
	{
		ent->client->ps.saberStylesKnown |= ent->client->ps.saber[saber_num].stylesLearned;
	}
	if (ent->client->ps.saber[saber_num].singleBladeStyle)
	{
		ent->client->ps.saberStylesKnown |= ent->client->ps.saber[saber_num].singleBladeStyle;
	}
	if (saber_num == 1 && ent->client->ps.saber[1].saberFlags & SFL_TWO_HANDED)
	{
		//not allowed to use a 2-handed saber as second saber
		WP_RemoveSaber(ent, saber_num);
		return;
	}
	G_ModelIndex(ent->client->ps.saber[saber_num].model);
	WP_SaberInitBladeData(ent);
	if (saber_num == 1)
	{
		//now have 2 sabers
		ent->client->ps.dualSabers = qtrue;
	}
	if (ent->client->ps.weapon == WP_SABER)
	{
		WP_SaberAddG2SaberModels(ent, saber_num);
		ent->client->ps.saber[saber_num].SetLength(0.0f);
		ent->client->ps.saber[saber_num].Activate();
	}
	else
	{
		G_RemoveHolsterModels(ent);
		WP_SaberAddHolsteredG2SaberModels(ent);
	}

	if (ent->client->ps.saber[saber_num].stylesLearned)
	{
		//change to the style we're supposed to be using
		ent->client->ps.saberStylesKnown |= ent->client->ps.saber[saber_num].stylesLearned;
	}
	if (ent->client->ps.saber[saber_num].singleBladeStyle)
	{
		ent->client->ps.saberStylesKnown |= ent->client->ps.saber[saber_num].singleBladeStyle;
	}
	WP_UseFirstValidSaberStyle(ent, &ent->client->ps.saber_anim_level);
	if (ent->s.number < MAX_CLIENTS)
	{
		cg.saberAnimLevelPending = ent->client->ps.saber_anim_level;
	}
}

void WP_SaberSetColor(const gentity_t* ent, const int saber_num, const int blade_num, const char* colorName)
{
	if (!ent || !ent->client)
	{
		return;
	}
	ent->client->ps.saber[saber_num].blade[blade_num].color = TranslateSaberColor(colorName);
}

extern void WP_SetSaberEntModelSkin(const gentity_t* ent, gentity_t* saberent);

qboolean WP_BreakSaber(gentity_t* ent, const char* surfName, const saberType_t saberType)
{
	//Make sure there *is* one specified and not using dualSabers
	if (ent == nullptr || ent->client == nullptr)
	{
		//invalid ent or client
		return qfalse;
	}

	if (ent->s.number < MAX_CLIENTS)
	{
		//player
		//if ( g_spskill->integer < 3 )
		{
			//only on the hardest level?
			//FIXME: add a cvar?
			return qfalse;
		}
	}

	if (ent->health <= 0)
	{
		//not if they're dead
		return qfalse;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		//not holding saber
		return qfalse;
	}

	if (ent->client->ps.dualSabers)
	{
		//FIXME: handle this?
		return qfalse;
	}

	if (!ent->client->ps.saber[0].brokenSaber1)
	{
		//not breakable into another type of saber
		return qfalse;
	}

	if (PM_SaberInStart(ent->client->ps.saber_move) //in a start
		|| PM_SaberInTransition(ent->client->ps.saber_move) //in a transition
		|| PM_SaberInAttack(ent->client->ps.saber_move)) //in an attack
	{
		//don't break when in the middle of an attack
		return qfalse;
	}

	if (Q_stricmpn("w_", surfName, 2)
		&& Q_stricmpn("saber", surfName, 5) //hack because using mod-community made saber
		&& Q_stricmp("cylinder01", surfName)) //hack because using mod-community made saber
	{
		//didn't hit my weapon
		return qfalse;
	}

	//Sith Sword should ALWAYS do this
	if (saberType != SABER_SITH_SWORD && Q_irand(0, 50)) //&& Q_irand( 0, 10 ) )
	{
		//10% chance - FIXME: extern this, too?
		return qfalse;
	}

	//break it
	const char* replacementSaber1 = G_NewString(ent->client->ps.saber[0].brokenSaber1);
	const char* replacementSaber2 = G_NewString(ent->client->ps.saber[0].brokenSaber2);
	int i;
	const int originalNumBlades = ent->client->ps.saber[0].numBlades;
	qboolean broken = qfalse;
	saber_colors_t colors[MAX_BLADES]{};

	//store the colors
	for (i = 0; i < MAX_BLADES; i++)
	{
		colors[i] = ent->client->ps.saber[0].blade[i].color;
	}

	//FIXME: chance of dropping the right-hand one?  Based on damage, or...?
	//FIXME: sound & effect when this happens, and send them into a broken parry?

	//remove saber[0], replace with replacementSaber1
	if (replacementSaber1)
	{
		WP_RemoveSaber(ent, 0);
		WP_SetSaber(ent, 0, replacementSaber1);
		for (i = 0; i < ent->client->ps.saber[0].numBlades; i++)
		{
			ent->client->ps.saber[0].blade[i].color = colors[i];
		}
		broken = qtrue;
		//change my saberent's model and skin to match my new right-hand saber
		WP_SetSaberEntModelSkin(ent, &g_entities[ent->client->ps.saberEntityNum]);
	}

	if (originalNumBlades <= 1)
	{
		//nothing to split off
		//FIXME: handle this?
	}
	else
	{
		//remove saber[1], replace with replacementSaber2
		if (replacementSaber2)
		{
			//FIXME: 25% chance that it just breaks - just spawn the second saber piece and toss it away immediately, can't be picked up.
			//shouldn't be one in this hand, but just in case, remove it
			WP_RemoveSaber(ent, 1);
			WP_SetSaber(ent, 1, replacementSaber2);

			//put the remainder of the original saber's blade colors onto this saber's blade(s)
			for (i = ent->client->ps.saber[0].numBlades; i < MAX_BLADES; i++)
			{
				ent->client->ps.saber[1].blade[i - ent->client->ps.saber[0].numBlades].color = colors[i];
			}
			broken = qtrue;
		}
	}
	return broken;
}

void WP_SaberLoadParms()
{
	int saberExtFNLen = 0;
	char* buffer = nullptr;
	char saberExtensionListBuf[2048]; //	The list of file names read in

	//gi.Printf( "Parsing *.sab saber definitions\n" );

	//set where to store the first one
	int totallen = 0;
	char* marker = SaberParms;
	marker[0] = '\0';

	//now load in the .sab definitions
	const int fileCnt = gi.FS_GetFileList("ext_data/sabers", ".sab", saberExtensionListBuf,
		sizeof saberExtensionListBuf);

	char* holdChar = saberExtensionListBuf;
	for (int i = 0; i < fileCnt; i++, holdChar += saberExtFNLen + 1)
	{
		saberExtFNLen = strlen(holdChar);

		int len = gi.FS_ReadFile(va("ext_data/sabers/%s", holdChar), reinterpret_cast<void**>(&buffer));

		if (len == -1)
		{
			gi.Printf("WP_SaberLoadParms: error reading %s\n", holdChar);
		}
		else
		{
			if (totallen && *(marker - 1) == '}')
			{
				//don't let it end on a } because that should be a stand-alone token
				strcat(marker, " ");
				totallen++;
				marker++;
			}
			len = COM_Compress(buffer);

			if (totallen + len >= MAX_SABER_DATA_SIZE)
			{
				G_Error("WP_SaberLoadParms: ran out of space before reading %s\n(you must make the .sab files smaller)",
					holdChar);
			}
			strcat(marker, buffer);
			gi.FS_FreeFile(buffer);

			totallen += len;
			marker += len;
		}
	}
}