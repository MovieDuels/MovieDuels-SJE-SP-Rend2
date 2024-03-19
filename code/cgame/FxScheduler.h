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

#pragma once
#if !defined(FX_UTIL_H_INC)
#include "FxUtil.h"
#endif

#include "../qcommon/sstring.h"
using fxString_t = sstring_t;

#include "../game/genericparser2.h"
#include "qcommon/safe/string.h"

#include <algorithm>

#ifndef FX_SCHEDULER_H_INC
#define FX_SCHEDULER_H_INC

constexpr auto FX_FILE_PATH = "effects";

constexpr auto FX_MAX_TRACE_DIST = 32768; // sje uses a larger scale;
constexpr auto FX_MAX_EFFECTS = 1024; // how many effects the system can store;
constexpr auto FX_MAX_EFFECT_COMPONENTS = 48; // how many primitives an effect can hold, this should be plenty;
constexpr auto FX_MAX_PRIM_NAME = 64;

//-----------------------------------------------
// These are spawn flags for primitiveTemplates
//-----------------------------------------------

constexpr auto FX_ORG_ON_SPHERE = 0x00001; // Pretty dang expensive, calculates a point on a sphere/ellipsoid;
constexpr auto FX_AXIS_FROM_SPHERE = 0x00002;
// Can be used in conjunction with org_on_sphere to cause particles to move out;
//	from the center of the sphere
constexpr auto FX_ORG_ON_CYLINDER = 0x00004; // calculate point on cylinder/disk;

constexpr auto FX_ORG2_FROM_TRACE = 0x00010;
constexpr auto FX_TRACE_IMPACT_FX = 0x00020; // if trace impacts, we should play one of the specified impact fx files;
constexpr auto FX_ORG2_IS_OFFSET = 0x00040; // template specified org2 should be the offset from a trace endpos or;
//	passed in org2. You might use this to lend a random flair to the endpos.
//	Note: this is done pre-trace, so you may have to specify large numbers for this

constexpr auto FX_CHEAP_ORG_CALC = 0x00100; // Origin is calculated relative to passed in axis unless this is on.;
constexpr auto FX_CHEAP_ORG2_CALC = 0x00200; // Origin2 is calculated relative to passed in axis unless this is on.;
constexpr auto FX_VEL_IS_ABSOLUTE = 0x00400; // Velocity isn't relative to passed in axis with this flag on.;
constexpr auto FX_ACCEL_IS_ABSOLUTE = 0x00800; // Acceleration isn't relative to passed in axis with this flag on.;

constexpr auto FX_RAND_ROT_AROUND_FWD = 0x01000; // Randomly rotates up and right around forward vector;
constexpr auto FX_EVEN_DISTRIBUTION = 0x02000; // When you have a delay, it normally picks a random time to play.  When;
// this flag is on, it generates an even time distribution
constexpr auto FX_RGB_COMPONENT_INTERP = 0x04000;
// Picks a color on the line defined by RGB min & max, default is to pick color in cube defined by min & max;

constexpr auto FX_AFFECTED_BY_WIND = 0x10000; // we take into account our wind vector when we spawn in;

constexpr auto FX_SND_LESS_ATTENUATION = 0x20000; // attenuate sounds less;

//-----------------------------------------------------------------
//
// CMediaHandles
//
// Primitive templates might want to use a list of sounds, shaders
//	or models to get a bit more variation in their effects.
//
//-----------------------------------------------------------------
class CMediaHandles
{
private:
	std::vector<int> mMediaList;

public:
	void AddHandle(const int item) { mMediaList.push_back(item); }

	int GetHandle() const
	{
		if (mMediaList.size() == 0) { return 0; }
		return mMediaList[irand(0, static_cast<int>(mMediaList.size()) - 1)];
	}

	void operator=(const CMediaHandles& that);
};

//-----------------------------------------------------------------
//
// CFxRange
//
// Primitive templates typically use this class to define each of
//	its members.  This is done to make it easier to create effects
//	with a desired range of characteristics.
//
//-----------------------------------------------------------------
class CFxRange
{
private:
	float mMin;
	float mMax;

public:
	CFxRange()
	{
		mMin = 0;
		mMax = 0;
	}

	void SetRange(const float min, const float max)
	{
		mMin = min;
		mMax = max;
	}

	void SetMin(const float min) { mMin = min; }
	void SetMax(const float max) { mMax = max; }

	float GetMax() const { return mMax; }
	float GetMin() const { return mMin; }

	float GetVal(const float percent) const
	{
		if (mMin == mMax) { return mMin; }
		return mMin + (mMax - mMin) * percent;
	}

	float GetVal() const
	{
		if (mMin == mMax) { return mMin; }
		return flrand(mMin, mMax);
	}

	int GetRoundedVal() const
	{
		if (mMin == mMax) { return mMin; }
		return static_cast<int>((flrand(mMin, mMax) + 0.5f));
	}

	void ForceRange(const float min, const float max)
	{
		if (mMin < min) { mMin = min; }
		if (mMin > max) { mMin = max; }
		if (mMax < min) { mMax = min; }
		if (mMax > max) { mMax = max; }
	}

	void Sort()
	{
		if (mMin > mMax)
		{
			const float temp = mMin;
			mMin = mMax;
			mMax = temp;
		}
	}

	void operator=(const CFxRange& that)
	{
		mMin = that.mMin;
		mMax = that.mMax;
	}

	bool operator==(const CFxRange& rhs) const
	{
		return mMin == rhs.mMin &&
			mMax == rhs.mMax;
	}
};

//----------------------------
// Supported primitive types
//----------------------------

enum EPrimType
{
	None = 0,
	Particle,
	// sprite
	Line,
	Tail,
	// comet-like tail thing
	Cylinder,
	Emitter,
	// emits effects as it moves, can also attach a chunk
	Sound,
	Decal,
	// projected onto architecture
	OrientedParticle,
	Electricity,
	FxRunner,
	Light,
	CameraShake,
	ScreenFlash
};

//-----------------------------------------------------------------
//
// CPrimitiveTemplate
//
// The primitive template is used to spawn 1 or more fx primitives
//	with the range of characteristics defined by the template.
//
// As such, I just made this one huge shared class knowing that
//	there won't be many of them in memory at once, 	and we won't
//	be dynamically creating and deleting them mid-game.  Also,
//	note that not every primitive type will use all of these fields.
//
//-----------------------------------------------------------------
class CPrimitiveTemplate
{
public:
	// These kinds of things should not even be allowed to be accessed publicly
	bool mCopy;
	int mRefCount; // For a copy of a primitive...when we figure out how many items we want to spawn,
	//	we'll store that here and then decrement us for each we actually spawn.  When we
	//	hit zero, we are no longer used and so we can just free ourselves

	char mName[FX_MAX_PRIM_NAME];

	EPrimType mType;

	CFxRange mSpawnDelay;
	CFxRange mSpawnCount;
	CFxRange mLife;
	int mCullRange;

	CMediaHandles mMediaHandles;
	CMediaHandles mImpactFxHandles;
	CMediaHandles mDeathFxHandles;
	CMediaHandles mEmitterFxHandles;
	CMediaHandles mPlayFxHandles;

	int mFlags; // These need to get passed on to the primitive
	int mSpawnFlags; // These are only used to control spawning, but never get passed to prims.

	vec3_t mMin;
	vec3_t mMax;

	CFxRange mOrigin1X;
	CFxRange mOrigin1Y;
	CFxRange mOrigin1Z;

	CFxRange mOrigin2X;
	CFxRange mOrigin2Y;
	CFxRange mOrigin2Z;

	CFxRange mRadius; // spawn on sphere/ellipse/disk stuff.
	CFxRange mHeight;
	CFxRange mWindModifier;

	CFxRange mRotation;
	CFxRange mRotationDelta;

	CFxRange mAngle1;
	CFxRange mAngle2;
	CFxRange mAngle3;

	CFxRange mAngle1Delta;
	CFxRange mAngle2Delta;
	CFxRange mAngle3Delta;

	CFxRange mVelX;
	CFxRange mVelY;
	CFxRange mVelZ;

	CFxRange mAccelX;
	CFxRange mAccelY;
	CFxRange mAccelZ;

	CFxRange mGravity;

	CFxRange mDensity;
	CFxRange mVariance;

	CFxRange mRedStart;
	CFxRange mGreenStart;
	CFxRange mBlueStart;

	CFxRange mRedEnd;
	CFxRange mGreenEnd;
	CFxRange mBlueEnd;

	CFxRange mRGBParm;

	CFxRange mAlphaStart;
	CFxRange mAlphaEnd;
	CFxRange mAlphaParm;

	CFxRange mSizeStart;
	CFxRange mSizeEnd;
	CFxRange mSizeParm;

	CFxRange mSize2Start;
	CFxRange mSize2End;
	CFxRange mSize2Parm;

	CFxRange mLengthStart;
	CFxRange mLengthEnd;
	CFxRange mLengthParm;

	CFxRange mTexCoordS;
	CFxRange mTexCoordT;

	CFxRange mElasticity;

private:
	// Lower level parsing utilities
	static bool ParseVector(const gsl::cstring_view& val, vec3_t min, vec3_t max);
	static bool ParseFloat(const gsl::cstring_view& val, float& min, float& max);
	static bool ParseGroupFlags(const gsl::cstring_view& val, int& flags);

	// Base key processing
	// Note that these all have their own parse functions in case it becomes important to do certain kinds
	//	of validation specific to that type.
	bool ParseMin(const gsl::cstring_view& val);
	bool ParseMax(const gsl::cstring_view& val);
	bool ParseDelay(const gsl::cstring_view& val);
	bool ParseCount(const gsl::cstring_view& val);
	bool ParseLife(const gsl::cstring_view& val);
	bool ParseElasticity(const gsl::cstring_view& val);
	bool ParseFlags(const gsl::cstring_view& val);
	bool ParseSpawnFlags(const gsl::cstring_view& val);

	bool ParseOrigin1(const gsl::cstring_view& val);
	bool ParseOrigin2(const gsl::cstring_view& val);
	bool ParseRadius(const gsl::cstring_view& val);
	bool ParseHeight(const gsl::cstring_view& val);
	bool ParseWindModifier(const gsl::cstring_view& val);
	bool ParseRotation(const gsl::cstring_view& val);
	bool ParseRotationDelta(const gsl::cstring_view& val);
	bool ParseAngle(const gsl::cstring_view& val);
	bool ParseAngleDelta(const gsl::cstring_view& val);
	bool ParseVelocity(const gsl::cstring_view& val);
	bool ParseAcceleration(const gsl::cstring_view& val);
	bool ParseGravity(const gsl::cstring_view& val);
	bool ParseDensity(const gsl::cstring_view& val);
	bool ParseVariance(const gsl::cstring_view& val);

	/// Case insensitive map from cstring_view to Value
	template <typename Value>
	using StringViewIMap = std::map<gsl::cstring_view, Value, Q::CStringViewILess>;
	using ParseMethod = bool (CPrimitiveTemplate::*)(const gsl::cstring_view&);
	// Group type processing
	bool ParseGroup(const CGPGroup& grp, const StringViewIMap<ParseMethod>& parseMethods, gsl::czstring name);
	bool ParseRGB(const CGPGroup& grp);
	bool ParseAlpha(const CGPGroup& grp);
	bool ParseSize(const CGPGroup& grp);
	bool ParseSize2(const CGPGroup& grp);
	bool ParseLength(const CGPGroup& grp);

	bool ParseModels(const CGPProperty& grp);
	bool ParseShaders(const CGPProperty& grp);
	bool ParseSounds(const CGPProperty& grp);

	bool ParseImpactFxStrings(const CGPProperty& grp);
	bool ParseDeathFxStrings(const CGPProperty& grp);
	bool ParseEmitterFxStrings(const CGPProperty& grp);
	bool ParsePlayFxStrings(const CGPProperty& grp);

	// Group keys
	bool ParseRGBStart(const gsl::cstring_view& val);
	bool ParseRGBEnd(const gsl::cstring_view& val);
	bool ParseRGBParm(const gsl::cstring_view& val);
	bool ParseRGBFlags(const gsl::cstring_view& val);

	bool ParseAlphaStart(const gsl::cstring_view& val);
	bool ParseAlphaEnd(const gsl::cstring_view& val);
	bool ParseAlphaParm(const gsl::cstring_view& val);
	bool ParseAlphaFlags(const gsl::cstring_view& val);

	bool ParseSizeStart(const gsl::cstring_view& val);
	bool ParseSizeEnd(const gsl::cstring_view& val);
	bool ParseSizeParm(const gsl::cstring_view& val);
	bool ParseSizeFlags(const gsl::cstring_view& val);

	bool ParseSize2Start(const gsl::cstring_view& val);
	bool ParseSize2End(const gsl::cstring_view& val);
	bool ParseSize2Parm(const gsl::cstring_view& val);
	bool ParseSize2Flags(const gsl::cstring_view& val);

	bool ParseLengthStart(const gsl::cstring_view& val);
	bool ParseLengthEnd(const gsl::cstring_view& val);
	bool ParseLengthParm(const gsl::cstring_view& val);
	bool ParseLengthFlags(const gsl::cstring_view& val);

public:
	CPrimitiveTemplate();
	CPrimitiveTemplate(const CPrimitiveTemplate& rhs);

	~CPrimitiveTemplate()
	{
	};

	bool ParsePrimitive(const CGPGroup& grp);

	void operator=(const CPrimitiveTemplate& that);
};

// forward declaration
struct SEffectTemplate;

// Effects are built of one or more primitives
struct SEffectTemplate
{
	bool mInUse;
	bool mCopy;
	char mEffectName[MAX_QPATH]; // is this extraneous??
	int mPrimitiveCount;
	int mRepeatDelay;
	CPrimitiveTemplate* mPrimitives[FX_MAX_EFFECT_COMPONENTS];

	bool operator ==(const char* name) const
	{
		return !Q_stricmp(mEffectName, name);
	}

	void operator=(const SEffectTemplate& that);
};

template <typename T, int N>
class PoolAllocator
{
public:
	PoolAllocator()
		: pool(new T[N])
		, freeAndAllocated(new int[N])
		, numFree(N)
		, highWatermark(0)
	{
		for (int i = 0; i < N; i++)
		{
			freeAndAllocated[i] = i;
		}
	}

	T* Alloc()
	{
		if (numFree == 0)
		{
			return nullptr;
		}

		T* ptr = new(&pool[freeAndAllocated[0]]) T;

		std::rotate(freeAndAllocated, freeAndAllocated + 1, freeAndAllocated + N);
		numFree--;

		highWatermark = Q_max(highWatermark, N - numFree);

		return ptr;
	}

	void TransferTo(PoolAllocator<T, N>& allocator)
	{
		allocator.freeAndAllocated = freeAndAllocated;
		allocator.highWatermark = highWatermark;
		allocator.numFree = numFree;
		allocator.pool = pool;

		highWatermark = 0;
		numFree = N;
		freeAndAllocated = nullptr;
		pool = nullptr;
	}

	bool OwnsPtr(const T* ptr) const
	{
		return ptr >= pool && ptr < pool + N;
	}

	void Free(T* ptr)
	{
		for (int i = numFree; i < N; i++)
		{
			T* p = &pool[freeAndAllocated[i]];

			if (p == ptr)
			{
				if (i > numFree)
				{
					std::rotate(freeAndAllocated + numFree, freeAndAllocated + i, freeAndAllocated + i + 1);
				}

				p->~T();
				numFree++;

				break;
			}
		}
	}

	int GetHighWatermark() const { return highWatermark; }

	~PoolAllocator()
	{
		for (int i = numFree; i < N; i++)
		{
			T* p = &pool[freeAndAllocated[i]];

			p->~T();
		}

		delete[] freeAndAllocated;
		delete[] pool;
	}

private:
	PoolAllocator(const PoolAllocator<T, N>&);
	PoolAllocator& operator =(const PoolAllocator<T, N>&);

	T* pool;

	// The first 'numFree' elements are the indexes of the free slots.
	// The remaining elements are the indexes of the allocated slots.
	int* freeAndAllocated;
	int numFree;

	int highWatermark;
};

template <typename T, int N>
class PagedPoolAllocator
{
public:
	PagedPoolAllocator()
		: numPages(1)
		, pages(new PoolAllocator<T, N>[1]())
	{
	}

	T* Alloc()
	{
		T* ptr = nullptr;
		for (int i = 0; i < numPages && ptr == nullptr; i++)
		{
			ptr = pages[i].Alloc();
		}

		if (ptr == nullptr)
		{
			PoolAllocator<T, N>* newPages = new PoolAllocator<T, N>[numPages + 1]();
			for (int i = 0; i < numPages; i++)
			{
				pages[i].TransferTo(newPages[i]);
			}

			delete[] pages;
			pages = newPages;

			ptr = pages[numPages].Alloc();
			if (ptr == nullptr)
			{
				return nullptr;
			}

			numPages++;
		}

		return ptr;
	}

	void Free(T* ptr)
	{
		for (int i = 0; i < numPages; i++)
		{
			if (pages[i].OwnsPtr(ptr))
			{
				pages[i].Free(ptr);
				break;
			}
		}
	}

	int GetHighWatermark() const
	{
		int total = 0;
		for (int i = 0; i < numPages; i++)
		{
			total += pages[i].GetHighWatermark();
		}

		return total;
	}

	~PagedPoolAllocator()
	{
		delete[] pages;
	}

private:
	int numPages;
	PoolAllocator<T, N>* pages;
};

//-----------------------------------------------------------------
//
// CFxScheduler
//
// The scheduler not only handles requests to play an effect, it
//	tracks the request throughout its life if necessary, creating
//	any of the delayed components as needed.
//
//-----------------------------------------------------------------
// needs to be in global space now (loadsave stuff)

constexpr auto MAX_LOOPED_FX = 32;

// We hold a looped effect here
struct SLoopedEffect
{
	int mId; // effect id
	int mBoltInfo; // used to determine which bolt on the ghoul2 model we should be attaching this effect to
	int mNextTime; //time to render again
	int mLoopStopTime; //time to die
	bool mPortalEffect; // rww - render this before skyportals, and not in the normal world view.
	bool mIsRelative; // bolt this puppy on keep it updated

	void sg_export(
		ojk::SavedGameHelper& saved_game) const
	{
		saved_game.write<int32_t>(mId);
		saved_game.write<int32_t>(mBoltInfo);
		saved_game.write<int32_t>(mNextTime);
		saved_game.write<int32_t>(mLoopStopTime);
		saved_game.write<int8_t>(mPortalEffect);
		saved_game.write<int8_t>(mIsRelative);
		saved_game.skip(2);
	}

	void sg_import(
		ojk::SavedGameHelper& saved_game)
	{
		saved_game.read<int32_t>(mId);
		saved_game.read<int32_t>(mBoltInfo);
		saved_game.read<int32_t>(mNextTime);
		saved_game.read<int32_t>(mLoopStopTime);
		saved_game.read<int8_t>(mPortalEffect);
		saved_game.read<int8_t>(mIsRelative);
		saved_game.skip(2);
	}
};

class CFxScheduler
{
private:
	// We hold a scheduled effect here
	struct SScheduledEffect
	{
		CPrimitiveTemplate* mpTemplate; // primitive template
		int mStartTime;
		char mModelNum; // uset to determine which ghoul2 model we want to bolt this effect to
		char mBoltNum; // used to determine which bolt on the ghoul2 model we should be attaching this effect to
		short mEntNum; // used to determine which entity this ghoul model is attached to.
		short mClientID; // FIXME: redundant. this is used for muzzle bolts, merge into normal bolting
		bool mPortalEffect; // rww - render this before skyportals, and not in the normal world view.
		bool mIsRelative; // bolt this puppy on keep it updated
		vec3_t mOrigin;
		vec3_t mAxis[3];
	};

	/* Looped Effects get stored and reschedule at mRepeatRate */

	// must be in sync with gLoopedEffectArray[MAX_LOOPED_FX]!
	//
	SLoopedEffect mLoopedEffectArray[MAX_LOOPED_FX];

	int ScheduleLoopedEffect(int id, int boltInfo, bool isPortal, int i_loop_time, bool is_relative);
	void AddLoopedEffects();

	// this makes looking up the index based on the string name much easier
	using TEffectID = std::map<fxString_t, int>;

	using TScheduledEffect = std::list<SScheduledEffect*>;

	// Effects
	SEffectTemplate mEffectTemplates[FX_MAX_EFFECTS];
	TEffectID mEffectIDs; // if you only have the unique effect name, you'll have to use this to get the ID.

	// List of scheduled effects that will need to be created at the correct time.
	TScheduledEffect mFxSchedule;

	PagedPoolAllocator<SScheduledEffect, 1024> mScheduledEffectsPool;

	// Private function prototypes
	SEffectTemplate* GetNewEffectTemplate(int* id, const char* file);

	static void AddPrimitiveToEffect(SEffectTemplate* fx, CPrimitiveTemplate* prim);
	int ParseEffect(const char* file, const CGPGroup& base);

	void CreateEffect(CPrimitiveTemplate* fx, const vec3_t origin, vec3_t axis[3], int late_time, int client_id = -1,
		int model_num = -1, int bolt_num = -1);
	void CreateEffect(CPrimitiveTemplate* fx, int client_id) const;

public:
	CFxScheduler();

	void LoadSave_Read();
	void LoadSave_Write();
	void FX_CopeWithAnyLoadedSaveGames();

	int RegisterEffect(const char* path, bool b_has_correct_path = false); // handles pre-caching

	// Nasty overloaded madness
	void PlayEffect(int id, vec3_t origin, bool isPortal = false); // uses a default up axis
	void PlayEffect(int id, vec3_t origin, vec3_t forward, bool isPortal = false);
	// builds arbitrary perp. right vector, does a cross product to define up
	void PlayEffect(int id, vec3_t origin, vec3_t axis[3], int boltInfo = -1, int entNum = -1, bool isPortal = false,
		int i_loop_time = false, bool is_relative = false);
	void PlayEffect(const char* file, vec3_t origin, bool isPortal = false); // uses a default up axis
	void PlayEffect(const char* file, vec3_t origin, vec3_t forward, bool isPortal = false);
	// builds arbitrary perp. right vector, does a cross product to define up
	void PlayEffect(const char* file, vec3_t origin, vec3_t axis[3], int boltInfo, int entNum, bool isPortal = false,
		int i_loop_time = false, bool is_relative = false);

	//for muzzle
	void PlayEffect(const char* file, int client_id, bool isPortal = false);

	void StopEffect(const char* file, int boltInfo, bool isPortal = false);
	//find a scheduled Looping effect with these parms and kill it

	void AddScheduledEffects(bool portal);
	// call once per CGame frame [rww ammendment - twice now actually, but first only renders portal effects]

	int NumScheduledFx() const { return static_cast<int>(mFxSchedule.size()); }
	void Clean(bool b_remove_templates = true, int id_to_preserve = 0); // clean out the system

	// FX Override functions
	SEffectTemplate* GetEffectCopy(int fx_handle, int* new_handle);
	SEffectTemplate* GetEffectCopy(const char* file, int* new_handle);

	static CPrimitiveTemplate* GetPrimitiveCopy(const SEffectTemplate* effect_copy, const char* component_name);
};

//-------------------
// The one and only
//-------------------
extern CFxScheduler theFxScheduler;

#endif // FX_SCHEDULER_H_INC
