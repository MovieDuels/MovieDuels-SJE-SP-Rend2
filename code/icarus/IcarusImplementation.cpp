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

// IcarusImplementation.cpp

#include <memory>
#include "StdAfx.h"
#include "IcarusInterface.h"
#include "IcarusImplementation.h"

#include "blockstream.h"
#include "sequence.h"
#include "taskmanager.h"
#include "sequencer.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "qcommon/ojk_saved_game_helper.h"

#define STL_ITERATE( a, b )		for ( a = b.begin(); a != b.end(); ++a )
#define STL_INSERT( a, b )		a.insert( a.end(), b );

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// required implementation of CIcarusInterface

IIcarusInterface* IIcarusInterface::GetIcarus(const int flavor, const bool construct_if_necessary)
{
	if (!CIcarus::s_instances && construct_if_necessary)
	{
		CIcarus::s_flavorsAvailable = IGameInterface::s_IcarusFlavorsNeeded;
		if (!CIcarus::s_flavorsAvailable)
		{
			return nullptr;
		}
		CIcarus::s_instances = new CIcarus * [CIcarus::s_flavorsAvailable];
		for (int index = 0; index < CIcarus::s_flavorsAvailable; index++)
		{
			CIcarus::s_instances[index] = new CIcarus(index);
			//OutputDebugString( "ICARUS flavor successfully created\n" );
		}
	}

	if (flavor >= CIcarus::s_flavorsAvailable || !CIcarus::s_instances)
	{
		return nullptr;
	}
	return CIcarus::s_instances[flavor];
}

void IIcarusInterface::DestroyIcarus()
{
	for (int index = 0; index < CIcarus::s_flavorsAvailable; index++)
	{
		delete CIcarus::s_instances[index];
	}
	delete[] CIcarus::s_instances;
	CIcarus::s_instances = nullptr;
	CIcarus::s_flavorsAvailable = 0;
}

IIcarusInterface::~IIcarusInterface() = default;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CIcarus

double CIcarus::ICARUS_VERSION = 1.40;

int CIcarus::s_flavorsAvailable = 0;

CIcarus** CIcarus::s_instances = nullptr;

CIcarus::CIcarus(const int flavor) :
	m_flavor(flavor), m_nextSequencerID(0)
{
	m_GUID = 0;

#ifdef _DEBUG

	m_DEBUG_NumSequencerAlloc = 0;
	m_DEBUG_NumSequencerFreed = 0;
	m_DEBUG_NumSequencerResidual = 0;

	m_DEBUG_NumSequenceAlloc = 0;
	m_DEBUG_NumSequenceFreed = 0;
	m_DEBUG_NumSequenceResidual = 0;

#endif

	m_ulBufferCurPos = 0;
	m_ulBytesRead = 0;
	m_byBuffer = nullptr;
}

CIcarus::~CIcarus()
{
	Delete();
}

void CIcarus::Delete()
{
	Free();

#ifdef _DEBUG

	Com_Printf("ICARUS Instance Debug Info:\n");
	Com_Printf("---------------------------\n");
	Com_Printf("Sequencers Allocated:\t%d\n", m_DEBUG_NumSequencerAlloc);
	Com_Printf("Sequencers Freed:\t\t%d\n", m_DEBUG_NumSequencerFreed);
	Com_Printf("Sequencers Residual:\t%d\n\n", m_DEBUG_NumSequencerResidual);
	Com_Printf("Sequences Allocated:\t%d\n", m_DEBUG_NumSequenceAlloc);
	Com_Printf("Sequences Freed:\t\t%d\n", m_DEBUG_NumSequenceFreed);
	Com_Printf("Sequences Residual:\t\t%d\n\n", m_DEBUG_NumSequenceResidual);

#endif
}

void CIcarus::Signal(const char* identifier)
{
	m_signals[identifier] = 1;
}

bool CIcarus::CheckSignal(const char* identifier)
{
	const auto smi = m_signals.find(identifier);

	if (smi == m_signals.end())
		return false;

	return true;
}

void CIcarus::ClearSignal(const char* identifier)
{
	m_signals.erase(identifier);
}

void CIcarus::Free()
{
	sequencer_l::iterator sri;

	//Delete any residual sequencers
	STL_ITERATE(sri, m_sequencers)
	{
		(*sri)->Free(this);

#ifdef _DEBUG

		m_DEBUG_NumSequencerResidual++;

#endif
	}

	m_sequencers.clear();
	m_signals.clear();

	sequence_l::iterator si;

	//Delete any residual sequences
	STL_ITERATE(si, m_sequences)
	{
		(*si)->Delete(this);
		delete* si;

#ifdef _DEBUG

		m_DEBUG_NumSequenceResidual++;

#endif
	}

	m_sequences.clear();

	m_sequencerMap.clear();
}

int CIcarus::GetIcarusID(const int game_id)
{
	CSequencer* sequencer = CSequencer::Create();
	CTaskManager* task_manager = CTaskManager::Create();

	sequencer->Init(game_id, task_manager);

	task_manager->Init(sequencer);

	STL_INSERT(m_sequencers, sequencer);

	m_sequencerMap[sequencer->GetID()] = sequencer;

#ifdef _DEBUG

	m_DEBUG_NumSequencerAlloc++;

#endif

	return sequencer->GetID();
}

void CIcarus::DeleteIcarusID(int& icarus_id)
{
	CSequencer* sequencer = FindSequencer(icarus_id);
	if (!sequencer)
	{
		icarus_id = -1;
		return;
	}

	CTaskManager* task_manager = sequencer->GetTaskManager();
	if (task_manager->IsResident())
	{
		IGameInterface::GetGame()->DebugPrint(IGameInterface::WL_ERROR,
			"Refusing DeleteIcarusID(%d) because it is running!\n", icarus_id);
		assert(0);
		return;
	}

	m_sequencerMap.erase(icarus_id);

	// added 2/12/2 to properly delete blocks that were passed to the task manager
	sequencer->Recall(this);

	if (task_manager)
	{
		task_manager->Free();
		delete task_manager;
	}

	m_sequencers.remove(sequencer);

	sequencer->Free(this);

#ifdef _DEBUG

	m_DEBUG_NumSequencerFreed++;

#endif
	icarus_id = -1;
}

CSequence* CIcarus::GetSequence()
{
	CSequence* sequence = CSequence::Create();

	//Assign the GUID
	sequence->SetID(m_GUID++);

	STL_INSERT(m_sequences, sequence);

#ifdef _DEBUG

	m_DEBUG_NumSequenceAlloc++;

#endif

	return sequence;
}

CSequence* CIcarus::GetSequence(const int id)
{
	sequence_l::iterator si;
	STL_ITERATE(si, m_sequences)
	{
		if ((*si)->GetID() == id)
			return *si;
	}

	return nullptr;
}

void CIcarus::DeleteSequence(CSequence* sequence)
{
	m_sequences.remove(sequence);

	sequence->Delete(this);
	delete sequence;

#ifdef _DEBUG

	m_DEBUG_NumSequenceFreed++;

#endif
}

int CIcarus::AllocateSequences(const int num_sequences, const int* id_table)
{
	CSequence* sequence;

	for (int i = 0; i < num_sequences; i++)
	{
		//If the GUID of this sequence is higher than the current, take this a the "current" GUID
		if (id_table[i] > m_GUID)
			m_GUID = id_table[i];

		//Allocate the container sequence
		if ((sequence = GetSequence()) == nullptr)
			return false;

		//Override the given GUID with the real one
		sequence->SetID(id_table[i]);
	}

	return true;
}

void CIcarus::Precache(char* buffer, const long length)
{
	IGameInterface* game = IGameInterface::GetGame(m_flavor);
	CBlockStream stream;
	CBlockMember* block_member;
	CBlock block;

	if (stream.Open(buffer, length) == 0)
		return;

	const char* s_val1;

	//Now iterate through all blocks of the script, searching for keywords
	while (stream.BlockAvailable())
	{
		//Get a block
		if (stream.ReadBlock(&block, this) == 0)
			return;

		//Determine what type of block this is
		switch (block.GetBlockID())
		{
		case ID_CAMERA: // to cache ROFF files
		{
			const float f = *static_cast<float*>(block.GetMemberData(0));

			if (f == TYPE_PATH)
			{
				s_val1 = static_cast<const char*>(block.GetMemberData(1));

				game->PrecacheRoff(s_val1);
			}
		}
		break;

		case ID_PLAY: // to cache ROFF files

			s_val1 = static_cast<const char*>(block.GetMemberData(0));

			if (!Q_stricmp(s_val1, "PLAY_ROFF"))
			{
				s_val1 = static_cast<const char*>(block.GetMemberData(1));

				game->PrecacheRoff(s_val1);
			}
			break;

			//Run commands
		case ID_RUN:
			s_val1 = static_cast<const char*>(block.GetMemberData(0));
			game->PrecacheScript(s_val1);
			break;

		case ID_SOUND:
			s_val1 = static_cast<const char*>(block.GetMemberData(1)); //0 is channel, 1 is filename
			game->PrecacheSound(s_val1);
			break;

		case ID_SET:
			block_member = block.GetMember(0);

			//NOTENOTE: This will not catch special case get() inlines! (There's not really a good way to do that)

			//Make sure we're testing against strings
			if (block_member->GetID() == TK_STRING)
			{
				s_val1 = static_cast<const char*>(block.GetMemberData(0));
				const auto s_val2 = static_cast<const char*>(block.GetMemberData(1));

				game->PrecacheFromSet(s_val1, s_val2);
			}
			break;

		default:
			break;
		}

		//Clean out the block for the next pass
		block.Free(this);
	}

	//All done
	stream.Free();
}

CSequencer* CIcarus::FindSequencer(const int sequencer_id)
{
	const auto mi = m_sequencerMap.find(sequencer_id);

	if (mi == m_sequencerMap.end())
		return nullptr;

	return (*mi).second;
}

int CIcarus::Run(const int icarus_id, char* buffer, const long length)
{
	CSequencer* sequencer = FindSequencer(icarus_id);
	if (sequencer)
	{
		return sequencer->Run(buffer, length, this);
	}
	return ICARUS_INVALID;
}

int CIcarus::SaveSequenceIDTable()
{
	//Save out the number of sequences to follow
	const int num_sequences = m_sequences.size();

	BufferWrite(&num_sequences, sizeof num_sequences);

	//Sequences are saved first, by ID and information
	sequence_l::iterator sqi;

	//First pass, save all sequences ID for reconstruction
	const auto id_table = new int[num_sequences];
	int itr = 0;

	if (id_table == nullptr)
		return false;

	STL_ITERATE(sqi, m_sequences)
	{
		id_table[itr++] = (*sqi)->GetID();
	}

	//game->WriteSaveData( INT_ID('S','Q','T','B'), idTable, sizeof( int ) * numSequences );
	BufferWrite(id_table, sizeof(int) * num_sequences);

	delete[] id_table;

	return true;
}

int CIcarus::SaveSequences()
{
	//Save out a listing of all the used sequences by ID
	SaveSequenceIDTable();

	//Save all the information in order
	sequence_l::iterator sqi;
	STL_ITERATE(sqi, m_sequences)
	{
		(*sqi)->Save();
	}

	return true;
}

int CIcarus::SaveSequencers()
{
	//Save out the number of sequences to follow
	const int num_sequencers = m_sequencers.size();
	BufferWrite(&num_sequencers, sizeof num_sequencers);

	//The sequencers are then saved
	int sequencessaved = 0;
	sequencer_l::iterator si;
	STL_ITERATE(si, m_sequencers)
	{
		(*si)->Save();
		sequencessaved++;
	}

	assert(sequencessaved == num_sequencers);

	return true;
}

int CIcarus::SaveSignals()
{
	const int num_signals = m_signals.size();

	//game->WriteSaveData( INT_ID('I','S','I','G'), &numSignals, sizeof( numSignals ) );
	BufferWrite(&num_signals, sizeof num_signals);

	signal_m::iterator si;
	STL_ITERATE(si, m_signals)
	{
		//game->WriteSaveData( INT_ID('I','S','I','G'), &numSignals, sizeof( numSignals ) );
		const char* name = (*si).first.c_str();

		int length = strlen(name) + 1;

		//Save out the string size
		BufferWrite(&length, sizeof length);

		//Write out the string
		BufferWrite(name, length);
	}

	return true;
}

// Get the current Game flavor.
int CIcarus::GetFlavor()
{
	return m_flavor;
}

int CIcarus::Save()
{
	// Allocate the temporary buffer.
	CreateBuffer();

	IGameInterface* game = IGameInterface::GetGame(m_flavor);

	ojk::SavedGameHelper saved_game(
		game->get_saved_game_file());

	//Save out a ICARUS save block header with the ICARUS version
	const double version = ICARUS_VERSION;

	saved_game.write_chunk<double>(
		INT_ID('I', 'C', 'A', 'R'),
		version);

	//Save out the signals
	if (SaveSignals() == false)
	{
		DestroyBuffer();
		return false;
	}

	//Save out the sequences
	if (SaveSequences() == false)
	{
		DestroyBuffer();
		return false;
	}

	//Save out the sequencers
	if (SaveSequencers() == false)
	{
		DestroyBuffer();
		return false;
	}

	// Write out the buffer with all our collected data.
	saved_game.write_chunk(
		INT_ID('I', 'S', 'E', 'Q'),
		m_byBuffer,
		static_cast<int>(m_ulBufferCurPos));

	// De-allocate the temporary buffer.
	DestroyBuffer();

	return true;
}

int CIcarus::LoadSignals()
{
	int num_signals;

	BufferRead(&num_signals, sizeof num_signals);

	for (int i = 0; i < num_signals; i++)
	{
		char buffer[1024];
		int length = 0;

		//Get the size of the string
		BufferRead(&length, sizeof length);

		//Get the string
		BufferRead(&buffer, length);

		//Turn it on and add it to the system
		Signal(reinterpret_cast<const char*>(&buffer));
	}

	return true;
}

int CIcarus::LoadSequence()
{
	CSequence* sequence = GetSequence();

	//Load the sequence back in
	sequence->Load(this);

	//If this sequence had a higher GUID than the current, save it
	if (sequence->GetID() > m_GUID)
		m_GUID = sequence->GetID();

	return true;
}

int CIcarus::LoadSequences()
{
	CSequence* sequence;
	int num_sequences;

	//Get the number of sequences to read in
	BufferRead(&num_sequences, sizeof num_sequences);

	const auto id_table = new int[num_sequences];

	if (id_table == nullptr)
		return false;

	//Load the sequencer ID table
	BufferRead(id_table, sizeof(int) * num_sequences);

	//First pass, allocate all container sequences and give them their proper IDs
	if (AllocateSequences(num_sequences, id_table) == false)
		return false;

	//Second pass, load all sequences
	for (int i = 0; i < num_sequences; i++)
	{
		//Get the proper sequence for this load
		if ((sequence = GetSequence(id_table[i])) == nullptr)
			return false;

		//Load the sequence
		if (sequence->Load(this) == false)
			return false;
	}

	//Free the idTable
	delete[] id_table;

	return true;
}

int CIcarus::LoadSequencers()
{
	CSequencer* sequencer;
	int num_sequencers;
	IGameInterface* game = IGameInterface::GetGame(m_flavor);

	//Get the number of sequencers to load
	BufferRead(&num_sequencers, sizeof num_sequencers);

	//Load all sequencers
	for (int i = 0; i < num_sequencers; i++)
	{
		//NOTENOTE: The ownerID will be replaced in the loading process
		const int sequencer_id = GetIcarusID(-1);
		if ((sequencer = FindSequencer(sequencer_id)) == nullptr)
			return false;

		if (sequencer->Load(this, game) == false)
			return false;
	}

	return true;
}

int CIcarus::Load()
{
	CreateBuffer();

	IGameInterface* game = IGameInterface::GetGame(m_flavor);

	ojk::SavedGameHelper saved_game(
		game->get_saved_game_file());

	//Clear out any old information
	Free();

	//Check to make sure we're at the ICARUS save block
	double version = 0.0;

	saved_game.read_chunk<double>(
		INT_ID('I', 'C', 'A', 'R'),
		version);

	//Versions must match!
	if (version != ICARUS_VERSION)
	{
		DestroyBuffer();
		game->DebugPrint(IGameInterface::WL_ERROR, "save game data contains outdated ICARUS version information!\n");
		return false;
	}

	// Read into the buffer all our data.
	saved_game.read_chunk(
		INT_ID('I', 'S', 'E', 'Q'));

	const auto sg_buffer_data = static_cast<const unsigned char*>(
		saved_game.get_buffer_data());

	const int sg_buffer_size = saved_game.get_buffer_size();

	if (sg_buffer_size < 0 || static_cast<size_t>(sg_buffer_size) > MAX_BUFFER_SIZE)
	{
		DestroyBuffer();
		game->DebugPrint(IGameInterface::WL_ERROR, "invalid ISEQ length: %d bytes\n", sg_buffer_size);
		return false;
	}

	std::uninitialized_copy_n(
		sg_buffer_data,
		sg_buffer_size,
		m_byBuffer);

	//Load all signals
	if (LoadSignals() == false)
	{
		DestroyBuffer();
		game->DebugPrint(IGameInterface::WL_ERROR, "failed to load signals from save game!\n");
		return false;
	}

	//Load in all sequences
	if (LoadSequences() == false)
	{
		DestroyBuffer();
		game->DebugPrint(IGameInterface::WL_ERROR, "failed to load sequences from save game!\n");
		return false;
	}

	//Load in all sequencers
	if (LoadSequencers() == false)
	{
		DestroyBuffer();
		game->DebugPrint(IGameInterface::WL_ERROR, "failed to load sequencers from save game!\n");
		return false;
	}

	DestroyBuffer();

	return true;
}

int CIcarus::Update(const int icarus_id)
{
	const CSequencer* sequencer = FindSequencer(icarus_id);
	if (sequencer)
	{
		return sequencer->GetTaskManager()->Update(this);
	}
	return -1;
}

int CIcarus::IsRunning(const int icarus_id)
{
	const CSequencer* sequencer = FindSequencer(icarus_id);
	if (sequencer)
	{
		return sequencer->GetTaskManager()->IsRunning();
	}
	return false;
}

void CIcarus::Completed(const int icarus_id, const int task_id)
{
	const CSequencer* sequencer = FindSequencer(icarus_id);
	if (sequencer)
	{
		sequencer->GetTaskManager()->Completed(task_id);
	}
}

// Destroy the File Buffer.
void CIcarus::DestroyBuffer()
{
	if (m_byBuffer)
	{
		IGameInterface::GetGame()->Free(m_byBuffer);
		m_byBuffer = nullptr;
	}
}

// Create the File Buffer.
void CIcarus::CreateBuffer()
{
	DestroyBuffer();
	m_byBuffer = static_cast<unsigned char*>(IGameInterface::GetGame()->Malloc(MAX_BUFFER_SIZE));
	m_ulBufferCurPos = 0;
}

// Write to a buffer.
void CIcarus::BufferWrite(const void* p_src_data, const unsigned long ul_num_bytes_to_write)
{
	if (!p_src_data)
		return;

	// Make sure we have enough space in the buffer to write to.
	if (MAX_BUFFER_SIZE - m_ulBufferCurPos < ul_num_bytes_to_write)
	{
		// Write out the buffer with all our collected data so far...
		IGameInterface::GetGame()->DebugPrint(IGameInterface::WL_ERROR, "BufferWrite: Out of buffer space, Flushing.");

		ojk::SavedGameHelper saved_game(
			IGameInterface::GetGame()->get_saved_game_file());

		saved_game.write_chunk(
			INT_ID('I', 'S', 'E', 'Q'),
			m_byBuffer,
			static_cast<int>(m_ulBufferCurPos));

		m_ulBufferCurPos = 0; //reset buffer
	}

	assert(MAX_BUFFER_SIZE - m_ulBufferCurPos >= ul_num_bytes_to_write);
	{
		memcpy(m_byBuffer + m_ulBufferCurPos, p_src_data, ul_num_bytes_to_write);
		m_ulBufferCurPos += ul_num_bytes_to_write;
	}
}

// Read from a buffer.
void CIcarus::BufferRead(void* p_dst_buff, const unsigned long ul_num_bytes_to_read)
{
	if (!p_dst_buff)
		return;

	// If we can read this data...
	if (m_ulBytesRead + ul_num_bytes_to_read > MAX_BUFFER_SIZE)
	{
		// We've tried to read past the buffer...
		IGameInterface::GetGame()->DebugPrint(IGameInterface::WL_ERROR,
			"BufferRead: Buffer underflow, Looking for new block.");
		// Read in the next block.

		const ojk::SavedGameHelper saved_game(
			IGameInterface::GetGame()->get_saved_game_file());

		saved_game.read_chunk(
			INT_ID('I', 'S', 'E', 'Q'));

		const auto sg_buffer_data = static_cast<const unsigned char*>(
			saved_game.get_buffer_data());

		const int sg_buffer_size = saved_game.get_buffer_size();

		if (sg_buffer_size < 0 || static_cast<size_t>(sg_buffer_size) > MAX_BUFFER_SIZE)
		{
			IGameInterface::GetGame()->DebugPrint(IGameInterface::WL_ERROR, "invalid ISEQ length: %d bytes\n",
				sg_buffer_size);
			return;
		}

		std::uninitialized_copy_n(
			sg_buffer_data,
			sg_buffer_size,
			m_byBuffer);

		m_ulBytesRead = 0; //reset buffer
	}

	assert(m_ulBytesRead + ul_num_bytes_to_read <= MAX_BUFFER_SIZE);
	{
		memcpy(p_dst_buff, m_byBuffer + m_ulBytesRead, ul_num_bytes_to_read);
		m_ulBytesRead += ul_num_bytes_to_read;
	}
}