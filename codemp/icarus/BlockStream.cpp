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

// Interpreted Block Stream Functions
//
//	-- jweier

// this include must remain at the top of every Icarus CPP file
#include "icarus.h"

#include <string.h>
#include "blockstream.h"

/*
===================================================================================================

  CBlockMember

===================================================================================================
*/

CBlockMember::CBlockMember(void)
{
	m_id = -1;
	m_size = -1;
	m_data = nullptr;
}

CBlockMember::~CBlockMember(void)
{
	Free();
}

/*
-------------------------
Free
-------------------------
*/

void CBlockMember::Free(void)
{
	if (m_data != nullptr)
	{
		ICARUS_Free(m_data);
		m_data = nullptr;

		m_id = m_size = -1;
	}
}

/*
-------------------------
GetInfo
-------------------------
*/

void CBlockMember::GetInfo(int* id, int* size, void** data) const
{
	*id = m_id;
	*size = m_size;
	*data = m_data;
}

/*
-------------------------
SetData overloads
-------------------------
*/

void CBlockMember::SetData(const char* data)
{
	WriteDataPointer(data, strlen(data) + 1);
}

void CBlockMember::SetData(vector_t data)
{
	WriteDataPointer(data, 3);
}

void CBlockMember::SetData(void* data, int size)
{
	if (m_data)
		ICARUS_Free(m_data);

	m_data = ICARUS_Malloc(size);
	memcpy(m_data, data, size);
	m_size = size;
}

//	Member I/O functions

/*
-------------------------
ReadMember
-------------------------
*/

int CBlockMember::ReadMember(char** stream, int* streamPos)
{
	m_id = LittleLong(*(int*)(*stream + *((int*)streamPos)));
	*streamPos += sizeof(int);

	if (m_id == ID_RANDOM)
	{//special case, need to initialize this member's data to Q3_INFINITE so we can randomize the number only the first time random is checked when inside a wait
		m_size = sizeof(float);
		*streamPos += sizeof(int);
		m_data = ICARUS_Malloc(m_size);
		const float infinite = Q3_INFINITE;
		memcpy(m_data, &infinite, m_size);
	}
	else
	{
		m_size = LittleLong(*(int*)(*stream + *streamPos));
		*streamPos += sizeof(int);
		m_data = ICARUS_Malloc(m_size);
		memcpy(m_data, (*stream + *streamPos), m_size);
#ifdef Q3_BIG_ENDIAN
		// only TK_INT, TK_VECTOR and TK_FLOAT has to be swapped, but just in case
		if (m_size == 4 && m_id != TK_STRING && m_id != TK_IDENTIFIER && m_id != TK_CHAR)
			*(int*)m_data = LittleLong(*(int*)m_data);
#endif
	}
	*streamPos += m_size;

	return true;
}

/*
-------------------------
WriteMember
-------------------------
*/

int CBlockMember::WriteMember(FILE* m_file_handle) const
{
	fwrite(&m_id, sizeof(m_id), 1, m_file_handle);
	fwrite(&m_size, sizeof(m_size), 1, m_file_handle);
	fwrite(m_data, m_size, 1, m_file_handle);

	return true;
}

/*
-------------------------
Duplicate
-------------------------
*/

CBlockMember* CBlockMember::Duplicate(void) const
{
	CBlockMember* newblock = new CBlockMember;

	if (newblock == nullptr)
		return nullptr;

	newblock->SetData(m_data, m_size);
	newblock->SetSize(m_size);
	newblock->SetID(m_id);

	return newblock;
}

/*
===================================================================================================

  CBlock

===================================================================================================
*/

CBlock::CBlock(void)
{
	m_flags = 0;
	m_id = 0;
}

CBlock::~CBlock(void)
{
	Free();
}

/*
-------------------------
Init
-------------------------
*/

int CBlock::Init(void)
{
	m_flags = 0;
	m_id = 0;

	return true;
}

/*
-------------------------
Create
-------------------------
*/

int CBlock::Create(int block_id)
{
	Init();

	m_id = block_id;

	return true;
}

/*
-------------------------
Free
-------------------------
*/

int CBlock::Free(void)
{
	int	numMembers = GetNumMembers();

	while (numMembers--)
	{
		const CBlockMember* bMember = GetMember(numMembers);

		if (!bMember)
			return false;

		delete bMember;
	}

	m_members.clear();			//List of all CBlockMembers owned by this list

	return true;
}

//	Write overloads

/*
-------------------------
Write
-------------------------
*/

int CBlock::Write(int member_id, const char* member_data)
{
	CBlockMember* bMember = new CBlockMember;

	bMember->SetID(member_id);

	bMember->SetData(member_data);
	bMember->SetSize(strlen(member_data) + 1);

	AddMember(bMember);

	return true;
}

int CBlock::Write(int member_id, vector_t member_data)
{
	CBlockMember* bMember = new CBlockMember;

	bMember->SetID(member_id);
	bMember->SetData(member_data);
	bMember->SetSize(sizeof(vector_t));

	AddMember(bMember);

	return true;
}

int CBlock::Write(int member_id, float member_data)
{
	CBlockMember* bMember = new CBlockMember;

	bMember->SetID(member_id);
	bMember->WriteData(member_data);
	bMember->SetSize(sizeof(member_data));

	AddMember(bMember);

	return true;
}

int CBlock::Write(int member_id, int member_data)
{
	CBlockMember* bMember = new CBlockMember;

	bMember->SetID(member_id);
	bMember->WriteData(member_data);
	bMember->SetSize(sizeof(member_data));

	AddMember(bMember);

	return true;
}

int CBlock::Write(CBlockMember* bMember)
{
	// findme: this is wrong:	bMember->SetSize( sizeof(bMember->GetData()) );

	AddMember(bMember);

	return true;
}

// Member list functions

/*
-------------------------
AddMember
-------------------------
*/

int	CBlock::AddMember(CBlockMember* member)
{
	m_members.insert(m_members.end(), member);
	return true;
}

/*
-------------------------
GetMember
-------------------------
*/

CBlockMember* CBlock::GetMember(int member_num) const
{
	if (member_num > GetNumMembers() - 1)
	{
		return nullptr;
	}
	return m_members[member_num];
}

/*
-------------------------
GetMemberData
-------------------------
*/

void* CBlock::GetMemberData(int member_num) const
{
	if (member_num >= GetNumMembers())
	{
		return nullptr;
	}
	return (void*)((GetMember(member_num))->GetData());
}

/*
-------------------------
Duplicate
-------------------------
*/

CBlock* CBlock::Duplicate(void)
{
	CBlock* newblock = new CBlock;

	if (newblock == nullptr)
		return nullptr;

	newblock->Create(m_id);

	//Duplicate entire block and return the cc
	for (blockMember_v::iterator mi = m_members.begin(); mi != m_members.end(); ++mi)
	{
		newblock->AddMember((*mi)->Duplicate());
	}

	return newblock;
}

/*
===================================================================================================

  CBlockStream

===================================================================================================
*/

CBlockStream::CBlockStream(void)
{
	m_stream = nullptr;
	m_streamPos = 0;
}

CBlockStream::~CBlockStream(void)
{
}

/*
-------------------------
GetChar
-------------------------
*/

char CBlockStream::GetChar(void)
{
	const char data = *(char*)(m_stream + m_streamPos);
	m_streamPos += sizeof(data);

	return data;
}

/*
-------------------------
GetUnsignedInteger
-------------------------
*/

unsigned CBlockStream::GetUnsignedInteger(void)
{
	const unsigned data = *(unsigned*)(m_stream + m_streamPos);
	m_streamPos += sizeof(data);

	return data;
}

/*
-------------------------
GetInteger
-------------------------
*/

int	CBlockStream::GetInteger(void)
{
	const int data = *(int*)(m_stream + m_streamPos);
	m_streamPos += sizeof(data);

	return data;
}

/*
-------------------------
GetLong
-------------------------
*/

long CBlockStream::GetLong(void)
{
	const long data = *(int*)(m_stream + m_streamPos);
	m_streamPos += sizeof(data);

	return data;
}

/*
-------------------------
GetFloat
-------------------------
*/

float CBlockStream::GetFloat(void)
{
	const float data = *(float*)(m_stream + m_streamPos);
	m_streamPos += sizeof(data);

	return data;
}

/*
-------------------------
Free
-------------------------
*/

int CBlockStream::Free(void)
{
	//NOTENOTE: It is assumed that the user will free the passed memory block (m_stream) immediately after the run call
	//			That's why this doesn't free the memory, it only clears its internal pointer

	m_stream = nullptr;
	m_streamPos = 0;

	return true;
}

/*
-------------------------
Create
-------------------------
*/

int CBlockStream::Create(const char* filename)
{
	const char* id_header = IBI_HEADER_ID;
	const float	version = IBI_VERSION;

	//Strip the extension and add the BLOCK_EXT extension
	COM_StripExtension(filename, m_fileName, sizeof(m_fileName));
	COM_DefaultExtension(m_fileName, sizeof(m_fileName), IBI_EXT);

	if ((m_fileHandle = fopen(m_fileName, "wb")) == nullptr)
	{
		return false;
	}

	fwrite(id_header, IBI_HEADER_ID_LENGTH, 1, m_fileHandle);
	fwrite(&version, sizeof(version), 1, m_fileHandle);

	return true;
}

/*
-------------------------
Init
-------------------------
*/

int CBlockStream::Init(void)
{
	m_fileHandle = nullptr;
	memset(m_fileName, 0, sizeof(m_fileName));

	m_stream = nullptr;
	m_streamPos = 0;

	return true;
}

//	Block I/O functions

/*
-------------------------
WriteBlock
-------------------------
*/

int CBlockStream::WriteBlock(CBlock* block) const
{
	const int				id = block->GetBlockID();
	const int				numMembers = block->GetNumMembers();
	const unsigned char	flags = block->GetFlags();

	fwrite(&id, sizeof(id), 1, m_fileHandle);
	fwrite(&numMembers, sizeof(numMembers), 1, m_fileHandle);
	fwrite(&flags, sizeof(flags), 1, m_fileHandle);

	for (int i = 0; i < numMembers; i++)
	{
		const CBlockMember* bMember = block->GetMember(i);
		bMember->WriteMember(m_fileHandle);
	}

	block->Free();

	return true;
}

/*
-------------------------
BlockAvailable
-------------------------
*/

int CBlockStream::BlockAvailable(void) const
{
	if (m_streamPos >= m_fileSize)
		return false;

	return true;
}

/*
-------------------------
ReadBlock
-------------------------
*/

int CBlockStream::ReadBlock(CBlock* get)
{
	if (!BlockAvailable())
		return false;

	const int b_id = (GetInteger());
	int numMembers = (GetInteger());
	const unsigned char flags = static_cast<unsigned char>(GetChar());

	if (numMembers < 0)
		return false;

	get->Create(b_id);
	get->SetFlags(flags);

	// Stream blocks are generally temporary as they
	// are just used in an initial parsing phase...
	while (numMembers-- > 0)
	{
		CBlockMember* bMember = new CBlockMember;
		bMember->ReadMember(&m_stream, &m_streamPos);
		get->AddMember(bMember);
	}

	return true;
}

/*
-------------------------
Open
-------------------------
*/

int CBlockStream::Open(char* buffer, long size)
{
	char	id_header[IBI_HEADER_ID_LENGTH];

	Init();

	m_fileSize = size;

	m_stream = buffer;

	for (size_t i = 0; i < sizeof(id_header); i++)
	{
		id_header[i] = GetChar();
	}

	float version = GetFloat();
	version = LittleFloat(version);

	//Check for valid header
	if (strcmp(id_header, IBI_HEADER_ID))
	{
		Free();
		return false;
	}

	//Check for valid version
	if (version != IBI_VERSION)
	{
		Free();
		return false;
	}

	return true;
}