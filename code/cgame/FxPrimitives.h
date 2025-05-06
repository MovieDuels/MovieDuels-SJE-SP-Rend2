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
#if !defined(FX_SYSTEM_H_INC)
#include "FxSystem.h"
#endif

#ifndef FX_PRIMITIVES_H_INC
#define FX_PRIMITIVES_H_INC

constexpr auto MAX_EFFECTS = 1200;

// Generic group flags, used by parser, then get converted to the appropriate specific flags
constexpr auto FX_PARM_MASK = 0xC; // use this to mask off any transition types that use a parm;
constexpr auto FX_GENERIC_MASK = 0xF;
constexpr auto FX_LINEAR = 0x1;
constexpr auto FX_RAND = 0x2;
constexpr auto FX_NONLINEAR = 0x4;
constexpr auto FX_WAVE = 0x8;
constexpr auto FX_CLAMP = 0xC;

// Group flags
constexpr auto FX_ALPHA_SHIFT = 0;
constexpr auto FX_ALPHA_PARM_MASK = 0x0000000C;
constexpr auto FX_ALPHA_LINEAR = 0x00000001;
constexpr auto FX_ALPHA_RAND = 0x00000002;
constexpr auto FX_ALPHA_NONLINEAR = 0x00000004;
constexpr auto FX_ALPHA_WAVE = 0x00000008;
constexpr auto FX_ALPHA_CLAMP = 0x0000000C;

constexpr auto FX_RGB_SHIFT = 4;
constexpr auto FX_RGB_PARM_MASK = 0x000000C0;
constexpr auto FX_RGB_LINEAR = 0x00000010;
constexpr auto FX_RGB_RAND = 0x00000020;
constexpr auto FX_RGB_NONLINEAR = 0x00000040;
constexpr auto FX_RGB_WAVE = 0x00000080;
constexpr auto FX_RGB_CLAMP = 0x000000C0;

constexpr auto FX_SIZE_SHIFT = 8;
constexpr auto FX_SIZE_PARM_MASK = 0x00000C00;
constexpr auto FX_SIZE_LINEAR = 0x00000100;
constexpr auto FX_SIZE_RAND = 0x00000200;
constexpr auto FX_SIZE_NONLINEAR = 0x00000400;
constexpr auto FX_SIZE_WAVE = 0x00000800;
constexpr auto FX_SIZE_CLAMP = 0x00000C00;

constexpr auto FX_LENGTH_SHIFT = 12;
constexpr auto FX_LENGTH_PARM_MASK = 0x0000C000;
constexpr auto FX_LENGTH_LINEAR = 0x00001000;
constexpr auto FX_LENGTH_RAND = 0x00002000;
constexpr auto FX_LENGTH_NONLINEAR = 0x00004000;
constexpr auto FX_LENGTH_WAVE = 0x00008000;
constexpr auto FX_LENGTH_CLAMP = 0x0000C000;

constexpr auto FX_SIZE2_SHIFT = 16;
constexpr auto FX_SIZE2_PARM_MASK = 0x000C0000;
constexpr auto FX_SIZE2_LINEAR = 0x00010000;
constexpr auto FX_SIZE2_RAND = 0x00020000;
constexpr auto FX_SIZE2_NONLINEAR = 0x00040000;
constexpr auto FX_SIZE2_WAVE = 0x00080000;
constexpr auto FX_SIZE2_CLAMP = 0x000C0000;

// Feature flags
constexpr auto FX_DEPTH_HACK = 0x00100000;
constexpr auto FX_RELATIVE = 0x00200000;
constexpr auto FX_SET_SHADER_TIME = 0x00400000;
// by having the effects system set the shader time, we can make animating textures start at the correct time;
constexpr auto FX_EXPENSIVE_PHYSICS = 0x00800000;

//rww - g2-related flags (these can slow things down significantly, use sparingly)
//These should be used only with particles/decals as they steal flags used by cylinders.
constexpr auto FX_GHOUL2_TRACE = 0x00020000;
//use in conjunction with particles - actually do full ghoul2 traces for physics collision against entities with a ghoul2 instance;
//shared FX_SIZE2_RAND (used only with cylinders)
constexpr auto FX_GHOUL2_DECALS = 0x00040000;
//use in conjunction with decals - can project decal as a ghoul2 gore skin object onto ghoul2 models;
//shared FX_SIZE2_NONLINEAR (used only with cylinders)

constexpr auto FX_ATTACHED_MODEL = 0x01000000;

constexpr auto FX_APPLY_PHYSICS = 0x02000000;
constexpr auto FX_USE_BBOX = 0x04000000; // can make physics more accurate at the expense of speed;

constexpr auto FX_USE_ALPHA = 0x08000000; // the FX system actually uses RGB to do fades, but this will override that;
//	and cause it to fill in the alpha.

constexpr auto FX_EMIT_FX = 0x10000000; // emitters technically don't have to emit stuff, but when they do;
//	this flag needs to be set
constexpr auto FX_DEATH_RUNS_FX = 0x20000000; // Normal death triggers effect, but not kill_on_impact;
constexpr auto FX_KILL_ON_IMPACT = 0x40000000; // works just like it says, but only when physics are on.;
constexpr auto FX_IMPACT_RUNS_FX = 0x80000000; // an effect can call another effect when it hits something.;

// Lightning flags, duplicates of existing flags, but lightning doesn't use those flags in that context...and nothing will ever use these in this context..so we are safe.
constexpr auto FX_TAPER = 0x01000000; // tapers as it moves towards its endpoint;
constexpr auto FX_BRANCH = 0x02000000; // enables lightning branching;
constexpr auto FX_GROW = 0x04000000; // lightning grows from start point to end point over the course of its life;

//------------------------------
class CEffect
{
protected:
	vec3_t mOrigin1;

	int mTimeStart;
	int mTimeEnd;

	unsigned int mFlags;

	// Size of our object, useful for things that have physics
	vec3_t mMin;
	vec3_t mMax;

	int mImpactFxID; // if we have an impact event, we may have to call an effect
	int mDeathFxID; // if we have a death event, we may have to call an effect

	refEntity_t mRefEnt;

public:
	CEffect() : mOrigin1{}, mTimeStart(0), mTimeEnd(0), mFlags(0), mMin{}, mMax{}, mImpactFxID(0), mDeathFxID(0)
	{
		memset(&mRefEnt, 0, sizeof(refEntity_t));
	}

	virtual ~CEffect()
	{
	}

	virtual void Die()
	{
	}

	virtual bool Update()
	{
		// Game pausing can cause dumb time things to happen, so kill the effect in this instance
		if (mTimeStart > theFxHelper.mTime)
		{
			return false;
		}
		return true;
	}

	void SetSTScale(const float s, const float t)
	{
		mRefEnt.shaderTexCoord[0] = s;
		mRefEnt.shaderTexCoord[1] = t;
	}

	void SetMin(const vec3_t min)
	{
		if (min) { VectorCopy(min, mMin); }
		else { VectorClear(mMin); }
	}

	void SetMax(const vec3_t max)
	{
		if (max) { VectorCopy(max, mMax); }
		else { VectorClear(mMax); }
	}

	void SetFlags(const int flags) { mFlags = flags; }
	void AddFlags(const int flags) { mFlags |= flags; }
	void ClearFlags(const int flags) { mFlags &= ~flags; }

	void SetOrigin1(const vec3_t org)
	{
		if (org) { VectorCopy(org, mOrigin1); }
		else { VectorClear(mOrigin1); }
	}

	void SetTimeStart(const int time)
	{
		mTimeStart = time;
		if (mFlags & FX_SET_SHADER_TIME) { mRefEnt.shaderTime = cg.time * 0.001f; }
	}

	void SetTimeEnd(const int time) { mTimeEnd = time; }
	void SetImpactFxID(const int id) { mImpactFxID = id; }
	void SetDeathFxID(const int id) { mDeathFxID = id; }
};

//---------------------------------------------------
// This class is kind of an exception to the "rule".
//	For now it exists only for allowing an easy way
//	to get the saber slash trails rendered.
//---------------------------------------------------
class CTrail : public CEffect
{
	// This is such a specific case thing, just grant public access to the goods.
protected:
	void Draw() const;

public:
	using TVert = struct
	{
		vec3_t origin;

		// very specifc case, we can modulate the color and the alpha
		vec3_t rgb;
		vec3_t destrgb;
		vec3_t curRGB;

		float alpha;
		float destAlpha;
		float curAlpha;

		// this is a very specific case thing...allow interpolating the st coords so we can map the texture
		//	properly as this segement progresses through it's life
		float ST[2];
		float destST[2];
		float curST[2];
	};

	TVert mVerts[4];
	qhandle_t mShader;

	CTrail() : mVerts{}, mShader(0)
	{
	}
	;

	~CTrail() override
	{
	};

	bool Update() override;
};

//------------------------------
class CLight : public CEffect
{
protected:
	float mSizeStart;
	float mSizeEnd;
	float mSizeParm;

	vec3_t mRGBStart;
	vec3_t mRGBEnd;
	float mRGBParm;

	void UpdateSize();
	void UpdateRGB();

	virtual void Draw()
	{
		theFxHelper.AddLightToScene(mOrigin1, mRefEnt.radius,
			mRefEnt.lightingOrigin[0], mRefEnt.lightingOrigin[1], mRefEnt.lightingOrigin[2]);
	}

public:
	CLight() : mSizeStart(0), mSizeEnd(0), mSizeParm(0), mRGBStart{}, mRGBEnd{}, mRGBParm(0)
	{
	}

	~CLight() override
	{
	}

	bool Update() override;

	void SetSizeStart(const float sz) { mSizeStart = sz; }
	void SetSizeEnd(const float sz) { mSizeEnd = sz; }
	void SetSizeParm(const float parm) { mSizeParm = parm; }

	void SetRGBStart(vec3_t rgb)
	{
		if (rgb) { VectorCopy(rgb, mRGBStart); }
		else { VectorClear(mRGBStart); }
	}

	void SetRGBEnd(vec3_t rgb)
	{
		if (rgb) { VectorCopy(rgb, mRGBEnd); }
		else { VectorClear(mRGBEnd); }
	}

	void SetRGBParm(const float parm) { mRGBParm = parm; }
};

//------------------------------
class CFlash : public CLight
{
protected:
	void Draw() override;

public:
	CFlash()
	{
	}

	~CFlash() override
	{
	}

	bool Update() override;

	void SetShader(const qhandle_t sh)
	{
		assert(sh);
		mRefEnt.customShader = sh;
	}

	void Init();
};

//------------------------------
class CParticle : public CEffect
{
protected:
	vec3_t mOrgOffset;

	vec3_t mVel;
	vec3_t mAccel;
	float mGravity;

	float mSizeStart;
	float mSizeEnd;
	float mSizeParm;

	vec3_t mRGBStart;
	vec3_t mRGBEnd;
	float mRGBParm;

	float mAlphaStart;
	float mAlphaEnd;
	float mAlphaParm;

	float mRotationDelta;
	float mElasticity;

	short mClientID;
	char mModelNum;
	char mBoltNum;

	bool UpdateOrigin();
	void UpdateVelocity() { VectorMA(mVel, theFxHelper.mFloatFrameTime, mAccel, mVel); }

	void UpdateSize();
	void UpdateRGB();
	void UpdateAlpha();
	void UpdateRotation() { mRefEnt.rotation += theFxHelper.mFrameTime * 0.01f * mRotationDelta; }

	virtual bool Cull() const;
	virtual void Draw();

public:
	CParticle() : mOrgOffset{}, mVel{}, mAccel{}, mGravity(0), mSizeStart(0), mSizeEnd(0), mSizeParm(0), mRGBStart{},
		mRGBEnd{},
		mRGBParm(0),
		mAlphaStart(0),
		mAlphaEnd(0),
		mAlphaParm(0),
		mRotationDelta(0),
		mElasticity(0)
	{
		mRefEnt.reType = RT_SPRITE;
		mClientID = -1;
		mModelNum = -1;
		mBoltNum = -1;
	}

	~CParticle() override
	{
	}

	void Die() override;
	bool Update() override;

	void SetShader(const qhandle_t sh) { mRefEnt.customShader = sh; }

	void SetOrgOffset(const vec3_t o)
	{
		if (o) { VectorCopy(o, mOrgOffset); }
		else { VectorClear(mOrgOffset); }
	}

	void SetVel(const vec3_t vel)
	{
		if (vel) { VectorCopy(vel, mVel); }
		else { VectorClear(mVel); }
	}

	void SetAccel(const vec3_t ac)
	{
		if (ac) { VectorCopy(ac, mAccel); }
		else { VectorClear(mAccel); }
	}

	void SetGravity(const float grav) { mGravity = grav; }

	void SetSizeStart(const float sz) { mSizeStart = sz; }
	void SetSizeEnd(const float sz) { mSizeEnd = sz; }
	void SetSizeParm(const float parm) { mSizeParm = parm; }

	void SetRGBStart(const vec3_t rgb)
	{
		if (rgb) { VectorCopy(rgb, mRGBStart); }
		else { VectorClear(mRGBStart); }
	}

	void SetRGBEnd(const vec3_t rgb)
	{
		if (rgb) { VectorCopy(rgb, mRGBEnd); }
		else { VectorClear(mRGBEnd); }
	}

	void SetRGBParm(const float parm) { mRGBParm = parm; }

	void SetAlphaStart(const float al) { mAlphaStart = al; }
	void SetAlphaEnd(const float al) { mAlphaEnd = al; }
	void SetAlphaParm(const float parm) { mAlphaParm = parm; }

	void SetRotation(const float rot) { mRefEnt.rotation = rot; }
	void SetRotationDelta(const float rot) { mRotationDelta = rot; }
	void SetElasticity(const float el) { mElasticity = el; }

	void SetClient(const int client_id, const int modelNum = -1, const int boltNum = -1)
	{
		mClientID = client_id;
		mModelNum = modelNum;
		mBoltNum = boltNum;
	}
};

//------------------------------
class CLine : public CParticle
{
protected:
	vec3_t mOrigin2;

	void Draw() override;

public:
	CLine() : mOrigin2{} { mRefEnt.reType = RT_LINE; }

	~CLine() override
	{
	}

	void Die() override
	{
	}

	bool Update() override;
	virtual bool Cull();

	void SetOrigin2(const vec3_t org2) { VectorCopy(org2, mOrigin2); }
};

//------------------------------
class CBezier : public CLine
{
protected:
	vec3_t mControl1;
	vec3_t mControl1Vel;

	vec3_t mControl2;
	vec3_t mControl2Vel;

	bool mInit;

	void Draw() override;

public:
	CBezier() : mControl1{}, mControl1Vel{}, mControl2{}, mControl2Vel{} { mInit = false; }

	~CBezier() override
	{
	}

	void Die() override
	{
	}

	bool Update() override;

	inline void DrawSegment(vec3_t start, vec3_t end, float texcoord1, float texcoord2);

	void SetControlPoints(const vec3_t ctrl1, const vec3_t ctrl2)
	{
		VectorCopy(ctrl1, mControl1);
		VectorCopy(ctrl2, mControl2);
	}

	void SetControlVel(const vec3_t ctrl1v, const vec3_t ctrl2v)
	{
		VectorCopy(ctrl1v, mControl1Vel);
		VectorCopy(ctrl2v, mControl2Vel);
	}
};

//------------------------------
class CElectricity : public CLine
{
protected:
	float mChaos;

	void Draw() override;

public:
	CElectricity() : mChaos(0) { mRefEnt.reType = RT_ELECTRICITY; }

	~CElectricity() override
	{
	}

	void Die() override
	{
	}

	bool Update() override;

	void Initialize();

	void SetChaos(const float chaos) { mChaos = chaos; }
};

// Oriented quad
//------------------------------
class COrientedParticle : public CParticle
{
protected:
	vec3_t mNormal;
	vec3_t mNormalOffset;

	bool Cull() const override;
	void Draw() override;

public:
	COrientedParticle() : mNormal{}, mNormalOffset{} { mRefEnt.reType = RT_ORIENTED_QUAD; }

	~COrientedParticle() override
	{
	}

	bool Update() override;

	void SetNormal(const vec3_t norm) { VectorCopy(norm, mNormal); }
	void SetNormalOffset(const vec3_t norm) { VectorCopy(norm, mNormalOffset); }
};

//------------------------------
class CTail : public CParticle
{
protected:
	vec3_t mOldOrigin;

	float mLengthStart;
	float mLengthEnd;
	float mLengthParm;

	float mLength;

	virtual void UpdateLength();
	void CalcNewEndpoint();

	void Draw() override;
	bool Cull() const override;

public:
	CTail() : mOldOrigin{}, mLengthStart(0), mLengthEnd(0), mLengthParm(0), mLength(0) { mRefEnt.reType = RT_LINE; }

	~CTail() override
	{
	}

	bool Update() override;

	void SetLengthStart(const float len) { mLengthStart = len; }
	void SetLengthEnd(const float len) { mLengthEnd = len; }
	void SetLengthParm(const float len) { mLengthParm = len; }
};

//------------------------------
class CCylinder : public CTail
{
protected:
	float mSize2Start;
	float mSize2End;
	float mSize2Parm;

	void UpdateSize2();

	void Draw() override;

public:
	CCylinder() : mSize2Start(0), mSize2End(0), mSize2Parm(0) { mRefEnt.reType = RT_CYLINDER; }

	~CCylinder() override
	{
	}

	bool Update() override;

	void SetSize2Start(const float sz) { mSize2Start = sz; }
	void SetSize2End(const float sz) { mSize2End = sz; }
	void SetSize2Parm(const float parm) { mSize2Parm = parm; }

	void SetNormal(const vec3_t norm) { VectorCopy(norm, mRefEnt.axis[0]); }
};

//------------------------------
// Emitters are derived from particles because, although they don't draw, any effect called
//	from them can borrow an initial or ending value from the emitters current alpha, rgb, etc..
class CEmitter : public CParticle
{
protected:
	vec3_t mOldOrigin; // we use these to do some nice
	vec3_t mLastOrigin; //	tricks...
	vec3_t mOldVelocity; //
	int mOldTime;

	vec3_t mAngles; // for a rotating thing, using a delta
	vec3_t mAngleDelta; //	as opposed to an end angle is probably much easier

	int mEmitterFxID; // if we have emitter fx, this is our id

	float mDensity; // controls how often emitter chucks an effect
	float mVariance; // density sloppiness

	void UpdateAngles();

	void Draw() override;

public:
	CEmitter() : mOldOrigin{}, mLastOrigin{}, mOldVelocity{}, mOldTime(0), mAngles{}, mAngleDelta{}, mEmitterFxID(0),
		mDensity(0),
		mVariance(0)
	{
		// There may or may not be a model, but if there isn't one,
		//	we just won't bother adding the refEnt in our Draw func
		mRefEnt.reType = RT_MODEL;
	}

	~CEmitter() override
	{
	}

	bool Update() override;

	void SetModel(const qhandle_t model) { mRefEnt.hModel = model; }

	void SetAngles(const vec3_t ang)
	{
		if (ang) { VectorCopy(ang, mAngles); }
		else { VectorClear(mAngles); }
	}

	void SetAngleDelta(const vec3_t ang)
	{
		if (ang) { VectorCopy(ang, mAngleDelta); }
		else { VectorClear(mAngleDelta); }
	}

	void SetEmitterFxID(const int id) { mEmitterFxID = id; }
	void SetDensity(const float density) { mDensity = density; }
	void SetVariance(const float var) { mVariance = var; }
	void SetOldTime(const int time) { mOldTime = time; }

	void SetLastOrg(const vec3_t org)
	{
		if (org) { VectorCopy(org, mLastOrigin); }
		else { VectorClear(mLastOrigin); }
	}

	void SetLastVel(const vec3_t vel)
	{
		if (vel) { VectorCopy(vel, mOldVelocity); }
		else { VectorClear(mOldVelocity); }
	}
};

// We're getting pretty low level here, not the kind of thing to abuse considering how much overhead this
//	adds to a SINGLE triangle or quad....
// The editor doesn't need to see or do anything with this
//------------------------------
constexpr auto MAX_CPOLY_VERTS = 5;

class CPoly : public CParticle
{
protected:
	int mCount;
	vec3_t mRotDelta;
	int mTimeStamp;

	bool Cull() const override;
	void Draw() const;

public:
	vec3_t mOrg[MAX_CPOLY_VERTS];
	vec2_t mST[MAX_CPOLY_VERTS];

	float mRot[3][3];
	int mLastFrameTime;

	CPoly() : mCount(0), mRotDelta{}, mTimeStamp(0), mOrg{}, mST{}, mRot{}, mLastFrameTime(0)
	{
	}

	~CPoly() override
	{
	}

	bool Update() override;

	void PolyInit();
	void CalcRotateMatrix();
	void Rotate();

	void SetNumVerts(const int c) { mCount = c; }

	void SetRot(vec3_t r)
	{
		if (r) { VectorCopy(r, mRotDelta); }
		else { VectorClear(mRotDelta); }
	}

	void SetMotionTimeStamp(const int t) { mTimeStamp = theFxHelper.mTime + t; }
	int GetMotionTimeStamp() const { return mTimeStamp; }
};

#endif //FX_PRIMITIVES_H_INC
