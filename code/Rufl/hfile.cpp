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

////////////////////////////////////////////////////////////////////////////////////////
// RAVEN STANDARD USEFUL FUNCTION LIBRARY
//  (c) 2002 Activision
//
//
// Handle File
// -----------
//
////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////////////
#include "../qcommon/q_shared.h"
#ifndef _WIN32
#define Pool FilePool
#endif
#include "hfile.h"
#if !defined(RATL_HANDLE_POOL_VS_INC)
#include "../Ratl/handle_pool_vs.h"
#endif
#if !defined(RATL_VECTOR_VS_INC)
#include "../Ratl/vector_vs.h"
#endif
#if !defined(RUFL_HSTRING_INC)
#include "hstring.h"
#endif

extern bool HFILEopen_read(int& handle, const char* filepath);
extern bool HFILEopen_write(int& handle, const char* filepath);
extern bool HFILEread(const int& handle, void* data, int size);
extern bool HFILEwrite(const int& handle, const void* data, int size);
extern bool HFILEclose(const int& handle);

////////////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////////////
constexpr auto MAX_OPEN_FILES = 20;

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
struct SOpenFile
{
	hstring mPath;
	bool mForRead;
	int mHandle;
	float mVersion;
	int mChecksum;
};

using TFilePool = ratl::handle_pool_vs<SOpenFile, MAX_OPEN_FILES>;

static TFilePool& Pool()
{
	static TFilePool TFP;
	return TFP;
}

////////////////////////////////////////////////////////////////////////////////////////
// Constructor
//
// Allocates a new OpenFile structure and initializes it.  DOES NOT OPEN!
//
////////////////////////////////////////////////////////////////////////////////////////
hfile::hfile(const char* file)
{
	if (Pool().full())
	{
		mHandle = 0;
		assert("HFILE: Too Many Files Open, Unable To Grab An Unused Handle" == nullptr);
		return;
	}

	mHandle = Pool().alloc();

	SOpenFile& sfile = Pool()[mHandle];

	sfile.mPath = file;
	sfile.mHandle = 0;
	sfile.mForRead = true;
}

////////////////////////////////////////////////////////////////////////////////////////
// Destructor
//
// Releases the open file structure for resue.  Also closes the file if open.
//
////////////////////////////////////////////////////////////////////////////////////////
hfile::~hfile()
{
	if (is_open())
	{
		close();
	}

	if (mHandle && Pool().is_used(mHandle))
	{
		Pool().free(mHandle);
	}
	mHandle = 0;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool hfile::is_open() const
{
	if (mHandle && Pool().is_used(mHandle))
	{
		return Pool()[mHandle].mHandle != 0;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool hfile::is_open_for_read() const
{
	if (mHandle && Pool().is_used(mHandle))
	{
		const SOpenFile& sfile = Pool()[mHandle];
		return sfile.mHandle != 0 && sfile.mForRead;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////
bool hfile::is_open_for_write() const
{
	if (mHandle && Pool().is_used(mHandle))
	{
		const SOpenFile& sfile = Pool()[mHandle];
		return sfile.mHandle != 0 && !sfile.mForRead;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////
bool hfile::open(const float version, const int checksum, const bool read) const
{
	// Make Sure This Is A Valid Handle
	//----------------------------------
	if (!mHandle || !Pool().is_used(mHandle))
	{
		assert("HFILE: Invalid Handle" == nullptr);
		return false;
	}

	// Make Sure The File Is Not ALREADY Open
	//----------------------------------------
	SOpenFile& sfile = Pool()[mHandle];
	if (sfile.mHandle != 0)
	{
		assert("HFILE: Attempt To Open An Already Open File" == nullptr);
		return false;
	}

	sfile.mForRead = read;
	if (read)
	{
		HFILEopen_read(sfile.mHandle, *sfile.mPath);
	}
	else
	{
		HFILEopen_write(sfile.mHandle, *sfile.mPath);
	}

	// If The Open Failed, Report It And Free The SOpenFile
	//------------------------------------------------------
	if (sfile.mHandle == 0)
	{
		if (!read)
		{
			assert("HFILE: Unable To Open File" == nullptr);
		}
		return false;
	}

	// Read The File's Header
	//------------------------
	if (read)
	{
		if (!HFILEread(sfile.mHandle, &sfile.mVersion, sizeof sfile.mVersion))
		{
			assert("HFILE: Unable To Read File Header" == nullptr);
			close();
			return false;
		}
		if (!HFILEread(sfile.mHandle, &sfile.mChecksum, sizeof sfile.mChecksum))
		{
			assert("HFILE: Unable To Read File Header" == nullptr);
			close();
			return false;
		}

		// Make Sure The Checksum & Version Match
		//----------------------------------------
		if (sfile.mVersion != version || sfile.mChecksum != checksum)
		{
			close();
			return false; // Failed To Match Checksum Or Version Number -> Old Data
		}
	}
	else
	{
		sfile.mVersion = version;
		sfile.mChecksum = checksum;

		if (!HFILEwrite(sfile.mHandle, &sfile.mVersion, sizeof sfile.mVersion))
		{
			assert("HFILE: Unable To Write File Header" == nullptr);
			close();
			return false;
		}
		if (!HFILEwrite(sfile.mHandle, &sfile.mChecksum, sizeof sfile.mChecksum))
		{
			assert("HFILE: Unable To Write File Header" == nullptr);
			close();
			return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////
bool hfile::close() const
{
	if (!mHandle || !Pool().is_used(mHandle))
	{
		assert("HFILE: Invalid Handle" == nullptr);
		return false;
	}

	SOpenFile& sfile = Pool()[mHandle];
	if (sfile.mHandle == 0)
	{
		assert("HFILE: Unable TO Close Unopened File" == nullptr);
		return false;
	}

	if (!HFILEclose(sfile.mHandle))
	{
		sfile.mHandle = 0;
		assert("HFILE: Unable To Close File" == nullptr);
		return false;
	}
	sfile.mHandle = 0;

	return true;
}

////////////////////////////////////////////////////////////////////////////////////
// Searches for the first block with the matching data size, and reads it in.
////////////////////////////////////////////////////////////////////////////////////
bool hfile::load(void* data, const int datasize) const
{
	// Go Ahead And Open The File For Reading
	//----------------------------------------
	bool auto_opened = false;
	if (!is_open())
	{
		if (!open_read())
		{
			return false;
		}
		auto_opened = true;
	}

	// Make Sure That The File Is Readable
	//-------------------------------------
	const SOpenFile& sfile = Pool()[mHandle];
	if (!sfile.mForRead)
	{
		assert("HFILE: Unable to load from a file that is opened for save" == nullptr);
		if (auto_opened)
		{
			close();
		}
		return false;
	}

	// Now Read It
	//-------------
	if (!HFILEread(sfile.mHandle, data, datasize))
	{
		assert("HFILE: Unable To Read Object" == nullptr);
		if (auto_opened)
		{
			close();
		}
		return false;
	}

	// Success!
	//----------
	if (auto_opened)
	{
		close();
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////
bool hfile::save(const void* data, const int datasize) const
{
	// Go Ahead And Open The File For Reading
	//----------------------------------------
	bool auto_opened = false;
	if (!is_open())
	{
		if (!open_write())
		{
			return false;
		}
		auto_opened = true;
	}

	// Make Sure That The File Is Readable
	//-------------------------------------
	const SOpenFile& sfile = Pool()[mHandle];
	if (sfile.mForRead)
	{
		assert("HFILE: Unable to save to a file that is opened for read" == nullptr);
		if (auto_opened)
		{
			close();
		}
		return false;
	}

	// Write The Actual Object
	//-------------------------
	if (!HFILEwrite(sfile.mHandle, data, datasize))
	{
		assert("HFILE: Unable To Write File Data" == nullptr);
		if (auto_opened)
		{
			close();
		}
		return false;
	}

	if (auto_opened)
	{
		close();
	}
	return true;
}