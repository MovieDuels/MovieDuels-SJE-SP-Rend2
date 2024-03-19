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

// BlockStream.h

#ifndef __INTERPRETED_BLOCK_STREAM__
#define	__INTERPRETED_BLOCK_STREAM__

#include <cassert>

using vec3_t = float[3];

// Templates

// CBlockMember

class CBlockMember
{
public:
	CBlockMember();

protected:
	~CBlockMember();

public:
	void Free(IGameInterface* game);

	int WriteMember(FILE*) const; //Writes the member's data, in block format, to FILE *
	int ReadMember(char**, long*, const CIcarus* icarus); //Reads the member's data, in block format, from FILE *

	void SetID(const int id) { m_id = id; } //Set the ID member variable
	void SetSize(const int size) { m_size = size; } //Set the size member variable

	void GetInfo(int*, int*, void**) const;

	//SetData overloads
	void SetData(const char*, CIcarus* icarus);
	void SetData(vec3_t, CIcarus* icarus);
	void SetData(const void* data, int size, const CIcarus* icarus);

	int GetID() const { return m_id; } //Get ID member variables
	void* GetData() const { return m_data; } //Get data member variable
	int GetSize() const { return m_size; } //Get size member variable

	// Overloaded new operator.
	void* operator new(const size_t size)
	{
		// Allocate the memory.
		return IGameInterface::GetGame()->Malloc(size);
	}

	// Overloaded delete operator.
	void operator delete(void* pRawData)
	{
		// Free the Memory.
		IGameInterface::GetGame()->Free(pRawData);
	}

	CBlockMember* Duplicate(const CIcarus* icarus) const;

	template <class T>
	void WriteData(T& data, CIcarus* icarus)
	{
		IGameInterface* game = icarus->GetGame();
		if (m_data)
		{
			game->Free(m_data);
		}

		m_data = game->Malloc(sizeof(T));
		*static_cast<T*>(m_data) = data;
		m_size = sizeof(T);
	}

	template <class T>
	void WriteDataPointer(const T* data, const int num, CIcarus* icarus)
	{
		IGameInterface* game = icarus->GetGame();
		if (m_data)
		{
			game->Free(m_data);
		}

		m_data = game->Malloc(num * sizeof(T));
		memcpy(m_data, data, num * sizeof(T));
		m_size = num * sizeof(T);
	}

protected:
	int m_id; //ID of the value contained in data
	int m_size; //Size of the data member variable
	void* m_data; //Data for this member
};

//CBlock

class CBlock
{
	using blockMember_v = std::vector<CBlockMember*>;

public:
	CBlock()
	{
		m_flags = 0;
		m_id = 0;
	}

	~CBlock()
	{
		assert(!GetNumMembers());
	}

	int Init();

	int Create(int);
	int Free(const CIcarus* icarus);

	//Write Overloads

	int Write(int, vec3_t, CIcarus* icarus);
	int Write(int, float, CIcarus* icarus);
	int Write(int, const char*, CIcarus* icarus);
	int Write(int, int, CIcarus* icarus);
	int Write(CBlockMember*, CIcarus* icaru);

	//Member push / pop functions

	int AddMember(CBlockMember*);
	CBlockMember* GetMember(int member_num) const;

	void* GetMemberData(int member_num) const;

	CBlock* Duplicate(const CIcarus* icarus);

	int GetBlockID() const { return m_id; } //Get the ID for the block
	int GetNumMembers() const { return static_cast<int>(m_members.size()); }
	//Get the number of member in the block's list

	void SetFlags(const unsigned char flags) { m_flags = flags; }
	void SetFlag(const unsigned char flag) { m_flags |= flag; }

	int HasFlag(const unsigned char flag) const { return m_flags & flag; }
	unsigned char GetFlags() const { return m_flags; }

	// Overloaded new operator.
	void* operator new(const size_t size)
	{
		// Allocate the memory.
		return IGameInterface::GetGame()->Malloc(size);
	}

	// Overloaded delete operator.
	void operator delete(void* pRawData)
	{
		// Validate data.
		if (pRawData == nullptr)
			return;

		// Free the Memory.
		IGameInterface::GetGame()->Free(pRawData);
	}

protected:
	blockMember_v m_members; //List of all CBlockMembers owned by this list
	int m_id; //ID of the block
	unsigned char m_flags;
};

// CBlockStream

class CBlockStream
{
public:
	CBlockStream() : m_fileSize(0), m_fileHandle(nullptr), m_fileName{}
	{
		m_stream = nullptr;
		m_streamPos = 0;
	}

	~CBlockStream()
	{
	};

	int Init();

	int Create(const char*);
	int Free();

	// Stream I/O functions

	int BlockAvailable() const;

	int WriteBlock(CBlock*, const CIcarus* icarus) const; //Write the block out
	int ReadBlock(CBlock*, const CIcarus* icarus); //Read the block in

	int Open(char*, long); //Open a stream for reading / writing

	// Overloaded new operator.
	void* operator new(const size_t size)
	{
		// Allocate the memory.
		return IGameInterface::GetGame()->Malloc(size);
	}

	// Overloaded delete operator.
	void operator delete(void* pRawData)
	{
		// Free the Memory.
		IGameInterface::GetGame()->Free(pRawData);
	}

protected:
	long m_fileSize; //Size of the file
	FILE* m_fileHandle; //Global file handle of current I/O source
	char m_fileName[CIcarus::MAX_FILENAME_LENGTH]; //Name of the current file

	char* m_stream; //Stream of data to be parsed
	long m_streamPos;

	static char* s_IBI_EXT;
	static char* s_IBI_HEADER_ID;
	static const float s_IBI_VERSION;
};

#endif	//__INTERPRETED_BLOCK_STREAM__
