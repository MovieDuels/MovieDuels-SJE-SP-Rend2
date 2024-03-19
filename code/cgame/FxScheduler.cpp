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

#include "common_headers.h"

#if !defined(FX_SCHEDULER_H_INC)
#include "FxScheduler.h"
#endif

#if !defined(GHOUL2_SHARED_H_INC)
#include "../game/ghoul2_shared.h"	//for CGhoul2Info_v
#endif

#if !defined(G2_H_INC)
#include "../ghoul2/G2.h"
#endif

#if !defined(__Q_SHARED_H)
#include "../qcommon/q_shared.h"
#endif

#include "qcommon/safe/string.h"
#include <cmath>
#include "qcommon/ojk_saved_game_helper.h"

CFxScheduler theFxScheduler;

// don't even ask,. it's to do with loadsave...
//
std::vector<sstring_t> g_vstrEffectsNeededPerSlot;
SLoopedEffect gLoopedEffectArray[MAX_LOOPED_FX]; // must be in sync with CFxScheduler::mLoopedEffectArray
void CFxScheduler::FX_CopeWithAnyLoadedSaveGames()
{
	if (!g_vstrEffectsNeededPerSlot.empty())
	{
		memcpy(mLoopedEffectArray, gLoopedEffectArray, sizeof mLoopedEffectArray);
		assert(g_vstrEffectsNeededPerSlot.size() == MAX_LOOPED_FX);

		for (size_t i_fx = 0; i_fx < g_vstrEffectsNeededPerSlot.size(); i_fx++)
		{
			const char* ps_fx_filename = g_vstrEffectsNeededPerSlot[i_fx].c_str();
			if (ps_fx_filename[0])
			{
				// register it...
				//
				mLoopedEffectArray[i_fx].mId = RegisterEffect(ps_fx_filename);
				//
				// cope with any relative stop time...
				//
				if (mLoopedEffectArray[i_fx].mLoopStopTime)
				{
					mLoopedEffectArray[i_fx].mLoopStopTime -= mLoopedEffectArray[i_fx].mNextTime;
				}
				//
				// and finally reset the time to be the newly-zeroed game time...
				//
				mLoopedEffectArray[i_fx].mNextTime = 0; // otherwise it won't process until game time catches up
			}
			else
			{
				mLoopedEffectArray[i_fx].mId = 0;
			}
		}

		g_vstrEffectsNeededPerSlot.clear();
	}
}

void FX_CopeWithAnyLoadedSaveGames()
{
	theFxScheduler.FX_CopeWithAnyLoadedSaveGames();
}

// for loadsave...
//
void FX_Read()
{
	theFxScheduler.LoadSave_Read();
}

// for loadsave...
//
void FX_Write()
{
	theFxScheduler.LoadSave_Write();
}

void CFxScheduler::LoadSave_Read()
{
	Clean(); // need to get rid of old pre-cache handles, or it thinks it has some older effects when it doesn't
	g_vstrEffectsNeededPerSlot.clear(); // jic

	ojk::SavedGameHelper saved_game(
		gi.saved_game);

	saved_game.read_chunk(
		INT_ID('F', 'X', 'L', 'E'),
		gLoopedEffectArray);

	//
	// now read in and re-register the effects we need for those structs...
	//
	for (int i_fx = 0; i_fx < MAX_LOOPED_FX; i_fx++)
	{
		char s_fx_filename[MAX_QPATH];

		saved_game.read_chunk(
			INT_ID('F', 'X', 'F', 'N'),
			s_fx_filename);

		g_vstrEffectsNeededPerSlot.emplace_back(s_fx_filename);
	}
}

void CFxScheduler::LoadSave_Write()
{
	ojk::SavedGameHelper saved_game(
		gi.saved_game);

	// bsave the data we need...
	//
	saved_game.write_chunk(
		INT_ID('F', 'X', 'L', 'E'),
		mLoopedEffectArray);

	//
	// then cope with the fact that the mID field in each struct of the array we've just saved will not
	//	necessarily point at the same thing when reloading, so save out the actual fx filename strings they
	//	need for re-registration...
	//
	// since this is only for savegames, and I've got < 2 hours to finish this and test it I'm going to be lazy
	//	with the ondisk data... (besides, the RLE compression will kill most of this anyway)
	//
	for (auto& i_fx : mLoopedEffectArray)
	{
		char s_fx_filename[MAX_QPATH] = {};
		// instead of "sFX_Filename[0]=0;" so RLE will squash whole array to nothing, not just stop at '\0' then have old crap after it to compress

		const int& i_id = i_fx.mId;
		if (i_id)
		{
			// now we need to look up what string this represents, unfortunately the existing
			//	lookup table is backwards (keywise) for our needs, so parse the whole thing...
			//
			for (auto& mEffectID : mEffectIDs)
			{
				if (mEffectID.second == i_id)
				{
					Q_strncpyz(s_fx_filename, mEffectID.first.c_str(), sizeof s_fx_filename);
					break;
				}
			}
		}

		// write out this string...
		//
		saved_game.write_chunk(
			INT_ID('F', 'X', 'F', 'N'),
			s_fx_filename);
	}
}

//-----------------------------------------------------------
void CMediaHandles::operator=(const CMediaHandles& that)
{
	mMediaList.clear();

	for (int i : that.mMediaList)
	{
		mMediaList.push_back(i);
	}
}

//------------------------------------------------------
CFxScheduler::CFxScheduler()
{
	memset(&mEffectTemplates, 0, sizeof mEffectTemplates);
	memset(&mLoopedEffectArray, 0, sizeof mLoopedEffectArray);
}

int CFxScheduler::ScheduleLoopedEffect(const int id, const int boltInfo, const bool isPortal, const int i_loop_time,
	const bool is_relative)
{
	int i;

	assert(id);
	assert(boltInfo != -1);

	for (i = 0; i < MAX_LOOPED_FX; i++) //see if it's already playing so we can just update it
	{
		if (mLoopedEffectArray[i].mId == id &&
			mLoopedEffectArray[i].mBoltInfo == boltInfo &&
			mLoopedEffectArray[i].mPortalEffect == isPortal
			)
		{
#ifdef _DEBUG
			//			theFxHelper.Print( "CFxScheduler::ScheduleLoopedEffect- updating %s\n", mEffectTemplates[id].mEffectName);
#endif
			break;
		}
	}

	if (i == MAX_LOOPED_FX) //didn't find it existing, so find a free spot
	{
		for (i = 0; i < MAX_LOOPED_FX; i++)
		{
			if (!mLoopedEffectArray[i].mId)
			{
				break;
			}
		}
	}

	if (i == MAX_LOOPED_FX)
	{
		//bad
		assert(i != MAX_LOOPED_FX);
		theFxHelper.Print("CFxScheduler::AddLoopedEffect- No Free Slots available for %d\n",
			mEffectTemplates[id].mEffectName);
		return -1;
	}
	mLoopedEffectArray[i].mId = id;
	mLoopedEffectArray[i].mBoltInfo = boltInfo;
	mLoopedEffectArray[i].mPortalEffect = isPortal;
	mLoopedEffectArray[i].mIsRelative = is_relative;
	mLoopedEffectArray[i].mNextTime = theFxHelper.mTime + mEffectTemplates[id].mRepeatDelay;
	mLoopedEffectArray[i].mLoopStopTime = i_loop_time == 1 ? 0 : theFxHelper.mTime + i_loop_time;
	return i;
}

void CFxScheduler::StopEffect(const char* file, const int boltInfo, const bool isPortal)
{
	char sfile[MAX_QPATH];

	// Get an extenstion stripped version of the file
	COM_StripExtension(file, sfile, sizeof sfile);
	const int id = mEffectIDs[sfile];
#ifndef FINAL_BUILD
	if (id == 0)
	{
		theFxHelper.Print("CFxScheduler::StopEffect- unregistered/non-existent effect: %s\n", sfile);
		return;
	}
#endif

	for (int i = 0; i < MAX_LOOPED_FX; i++)
	{
		if (mLoopedEffectArray[i].mId == id &&
			mLoopedEffectArray[i].mBoltInfo == boltInfo &&
			mLoopedEffectArray[i].mPortalEffect == isPortal
			)
		{
			memset(&mLoopedEffectArray[i], 0, sizeof mLoopedEffectArray[i]);
			return;
		}
	}
#ifdef _DEBUG_FX
	theFxHelper.Print("CFxScheduler::StopEffect- (%s) is not looping!\n", file);
#endif
}

void CFxScheduler::AddLoopedEffects()
{
	for (int i = 0; i < MAX_LOOPED_FX; i++)
	{
		if (mLoopedEffectArray[i].mId && mLoopedEffectArray[i].mNextTime < theFxHelper.mTime)
		{
			const int entNum = mLoopedEffectArray[i].mBoltInfo >> ENTITY_SHIFT & ENTITY_AND;
			if (cg_entities[entNum].gent->inuse)
			{
				// only play the looped effect when the ent is still inUse....
				PlayEffect(mLoopedEffectArray[i].mId, cg_entities[entNum].lerpOrigin, nullptr,
					mLoopedEffectArray[i].mBoltInfo, -1, mLoopedEffectArray[i].mPortalEffect, false,
					mLoopedEffectArray[i].mIsRelative);
				//very important to send FALSE looptime to not recursively add me!
				mLoopedEffectArray[i].mNextTime = theFxHelper.mTime + mEffectTemplates[mLoopedEffectArray[i].mId].
					mRepeatDelay;
			}
			else
			{
				theFxHelper.Print(
					"CFxScheduler::AddLoopedEffects- entity was removed without stopping any looping fx it owned.");
				memset(&mLoopedEffectArray[i], 0, sizeof mLoopedEffectArray[i]);
				continue;
			}
			if (mLoopedEffectArray[i].mLoopStopTime && mLoopedEffectArray[i].mLoopStopTime < theFxHelper.mTime)
				//time's up
			{
				//kill this entry
				memset(&mLoopedEffectArray[i], 0, sizeof mLoopedEffectArray[i]);
			}
		}
	}
}

//-----------------------------------------------------------
void SEffectTemplate::operator=(const SEffectTemplate& that)
{
	mCopy = true;

	strcpy(mEffectName, that.mEffectName);

	mPrimitiveCount = that.mPrimitiveCount;

	for (int i = 0; i < mPrimitiveCount; i++)
	{
		mPrimitives[i] = new CPrimitiveTemplate;
		*mPrimitives[i] = *that.mPrimitives[i];
		// Mark use as a copy so that we know that we should be chucked when used up
		mPrimitives[i]->mCopy = true;
	}
}

//------------------------------------------------------
// Clean
//	Free up any memory we've allocated so we aren't leaking memory
//
// Input:
//	Whether to clean everything or just stop the playing (active) effects
//
// Return:
//	None
//
//------------------------------------------------------
void CFxScheduler::Clean(const bool b_remove_templates /*= true*/, const int id_to_preserve /*= 0*/)
{
	// Ditch any scheduled effects
	auto itr = mFxSchedule.begin();

	while (itr != mFxSchedule.end())
	{
		auto next = itr;
		++next;

		mScheduledEffectsPool.Free(*itr);
		mFxSchedule.erase(itr);

		itr = next;
	}

	if (b_remove_templates)
	{
		// Ditch any effect templates
		for (int i = 1; i < FX_MAX_EFFECTS; i++)
		{
			if (i == id_to_preserve)
			{
				continue;
			}

			if (mEffectTemplates[i].mInUse)
			{
				// Ditch the primitives
				for (int j = 0; j < mEffectTemplates[i].mPrimitiveCount; j++)
				{
					delete mEffectTemplates[i].mPrimitives[j];
				}
			}

			mEffectTemplates[i].mInUse = false;
		}

		if (id_to_preserve == 0)
		{
			mEffectIDs.clear();
		}
		else
		{
			// Clear the effect names, but first get the name of the effect to preserve,
			// and restore it after clearing.
			fxString_t str;

			for (auto iter = mEffectIDs.begin(); iter != mEffectIDs.end(); ++iter)
			{
				if ((*iter).second == id_to_preserve)
				{
					str = (*iter).first;
					break;
				}
			}

			mEffectIDs.clear();

			mEffectIDs[str] = id_to_preserve;
		}
	}
}

//------------------------------------------------------
// RegisterEffect
//	Attempt to open the specified effect file, if
//	file read succeeds, parse the file.
//
// Input:
//	path or filename to open
//
// Return:
//	int handle to the effect
//------------------------------------------------------
int CFxScheduler::RegisterEffect(const char* path, const bool b_has_correct_path /*= false*/)
{
	// Dealing with file names:
	// File names can come from two places - the editor, in which case we should use the given
	// path as is, and the effect file, in which case we should add the correct path and extension.
	// In either case we create a stripped file name to use for naming effects.
	//

	// FIXME: this could maybe be a cstring_view, if mEffectIDs were to use a transparent comparator, but those were only added in C++14, which we don't support yet (sigh)
	char filename_no_ext[MAX_QPATH];

	// Get an extension stripped version of the file
	if (b_has_correct_path)
	{
		// FIXME: this is basically COM_SkipPath, except it also accepts '\\' instead of '/'
		const char* last = path, * p = path;
		while (*p != '\0')
		{
			if (*p == '/' || *p == '\\')
			{
				last = p + 1;
			}
			p++;
		}

		COM_StripExtension(last, filename_no_ext, sizeof filename_no_ext);
	}
	else
	{
		COM_StripExtension(path, filename_no_ext, sizeof filename_no_ext);
	}

	const auto itr = mEffectIDs.find(filename_no_ext);

	if (itr != mEffectIDs.end())
	{
		return (*itr).second;
	}

	const char* pfile;
	if (b_has_correct_path)
	{
		pfile = path;
	}
	else
	{
		char correctFilenameBuffer[MAX_QPATH];
		// Add on our extension and prepend the file with the default path
		Com_sprintf(correctFilenameBuffer, sizeof correctFilenameBuffer, "%s/%s.efx", FX_FILE_PATH, filename_no_ext);
		pfile = correctFilenameBuffer;
	}

	// Let the generic parser process the whole file
	CGenericParser2 parser;
	if (!parser.Parse(pfile))
	{
		if (!parser.ValidFile())
		{
			theFxHelper.Print("RegisterEffect: INVALID file: %s\n", pfile);
		}
		return false;
	}
	// Lets convert the effect file into something that we can work with
	return ParseEffect(filename_no_ext, parser.GetBaseParseGroup());
}

//------------------------------------------------------
// ParseEffect
//	Starts at ground zero, using each group header to
//	determine which kind of effect we are working with.
//	Then we call the appropriate function to parse the
//	specified effect group.
//
// Input:
//	base group, essentially the whole files contents
//
// Return:
//	int handle of the effect
//------------------------------------------------------

int CFxScheduler::ParseEffect(const char* file, const CGPGroup& base)
{
	int handle;
	SEffectTemplate* effect = GetNewEffectTemplate(&handle, file);

	if (!handle || !effect)
	{
		// failure
		return 0;
	}

	for (auto& property : base.GetProperties())
	{
		if (Q::stricmp(property.GetName(), CSTRING_VIEW("repeatDelay")) == Q::Ordering::EQ)
		{
			effect->mRepeatDelay = Q::svtoi(property.GetTopValue());
		}
	}

	for (const auto& primitive_group : base.GetSubGroups())
	{
		static std::map<gsl::cstring_view, EPrimType, Q::CStringViewILess> primitiveTypes{
			{CSTRING_VIEW("particle"), Particle},
			{ CSTRING_VIEW("line"), Line },
			{ CSTRING_VIEW("tail"), Tail },
			{ CSTRING_VIEW("sound"), Sound },
			{ CSTRING_VIEW("cylinder"), Cylinder },
			{ CSTRING_VIEW("electricity"), Electricity },
			{ CSTRING_VIEW("emitter"), Emitter },
			{ CSTRING_VIEW("decal"), Decal },
			{ CSTRING_VIEW("orientedparticle"), OrientedParticle },
			{ CSTRING_VIEW("fxrunner"), FxRunner },
			{ CSTRING_VIEW("light"), Light },
			{ CSTRING_VIEW("cameraShake"), CameraShake },
			{ CSTRING_VIEW("flash"), ScreenFlash }
		};
		auto pos = primitiveTypes.find(primitive_group.GetName());
		if (pos != primitiveTypes.end())
		{
			const auto prim = new CPrimitiveTemplate;

			prim->mType = pos->second;
			prim->ParsePrimitive(primitive_group);

			// Add our primitive template to the effect list
			AddPrimitiveToEffect(effect, prim);
		}
	}

	return handle;
}

//------------------------------------------------------
// AddPrimitiveToEffect
//	Takes a primitive and attaches it to the effect.
//
// Input:
//	Effect template that we tack the primitive on to
//	Primitive to add to the effect template
//
// Return:
//	None
//------------------------------------------------------
void CFxScheduler::AddPrimitiveToEffect(SEffectTemplate* fx, CPrimitiveTemplate* prim)
{
	const int ct = fx->mPrimitiveCount;

	if (ct >= FX_MAX_EFFECT_COMPONENTS)
	{
		theFxHelper.Print("FxScheduler:  Error--too many primitives in an effect\n");
	}
	else
	{
		fx->mPrimitives[ct] = prim;
		fx->mPrimitiveCount++;
	}
}

//------------------------------------------------------
// GetNewEffectTemplate
//	Finds an unused effect template and returns it to the
//	caller.
//
// Input:
//	pointer to an id that will be filled in,
//	file name-- should be NULL when requesting a copy
//
// Return:
//	the id of the added effect template
//------------------------------------------------------
SEffectTemplate* CFxScheduler::GetNewEffectTemplate(int* id, const char* file)
{
	// wanted zero to be a bogus effect ID, so we just skip it.
	for (int i = 1; i < FX_MAX_EFFECTS; i++)
	{
		SEffectTemplate* effect = &mEffectTemplates[i];

		if (!effect->mInUse)
		{
			*id = i;
			memset(effect, 0, sizeof(SEffectTemplate));

			// If we are a copy, we really won't have a name that we care about saving for later
			if (file)
			{
				mEffectIDs[file] = i;
				strcpy(effect->mEffectName, file);
			}

			effect->mInUse = true;
			effect->mRepeatDelay = 300;
			return effect;
		}
	}

	theFxHelper.Print("FxScheduler:  Error--reached max effects\n");
	*id = 0;
	return nullptr;
}

//------------------------------------------------------
// GetEffectCopy
//	Returns a copy of the desired effect so that it can
//	easily be modified run-time.
//
// Input:
//	file-- the name of the effect file that you want a copy of
//	newHandle-- will actually be the returned handle to the new effect
//				you have to hold onto this if you intend to call it again
//
// Return:
//	the pointer to the copy
//------------------------------------------------------
SEffectTemplate* CFxScheduler::GetEffectCopy(const char* file, int* new_handle)
{
	return GetEffectCopy(mEffectIDs[file], new_handle);
}

//------------------------------------------------------
// GetEffectCopy
//	Returns a copy of the desired effect so that it can
//	easily be modified run-time.
//
// Input:
//	fxHandle-- the handle to the effect that you want a copy of
//	newHandle-- will actually be the returned handle to the new effect
//				you have to hold onto this if you intend to call it again
//
// Return:
//	the pointer to the copy
//------------------------------------------------------
SEffectTemplate* CFxScheduler::GetEffectCopy(const int fx_handle, int* new_handle)
{
	if (fx_handle < 1 || fx_handle >= FX_MAX_EFFECTS || !mEffectTemplates[fx_handle].mInUse)
	{
		// Didn't even request a valid effect to copy!!!
		theFxHelper.Print("FxScheduler: Bad effect file copy request\n");

		*new_handle = 0;
		return nullptr;
	}

	// never get a copy when time is frozen
	if (fx_freeze.integer)
	{
		return nullptr;
	}

	// Copies shouldn't have names, otherwise they could trash our stl map used for getting ID from name
	SEffectTemplate* copy = GetNewEffectTemplate(new_handle, nullptr);

	if (copy && *new_handle)
	{
		// do the effect copy and mark us as what we are
		*copy = mEffectTemplates[fx_handle];
		copy->mCopy = true;

		// the user had better hold onto this handle if they ever hope to call this effect.
		return copy;
	}

	// No space left to return an effect
	*new_handle = 0;
	return nullptr;
}

//------------------------------------------------------
// GetPrimitiveCopy
//	Helper function that returns a copy of the desired primitive
//
// Input:
//	fxHandle - the pointer to the effect copy you want to override
//	componentName - name of the component to find
//
// Return:
//	the pointer to the desired primitive
//------------------------------------------------------
CPrimitiveTemplate* CFxScheduler::GetPrimitiveCopy(const SEffectTemplate* effect_copy, const char* component_name)
{
	if (!effect_copy || !effect_copy->mInUse)
	{
		return nullptr;
	}

	for (int i = 0; i < effect_copy->mPrimitiveCount; i++)
	{
		if (!Q_stricmp(effect_copy->mPrimitives[i]->mName, component_name))
		{
			// we found a match, so return it
			return effect_copy->mPrimitives[i];
		}
	}

	// bah, no good.
	return nullptr;
}

//------------------------------------------------------
static void ReportPlayEffectError(const int id)
{
#ifdef _DEBUG
	theFxHelper.Print("CFxScheduler::PlayEffect called with invalid effect ID: %i\n", id);
#endif
}

//------------------------------------------------------
// PlayEffect
//	Handles scheduling an effect so all the components
//	happen at the specified time.  Applies a default up
//	axis.
//
// Input:
//	Effect file id and the origin
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::PlayEffect(const int id, vec3_t origin, const bool isPortal)
{
	vec3_t axis[3]{};

	VectorSet(axis[0], 0, 0, 1);
	VectorSet(axis[1], 1, 0, 0);
	VectorSet(axis[2], 0, 1, 0);

	PlayEffect(id, origin, axis, -1, -1, isPortal);
}

//------------------------------------------------------
// PlayEffect
//	Handles scheduling an effect so all the components
//	happen at the specified time.  Takes a fwd vector
//	and builds a right and up vector
//
// Input:
//	Effect file id, the origin, and a fwd vector
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::PlayEffect(const int id, vec3_t origin, vec3_t forward, const bool isPortal)
{
	vec3_t axis[3]{};

	// Take the forward vector and create two arbitrary but perpendicular vectors
	VectorCopy(forward, axis[0]);
	MakeNormalVectors(forward, axis[1], axis[2]);

	PlayEffect(id, origin, axis, -1, -1, isPortal);
}

//------------------------------------------------------
// PlayEffect
//	Handles scheduling an effect so all the components
//	happen at the specified time.  Uses the specified axis
//
// Input:
//	Effect file name, the origin, and axis.
//	Optional boltInfo (defaults to -1)
//  Optional entity number to be used by a cheap entity origin bolt (defaults to -1)
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::PlayEffect(const char* file, vec3_t origin, vec3_t axis[3], const int boltInfo, const int entNum,
	const bool isPortal, const int i_loop_time, const bool is_relative)
{
	char sfile[MAX_QPATH];

	// Get an extenstion stripped version of the file
	COM_StripExtension(file, sfile, sizeof sfile);

	// This is a horribly dumb thing to have to do, but QuakeIII might not have calc'd the lerpOrigin
	//	for the entity we may be trying to bolt onto.  We like having the correct origin, so we are
	//	forced to call this function....
	if (entNum > -1)
	{
		CG_CalcEntityLerpPositions(&cg_entities[entNum]);
	}

#ifndef FINAL_BUILD
	if (mEffectIDs[sfile] == 0)
	{
		theFxHelper.Print("CFxScheduler::PlayEffect unregistered/non-existent effect: %s\n", sfile);
	}
#endif

	PlayEffect(mEffectIDs[sfile], origin, axis, boltInfo, entNum, isPortal, i_loop_time, is_relative);
}

//------------------------------------------------------
// PlayEffect
//	Handles scheduling an effect so all the components
//	happen at the specified time.  Uses the specified axis
//
// Input:
//	Effect file name, the origin, and axis.
//	Optional boltInfo (defaults to -1)
//  Optional entity number to be used by a cheap entity origin bolt (defaults to -1)
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::PlayEffect(const char* file, const int client_id, const bool isPortal)
{
	char sfile[MAX_QPATH];

	// Get an extenstion stripped version of the file
	COM_StripExtension(file, sfile, sizeof sfile);
	int id = mEffectIDs[sfile];

#ifndef FINAL_BUILD
	if (id == 0)
	{
		theFxHelper.Print("CFxScheduler::PlayEffect unregistered/non-existent effect: %s\n", file);
	}
#endif

	int delay;
	float factor = 0.0f;

	if (id < 1 || id >= FX_MAX_EFFECTS || !mEffectTemplates[id].mInUse)
	{
		// Now you've done it!
		ReportPlayEffectError(id);
		return;
	}

	// Don't bother scheduling the effect if the system is currently frozen

	// Get the effect.
	SEffectTemplate* fx = &mEffectTemplates[id];

	// Loop through the primitives and schedule each bit
	for (int i = 0; i < fx->mPrimitiveCount; i++)
	{
		CPrimitiveTemplate* prim = fx->mPrimitives[i];

		const int count = prim->mSpawnCount.GetRoundedVal();

		if (prim->mCopy)
		{
			// If we are a copy, we need to store a "how many references count" so that we
			//	can keep the primitive template around for the correct amount of time.
			prim->mRefCount = count;
		}

		if (prim->mSpawnFlags & FX_EVEN_DISTRIBUTION)
		{
			factor = abs(prim->mSpawnDelay.GetMax() - prim->mSpawnDelay.GetMin()) / static_cast<float>(count);
		}

		// Schedule the random number of bits
		for (int t = 0; t < count; t++)
		{
			if (prim->mSpawnFlags & FX_EVEN_DISTRIBUTION)
			{
				delay = t * factor;
			}
			else
			{
				delay = prim->mSpawnDelay.GetVal();
			}

			// if the delay is so small, we may as well just create this bit right now
			if (delay < 1 && !isPortal)
			{
				CreateEffect(prim, client_id);
			}
			else
			{
				SScheduledEffect* sfx = mScheduledEffectsPool.Alloc();

				if (sfx == nullptr)
				{
					Com_Error(ERR_DROP, "ERROR: Failed to allocate EFX from memory pool.");
				}

				sfx->mStartTime = theFxHelper.mTime + delay;
				sfx->mpTemplate = prim;
				sfx->mClientID = client_id;

				if (isPortal)
				{
					sfx->mPortalEffect = true;
				}
				else
				{
					sfx->mPortalEffect = false;
				}

				mFxSchedule.push_front(sfx);
			}
		}
	}

	// We track effect templates and primitive templates separately.
	if (fx->mCopy)
	{
		// We don't use dynamic memory allocation, so just mark us as dead
		fx->mInUse = false;
	}
}

bool gEffectsInPortal = false;
//this is just because I don't want to have to add an mPortalEffect field to every actual effect.

//------------------------------------------------------
// CreateEffect
//	Creates the specified fx taking into account the
//	multitude of different ways it could be spawned.
//
// Input:
//	template used to build the effect, desired effect origin,
//	desired orientation and how late the effect is so that
//	it can be moved to the correct spot
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::CreateEffect(CPrimitiveTemplate* fx, const int client_id) const
{
	vec3_t s_rgb, e_rgb;
	vec3_t vel, accel;
	vec3_t org, org2;
	int flags;

	// Origin calculations -- completely ignores most things
	//-------------------------------------
	VectorSet(org, fx->mOrigin1X.GetVal(), fx->mOrigin1Y.GetVal(), fx->mOrigin1Z.GetVal());
	VectorSet(org2, fx->mOrigin2X.GetVal(), fx->mOrigin2Y.GetVal(), fx->mOrigin2Z.GetVal());

	// handle RGB color
	if (fx->mSpawnFlags & FX_RGB_COMPONENT_INTERP)
	{
		const float perc = Q_flrand(0.0f, 1.0f);

		VectorSet(s_rgb, fx->mRedStart.GetVal(perc), fx->mGreenStart.GetVal(perc), fx->mBlueStart.GetVal(perc));
		VectorSet(e_rgb, fx->mRedEnd.GetVal(perc), fx->mGreenEnd.GetVal(perc), fx->mBlueEnd.GetVal(perc));
	}
	else
	{
		VectorSet(s_rgb, fx->mRedStart.GetVal(), fx->mGreenStart.GetVal(), fx->mBlueStart.GetVal());
		VectorSet(e_rgb, fx->mRedEnd.GetVal(), fx->mGreenEnd.GetVal(), fx->mBlueEnd.GetVal());
	}

	// NOTE: This completely disregards a few specialty flags.
	VectorSet(vel, fx->mVelX.GetVal(), fx->mVelY.GetVal(), fx->mVelZ.GetVal());
	VectorSet(accel, fx->mAccelX.GetVal(), fx->mAccelY.GetVal(), fx->mAccelZ.GetVal());

	// If depth hack ISN'T already on, then turn it on.  Otherwise, we treat a pre-existing depth_hack flag as NOT being depth_hack.
	//	This is done because muzzle flash fx files are shared amongst all shooters, but for the player we need to do depth hack in first person....
	if (!(fx->mFlags & FX_DEPTH_HACK) && !cg.renderingThirdPerson) // hack!
	{
		flags = fx->mFlags | FX_RELATIVE | FX_DEPTH_HACK;
	}
	else
	{
		flags = (fx->mFlags | FX_RELATIVE) & ~FX_DEPTH_HACK;
	}

	// We only support particles for now
	//------------------------
	switch (fx->mType)
	{
		//---------
	case Particle:
		//---------

		FX_AddParticle(client_id, org, vel, accel, fx->mGravity.GetVal(),
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mRotation.GetVal(), fx->mRotationDelta.GetVal(),
			fx->mMin, fx->mMax, fx->mElasticity.GetVal(),
			fx->mDeathFxHandles.GetHandle(), fx->mImpactFxHandles.GetHandle(),
			fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), flags);
		break;

		//---------
	case Line:
		//---------

		FX_AddLine(client_id, org, org2,
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), fx->mImpactFxHandles.GetHandle(), flags);
		break;

		//---------
	case Tail:
		//---------

		FX_AddTail(client_id, org, vel, accel,
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mLengthStart.GetVal(), fx->mLengthEnd.GetVal(), fx->mLengthParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mMin, fx->mMax, fx->mElasticity.GetVal(),
			fx->mDeathFxHandles.GetHandle(), fx->mImpactFxHandles.GetHandle(),
			fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), flags);
		break;

		//---------
	case Sound:
		//---------

		if (gEffectsInPortal)
		{
			//could orient this anyway for panning, but eh. It's going to appear to the player in the sky the same place no matter what, so just make it a local sound.
			theFxHelper.PlayLocalSound(fx->mMediaHandles.GetHandle(), CHAN_AUTO);
		}
		else
		{
			// bolted sounds actually play on the client....
			theFxHelper.PlaySound(nullptr, client_id, CHAN_WEAPON, fx->mMediaHandles.GetHandle());
		}
		break;
		//---------
	case Light:
		//---------

		// don't much care if the light stays bolted...so just add it.
		if (client_id >= 0 && client_id < ENTITYNUM_WORLD)
		{
			// ..um, ok.....
			const centity_t* cent = &cg_entities[client_id];

			if (cent && cent->gent && cent->gent->client)
			{
				FX_AddLight(cent->gent->client->renderInfo.muzzlePoint, fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(),
					fx->mSizeParm.GetVal(),
					s_rgb, e_rgb, fx->mRGBParm.GetVal(),
					fx->mLife.GetVal(), fx->mFlags);
			}
		}
		break;

		//---------
	case CameraShake:
		//---------

		if (client_id >= 0 && client_id < ENTITYNUM_WORLD)
		{
			// ..um, ok.....
			const centity_t* cent = &cg_entities[client_id];

			if (cent && cent->gent && cent->gent->client)
			{
				theFxHelper.CameraShake(cent->gent->currentOrigin, fx->mElasticity.GetVal(), fx->mRadius.GetVal(),
					fx->mLife.GetVal());
			}
		}
		break;

	default:
		break;
	}

	// Track when we need to clean ourselves up if we are a copy
	if (fx->mCopy)
	{
		fx->mRefCount--;

		if (fx->mRefCount <= 0)
		{
			delete fx;
		}
	}
}

//------------------------------------------------------
// PlayEffect
//	Handles scheduling an effect so all the components
//	happen at the specified time.  Uses the specified axis
//
// Input:
//	Effect id, the origin, and axis.
//	Optional boltInfo (defaults to -1)
//  Optional entity number to be used by a cheap entity origin bolt (defaults to -1)
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::PlayEffect(const int id, vec3_t origin, vec3_t axis[3], const int boltInfo, const int entNum,
	const bool isPortal, const int i_loop_time, const bool is_relative)
{
	int delay;
	float factor = 0.0f;
	bool forceScheduling = false;

	if (id < 1 || id >= FX_MAX_EFFECTS || !mEffectTemplates[id].mInUse)
	{
		// Now you've done it!
		ReportPlayEffectError(id);
		return;
	}

	// Don't bother scheduling the effect if the system is currently frozen
	if (fx_freeze.integer)
	{
		return;
	}

	int model_num = 0, bolt_num = -1;
	int entityNum = entNum;

	if (boltInfo > 0)
	{
		// extract the wraith ID from the bolt info
		model_num = boltInfo >> MODEL_SHIFT & MODEL_AND;
		bolt_num = boltInfo >> BOLT_SHIFT & BOLT_AND;
		entityNum = boltInfo >> ENTITY_SHIFT & ENTITY_AND;

		// We always force ghoul bolted objects to be scheduled so that they don't play right away.
		forceScheduling = true;

		if (i_loop_time) //0 = not looping, 1 for infinite, else duration
		{
			//store off the id to reschedule every frame
			ScheduleLoopedEffect(id, boltInfo, isPortal, i_loop_time, is_relative);
		}
	}

	// Get the effect.
	SEffectTemplate* fx = &mEffectTemplates[id];

	// Loop through the primitives and schedule each bit
	for (int i = 0; i < fx->mPrimitiveCount; i++)
	{
		CPrimitiveTemplate* prim = fx->mPrimitives[i];

		if (prim->mCullRange)
		{
			if (DistanceSquared(origin, cg.refdef.vieworg) > prim->mCullRange) // cull range has already been squared
			{
				// is too far away, so don't add this primitive group
				continue;
			}
		}

		const int count = prim->mSpawnCount.GetRoundedVal();

		if (prim->mCopy)
		{
			// If we are a copy, we need to store a "how many references count" so that we
			//	can keep the primitive template around for the correct amount of time.
			prim->mRefCount = count;
		}

		if (prim->mSpawnFlags & FX_EVEN_DISTRIBUTION)
		{
			factor = abs(prim->mSpawnDelay.GetMax() - prim->mSpawnDelay.GetMin()) / static_cast<float>(count);
		}

		// Schedule the random number of bits
		for (int t = 0; t < count; t++)
		{
			if (prim->mSpawnFlags & FX_EVEN_DISTRIBUTION)
			{
				delay = t * factor;
			}
			else
			{
				delay = prim->mSpawnDelay.GetVal();
			}

			// if the delay is so small, we may as well just create this bit right now
			if (delay < 1 && !forceScheduling && !isPortal)
			{
				if (boltInfo == -1 && entNum != -1)
				{
					// Find out where the entity currently is
					CreateEffect(prim, cg_entities[entNum].lerpOrigin, axis, -delay);
				}
				else
				{
					CreateEffect(prim, origin, axis, -delay);
				}
			}
			else
			{
				SScheduledEffect* sfx = mScheduledEffectsPool.Alloc();

				if (sfx == nullptr)
				{
					Com_Error(ERR_DROP, "ERROR: Failed to allocate EFX from memory pool.");
				}

				sfx->mStartTime = theFxHelper.mTime + delay;
				sfx->mpTemplate = prim;
				sfx->mClientID = -1;
				sfx->mIsRelative = is_relative;
				sfx->mEntNum = entityNum; //ent if bolted, else -1 for none, or -2 for _Immersion client 0

				sfx->mPortalEffect = isPortal;

				if (boltInfo == -1)
				{
					if (entNum == -1)
					{
						// we aren't bolting, so make sure the spawn system knows this by putting -1's in these fields
						sfx->mBoltNum = -1;
						sfx->mModelNum = 0;

						if (origin)
						{
							VectorCopy(origin, sfx->mOrigin);
						}
						else
						{
							VectorClear(sfx->mOrigin);
						}

						AxisCopy(axis, sfx->mAxis);
					}
					else
					{
						// we are doing bolting onto the origin of the entity, so use a cheaper method
						sfx->mBoltNum = -1;
						sfx->mModelNum = 0;

						AxisCopy(axis, sfx->mAxis);
					}
				}
				else
				{
					// we are bolting, so store the extra info
					sfx->mBoltNum = bolt_num;
					sfx->mModelNum = model_num;

					// Also, the ghoul bolt may not be around yet, so delay the creation one frame
					sfx->mStartTime++;
				}

				mFxSchedule.push_front(sfx);
			}
		}
	}

	// We track effect templates and primitive templates separately.
	if (fx->mCopy)
	{
		// We don't use dynamic memory allocation, so just mark us as dead
		fx->mInUse = false;
	}
}

//------------------------------------------------------
// PlayEffect
//	Handles scheduling an effect so all the components
//	happen at the specified time.  Applies a default up
//	axis.
//
// Input:
//	Effect file name and the origin
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::PlayEffect(const char* file, vec3_t origin, const bool isPortal)
{
	char sfile[MAX_QPATH];

	// Get an extenstion stripped version of the file
	COM_StripExtension(file, sfile, sizeof sfile);

	PlayEffect(mEffectIDs[sfile], origin, isPortal);

#ifndef FINAL_BUILD
	if (mEffectIDs[sfile] == 0)
	{
		theFxHelper.Print("CFxScheduler::PlayEffect unregistered/non-existent effect: %s\n", file);
	}
#endif
}

//------------------------------------------------------
// PlayEffect
//	Handles scheduling an effect so all the components
//	happen at the specified time.  Takes a forward vector
//	and uses this to complete the axis field.
//
// Input:
//	Effect file name, the origin, and a forward vector
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::PlayEffect(const char* file, vec3_t origin, vec3_t forward, const bool isPortal)
{
	char sfile[MAX_QPATH];

	// Get an extenstion stripped version of the file
	COM_StripExtension(file, sfile, sizeof sfile);

	PlayEffect(mEffectIDs[sfile], origin, forward, isPortal);

#ifndef FINAL_BUILD
	if (mEffectIDs[sfile] == 0)
	{
		theFxHelper.Print("CFxScheduler::PlayEffect unregistered/non-existent effect: %s\n", file);
	}
#endif
}

//------------------------------------------------------
// AddScheduledEffects
//	Handles determining if a scheduled effect should
//	be created or not.  If it should it handles converting
//	the template effect into a real one.
//
// Input:
//	boolean portal (true when adding effects to be drawn in the skyportal)
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::AddScheduledEffects(const bool portal)
{
	int oldEntNum = -1, old_bolt_index = -1, old_model_num = -1;
	qboolean does_bolt_exist = qfalse;

	if (portal)
	{
		gEffectsInPortal = true;
	}
	else
	{
		AddLoopedEffects();
	}

	for (auto itr = mFxSchedule.begin(); itr != mFxSchedule.end(); /* do nothing */)
	{
		SScheduledEffect* effect = *itr;

		if (portal == effect->mPortalEffect && effect->mStartTime <= theFxHelper.mTime)
		{
			if (effect->mClientID >= 0)
			{
				CreateEffect(effect->mpTemplate, effect->mClientID);
			}
			else if (effect->mBoltNum == -1)
			{
				// normal effect
				if (effect->mEntNum != -1) // -1
				{
					// Find out where the entity currently is
					CreateEffect(effect->mpTemplate,
						cg_entities[effect->mEntNum].lerpOrigin, effect->mAxis,
						theFxHelper.mTime - (*itr)->mStartTime);
				}
				else
				{
					CreateEffect(effect->mpTemplate,
						effect->mOrigin, effect->mAxis,
						theFxHelper.mTime - effect->mStartTime);
				}
			}
			else
			{
				vec3_t axis[3];
				vec3_t origin;
				//bolted on effect
				// do we need to go and re-get the bolt matrix again? Since it takes time lets try to do it only once
				if (effect->mModelNum != old_model_num || effect->mEntNum != oldEntNum || effect->mBoltNum !=
					old_bolt_index)
				{
					const centity_t& cent = cg_entities[effect->mEntNum];
					if (cent.gent->ghoul2.IsValid())
					{
						if (effect->mModelNum >= 0 && effect->mModelNum < cent.gent->ghoul2.size())
						{
							if (cent.gent->ghoul2[effect->mModelNum].mModelindex >= 0)
							{
								does_bolt_exist = static_cast<qboolean>(theFxHelper.GetOriginAxisFromBolt(
									cent, effect->mModelNum, effect->mBoltNum, origin, axis) != 0);
							}
						}
					}

					old_model_num = effect->mModelNum;
					oldEntNum = effect->mEntNum;
					old_bolt_index = effect->mBoltNum;
				}

				// only do this if we found the bolt
				if (does_bolt_exist)
				{
					if (effect->mIsRelative)
					{
						CreateEffect(effect->mpTemplate,
							vec3_origin, axis,
							0, effect->mEntNum, effect->mModelNum, effect->mBoltNum);
					}
					else
					{
						CreateEffect(effect->mpTemplate,
							origin, axis,
							theFxHelper.mTime - effect->mStartTime);
					}
				}
			}

			mScheduledEffectsPool.Free(effect);
			itr = mFxSchedule.erase(itr);
		}
		else
		{
			++itr;
		}
	}

	// Add all active effects into the scene
	FX_Add(portal);

	gEffectsInPortal = false;
}

//------------------------------------------------------
// CreateEffect
//	Creates the specified fx taking into account the
//	multitude of different ways it could be spawned.
//
// Input:
//	template used to build the effect, desired effect origin,
//	desired orientation and how late the effect is so that
//	it can be moved to the correct spot
//
// Return:
//	none
//------------------------------------------------------
void CFxScheduler::CreateEffect(CPrimitiveTemplate* fx, const vec3_t origin, vec3_t axis[3], const int late_time,
	const int client_id,
	const int model_num, const int bolt_num)
{
	vec3_t org, org2, temp,
		vel, accel,
		s_rgb, e_rgb,
		ang, ang_delta,
		ax[3];
	trace_t tr;
	int emitter_model;

	// We may modify the axis, so make a work copy
	AxisCopy(axis, ax);

	int flags = fx->mFlags;
	if (client_id >= 0 && model_num >= 0 && bolt_num >= 0)
	{
		//since you passed in these values, mark as relative to use them
		flags |= FX_RELATIVE;
	}

	if (fx->mSpawnFlags & FX_RAND_ROT_AROUND_FWD)
	{
		RotatePointAroundVector(ax[1], ax[0], axis[1], Q_flrand(0.0f, 1.0f) * 360.0f);
		CrossProduct(ax[0], ax[1], ax[2]);
	}

	// Origin calculations
	//-------------------------------------
	if (fx->mSpawnFlags & FX_CHEAP_ORG_CALC || flags & FX_RELATIVE)
	{
		// let's take the easy way out
		VectorSet(org, fx->mOrigin1X.GetVal(), fx->mOrigin1Y.GetVal(), fx->mOrigin1Z.GetVal());
	}
	else
	{
		// time for some extra work
		VectorScale(ax[0], fx->mOrigin1X.GetVal(), org);
		VectorMA(org, fx->mOrigin1Y.GetVal(), ax[1], org);
		VectorMA(org, fx->mOrigin1Z.GetVal(), ax[2], org);
	}

	// We always add our calculated offset to the passed in origin...
	VectorAdd(org, origin, org);

	// Now, we may need to calc a point on a sphere/ellipsoid/cylinder/disk and add that to it
	//----------------------------------------------------------------
	if (fx->mSpawnFlags & FX_ORG_ON_SPHERE)
	{
		const float x = DEG2RAD(Q_flrand(0.0f, 1.0f) * 360.0f);
		const float y = DEG2RAD(Q_flrand(0.0f, 1.0f) * 180.0f);

		const float width = fx->mRadius.GetVal();
		const float height = fx->mHeight.GetVal();

		// calculate point on ellipse
		VectorSet(temp, sin(x) * width * sin(y), cos(x) * width * sin(y), cos(y) * height);
		// sinx * siny, cosx * siny, cosy
		VectorAdd(org, temp, org);

		if (fx->mSpawnFlags & FX_AXIS_FROM_SPHERE)
		{
			// well, we will now override the axis at the users request
			VectorNormalize2(temp, ax[0]);
			MakeNormalVectors(ax[0], ax[1], ax[2]);
		}
	}
	else if (fx->mSpawnFlags & FX_ORG_ON_CYLINDER)
	{
		vec3_t pt;

		// set up our point, then rotate around the current direction to.  Make unrotated cylinder centered around 0,0,0
		VectorScale(ax[1], fx->mRadius.GetVal(), pt);
		VectorMA(pt, Q_flrand(-1.0f, 1.0f) * 0.5f * fx->mHeight.GetVal(), ax[0], pt);
		RotatePointAroundVector(temp, ax[0], pt, Q_flrand(0.0f, 1.0f) * 360.0f);

		VectorAdd(org, temp, org);

		if (fx->mSpawnFlags & FX_AXIS_FROM_SPHERE)
		{
			vec3_t up = { 0, 0, 1 };

			// well, we will now override the axis at the users request
			VectorNormalize2(temp, ax[0]);

			if (ax[0][2] == 1.0f)
			{
				// readjust up
				VectorSet(up, 0, 1, 0);
			}

			CrossProduct(up, ax[0], ax[1]);
			CrossProduct(ax[0], ax[1], ax[2]);
		}
	}

	// There are only a few types that really use velocity and acceleration, so do extra work for those types
	//--------------------------------------------------------------------------------------------------------
	if (fx->mType == Particle || fx->mType == OrientedParticle || fx->mType == Tail || fx->mType == Emitter)
	{
		// Velocity calculations
		//-------------------------------------
		if (fx->mSpawnFlags & FX_VEL_IS_ABSOLUTE || flags & FX_RELATIVE)
		{
			VectorSet(vel, fx->mVelX.GetVal(), fx->mVelY.GetVal(), fx->mVelZ.GetVal());
		}
		else
		{
			// bah, do some extra work to coerce it
			VectorScale(ax[0], fx->mVelX.GetVal(), vel);
			VectorMA(vel, fx->mVelY.GetVal(), ax[1], vel);
			VectorMA(vel, fx->mVelZ.GetVal(), ax[2], vel);
		}

		// Acceleration calculations
		//-------------------------------------
		if (fx->mSpawnFlags & FX_ACCEL_IS_ABSOLUTE || flags & FX_RELATIVE)
		{
			VectorSet(accel, fx->mAccelX.GetVal(), fx->mAccelY.GetVal(), fx->mAccelZ.GetVal());
		}
		else
		{
			VectorScale(ax[0], fx->mAccelX.GetVal(), accel);
			VectorMA(accel, fx->mAccelY.GetVal(), ax[1], accel);
			VectorMA(accel, fx->mAccelZ.GetVal(), ax[2], accel);
		}

		// Gravity is completely decoupled from acceleration since it is __always__ absolute
		// NOTE: I only effect Z ( up/down in the Quake world )
		accel[2] += fx->mGravity.GetVal();

		// There may be a lag between when the effect should be created and when it actually gets created.
		//	Since we know what the discrepancy is, we can attempt to compensate...
		if (late_time > 0)
		{
			// Calc the time differences
			const float ftime = late_time * 0.001f;
			const float time2 = ftime * ftime * 0.5f;

			VectorMA(vel, ftime, accel, vel);

			// Predict the new position
			for (int i = 0; i < 3; i++)
			{
				org[i] = org[i] + ftime * vel[i] + time2 * vel[i];
			}
		}
	} // end moving types

	// Line type primitives work with an origin2, so do the extra work for them
	//--------------------------------------------------------------------------
	if (fx->mType == Line || fx->mType == Electricity)
	{
		// We may have to do a trace to find our endpoint
		if (fx->mSpawnFlags & FX_ORG2_FROM_TRACE)
		{
			VectorMA(org, FX_MAX_TRACE_DIST, ax[0], temp);

			if (fx->mSpawnFlags & FX_ORG2_IS_OFFSET)
			{
				// add a random flair to the endpoint...note: org2 will have to be pretty large to affect this much
				// we also do this pre-trace as opposed to post trace since we may have to render an impact effect
				//	and we will want the normal at the exact endpos...
				if (fx->mSpawnFlags & FX_CHEAP_ORG2_CALC || flags & FX_RELATIVE)
				{
					VectorSet(org2, fx->mOrigin2X.GetVal(), fx->mOrigin2Y.GetVal(), fx->mOrigin2Z.GetVal());
					VectorAdd(org2, temp, temp);
				}
				else
				{
					// I can only imagine a few cases where you might want to do this...
					VectorMA(temp, fx->mOrigin2X.GetVal(), ax[0], temp);
					VectorMA(temp, fx->mOrigin2Y.GetVal(), ax[1], temp);
					VectorMA(temp, fx->mOrigin2Z.GetVal(), ax[2], temp);
				}
			}

			theFxHelper.Trace(&tr, org, nullptr, nullptr, temp, -1, CONTENTS_SOLID | CONTENTS_SHOTCLIP); //MASK_SHOT );

			if (tr.startsolid || tr.allsolid)
			{
				VectorCopy(org, org2); // this is not a very good solution
			}
			else
			{
				VectorCopy(tr.endpos, org2);
			}

			if (fx->mSpawnFlags & FX_TRACE_IMPACT_FX)
			{
				PlayEffect(fx->mImpactFxHandles.GetHandle(), org2, tr.plane.normal);
			}
		}
		else
		{
			if (fx->mSpawnFlags & FX_CHEAP_ORG2_CALC || flags & FX_RELATIVE)
			{
				VectorSet(org2, fx->mOrigin2X.GetVal(), fx->mOrigin2Y.GetVal(), fx->mOrigin2Z.GetVal());
			}
			else
			{
				VectorScale(ax[0], fx->mOrigin2X.GetVal(), org2);
				VectorMA(org2, fx->mOrigin2Y.GetVal(), ax[1], org2);
				VectorMA(org2, fx->mOrigin2Z.GetVal(), ax[2], org2);

				VectorAdd(org2, origin, org2);
			}
		}
	} // end special org2 types

	// handle RGB color, but only for types that will use it
	//---------------------------------------------------------------------------
	if (fx->mType != Sound && fx->mType != FxRunner && fx->mType != CameraShake)
	{
		if (fx->mSpawnFlags & FX_RGB_COMPONENT_INTERP)
		{
			const float perc = Q_flrand(0.0f, 1.0f);

			VectorSet(s_rgb, fx->mRedStart.GetVal(perc), fx->mGreenStart.GetVal(perc), fx->mBlueStart.GetVal(perc));
			VectorSet(e_rgb, fx->mRedEnd.GetVal(perc), fx->mGreenEnd.GetVal(perc), fx->mBlueEnd.GetVal(perc));
		}
		else
		{
			VectorSet(s_rgb, fx->mRedStart.GetVal(), fx->mGreenStart.GetVal(), fx->mBlueStart.GetVal());
			VectorSet(e_rgb, fx->mRedEnd.GetVal(), fx->mGreenEnd.GetVal(), fx->mBlueEnd.GetVal());
		}
	}

	// Now create the appropriate effect entity
	//------------------------
	switch (fx->mType)
	{
		//---------
	case Particle:
		//---------

		FX_AddParticle(client_id, org, vel, accel, fx->mGravity.GetVal(),
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mRotation.GetVal(), fx->mRotationDelta.GetVal(),
			fx->mMin, fx->mMax, fx->mElasticity.GetVal(),
			fx->mDeathFxHandles.GetHandle(), fx->mImpactFxHandles.GetHandle(),
			fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), flags, model_num, bolt_num);
		break;

		//---------
	case Line:
		//---------

		FX_AddLine(client_id, org, org2,
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), fx->mImpactFxHandles.GetHandle(), flags, model_num,
			bolt_num);
		break;

		//---------
	case Tail:
		//---------

		FX_AddTail(client_id, org, vel, accel,
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mLengthStart.GetVal(), fx->mLengthEnd.GetVal(), fx->mLengthParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mMin, fx->mMax, fx->mElasticity.GetVal(),
			fx->mDeathFxHandles.GetHandle(), fx->mImpactFxHandles.GetHandle(),
			fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), flags, model_num, bolt_num);
		break;

		//----------------
	case Electricity:
		//----------------

		FX_AddElectricity(client_id, org, org2,
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mElasticity.GetVal(), fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), flags, model_num,
			bolt_num);
		break;

		//---------
	case Cylinder:
		//---------

		FX_AddCylinder(client_id, org, ax[0],
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mSize2Start.GetVal(), fx->mSize2End.GetVal(), fx->mSize2Parm.GetVal(),
			fx->mLengthStart.GetVal(), fx->mLengthEnd.GetVal(), fx->mLengthParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), flags, model_num, bolt_num);
		break;

		//---------
	case Emitter:
		//---------

		// for chunk angles, you don't really need much control over the end result...you just want variation..
		VectorSet(ang,
			fx->mAngle1.GetVal(),
			fx->mAngle2.GetVal(),
			fx->mAngle3.GetVal());

		vectoangles(ax[0], temp);
		VectorAdd(ang, temp, ang);

		VectorSet(ang_delta,
			fx->mAngle1Delta.GetVal(),
			fx->mAngle2Delta.GetVal(),
			fx->mAngle3Delta.GetVal());

		emitter_model = fx->mMediaHandles.GetHandle();

		FX_AddEmitter(org, vel, accel,
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			ang, ang_delta,
			fx->mMin, fx->mMax, fx->mElasticity.GetVal(),
			fx->mDeathFxHandles.GetHandle(), fx->mImpactFxHandles.GetHandle(),
			fx->mEmitterFxHandles.GetHandle(),
			fx->mDensity.GetVal(), fx->mVariance.GetVal(),
			fx->mLife.GetVal(), emitter_model, flags);
		break;

		//---------
	case Decal:
		//---------

		// I'm calling this function ( at least for now ) because it handles projecting
		//	the decal mark onto the surfaces properly.  This is especially important for large marks.
		// The downside is that it's much less flexible....
		CG_ImpactMark(fx->mMediaHandles.GetHandle(), org, ax[0], fx->mRotation.GetVal(),
			s_rgb[0], s_rgb[1], s_rgb[2], fx->mAlphaStart.GetVal(),
			qtrue, fx->mSizeStart.GetVal(), qfalse);

		if (fx->mFlags & FX_GHOUL2_DECALS)
		{
			trace_t trace;
			vec3_t end;

			VectorMA(org, 64, ax[0], end);

			theFxHelper.G2Trace(&trace, org, nullptr, nullptr, end, ENTITYNUM_NONE, MASK_PLAYERSOLID);

			if (trace.entityNum < ENTITYNUM_WORLD &&
				g_entities[trace.entityNum].ghoul2.size())
			{
				gentity_t* ent = &g_entities[trace.entityNum];

				if (ent != nullptr)
				{
					if (!(ent->s.eFlags & EF_NODRAW))
					{
						constexpr float first_model = 0;
						float ent_yaw;
						vec3_t ent_org;
						//not drawn, no marks
						if (ent->client)
						{
							VectorCopy(ent->client->ps.origin, ent_org);
						}
						else
						{
							VectorCopy(ent->currentOrigin, ent_org);
						}
						if (ent->client)
						{
							ent_yaw = ent->client->ps.viewangles[YAW];
						}
						else
						{
							ent_yaw = ent->currentAngles[YAW];
						}
						//if ( VectorCompare( tr.plane.normal, vec3_origin ) )
						{
							vec3_t hit_dir;
							//hunh, no plane?  Use trace dir
							VectorCopy(ax[0], hit_dir);
						}
						/*
						else
						{
							VectorCopy( tr.plane.normal, hitDir );
						}
						*/

						CG_AddGhoul2Mark(fx->mMediaHandles.GetHandle(), fx->mSizeStart.GetVal(), trace.endpos,
							trace.plane.normal,
							trace.entityNum, ent_org, ent_yaw,
							ent->ghoul2, ent->s.modelScale, Q_irand(40000, 60000), first_model);
					}
				}
			}
		}
		break;

		//-------------------
	case OrientedParticle:
		//-------------------

		FX_AddOrientedParticle(client_id, org, ax[0], vel, accel,
			fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			fx->mAlphaStart.GetVal(), fx->mAlphaEnd.GetVal(), fx->mAlphaParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mRotation.GetVal(), fx->mRotationDelta.GetVal(),
			fx->mMin, fx->mMax, fx->mElasticity.GetVal(),
			fx->mDeathFxHandles.GetHandle(), fx->mImpactFxHandles.GetHandle(),
			fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), flags, model_num, bolt_num);
		break;

		//---------
	case Sound:
		//---------
		if (gEffectsInPortal)
		{
			//could orient this anyway for panning, but eh. It's going to appear to the player in the sky the same place no matter what, so just make it a local sound.
			theFxHelper.PlayLocalSound(fx->mMediaHandles.GetHandle(), CHAN_AUTO);
		}
		else if (fx->mSpawnFlags & FX_SND_LESS_ATTENUATION)
		{
			theFxHelper.PlaySound(org, ENTITYNUM_NONE, CHAN_LESS_ATTEN, fx->mMediaHandles.GetHandle());
		}
		else
		{
			theFxHelper.PlaySound(org, ENTITYNUM_NONE, CHAN_AUTO, fx->mMediaHandles.GetHandle());
		}
		break;
		//---------
	case FxRunner:
		//---------

		PlayEffect(fx->mPlayFxHandles.GetHandle(), org, ax);
		break;

		//---------
	case Light:
		//---------

		FX_AddLight(org, fx->mSizeStart.GetVal(), fx->mSizeEnd.GetVal(), fx->mSizeParm.GetVal(),
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mLife.GetVal(), fx->mFlags);
		break;

		//---------
	case CameraShake:
		//---------
		// It calculates how intense the shake should be based on how close you are to the origin you pass in here
		//	elasticity is actually the intensity...radius is the distance in which the shake will have some effect
		//	life is how long the effect lasts.
		theFxHelper.CameraShake(org, fx->mElasticity.GetVal(), fx->mRadius.GetVal(), fx->mLife.GetVal());
		break;

		//--------------
	case ScreenFlash:
		//--------------

		FX_AddFlash(org,
			s_rgb, e_rgb, fx->mRGBParm.GetVal(),
			fx->mLife.GetVal(), fx->mMediaHandles.GetHandle(), fx->mFlags);
		break;

	default:
		break;
	}

	// Track when we need to clean ourselves up if we are a copy
	if (fx->mCopy)
	{
		fx->mRefCount--;

		if (fx->mRefCount <= 0)
		{
			delete fx;
		}
	}
}