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

#ifdef DEBUG_LINKING
#pragma message("...including GenericParser2.h")
#endif

#include "disablewarnings.h"

#ifdef USE_LOCAL_GENERICPARSER
#include <memory.h>
#include <malloc.h>
#include <string.h>

#define trap_Z_Malloc(x, y)		malloc(x)
#define trap_Z_Free(x)			free(x)

#endif

class CTextPool;
class CGPObject;

class CTextPool
{
private:
	char* mPool;
	CTextPool* mNext;
	int			mSize, mUsed;

public:
	CTextPool(int initSize = 10240);
	~CTextPool(void);

	CTextPool* GetNext(void) const { return mNext; }
	void		SetNext(CTextPool* which) { mNext = which; }
	char* GetPool(void) const { return mPool; }
	int			GetUsed(void) const { return mUsed; }

	char* AllocText(char* text, bool addNULL = true, CTextPool** poolPtr = nullptr);
};

void CleanTextPool(CTextPool* pool);

class CGPObject
{
protected:
	const char* mName;
	CGPObject* mNext, * mInOrderNext, * mInOrderPrevious;

public:
	CGPObject(const char* initName);
	virtual	~CGPObject(void) {}

	const char* GetName(void) const { return mName; }

	CGPObject* GetNext(void) const { return mNext; }
	void		SetNext(CGPObject* which) { mNext = which; }
	CGPObject* GetInOrderNext(void) const { return mInOrderNext; }
	void		SetInOrderNext(CGPObject* which) { mInOrderNext = which; }
	CGPObject* GetInOrderPrevious(void) const { return mInOrderPrevious; }
	void		SetInOrderPrevious(CGPObject* which) { mInOrderPrevious = which; }

	bool		WriteText(CTextPool** textPool, const char* text) const;
};

class CGPValue : public CGPObject
{
private:
	CGPObject* mList;

public:
	CGPValue(const char* initName, const char* initValue = nullptr);
	~CGPValue(void);

	CGPValue* GetNext(void) const { return (CGPValue*)mNext; }

	CGPValue* Duplicate(CTextPool** textPool = nullptr) const;

	bool			IsList(void) const;
	const char* GetTopValue(void) const;
	CGPObject* GetList(void) const { return mList; }
	void			AddValue(const char* newValue, CTextPool** textPool = nullptr);

	bool			Parse(char** dataPtr, CTextPool** textPool);

	bool			Write(CTextPool** textPool, int depth) const;
};

class CGPGroup : public CGPObject
{
private:
	CGPValue* mPairs, * mInOrderPairs;
	CGPValue* mCurrentPair;
	CGPGroup* mSubGroups, * mInOrderSubGroups;
	CGPGroup* mCurrentSubGroup;
	CGPGroup* mParent;
	bool				mWriteable;

	static void	SortObject(CGPObject* object, CGPObject** unsortedList, CGPObject** sortedList,
		CGPObject** lastObject);

public:
	CGPGroup(const char* initName = "Top Level", CGPGroup* initParent = nullptr);
	~CGPGroup(void);

	CGPGroup* GetParent(void) const { return mParent; }
	CGPGroup* GetNext(void) const { return (CGPGroup*)mNext; }
	int			GetNumSubGroups(void) const;
	int			GetNumPairs(void) const;

	void		Clean(void);
	CGPGroup* Duplicate(CTextPool** textPool = nullptr, CGPGroup* initParent = nullptr) const;

	void		SetWriteable(const bool writeable) { mWriteable = writeable; }
	CGPValue* GetPairs(void) const { return mPairs; }
	CGPValue* GetInOrderPairs(void) const { return mInOrderPairs; }
	CGPGroup* GetSubGroups(void) const { return mSubGroups; }
	CGPGroup* GetInOrderSubGroups(void) const { return mInOrderSubGroups; }

	CGPValue* AddPair(const char* name, const char* value, CTextPool** textPool = nullptr);
	void		AddPair(CGPValue* NewPair);
	CGPGroup* AddGroup(const char* name, CTextPool** textPool = nullptr);
	void		AddGroup(CGPGroup* NewGroup);
	CGPGroup* FindSubGroup(const char* name) const;
	bool		Parse(char** dataPtr, CTextPool** textPool);
	bool		Write(CTextPool** textPool, int depth) const;

	CGPValue* FindPair(const char* key) const;
	const char* FindPairValue(const char* key, const char* defaultVal = nullptr) const;
};

class CGenericParser2
{
private:
	CGPGroup		mTopLevel;
	CTextPool* mTextPool;
	bool			mWriteable;

public:
	CGenericParser2(void);
	~CGenericParser2(void);

	void		SetWriteable(const bool writeable) { mWriteable = writeable; }
	CGPGroup* GetBaseParseGroup(void) { return &mTopLevel; }

	bool	Parse(char** dataPtr, bool cleanFirst = true, bool writeable = false);
	bool	Parse(char* dataPtr, bool cleanFirst = true, bool writeable = false)
	{
		return Parse(&dataPtr, cleanFirst, writeable);
	}
	void	Clean(void);

	bool	Write(CTextPool* textPool) const;
};

// The following groups of routines are used for a C interface into GP2.
// C++ users should just use the objects as normally and not call these routines below
//

typedef		void* TGenericParser2;
typedef		void* TGPGroup;
typedef		void* TGPValue;

// CGenericParser2 (void *) routines
TGenericParser2		GP_Parse(char** dataPtr, bool cleanFirst, bool writeable);
void				GP_Clean(TGenericParser2 GP2);
void				GP_Delete(TGenericParser2* GP2);
TGPGroup			GP_GetBaseParseGroup(TGenericParser2 GP2);

// CGPGroup (void *) routines
const char* GPG_GetName(TGPGroup GPG);
bool		GPG_GetName(TGPGroup GPG, char* Value);
TGPGroup	GPG_GetNext(TGPGroup GPG);
TGPGroup	GPG_GetInOrderNext(TGPGroup GPG);
TGPGroup	GPG_GetInOrderPrevious(TGPGroup GPG);
TGPGroup	GPG_GetPairs(TGPGroup GPG);
TGPGroup	GPG_GetInOrderPairs(TGPGroup GPG);
TGPGroup	GPG_GetSubGroups(TGPGroup GPG);
TGPGroup	GPG_GetInOrderSubGroups(TGPGroup GPG);
TGPGroup	GPG_FindSubGroup(TGPGroup GPG, const char* name);
TGPValue	GPG_FindPair(TGPGroup GPG, const char* key);
const char* GPG_FindPairValue(TGPGroup GPG, const char* key, const char* defaultVal);
bool		GPG_FindPairValue(TGPGroup GPG, const char* key, const char* defaultVal, char* Value);

// CGPValue (void *) routines
const char* GPV_GetName(TGPValue GPV);
bool		GPV_GetName(TGPValue GPV, char* Value);
TGPValue	GPV_GetNext(TGPValue GPV);
TGPValue	GPV_GetInOrderNext(TGPValue GPV);
TGPValue	GPV_GetInOrderPrevious(TGPValue GPV);
bool		GPV_IsList(TGPValue GPV);
const char* GPV_GetTopValue(TGPValue GPV);
bool		GPV_GetTopValue(TGPValue GPV, char* Value);
TGPValue	GPV_GetList(TGPValue GPV);
