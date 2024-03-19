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

// Task Manager header file

#ifndef __TASK_MANAGER__
#define __TASK_MANAGER__

#include "../qcommon/q_shared.h"

constexpr auto MAX_TASK_NAME = 64;
constexpr auto TASKFLAG_NORMAL = 0x00000000;
constexpr int RUNAWAY_LIMIT = 256;

enum
{
	TASK_RETURN_COMPLETE,
	TASK_RETURN_FAILED,
};

enum
{
	TASK_OK,
	TASK_FAILED,
	TASK_START,
	TASK_END,
};

// CTask

class CTask
{
public:
	CTask();
	~CTask();

	static CTask* Create(int guid, CBlock* block);

	void Free() const;

	unsigned int GetTimeStamp() const { return m_timeStamp; }
	CBlock* GetBlock() const { return m_block; }
	int GetGUID() const { return m_id; }
	int GetID() const { return m_block->GetBlockID(); }

	void SetTimeStamp(const unsigned int timeStamp) { m_timeStamp = timeStamp; }
	void SetBlock(CBlock* block) { m_block = block; }
	void SetGUID(const int id) { m_id = id; }

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
	int m_id;
	unsigned int m_timeStamp;
	CBlock* m_block;
};

// CTaskGroup

class CTaskGroup
{
public:
	using taskCallback_m = std::map<int, bool>;

	CTaskGroup();
	~CTaskGroup();

	void Init();

	int Add(const CTask* task);

	void SetGUID(int guid);
	void SetParent(CTaskGroup* group) { m_parent = group; }

	bool Complete() const { return m_numCompleted == m_completedTasks.size(); }

	bool MarkTaskComplete(int id);

	CTaskGroup* GetParent() const { return m_parent; }
	int GetGUID() const { return m_GUID; }

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

	//protected:

	taskCallback_m m_completedTasks;

	CTaskGroup* m_parent;

	unsigned int m_numCompleted;
	int m_GUID;
};

// CTaskManager
class CSequencer;

class CTaskManager
{
	using taskID_m = std::map<int, CTask*>;
	using taskGroupName_m = std::map<std::string, CTaskGroup*>;
	using taskGroupID_m = std::map<int, CTaskGroup*>;
	using taskGroup_v = std::vector<CTaskGroup*>;
	using tasks_l = std::list<CTask*>;

public:
	CTaskManager();
	~CTaskManager();

	int GetID() const;

	static CTaskManager* Create();

	CBlock* GetCurrentTask();

	int Init(CSequencer* owner);
	int Free();

	static int Flush();

	int SetCommand(CBlock* command, int type, const CIcarus* icarus);
	int Completed(int id);

	int Update(CIcarus* icarus);
	int IsRunning() const { return !m_tasks.empty(); };
	bool IsResident() const { return m_resident; };

	CTaskGroup* AddTaskGroup(const char* name, const CIcarus* icarus);
	CTaskGroup* GetTaskGroup(const char* name, const CIcarus* icarus);
	CTaskGroup* GetTaskGroup(int id, const CIcarus* icarus);

	int MarkTask(int id, int operation, const CIcarus* icarus);
	CBlock* RecallTask();

	void Save();
	void Load(CIcarus* icarus);

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
	int Go(CIcarus* icarus); //Heartbeat function called once per game frame
	int CallbackCommand(const CTask* task, int return_code, CIcarus* icarus);

	static inline bool Check(int target_id, const CBlock* block, int member_num);

	static int GetVector(int ent_id, CBlock* block, int& member_num, vec3_t& value, CIcarus* icarus);
	static int GetFloat(int ent_id, const CBlock* block, int& member_num, float& value, const CIcarus* icarus);
	static int Get(int ent_id, CBlock* block, int& member_num, char** value, CIcarus* icarus);

	int PushTask(CTask* task, int flag);
	CTask* PopTask(int flag);

	// Task functions
	int Rotate(const CTask* task, CIcarus* icarus) const;
	int Remove(const CTask* task, CIcarus* icarus);
	int Camera(const CTask* task, CIcarus* icarus);
	int Print(const CTask* task, CIcarus* icarus);
	int Sound(const CTask* task, CIcarus* icarus);
	int Move(const CTask* task, CIcarus* icarus) const;
	int Kill(const CTask* task, CIcarus* icarus);
	int Set(const CTask* task, CIcarus* icarus) const;
	int Use(const CTask* task, CIcarus* icarus);
	int DeclareVariable(const CTask* task, CIcarus* icarus);
	int FreeVariable(const CTask* task, CIcarus* icarus);
	int Signal(const CTask* task, CIcarus* icarus);
	int Play(const CTask* task, CIcarus* icarus) const;

	int Wait(const CTask* task, bool& completed, CIcarus* icarus);
	int WaitSignal(const CTask* task, bool& completed, CIcarus* icarus) const;

	static int SaveCommand(const CBlock* block);

	// Variables

	CSequencer* m_owner;
	int m_ownerID;

	CTaskGroup* m_curGroup;

	taskGroup_v m_taskGroups;
	tasks_l m_tasks;

	int m_GUID;
	int m_count;

	taskGroupName_m m_taskGroupNameMap;
	taskGroupID_m m_taskGroupIDMap;

	bool m_resident;

	int m_id;

	//CTask	*m_waitTask;		//Global pointer to the current task that is waiting for callback completion
};

#endif	//__TASK_MANAGER__
