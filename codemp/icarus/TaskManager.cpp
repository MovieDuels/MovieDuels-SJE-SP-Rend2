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

// Task Manager
//
//	-- jweier

#include "icarus.h"

#include <assert.h>
#include "server/server.h"

#define ICARUS_VALIDATE(a) if ( a == false ) return TASK_FAILED;

/*
=================================================

CTask

=================================================
*/

CTask::CTask(void)
{
}

CTask::~CTask(void)
{
}

CTask* CTask::Create(int guid, CBlock* block)
{
	CTask* task = new CTask;

	//TODO: Emit warning
	assert(task);
	if (task == nullptr)
		return nullptr;

	task->SetTimeStamp(0);
	task->SetBlock(block);
	task->SetGUID(guid);

	return task;
}

/*
-------------------------
Free
-------------------------
*/

void CTask::Free(void) const
{
	//NOTENOTE: The block is not consumed by the task, it is the sequencer's job to clean blocks up
	delete this;
}

/*
=================================================

CTaskGroup

=================================================
*/

CTaskGroup::CTaskGroup(void)
{
	Init();

	m_GUID = 0;
	m_parent = nullptr;
}

CTaskGroup::~CTaskGroup(void)
{
	m_completedTasks.clear();
}

/*
-------------------------
SetGUID
-------------------------
*/

void CTaskGroup::SetGUID(int guid)
{
	m_GUID = guid;
}

/*
-------------------------
Init
-------------------------
*/

void CTaskGroup::Init(void)
{
	m_completedTasks.clear();

	m_numCompleted = 0;
	m_parent = nullptr;
}

/*
-------------------------
Add
-------------------------
*/

int CTaskGroup::Add(const CTask* task)
{
	m_completedTasks[task->GetGUID()] = false;
	return TASK_OK;
}

/*
-------------------------
MarkTaskComplete
-------------------------
*/

bool CTaskGroup::MarkTaskComplete(int id)
{
	if ((m_completedTasks.find(id)) != m_completedTasks.end())
	{
		m_completedTasks[id] = true;
		m_numCompleted++;

		return true;
	}

	return false;
}

/*
=================================================

CTaskManager

=================================================
*/

CTaskManager::CTaskManager(void)
{
}

CTaskManager::~CTaskManager(void)
{
}

/*
-------------------------
Create
-------------------------
*/

CTaskManager* CTaskManager::Create(void)
{
	return new CTaskManager;
}

/*
-------------------------
Init
-------------------------
*/

int	CTaskManager::Init(CSequencer* owner)
{
	//TODO: Emit warning
	if (owner == nullptr)
		return TASK_FAILED;

	m_tasks.clear();
	m_owner = owner;
	m_ownerID = owner->GetOwnerID();
	m_curGroup = nullptr;
	m_GUID = 0;
	m_resident = false;

	return TASK_OK;
}

/*
-------------------------
Free
-------------------------
*/

int CTaskManager::Free(void)
{
	//Clear out all pending tasks
	for (tasks_l::iterator ti = m_tasks.begin(); ti != m_tasks.end(); ++ti)
	{
		(*ti)->Free();
	}

	m_tasks.clear();

	//Clear out all taskGroups
	for (taskGroup_v::iterator gi = m_taskGroups.begin(); gi != m_taskGroups.end(); ++gi)
	{
		delete (*gi);
	}

	m_taskGroups.clear();
	m_taskGroupNameMap.clear();
	m_taskGroupIDMap.clear();

	return TASK_OK;
}

/*
-------------------------
Flush
-------------------------
*/

int CTaskManager::Flush(void)
{
	//FIXME: Rewrite

	return true;
}

/*
-------------------------
AddTaskGroup
-------------------------
*/

CTaskGroup* CTaskManager::AddTaskGroup(const char* name)
{
	CTaskGroup* group;

	const taskGroupName_m::iterator tgni = m_taskGroupNameMap.find(name);

	if (tgni != m_taskGroupNameMap.end())
	{
		group = (*tgni).second;

		//Clear it and just move on
		group->Init();

		return group;
	}

	//Allocate a new one
	group = new CTaskGroup;

	//TODO: Emit warning
	assert(group);
	if (group == nullptr)
	{
		(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Unable to allocate task group \"%s\"\n", name);
		return nullptr;
	}

	//Setup the internal information
	group->SetGUID(m_GUID++);

	//Add it to the list and associate it for retrieval later
	m_taskGroups.insert(m_taskGroups.end(), group);
	m_taskGroupNameMap[name] = group;
	m_taskGroupIDMap[group->GetGUID()] = group;

	return group;
}

/*
-------------------------
GetTaskGroup
-------------------------
*/

CTaskGroup* CTaskManager::GetTaskGroup(const char* name)
{
	const taskGroupName_m::iterator tgi = m_taskGroupNameMap.find(name);

	if (tgi == m_taskGroupNameMap.end())
	{
		(m_owner->GetInterface())->I_DPrintf(WL_WARNING, "Could not find task group \"%s\"\n", name);
		return nullptr;
	}

	return (*tgi).second;
}

CTaskGroup* CTaskManager::GetTaskGroup(int id)
{
	const taskGroupID_m::iterator tgi = m_taskGroupIDMap.find(id);

	if (tgi == m_taskGroupIDMap.end())
	{
		(m_owner->GetInterface())->I_DPrintf(WL_WARNING, "Could not find task group \"%d\"\n", id);
		return nullptr;
	}

	return (*tgi).second;
}

/*
-------------------------
Update
-------------------------
*/

int CTaskManager::Update(void)
{
	const sharedEntity_t* owner = SV_GentityNum(m_ownerID);

	if ((owner->r.svFlags & SVF_ICARUS_FREEZE))
	{
		return TASK_FAILED;
	}
	m_count = 0;	//Needed for runaway init
	m_resident = true;

	const int returnVal = Go();

	m_resident = false;

	return returnVal;
}

/*
-------------------------
IsRunning
-------------------------
*/

qboolean CTaskManager::IsRunning(void) const
{
	return static_cast<qboolean>(m_tasks.empty() == false);
}
/*
-------------------------
Check
-------------------------
*/

inline bool CTaskManager::Check(int target_id, const CBlock* block, int member_num)
{
	if ((block->GetMember(member_num))->GetID() == target_id)
		return true;

	return false;
}

/*
-------------------------
GetFloat
-------------------------
*/

int CTaskManager::GetFloat(int entID, CBlock* block, int& memberNum, float& value) const
{
	//See if this is a get() command replacement
	if (Check(ID_GET, block, memberNum))
	{
		//Update the member past the header id
		memberNum++;

		//get( TYPE, NAME )
		const int type = static_cast<int>(*(float*)block->GetMemberData(memberNum++));
		const char* name = static_cast<char*>(block->GetMemberData(memberNum++));

		//TODO: Emit warning
		if (type != TK_FLOAT)
		{
			(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Get() call tried to return a non-FLOAT parameter!\n");
			return false;
		}

		return (m_owner->GetInterface())->I_GetFloat(entID, type, name, &value);
	}

	//Look for a Q_flrand(0.0f, 1.0f) inline call
	if (Check(ID_RANDOM, block, memberNum))
	{
		memberNum++;

		const float min = *static_cast<float*>(block->GetMemberData(memberNum++));
		const float max = *static_cast<float*>(block->GetMemberData(memberNum++));

		value = (m_owner->GetInterface())->I_Random(min, max);

		return true;
	}

	//Look for a tag() inline call
	if (Check(ID_TAG, block, memberNum))
	{
		(m_owner->GetInterface())->I_DPrintf(WL_WARNING, "Invalid use of \"tag\" inline.  Not a valid replacement for type FLOAT\n");
		return false;
	}

	const CBlockMember* bm = block->GetMember(memberNum);

	if (bm->GetID() == TK_INT)
	{
		value = static_cast<float>(*(int*)block->GetMemberData(memberNum++));
	}
	else if (bm->GetID() == TK_FLOAT)
	{
		value = *static_cast<float*>(block->GetMemberData(memberNum++));
	}
	else
	{
		assert(0);
		(m_owner->GetInterface())->I_DPrintf(WL_WARNING, "Unexpected value; expected type FLOAT\n");
		return false;
	}

	return true;
}

/*
-------------------------
GetVector
-------------------------
*/

int CTaskManager::GetVector(int entID, CBlock* block, int& memberNum, vector_t& value)
{
	int		type, i;

	//See if this is a get() command replacement
	if (Check(ID_GET, block, memberNum))
	{
		//Update the member past the header id
		memberNum++;

		//get( TYPE, NAME )
		type = static_cast<int>(*(float*)block->GetMemberData(memberNum++));
		const char* name = static_cast<char*>(block->GetMemberData(memberNum++));

		//TODO: Emit warning
		if (type != TK_VECTOR)
		{
			(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Get() call tried to return a non-VECTOR parameter!\n");
		}

		return (m_owner->GetInterface())->I_GetVector(entID, type, name, value);
	}

	//Look for a Q_flrand(0.0f, 1.0f) inline call
	if (Check(ID_RANDOM, block, memberNum))
	{
		memberNum++;

		const float min = *static_cast<float*>(block->GetMemberData(memberNum++));
		const float max = *static_cast<float*>(block->GetMemberData(memberNum++));

		for (i = 0; i < 3; i++)
		{
			value[i] = (float)(m_owner->GetInterface())->I_Random(min, max);	//FIXME: Just truncating it for now.. should be fine though
		}

		return true;
	}

	//Look for a tag() inline call
	if (Check(ID_TAG, block, memberNum))
	{
		char* tagName;
		float	tagLookup;

		memberNum++;
		ICARUS_VALIDATE(Get(entID, block, memberNum, &tagName));
		ICARUS_VALIDATE(GetFloat(entID, block, memberNum, tagLookup));

		if ((m_owner->GetInterface())->I_GetTag(entID, tagName, static_cast<int>(tagLookup), value) == false)
		{
			(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Unable to find tag \"%s\" for ent %i!\n", tagName, entID);
			//			assert(0);
			return TASK_FAILED;
		}

		return true;
	}

	//Check for a real vector here
	type = static_cast<int>(*(float*)block->GetMemberData(memberNum));

	if (type != TK_VECTOR)
	{
		//		(m_owner->GetInterface())->I_DPrintf( WL_WARNING, "Unexpected value; expected type VECTOR\n" );
		return false;
	}

	memberNum++;

	for (i = 0; i < 3; i++)
	{
		if (GetFloat(entID, block, memberNum, value[i]) == false)
			return false;
	}

	return true;
}

/*
-------------------------
Get
-------------------------
*/

int CTaskManager::Get(int entID, CBlock* block, int& memberNum, char** value)
{
	static	char	tempBuffer[128];	//FIXME: EEEK!
	vector_t		vector;
	char* tagName;
	float			tagLookup;

	//Look for a get() inline call
	if (Check(ID_GET, block, memberNum))
	{
		//Update the member past the header id
		memberNum++;

		//get( TYPE, NAME )
		const int type = static_cast<int>(*(float*)block->GetMemberData(memberNum++));
		char* name = static_cast<char*>(block->GetMemberData(memberNum++));

		//Format the return properly
		//FIXME: This is probably doing double formatting in certain cases...
		//FIXME: STRING MANAGEMENT NEEDS TO BE IMPLEMENTED, MY CURRENT SOLUTION IS NOT ACCEPTABLE!!
		switch (type)
		{
		case TK_STRING:
			if ((m_owner->GetInterface())->I_GetString(entID, type, name, value) == false)
			{
				(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Get() parameter \"%s\" could not be found!\n", name);
				return false;
			}

			return true;

		case TK_FLOAT:
		{
			float	temp;

			if ((m_owner->GetInterface())->I_GetFloat(entID, type, name, &temp) == false)
			{
				(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Get() parameter \"%s\" could not be found!\n", name);
				return false;
			}

			Com_sprintf(tempBuffer, sizeof(tempBuffer), "%f", temp);
			*value = static_cast<char*>(tempBuffer);
		}

		return true;

		case TK_VECTOR:
		{
			vector_t	vval;

			if ((m_owner->GetInterface())->I_GetVector(entID, type, name, vval) == false)
			{
				(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Get() parameter \"%s\" could not be found!\n", name);
				return false;
			}

			Com_sprintf(tempBuffer, sizeof(tempBuffer), "%f %f %f", vval[0], vval[1], vval[2]);
			*value = static_cast<char*>(tempBuffer);
		}

		return true;

		default:
			(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Get() call tried to return an unknown type!\n");
			return false;
		}
	}

	//Look for a Q_flrand(0.0f, 1.0f) inline call
	if (Check(ID_RANDOM, block, memberNum))
	{
		memberNum++;

		const float min = *static_cast<float*>(block->GetMemberData(memberNum++));
		const float max = *static_cast<float*>(block->GetMemberData(memberNum++));

		const float ret = (m_owner->GetInterface())->I_Random(min, max);

		Com_sprintf(tempBuffer, sizeof(tempBuffer), "%f", ret);
		*value = static_cast<char*>(tempBuffer);

		return true;
	}

	//Look for a tag() inline call
	if (Check(ID_TAG, block, memberNum))
	{
		memberNum++;
		ICARUS_VALIDATE(Get(entID, block, memberNum, &tagName));
		ICARUS_VALIDATE(GetFloat(entID, block, memberNum, tagLookup));

		if ((m_owner->GetInterface())->I_GetTag(entID, tagName, static_cast<int>(tagLookup), vector) == false)
		{
			(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Unable to find tag \"%s\"!\n", tagName);
			assert(0 && "Unable to find tag");
			return false;
		}

		Com_sprintf(tempBuffer, sizeof(tempBuffer), "%f %f %f", vector[0], vector[1], vector[2]);
		*value = static_cast<char*>(tempBuffer);

		return true;
	}

	//Get an actual piece of data

	const CBlockMember* bm = block->GetMember(memberNum);

	if (bm->GetID() == TK_INT)
	{
		const float fval = static_cast<float>(*(int*)block->GetMemberData(memberNum++));
		Com_sprintf(tempBuffer, sizeof(tempBuffer), "%f", fval);
		*value = static_cast<char*>(tempBuffer);

		return true;
	}
	if (bm->GetID() == TK_FLOAT)
	{
		const float fval = *static_cast<float*>(block->GetMemberData(memberNum++));
		Com_sprintf(tempBuffer, sizeof(tempBuffer), "%f", fval);
		*value = static_cast<char*>(tempBuffer);

		return true;
	}
	if (bm->GetID() == TK_VECTOR)
	{
		vector_t	vval;

		memberNum++;

		for (int i = 0; i < 3; i++)
		{
			if (GetFloat(entID, block, memberNum, vval[i]) == false)
				return false;
		}

		Com_sprintf(tempBuffer, sizeof(tempBuffer), "%f %f %f", vval[0], vval[1], vval[2]);
		*value = static_cast<char*>(tempBuffer);

		return true;
	}
	if ((bm->GetID() == TK_STRING) || (bm->GetID() == TK_IDENTIFIER))
	{
		*value = static_cast<char*>(block->GetMemberData(memberNum++));

		return true;
	}

	//TODO: Emit warning
	assert(0);
	(m_owner->GetInterface())->I_DPrintf(WL_WARNING, "Unexpected value; expected type STRING\n");

	return false;
}

/*
-------------------------
Go
-------------------------
*/

int	CTaskManager::Go(void)
{
	bool	completed = false;

	//Check for run away scripts
	if (m_count++ > RUNAWAY_LIMIT)
	{
		assert(0);
		(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Runaway loop detected!\n");
		return TASK_FAILED;
	}

	//If there are tasks to complete, do so
	if (m_tasks.empty() == false)
	{
		//Get the next task
		CTask* task = PopTask(POP_BACK);

		assert(task);
		if (task == nullptr)
		{
			(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Invalid task found in Go()!\n");
			return TASK_FAILED;
		}

		//If this hasn't been stamped, do so
		if (task->GetTimeStamp() == 0)
			task->SetTimeStamp((m_owner->GetInterface())->I_GetTime());

		//Switch and call the proper function
		switch (task->GetID())
		{
		case ID_WAIT:

			Wait(task, completed);

			//Push it to consider it again on the next frame if not complete
			if (completed == false)
			{
				PushTask(task, PUSH_BACK);
				return TASK_OK;
			}

			Completed(task->GetGUID());

			break;

		case ID_WAITSIGNAL:

			WaitSignal(task, completed);

			//Push it to consider it again on the next frame if not complete
			if (completed == false)
			{
				PushTask(task, PUSH_BACK);
				return TASK_OK;
			}

			Completed(task->GetGUID());

			break;

		case ID_PRINT:	//print( STRING )
			Print(task);
			break;

		case ID_SOUND:	//sound( name )
			Sound(task);
			break;

		case ID_MOVE:	//move ( ORIGIN, ANGLES, DURATION )
			Move(task);
			break;

		case ID_ROTATE:	//rotate( ANGLES, DURATION )
			Rotate(task);
			break;

		case ID_KILL:	//kill( NAME )
			Kill(task);
			break;

		case ID_REMOVE:	//remove( NAME )
			Remove(task);
			break;

		case ID_CAMERA:	//camera( ? )
			Camera(task);
			break;

		case ID_SET:	//set( NAME, ? )
			Set(task);
			break;

		case ID_USE:	//use( NAME )
			Use(task);
			break;

		case ID_DECLARE://declare( TYPE, NAME )
			DeclareVariable(task);
			break;

		case ID_FREE:	//free( NAME )
			FreeVariable(task);
			break;

		case ID_SIGNAL:	//signal( NAME )
			Signal(task);
			break;

		case ID_PLAY:	//play ( NAME )
			Play(task);
			break;

		default:
			assert(0);
			task->Free();
			(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Found unknown task type!\n");
			return TASK_FAILED;
		}

		//Pump the sequencer for another task
		CallbackCommand(task, TASK_RETURN_COMPLETE);

		task->Free();
	}

	//FIXME: A command surge limiter could be implemented at this point to be sure a script doesn't
	//		 execute too many commands in one cycle.  This may, however, cause timing errors to surface.

	return TASK_OK;
}

/*
-------------------------
SetCommand
-------------------------
*/

int	CTaskManager::SetCommand(CBlock* command, int type)
{
	CTask* task = CTask::Create(m_GUID++, command);

	//If this is part of a task group, add it in
	if (m_curGroup)
	{
		m_curGroup->Add(task);
	}

	//TODO: Emit warning
	assert(task);
	if (task == nullptr)
	{
		(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Unable to allocate new task!\n");
		return TASK_FAILED;
	}

	PushTask(task, type);

	return TASK_OK;
}

/*
-------------------------
MarkTask
-------------------------
*/

int CTaskManager::MarkTask(int id, int operation)
{
	CTaskGroup* group = GetTaskGroup(id);

	assert(group);

	if (group == nullptr)
		return TASK_FAILED;

	if (operation == TASK_START)
	{
		//Reset all the completion information
		group->Init();

		group->SetParent(m_curGroup);
		m_curGroup = group;
	}
	else if (operation == TASK_END)
	{
		assert(m_curGroup);
		if (m_curGroup == nullptr)
			return TASK_FAILED;

		m_curGroup = m_curGroup->GetParent();
	}

#ifdef _DEBUG
	else
	{
		assert(0);
	}
#endif

	return TASK_OK;
}

/*
-------------------------
Completed
-------------------------
*/

int CTaskManager::Completed(int id)
{
	//Mark the task as completed
	for (taskGroup_v::iterator tgi = m_taskGroups.begin(); tgi != m_taskGroups.end(); ++tgi)
	{
		//If this returns true, then the task was marked properly
		if ((*tgi)->MarkTaskComplete(id))
			break;
	}

	return TASK_OK;
}

/*
-------------------------
CallbackCommand
-------------------------
*/

int	CTaskManager::CallbackCommand(CTask* task, int returnCode)
{
	if (m_owner->Callback(this, task->GetBlock(), returnCode) == SEQ_OK)
		return Go();

	assert(0);

	(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Command callback failure!\n");
	return TASK_FAILED;
}

/*
-------------------------
RecallTask
-------------------------
*/

CBlock* CTaskManager::RecallTask(void)
{
	const CTask* task = PopTask(POP_BACK);

	if (task)
	{
		// fixed 2/12/2 to free the task that has been popped (called from sequencer Recall)
		CBlock* retBlock = task->GetBlock();
		task->Free();

		return retBlock;
		//	return task->GetBlock();
	}

	return nullptr;
}

/*
-------------------------
PushTask
-------------------------
*/

int	CTaskManager::PushTask(CTask* task, int flag)
{
	assert((flag == PUSH_FRONT) || (flag == PUSH_BACK));

	switch (flag)
	{
	case PUSH_FRONT:
		m_tasks.insert(m_tasks.begin(), task);

		return TASK_OK;

	case PUSH_BACK:
		m_tasks.insert(m_tasks.end(), task);

		return TASK_OK;
	}

	//Invalid flag
	return SEQ_FAILED;
}

/*
-------------------------
PopTask
-------------------------
*/

CTask* CTaskManager::PopTask(int flag)
{
	CTask* task;

	assert((flag == POP_FRONT) || (flag == POP_BACK));

	if (m_tasks.empty())
		return nullptr;

	switch (flag)
	{
	case POP_FRONT:
		task = m_tasks.front();
		m_tasks.pop_front();

		return task;

	case POP_BACK:
		task = m_tasks.back();
		m_tasks.pop_back();

		return task;
	}

	//Invalid flag
	return nullptr;
}

/*
-------------------------
GetCurrentTask
-------------------------
*/

CBlock* CTaskManager::GetCurrentTask(void)
{
	const CTask* task = PopTask(POP_BACK);

	if (task == nullptr)
		return nullptr;
	// fixed 2/12/2 to free the task that has been popped (called from sequencer Interrupt)
	CBlock* retBlock = task->GetBlock();
	task->Free();

	return retBlock;
	//	return task->GetBlock();
}

/*
=================================================

  Task Functions

=================================================
*/

int CTaskManager::Wait(CTask* task, bool& completed)
{
	CBlock* block = task->GetBlock();
	char* sVal;
	float			dwtime;
	int				memberNum = 0;

	completed = false;

	CBlockMember* bm = block->GetMember(0);

	//Check if this is a task completion wait
	if (bm->GetID() == TK_STRING)
	{
		ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

		if (task->GetTimeStamp() == (m_owner->GetInterface())->I_GetTime())
		{
			//Print out the debug info
			(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d wait(\"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp());
		}

		const CTaskGroup* group = GetTaskGroup(sVal);

		if (group == nullptr)
		{
			//TODO: Emit warning
			completed = false;
			return TASK_FAILED;
		}

		completed = group->Complete();
	}
	else	//Otherwise it's a time completion wait
	{
		if (Check(ID_RANDOM, block, memberNum))
		{//get it random only the first time
			dwtime = *static_cast<float*>(block->GetMemberData(memberNum++));
			if (dwtime == Q3_INFINITE)
			{//we have not evaluated this random yet
				const float min = *static_cast<float*>(block->GetMemberData(memberNum++));
				const float max = *static_cast<float*>(block->GetMemberData(memberNum++));

				dwtime = (m_owner->GetInterface())->I_Random(min, max);

				//store the result in the first member
				bm->SetData(&dwtime, sizeof(dwtime));
			}
		}
		else
		{
			ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, dwtime));
		}

		if (task->GetTimeStamp() == (m_owner->GetInterface())->I_GetTime())
		{
			//Print out the debug info
			(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d wait( %d ); [%d]", m_ownerID, static_cast<int>(dwtime), task->GetTimeStamp());
		}

		if ((task->GetTimeStamp() + dwtime) < ((m_owner->GetInterface())->I_GetTime()))
		{
			completed = true;
			memberNum = 0;
			if (Check(ID_RANDOM, block, memberNum))
			{//set the data back to 0 so it will be re-randomized next time
				dwtime = Q3_INFINITE;
				bm->SetData(&dwtime, sizeof(dwtime));
			}
		}
	}

	return TASK_OK;
}

/*
-------------------------
WaitSignal
-------------------------
*/

int CTaskManager::WaitSignal(CTask* task, bool& completed)
{
	CBlock* block = task->GetBlock();
	char* sVal;
	int		memberNum = 0;

	completed = false;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

	if (task->GetTimeStamp() == (m_owner->GetInterface())->I_GetTime())
	{
		//Print out the debug info
		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d waitsignal(\"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp());
	}

	if ((m_owner->GetOwner())->CheckSignal(sVal))
	{
		completed = true;
		(m_owner->GetOwner())->ClearSignal(sVal);
	}

	return TASK_OK;
}

/*
-------------------------
Print
-------------------------
*/

int CTaskManager::Print(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d print(\"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp());

	(m_owner->GetInterface())->I_CenterPrint(sVal);

	Completed(task->GetGUID());

	return TASK_OK;
}

/*
-------------------------
Sound
-------------------------
*/

int CTaskManager::Sound(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal, * sVal2;
	int		memberNum = 0;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));
	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal2));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d sound(\"%s\", \"%s\"); [%d]", m_ownerID, sVal, sVal2, task->GetTimeStamp());

	//Only instantly complete if the user has requested it
	if ((m_owner->GetInterface())->I_PlaySound(task->GetGUID(), m_ownerID, sVal2, sVal))
		Completed(task->GetGUID());

	return TASK_OK;
}

/*
-------------------------
Rotate
-------------------------
*/

int CTaskManager::Rotate(CTask* task)
{
	vector_t	vector;
	CBlock* block = task->GetBlock();
	char* tagName;
	float		tagLookup, duration;
	int			memberNum = 0;

	//Check for a tag reference
	if (Check(ID_TAG, block, memberNum))
	{
		memberNum++;

		ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &tagName));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, tagLookup));

		if ((m_owner->GetInterface())->I_GetTag(m_ownerID, tagName, static_cast<int>(tagLookup), vector) == false)
		{
			//TODO: Emit warning
			(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Unable to find tag \"%s\"!\n", tagName);
			assert(0);
			return TASK_FAILED;
		}
	}
	else
	{
		//Get a normal vector
		ICARUS_VALIDATE(GetVector(m_ownerID, block, memberNum, vector));
	}

	//Find the duration
	ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, duration));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d rotate( <%f,%f,%f>, %d); [%d]", m_ownerID, vector[0], vector[1], vector[2], static_cast<int>(duration), task->GetTimeStamp());
	(m_owner->GetInterface())->I_Lerp2Angles(task->GetGUID(), m_ownerID, vector, duration);

	return TASK_OK;
}

/*
-------------------------
Remove
-------------------------
*/

int CTaskManager::Remove(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d remove(\"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp());
	(m_owner->GetInterface())->I_Remove(m_ownerID, sVal);

	Completed(task->GetGUID());

	return TASK_OK;
}

/*
-------------------------
Camera
-------------------------
*/

int CTaskManager::Camera(CTask* task)
{
	const interface_export_t* ie = (m_owner->GetInterface());
	CBlock* block = task->GetBlock();
	vector_t	vector, vector2;
	float		type, fVal, fVal2, fVal3;
	char* sVal;
	int			memberNum = 0;

	//Get the camera function type
	ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, type));

	switch (static_cast<int>(type))
	{
	case TYPE_PAN:

		ICARUS_VALIDATE(GetVector(m_ownerID, block, memberNum, vector));
		ICARUS_VALIDATE(GetVector(m_ownerID, block, memberNum, vector2));

		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( PAN, <%f %f %f>, <%f %f %f>, %f); [%d]", m_ownerID, vector[0], vector[1], vector[2], vector2[0], vector2[1], vector2[2], fVal, task->GetTimeStamp());
		ie->I_CameraPan(vector, vector2, fVal);
		break;

	case TYPE_ZOOM:

		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal2));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( ZOOM, %f, %f); [%d]", m_ownerID, fVal, fVal2, task->GetTimeStamp());
		ie->I_CameraZoom(fVal, fVal2);
		break;

	case TYPE_MOVE:

		ICARUS_VALIDATE(GetVector(m_ownerID, block, memberNum, vector));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( MOVE, <%f %f %f>, %f); [%d]", m_ownerID, vector[0], vector[1], vector[2], fVal, task->GetTimeStamp());
		ie->I_CameraMove(vector, fVal);
		break;

	case TYPE_ROLL:

		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal2));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( ROLL, %f, %f); [%d]", m_ownerID, fVal, fVal2, task->GetTimeStamp());
		ie->I_CameraRoll(fVal, fVal2);

		break;

	case TYPE_FOLLOW:

		ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal2));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( FOLLOW, \"%s\", %f, %f); [%d]", m_ownerID, sVal, fVal, fVal2, task->GetTimeStamp());
		ie->I_CameraFollow(static_cast<const char*>(sVal), fVal, fVal2);

		break;

	case TYPE_TRACK:

		ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal2));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( TRACK, \"%s\", %f, %f); [%d]", m_ownerID, sVal, fVal, fVal2, task->GetTimeStamp());
		ie->I_CameraTrack(static_cast<const char*>(sVal), fVal, fVal2);
		break;

	case TYPE_DISTANCE:

		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal2));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( DISTANCE, %f, %f); [%d]", m_ownerID, fVal, fVal2, task->GetTimeStamp());
		ie->I_CameraDistance(fVal, fVal2);
		break;

	case TYPE_FADE:

		ICARUS_VALIDATE(GetVector(m_ownerID, block, memberNum, vector));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));

		ICARUS_VALIDATE(GetVector(m_ownerID, block, memberNum, vector2));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal2));

		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal3));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( FADE, <%f %f %f>, %f, <%f %f %f>, %f, %f); [%d]", m_ownerID, vector[0], vector[1], vector[2], fVal, vector2[0], vector2[1], vector2[2], fVal2, fVal3, task->GetTimeStamp());
		ie->I_CameraFade(vector[0], vector[1], vector[2], fVal, vector2[0], vector2[1], vector2[2], fVal2, fVal3);
		break;

	case TYPE_PATH:
		ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( PATH, \"%s\"); [%d]", m_ownerID, sVal, task->GetTimeStamp());
		ie->I_CameraPath(sVal);
		break;

	case TYPE_ENABLE:
		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( ENABLE ); [%d]", m_ownerID, task->GetTimeStamp());
		ie->I_CameraEnable();
		break;

	case TYPE_DISABLE:
		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( DISABLE ); [%d]", m_ownerID, task->GetTimeStamp());
		ie->I_CameraDisable();
		break;

	case TYPE_SHAKE:
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal2));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d camera( SHAKE, %f, %f ); [%d]", m_ownerID, fVal, fVal2, task->GetTimeStamp());
		ie->I_CameraShake(fVal, static_cast<int>(fVal2));
		break;
	}

	Completed(task->GetGUID());

	return TASK_OK;
}

/*
-------------------------
Move
-------------------------
*/

int CTaskManager::Move(CTask* task)
{
	vector_t	vector, vector2;
	CBlock* block = task->GetBlock();
	float		duration;
	int			memberNum = 0;

	//Get the goal position
	ICARUS_VALIDATE(GetVector(m_ownerID, block, memberNum, vector));

	//Check for possible angles field
	if (GetVector(m_ownerID, block, memberNum, vector2) == false)
	{
		ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, duration));

		(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d move( <%f %f %f>, %f ); [%d]", m_ownerID, vector[0], vector[1], vector[2], duration, task->GetTimeStamp());
		(m_owner->GetInterface())->I_Lerp2Pos(task->GetGUID(), m_ownerID, vector, nullptr, duration);

		return TASK_OK;
	}

	//Get the duration and make the call
	ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, duration));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d move( <%f %f %f>, <%f %f %f>, %f ); [%d]", m_ownerID, vector[0], vector[1], vector[2], vector2[0], vector2[1], vector2[2], duration, task->GetTimeStamp());
	(m_owner->GetInterface())->I_Lerp2Pos(task->GetGUID(), m_ownerID, vector, vector2, duration);

	return TASK_OK;
}

/*
-------------------------
Kill
-------------------------
*/

int CTaskManager::Kill(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d kill( \"%s\" ); [%d]", m_ownerID, sVal, task->GetTimeStamp());
	(m_owner->GetInterface())->I_Kill(m_ownerID, sVal);

	Completed(task->GetGUID());

	return TASK_OK;
}

/*
-------------------------
Set
-------------------------
*/

int CTaskManager::Set(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal, * sVal2;
	int				memberNum = 0;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));
	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal2));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d set( \"%s\", \"%s\" ); [%d]", m_ownerID, sVal, sVal2, task->GetTimeStamp());
	(m_owner->GetInterface())->I_Set(task->GetGUID(), m_ownerID, sVal, sVal2);

	return TASK_OK;
}

/*
-------------------------
Use
-------------------------
*/

int CTaskManager::Use(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d use( \"%s\" ); [%d]", m_ownerID, sVal, task->GetTimeStamp());
	(m_owner->GetInterface())->I_Use(m_ownerID, sVal);

	Completed(task->GetGUID());

	return TASK_OK;
}

/*
-------------------------
DeclareVariable
-------------------------
*/

int CTaskManager::DeclareVariable(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal;
	int		memberNum = 0;
	float	fVal;

	ICARUS_VALIDATE(GetFloat(m_ownerID, block, memberNum, fVal));
	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d declare( %d, \"%s\" ); [%d]", m_ownerID, static_cast<int>(fVal), sVal, task->GetTimeStamp());
	(m_owner->GetInterface())->I_DeclareVariable(static_cast<int>(fVal), sVal);

	Completed(task->GetGUID());

	return TASK_OK;
}

/*
-------------------------
FreeVariable
-------------------------
*/

int CTaskManager::FreeVariable(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d free( \"%s\" ); [%d]", m_ownerID, sVal, task->GetTimeStamp());
	(m_owner->GetInterface())->I_FreeVariable(sVal);

	Completed(task->GetGUID());

	return TASK_OK;
}

/*
-------------------------
Signal
-------------------------
*/

int CTaskManager::Signal(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal;
	int		memberNum = 0;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d signal( \"%s\" ); [%d]", m_ownerID, sVal, task->GetTimeStamp());
	m_owner->GetOwner()->Signal((const char*)sVal);

	Completed(task->GetGUID());

	return TASK_OK;
}

/*
-------------------------
Play
-------------------------
*/

int CTaskManager::Play(CTask* task)
{
	CBlock* block = task->GetBlock();
	char* sVal, * sVal2;
	int		memberNum = 0;

	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal));
	ICARUS_VALIDATE(Get(m_ownerID, block, memberNum, &sVal2));

	(m_owner->GetInterface())->I_DPrintf(WL_DEBUG, "%4d play( \"%s\", \"%s\" ); [%d]", m_ownerID, sVal, sVal2, task->GetTimeStamp());
	(m_owner->GetInterface())->I_Play(task->GetGUID(), m_ownerID, static_cast<const char*>(sVal), static_cast<const char*>(sVal2));

	return TASK_OK;
}

/*
-------------------------
SaveCommand
-------------------------
*/

//FIXME: ARGH!  This is duplicated from CSequence because I can't directly link it any other way...

int CTaskManager::SaveCommand(CBlock* block) const
{
	unsigned char	flags;
	int				numMembers, bID, size;

	//Save out the block ID
	bID = block->GetBlockID();
	(m_owner->GetInterface())->I_WriteSaveData(INT_ID('B', 'L', 'I', 'D'), &bID, sizeof(bID));

	//Save out the block's flags
	flags = block->GetFlags();
	(m_owner->GetInterface())->I_WriteSaveData(INT_ID('B', 'F', 'L', 'G'), &flags, sizeof(flags));

	//Save out the number of members to read
	numMembers = block->GetNumMembers();
	(m_owner->GetInterface())->I_WriteSaveData(INT_ID('B', 'N', 'U', 'M'), &numMembers, sizeof(numMembers));

	for (int i = 0; i < numMembers; i++)
	{
		const CBlockMember* bm = block->GetMember(i);

		//Save the block id
		bID = bm->GetID();
		(m_owner->GetInterface())->I_WriteSaveData(INT_ID('B', 'M', 'I', 'D'), &bID, sizeof(bID));

		//Save out the data size
		size = bm->GetSize();
		(m_owner->GetInterface())->I_WriteSaveData(INT_ID('B', 'S', 'I', 'Z'), &size, sizeof(size));

		//Save out the raw data
		(m_owner->GetInterface())->I_WriteSaveData(INT_ID('B', 'M', 'E', 'M'), bm->GetData(), size);
	}

	return true;
}

/*
-------------------------
Save
-------------------------
*/

void CTaskManager::Save(void)
{
#if 0
	CTaskGroup* taskGroup;
	const char* name;
	CBlock* block;
	unsigned int		timeStamp;
	bool		completed;
	int			id, numCommands;
	int			numWritten;

	//Save the taskmanager's GUID
	(m_owner->GetInterface())->I_WriteSaveData('TMID', &m_GUID, sizeof(m_GUID));	//FIXME: This can be reconstructed

	//Save out the number of tasks that will follow
	int iNumTasks = m_tasks.size();
	(m_owner->GetInterface())->I_WriteSaveData('TSK#', &iNumTasks, sizeof(iNumTasks));

	//Save out all the tasks
	tasks_l::iterator	ti;

	STL_ITERATE(ti, m_tasks)
	{
		//Save the GUID
		id = (*ti)->GetGUID();
		(m_owner->GetInterface())->I_WriteSaveData('TKID', &id, sizeof(id));

		//Save the timeStamp (FIXME: Although, this is going to be worthless if time is not consistent...)
		timeStamp = (*ti)->GetTimeStamp();
		(m_owner->GetInterface())->I_WriteSaveData('TKTS', &timeStamp, sizeof(timeStamp));

		//Save out the block
		block = (*ti)->GetBlock();
		SaveCommand(block);
	}

	//Save out the number of task groups
	int numTaskGroups = m_taskGroups.size();
	(m_owner->GetInterface())->I_WriteSaveData('TG#G', &numTaskGroups, sizeof(numTaskGroups));
	//Save out the IDs of all the task groups
	numWritten = 0;
	taskGroup_v::iterator	tgi;
	STL_ITERATE(tgi, m_taskGroups)
	{
		id = (*tgi)->GetGUID();
		(m_owner->GetInterface())->I_WriteSaveData('TKG#', &id, sizeof(id));
		numWritten++;
	}
	assert(numWritten == numTaskGroups);

	//Save out the task groups
	numWritten = 0;
	STL_ITERATE(tgi, m_taskGroups)
	{
		//Save out the parent
		id = ((*tgi)->GetParent() == NULL) ? -1 : ((*tgi)->GetParent())->GetGUID();
		(m_owner->GetInterface())->I_WriteSaveData('TKGP', &id, sizeof(id));

		//Save out the number of commands
		numCommands = (*tgi)->m_completedTasks.size();
		(m_owner->GetInterface())->I_WriteSaveData('TGNC', &numCommands, sizeof(numCommands));

		//Save out the command map
		CTaskGroup::taskCallback_m::iterator	tci;

		STL_ITERATE(tci, (*tgi)->m_completedTasks)
		{
			//Write out the ID
			id = (*tci).first;
			(m_owner->GetInterface())->I_WriteSaveData('GMID', &id, sizeof(id));

			//Write out the state of completion
			completed = (*tci).second;
			(m_owner->GetInterface())->I_WriteSaveData('GMDN', &completed, sizeof(completed));
		}

		//Save out the number of completed commands
		id = (*tgi)->m_numCompleted;
		(m_owner->GetInterface())->I_WriteSaveData('TGDN', &id, sizeof(id));	//FIXME: This can be reconstructed
		numWritten++;
	}
	assert(numWritten == numTaskGroups);

	//Only bother if we've got tasks present
	if (m_taskGroups.size())
	{
		//Save out the currently active group
		int	curGroupID = (m_curGroup == NULL) ? -1 : m_curGroup->GetGUID();
		(m_owner->GetInterface())->I_WriteSaveData('TGCG', &curGroupID, sizeof(curGroupID));
	}

	//Save out the task group name maps
	taskGroupName_m::iterator	tmi;
	numWritten = 0;
	STL_ITERATE(tmi, m_taskGroupNameMap)
	{
		name = ((*tmi).first).c_str();

		//Make sure this is a valid string
		assert((name != NULL) && (name[0] != NULL));

		int length = strlen(name) + 1;

		//Save out the string size
		(m_owner->GetInterface())->I_WriteSaveData('TGNL', &length, sizeof(length));

		//Write out the string
		(m_owner->GetInterface())->I_WriteSaveData('TGNS', (void*)name, length);

		taskGroup = (*tmi).second;

		id = taskGroup->GetGUID();

		//Write out the ID
		(m_owner->GetInterface())->I_WriteSaveData('TGNI', &id, sizeof(id));
		numWritten++;
	}
	assert(numWritten == numTaskGroups);
#endif
}

/*
-------------------------
Load
-------------------------
*/

void CTaskManager::Load(void)
{
#if 0
	unsigned char	flags;
	CTaskGroup* taskGroup;
	CBlock* block;
	CTask* task;
	unsigned int			timeStamp;
	bool			completed;
	void* bData;
	int				id, numTasks, numMembers;
	int				bID, bSize;

	//Get the GUID
	(m_owner->GetInterface())->I_ReadSaveData('TMID', &m_GUID, sizeof(m_GUID));

	//Get the number of tasks to follow
	(m_owner->GetInterface())->I_ReadSaveData('TSK#', &numTasks, sizeof(numTasks));

	//Reload all the tasks
	for (int i = 0; i < numTasks; i++)
	{
		task = new CTask;

		assert(task);

		//Get the GUID
		(m_owner->GetInterface())->I_ReadSaveData('TKID', &id, sizeof(id));
		task->SetGUID(id);

		//Get the time stamp
		(m_owner->GetInterface())->I_ReadSaveData('TKTS', &timeStamp, sizeof(timeStamp));
		task->SetTimeStamp(timeStamp);

		//
		// BLOCK LOADING
		//

		//Get the block ID and create a new container
		(m_owner->GetInterface())->I_ReadSaveData('BLID', &id, sizeof(id));
		block = new CBlock;

		block->Create(id);

		//Read the block's flags
		(m_owner->GetInterface())->I_ReadSaveData('BFLG', &flags, sizeof(flags));
		block->SetFlags(flags);

		//Get the number of block members
		(m_owner->GetInterface())->I_ReadSaveData('BNUM', &numMembers, sizeof(numMembers));

		for (int j = 0; j < numMembers; j++)
		{
			//Get the member ID
			(m_owner->GetInterface())->I_ReadSaveData('BMID', &bID, sizeof(bID));

			//Get the member size
			(m_owner->GetInterface())->I_ReadSaveData('BSIZ', &bSize, sizeof(bSize));

			//Get the member's data
			if ((bData = ICARUS_Malloc(bSize)) == NULL)
			{
				assert(0);
				return;
			}

			//Get the actual raw data
			(m_owner->GetInterface())->I_ReadSaveData('BMEM', bData, bSize);

			//Write out the correct type
			switch (bID)
			{
			case TK_FLOAT:
				block->Write(TK_FLOAT, *(float*)bData);
				break;

			case TK_IDENTIFIER:
				block->Write(TK_IDENTIFIER, (char*)bData);
				break;

			case TK_STRING:
				block->Write(TK_STRING, (char*)bData);
				break;

			case TK_VECTOR:
				block->Write(TK_VECTOR, *(vec3_t*)bData);
				break;

			case ID_RANDOM:
				block->Write(ID_RANDOM, *(float*)bData);//ID_RANDOM );
				break;

			case ID_TAG:
				block->Write(ID_TAG, (float)ID_TAG);
				break;

			case ID_GET:
				block->Write(ID_GET, (float)ID_GET);
				break;

			default:
				(m_owner->GetInterface())->I_DPrintf(WL_ERROR, "Invalid Block id %d\n", bID);
				assert(0);
				break;
			}

			//Get rid of the temp memory
			ICARUS_Free(bData);
		}

		task->SetBlock(block);

		STL_INSERT(m_tasks, task);
	}

	//Load the task groups
	int numTaskGroups;

	(m_owner->GetInterface())->I_ReadSaveData('TG#G', &numTaskGroups, sizeof(numTaskGroups));

	if (numTaskGroups == 0)
		return;

	int* taskIDs = new int[numTaskGroups];

	//Get the task group IDs
	for (i = 0; i < numTaskGroups; i++)
	{
		//Creat a new task group
		taskGroup = new CTaskGroup;
		assert(taskGroup);

		//Get this task group's ID
		(m_owner->GetInterface())->I_ReadSaveData('TKG#', &taskIDs[i], sizeof(taskIDs[i]));
		taskGroup->m_GUID = taskIDs[i];

		m_taskGroupIDMap[taskIDs[i]] = taskGroup;

		STL_INSERT(m_taskGroups, taskGroup);
	}

	//Recreate and load the task groups
	for (i = 0; i < numTaskGroups; i++)
	{
		taskGroup = GetTaskGroup(taskIDs[i]);
		assert(taskGroup);

		//Load the parent ID
		(m_owner->GetInterface())->I_ReadSaveData('TKGP', &id, sizeof(id));

		if (id != -1)
			taskGroup->m_parent = (GetTaskGroup(id) != NULL) ? GetTaskGroup(id) : NULL;

		//Get the number of commands in this group
		(m_owner->GetInterface())->I_ReadSaveData('TGNC', &numMembers, sizeof(numMembers));

		//Get each command and its completion state
		for (int j = 0; j < numMembers; j++)
		{
			//Get the ID
			(m_owner->GetInterface())->I_ReadSaveData('GMID', &id, sizeof(id));

			//Write out the state of completion
			(m_owner->GetInterface())->I_ReadSaveData('GMDN', &completed, sizeof(completed));

			//Save it out
			taskGroup->m_completedTasks[id] = completed;
		}

		//Get the number of completed tasks
		(m_owner->GetInterface())->I_ReadSaveData('TGDN', &taskGroup->m_numCompleted, sizeof(taskGroup->m_numCompleted));
	}

	//Reload the currently active group
	int curGroupID;

	(m_owner->GetInterface())->I_ReadSaveData('TGCG', &curGroupID, sizeof(curGroupID));

	//Reload the map entries
	for (i = 0; i < numTaskGroups; i++)
	{
		char	name[1024];
		int		length;

		//Get the size of the string
		(m_owner->GetInterface())->I_ReadSaveData('TGNL', &length, sizeof(length));

		//Get the string
		(m_owner->GetInterface())->I_ReadSaveData('TGNS', &name, length);

		//Get the id
		(m_owner->GetInterface())->I_ReadSaveData('TGNI', &id, sizeof(id));

		taskGroup = GetTaskGroup(id);
		assert(taskGroup);

		m_taskGroupNameMap[name] = taskGroup;
		m_taskGroupIDMap[taskGroup->GetGUID()] = taskGroup;
	}

	m_curGroup = (curGroupID == -1) ? NULL : m_taskGroupIDMap[curGroupID];

	delete[] taskIDs;
#endif
}