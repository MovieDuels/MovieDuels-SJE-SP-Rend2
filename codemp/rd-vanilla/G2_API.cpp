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

#include "ghoul2/G2.h"
#include "ghoul2/g2_local.h"
#include "ghoul2/G2_gore.h"

#include "qcommon/MiniHeap.h"
#include "tr_local.h"

#include <set>
#include <list>

#ifdef _FULL_G2_LEAK_CHECKING
int g_Ghoul2Allocations = 0;
int g_G2ServerAlloc = 0;
int g_G2ClientAlloc = 0;
int g_G2AllocServer = 0;

//stupid debug crap to track leaks in case they happen.
//we used to shove everything into a map and delete it all and not care about
//leaks, but that was not the Right Thing. -rww
#define MAX_TRACKED_ALLOC					4096
static bool g_G2AllocTrackInit = false; //want to keep this thing contained
static CGhoul2Info_v* g_G2AllocTrack[MAX_TRACKED_ALLOC];

void G2_DEBUG_InitPtrTracker(void)
{
	memset(g_G2AllocTrack, 0, sizeof(g_G2AllocTrack));
	g_G2AllocTrackInit = true;
}

void G2_DEBUG_ReportLeaks(void)
{
	int i = 0;

	if (!g_G2AllocTrackInit)
	{
		ri->Printf(PRINT_ALL, "g2 leak tracker was never initialized!\n");
		return;
	}

	while (i < MAX_TRACKED_ALLOC)
	{
		if (g_G2AllocTrack[i])
		{
			ri->Printf(PRINT_ALL, "Bad guy found in slot %i, attempting to access...", i);
			CGhoul2Info_v& g2v = *g_G2AllocTrack[i];
			CGhoul2Info& g2 = g2v[0];

			if (g2v.IsValid() && g2.mFileName && g2.mFileName[0])
			{
				ri->Printf(PRINT_ALL, "Bad guy's filename is %s\n", g2.mFileName);
			}
			else
			{
				ri->Printf(PRINT_ALL, "He's not valid! BURN HIM!\n");
			}
		}
		i++;
	}
}

void G2_DEBUG_ShovePtrInTracker(CGhoul2Info_v* g2)
{
	int i = 0;

	if (!g_G2AllocTrackInit)
	{
		G2_DEBUG_InitPtrTracker();
	}

	if (!g_G2AllocTrackInit)
	{
		G2_DEBUG_InitPtrTracker();
	}

	while (i < MAX_TRACKED_ALLOC)
	{
		if (!g_G2AllocTrack[i])
		{
			g_G2AllocTrack[i] = g2;
			return;
		}
		i++;
	}

	CGhoul2Info_v& g2v = *g2;

	if (g2v[0].current_model && g2v[0].current_model->name && g2v[0].current_model->name[0])
	{
		ri->Printf(PRINT_ALL, "%s could not be fit into g2 debug instance tracker.\n", g2v[0].current_model->name);
	}
	else
	{
		ri->Printf(PRINT_ALL, "Crap g2 instance passed to instance tracker (in).\n");
	}
}

void G2_DEBUG_RemovePtrFromTracker(CGhoul2Info_v* g2)
{
	int i = 0;

	if (!g_G2AllocTrackInit)
	{
		G2_DEBUG_InitPtrTracker();
	}

	while (i < MAX_TRACKED_ALLOC)
	{
		if (g_G2AllocTrack[i] == g2)
		{
			g_G2AllocTrack[i] = NULL;
			return;
		}
		i++;
	}

	CGhoul2Info_v& g2v = *g2;

	if (g2v[0].current_model && g2v[0].current_model->name && g2v[0].current_model->name[0])
	{
		ri->Printf(PRINT_ALL, "%s not in g2 debug instance tracker.\n", g2v[0].current_model->name);
	}
	else
	{
		ri->Printf(PRINT_ALL, "Crap g2 instance passed to instance tracker (out).\n");
	}
}
#endif

qboolean G2_SetupModelPointers(CGhoul2Info* ghl_info);
qboolean G2_SetupModelPointers(CGhoul2Info_v& ghoul2);
qboolean G2_TestModelPointers(CGhoul2Info* ghl_info);

//rww - RAGDOLL_BEGIN
#define NUM_G2T_TIME (2)
static int G2TimeBases[NUM_G2T_TIME];

void G2API_SetTime(int current_time, int clock)
{
	assert(clock >= 0 && clock < NUM_G2T_TIME);
#if G2_DEBUG_TIME
	ri->Printf(PRINT_ALL, "Set Time: before c%6d  s%6d", G2TimeBases[1], G2TimeBases[0]);
#endif
	G2TimeBases[clock] = current_time;
	if (G2TimeBases[1] > G2TimeBases[0] + 200)
	{
		G2TimeBases[1] = 0; // use server time instead
	}
#if G2_DEBUG_TIME
	ri->Printf(PRINT_ALL, " after c%6d  s%6d\n", G2TimeBases[1], G2TimeBases[0]);
#endif
}

int	G2API_GetTime(int arg_time) // this may or may not return arg depending on ghoul2_time cvar
{
	int ret = G2TimeBases[1];
	if (!ret)
	{
		ret = G2TimeBases[0];
	}

	return ret;
}
//rww - RAGDOLL_END

//rww - Stuff to allow association of ghoul2 instances to entity numbers.
//This way, on listen servers when both the client and server are doing
//ghoul2 operations, we can copy relevant data off the client instance
//directly onto the server instance and slash the transforms and whatnot
//right in half.
#ifdef _G2_LISTEN_SERVER_OPT
CGhoul2Info_v* g2ClientAttachments[MAX_GENTITIES];
#endif

void G2API_AttachInstanceToEntNum(CGhoul2Info_v& ghoul2, int entity_num, qboolean server)
{ //Assign the pointers in the arrays
#ifdef _G2_LISTEN_SERVER_OPT
	if (server)
	{
		ghoul2[0].entity_num = entity_num;
	}
	else
	{
		g2ClientAttachments[entity_num] = &ghoul2;
	}
#endif
}

void G2API_ClearAttachedInstance(int entity_num)
{
#ifdef _G2_LISTEN_SERVER_OPT
	g2ClientAttachments[entity_num] = NULL;
#endif
}

void G2API_CleanEntAttachments(void)
{
#ifdef _G2_LISTEN_SERVER_OPT
	int i = 0;

	while (i < MAX_GENTITIES)
	{
		g2ClientAttachments[i] = NULL;
		i++;
	}
#endif
}

#ifdef _G2_LISTEN_SERVER_OPT
void CopyBoneCache(CBoneCache* to, CBoneCache* from);
#endif

qboolean G2API_OverrideServerWithClientData(CGhoul2Info_v& ghoul2, int model_index)
{
#ifndef _G2_LISTEN_SERVER_OPT
	return qfalse;
#else
	CGhoul2Info* serverInstance = &ghoul2[model_index];
	CGhoul2Info* clientInstance;

	if (ri->Cvar_VariableIntegerValue("dedicated"))
	{ //No client to get from!
		return qfalse;
	}

	if (!g2ClientAttachments[serverInstance->entity_num])
	{ //No clientside instance is attached to this entity
		return qfalse;
	}

	CGhoul2Info_v& g2Ref = *g2ClientAttachments[serverInstance->entity_num];
	clientInstance = &g2Ref[0];

	int frame_num = G2API_GetTime(0);

	if (clientInstance->mSkelFrameNum != frame_num)
	{ //it has to be constructed already
		return qfalse;
	}

	if (!clientInstance->mBoneCache)
	{ //that just won't do
		return qfalse;
	}

	//Just copy over the essentials
	serverInstance->aHeader = clientInstance->aHeader;
	serverInstance->animModel = clientInstance->animModel;
	serverInstance->currentAnimModelSize = clientInstance->currentAnimModelSize;
	serverInstance->current_model = clientInstance->current_model;
	serverInstance->currentModelSize = clientInstance->currentModelSize;
	serverInstance->mAnimFrameDefault = clientInstance->mAnimFrameDefault;
	serverInstance->mModel = clientInstance->mModel;
	serverInstance->mModelindex = clientInstance->mModelindex;
	serverInstance->mSurfaceRoot = clientInstance->mSurfaceRoot;
	serverInstance->mTransformedVertsArray = clientInstance->mTransformedVertsArray;

	if (!serverInstance->mBoneCache)
	{ //if this is the case.. I guess we can use the client one instead
		serverInstance->mBoneCache = clientInstance->mBoneCache;
	}

	//Copy the contents of the client cache over the contents of the server cache
	if (serverInstance->mBoneCache != clientInstance->mBoneCache)
	{
		CopyBoneCache(serverInstance->mBoneCache, clientInstance->mBoneCache);
	}

	serverInstance->mSkelFrameNum = clientInstance->mSkelFrameNum;
	return qtrue;
#endif
}

// must be a power of two
#define MAX_G2_MODELS (1024)
#define G2_MODEL_BITS (10)

#define G2_INDEX_MASK (MAX_G2_MODELS-1)

static size_t GetSizeOfGhoul2Info(const CGhoul2Info& g2_info)
{
	size_t size = 0;

	// This is pretty ugly, but we don't want to save everything in the CGhoul2Info object.
	size += offsetof(CGhoul2Info, mTransformedVertsArray) - offsetof(CGhoul2Info, mModelindex);

	// Surface vector + size
	size += sizeof(int);
	size += g2_info.mSlist.size() * sizeof(surfaceInfo_t);

	// Bone vector + size
	size += sizeof(int);
	size += g2_info.mBlist.size() * sizeof(boneInfo_t);

	// Bolt vector + size
	size += sizeof(int);
	size += g2_info.mBltlist.size() * sizeof(boltInfo_t);

	return size;
}

static size_t SerializeGhoul2Info(char* buffer, const CGhoul2Info& g2_info)
{
	const char* base = buffer;

	// Oh the ugliness...
	size_t blockSize = offsetof(CGhoul2Info, mTransformedVertsArray) - offsetof(CGhoul2Info, mModelindex);
	memcpy(buffer, &g2_info.mModelindex, blockSize);
	buffer += blockSize;

	// Surfaces vector + size
	*(int*)buffer = g2_info.mSlist.size();
	buffer += sizeof(int);

	blockSize = g2_info.mSlist.size() * sizeof(surfaceInfo_t);
	memcpy(buffer, g2_info.mSlist.data(), g2_info.mSlist.size() * sizeof(surfaceInfo_t));
	buffer += blockSize;

	// Bones vector + size
	*(int*)buffer = g2_info.mBlist.size();
	buffer += sizeof(int);

	blockSize = g2_info.mBlist.size() * sizeof(boneInfo_t);
	memcpy(buffer, g2_info.mBlist.data(), g2_info.mBlist.size() * sizeof(boneInfo_t));
	buffer += blockSize;

	// Bolts vector + size
	*(int*)buffer = g2_info.mBltlist.size();
	buffer += sizeof(int);

	blockSize = g2_info.mBltlist.size() * sizeof(boltInfo_t);
	memcpy(buffer, g2_info.mBltlist.data(), g2_info.mBltlist.size() * sizeof(boltInfo_t));
	buffer += blockSize;

	return static_cast<size_t>(buffer - base);
}

static size_t DeserializeGhoul2Info(const char* buffer, CGhoul2Info& g2_info)
{
	const char* base = buffer;

	size_t size = offsetof(CGhoul2Info, mTransformedVertsArray) - offsetof(CGhoul2Info, mModelindex);
	memcpy(&g2_info.mModelindex, buffer, size);
	buffer += size;

	// Surfaces vector
	size = *(int*)buffer;
	buffer += sizeof(int);

	g2_info.mSlist.assign((surfaceInfo_t*)buffer, (surfaceInfo_t*)buffer + size);
	buffer += sizeof(surfaceInfo_t) * size;

	// Bones vector
	size = *(int*)buffer;
	buffer += sizeof(int);

	g2_info.mBlist.assign((boneInfo_t*)buffer, (boneInfo_t*)buffer + size);
	buffer += sizeof(boneInfo_t) * size;

	// Bolt vector
	size = *(int*)buffer;
	buffer += sizeof(int);

	g2_info.mBltlist.assign((boltInfo_t*)buffer, (boltInfo_t*)buffer + size);
	buffer += sizeof(boltInfo_t) * size;

	return static_cast<size_t>(buffer - base);
}

class Ghoul2InfoArray : public IGhoul2InfoArray
{
	std::vector<CGhoul2Info>	m_infos_[MAX_G2_MODELS];
	int					m_ids_[MAX_G2_MODELS];
	std::list<int>			m_free_indecies_;
	void DeleteLow(int idx)
	{
		for (size_t model = 0; model < m_infos_[idx].size(); model++)
		{
			if (m_infos_[idx][model].mBoneCache)
			{
				RemoveBoneCache(m_infos_[idx][model].mBoneCache);
				m_infos_[idx][model].mBoneCache = nullptr;
			}
		}

		m_infos_[idx].clear();

		if ((m_ids_[idx] >> G2_MODEL_BITS) > (1 << (31 - G2_MODEL_BITS)))
		{
			m_ids_[idx] = MAX_G2_MODELS + idx; //rollover reset id to minimum value
			m_free_indecies_.push_back(idx);
		}
		else
		{
			m_ids_[idx] += MAX_G2_MODELS;
			m_free_indecies_.push_front(idx);
		}
	}
public:
	Ghoul2InfoArray()
	{
		for (int i = 0; i < MAX_G2_MODELS; i++)
		{
			m_ids_[i] = MAX_G2_MODELS + i;
			m_free_indecies_.push_back(i);
		}
	}

	size_t GetSerializedSize() const
	{
		size_t size = 0;

		size += sizeof(int); // size of mFreeIndecies linked list
		size += m_free_indecies_.size() * sizeof(int);

		size += sizeof(m_ids_);

		for (size_t i = 0; i < MAX_G2_MODELS; i++)
		{
			size += sizeof(int); // size of the mInfos[i] vector

			for (size_t j = 0; j < m_infos_[i].size(); j++)
			{
				size += GetSizeOfGhoul2Info(m_infos_[i][j]);
			}
		}

		return size;
	}

	size_t Serialize(char* buffer) const
	{
		const char* base = buffer;

		// Free indices
		*(int*)buffer = m_free_indecies_.size();
		buffer += sizeof(int);

		std::copy(m_free_indecies_.begin(), m_free_indecies_.end(), (int*)buffer);
		buffer += sizeof(int) * m_free_indecies_.size();

		// IDs
		memcpy(buffer, m_ids_, sizeof(m_ids_));
		buffer += sizeof(m_ids_);

		// Ghoul2 infos
		for (size_t i = 0; i < MAX_G2_MODELS; i++)
		{
			*(int*)buffer = m_infos_[i].size();
			buffer += sizeof(int);

			for (size_t j = 0; j < m_infos_[i].size(); j++)
			{
				buffer += SerializeGhoul2Info(buffer, m_infos_[i][j]);
			}
		}

		return static_cast<size_t>(buffer - base);
	}

	size_t Deserialize(const char* buffer)
	{
		const char* base = buffer;

		// Free indices
		size_t count = *(int*)buffer;
		buffer += sizeof(int);

		m_free_indecies_.assign((int*)buffer, (int*)buffer + count);
		buffer += sizeof(int) * count;

		// IDs
		memcpy(m_ids_, buffer, sizeof(m_ids_));
		buffer += sizeof(m_ids_);

		// Ghoul2 infos
		for (size_t i = 0; i < MAX_G2_MODELS; i++)
		{
			m_infos_[i].clear();

			count = *(int*)buffer;
			buffer += sizeof(int);

			m_infos_[i].resize(count);

			for (size_t j = 0; j < count; j++)
			{
				buffer += DeserializeGhoul2Info(buffer, m_infos_[i][j]);
			}
		}

		return static_cast<size_t>(buffer - base);
	}

#if G2API_DEBUG
	~Ghoul2InfoArray()
	{
		char mess[1000];
		if (mFreeIndecies.size() < MAX_G2_MODELS)
		{
			sprintf(mess, "************************\nLeaked %d ghoul2info slots\n", MAX_G2_MODELS - mFreeIndecies.size());
			OutputDebugString(mess);
			int i;
			for (i = 0; i < MAX_G2_MODELS; i++)
			{
				list<int>::iterator j;
				for (j = mFreeIndecies.begin(); j != mFreeIndecies.end(); ++j)
				{
					if (*j == i)
						break;
				}
				if (j == mFreeIndecies.end())
				{
					sprintf(mess, "Leaked Info idx=%d id=%d sz=%d\n", i, mIds[i], mInfos[i].size());
					OutputDebugString(mess);
					if (mInfos[i].size())
					{
						sprintf(mess, "%s\n", mInfos[i][0].mFileName);
						OutputDebugString(mess);
					}
				}
			}
		}
		else
		{
			OutputDebugString("No ghoul2 info slots leaked\n");
		}
	}
#endif
	int New()
	{
		if (m_free_indecies_.empty())
		{
			assert(0);
			Com_Error(ERR_FATAL, "Out of ghoul2 info slots");
		}
		// gonna pull from the front, doing a
		const int idx = *m_free_indecies_.begin();
		m_free_indecies_.erase(m_free_indecies_.begin());
		return m_ids_[idx];
	}
	bool IsValid(int handle) const
	{
		if (handle <= 0)
		{
			return false;
		}
		assert((handle & G2_INDEX_MASK) >= 0 && (handle & G2_INDEX_MASK) < MAX_G2_MODELS); //junk handle
		if (m_ids_[handle & G2_INDEX_MASK] != handle) // not a valid handle, could be old
		{
			return false;
		}
		return true;
	}
	void Delete(int handle)
	{
		if (handle <= 0)
		{
			return;
		}
		assert((handle & G2_INDEX_MASK) >= 0 && (handle & G2_INDEX_MASK) < MAX_G2_MODELS); //junk handle
		assert(m_ids_[handle & G2_INDEX_MASK] == handle); // not a valid handle, could be old or garbage
		if (m_ids_[handle & G2_INDEX_MASK] == handle)
		{
			DeleteLow(handle & G2_INDEX_MASK);
		}
	}
	std::vector<CGhoul2Info>& Get(int handle)
	{
		assert(handle > 0); //null handle
		assert((handle & G2_INDEX_MASK) >= 0 && (handle & G2_INDEX_MASK) < MAX_G2_MODELS); //junk handle
		assert(m_ids_[handle & G2_INDEX_MASK] == handle); // not a valid handle, could be old or garbage
		assert(!(handle <= 0 || (handle & G2_INDEX_MASK) < 0 || (handle & G2_INDEX_MASK) >= MAX_G2_MODELS || m_ids_[handle & G2_INDEX_MASK] != handle));

		return m_infos_[handle & G2_INDEX_MASK];
	}
	const std::vector<CGhoul2Info>& Get(int handle) const
	{
		assert(handle > 0);
		assert(m_ids_[handle & G2_INDEX_MASK] == handle); // not a valid handle, could be old or garbage
		return m_infos_[handle & G2_INDEX_MASK];
	}

#if G2API_DEBUG
	vector<CGhoul2Info>& GetDebug(int handle)
	{
		static vector<CGhoul2Info> null;
		if (handle <= 0 || (handle & G2_INDEX_MASK) < 0 || (handle & G2_INDEX_MASK) >= MAX_G2_MODELS || mIds[handle & G2_INDEX_MASK] != handle)
		{
			return *(vector<CGhoul2Info> *)0; // null reference, intentional
		}
		return mInfos[handle & G2_INDEX_MASK];
	}
	void TestAllAnims()
	{
		int j;
		for (j = 0; j < MAX_G2_MODELS; j++)
		{
			vector<CGhoul2Info>& ghoul2 = mInfos[j];
			int i;
			for (i = 0; i < ghoul2.size(); i++)
			{
				if (G2_SetupModelPointers(&ghoul2[i]))
				{
					G2ANIM(&ghoul2[i], "Test All");
				}
			}
		}
	}

#endif
};

static Ghoul2InfoArray* singleton = nullptr;
IGhoul2InfoArray& TheGhoul2InfoArray()
{
	if (!singleton) {
		singleton = new Ghoul2InfoArray;
	}
	return *singleton;
}

#define PERSISTENT_G2DATA "g2infoarray"

void RestoreGhoul2InfoArray()
{
	if (singleton == nullptr)
	{
		// Create the ghoul2 info array
		TheGhoul2InfoArray();

		size_t size;
		const void* data = ri->PD_Load(PERSISTENT_G2DATA, &size);
		if (data == nullptr)
		{
			return;
		}

#ifdef _DEBUG
		const size_t read =
#endif
			singleton->Deserialize(static_cast<const char*>(data));
		Z_Free((void*)data);

		assert(read == size);
	}
}

void SaveGhoul2InfoArray()
{
	const size_t size = singleton->GetSerializedSize();
	void* data = Z_Malloc(size, TAG_GHOUL2);
#ifdef _DEBUG
	const size_t written =
#endif
		singleton->Serialize(static_cast<char*>(data));

	assert(written == size);

	if (!ri->PD_Store(PERSISTENT_G2DATA, data, size))
	{
		Com_Printf(S_COLOR_RED "ERROR: Failed to store persistent renderer data.\n");
	}
}

void Ghoul2InfoArray_Free(void)
{
	if (singleton) {
		delete singleton;
		singleton = nullptr;
	}
}

// this is the ONLY function to read entity states directly
void G2API_CleanGhoul2Models(CGhoul2Info_v** ghoul2Ptr)
{
	if (*ghoul2Ptr)
	{
		CGhoul2Info_v& ghoul2 = *(*ghoul2Ptr);

#if 0 //rwwFIXMEFIXME: Disable this before release!!!!!! I am just trying to find a crash bug.
		extern int R_GetRNumEntities(void);
		extern void R_SetRNumEntities(int num);
		//check if this instance is actually on a refentity
		int i = 0;
		int r = R_GetRNumEntities();

		while (i < r)
		{
			if ((CGhoul2Info_v*)backEndData->entities[i].e.ghoul2 == *ghoul2Ptr)
			{
				char fName[MAX_QPATH];
				char mName[MAX_QPATH];

				if (ghoul2[0].current_model)
				{
					strcpy(mName, ghoul2[0].current_model->name);
				}
				else
				{
					strcpy(mName, "NULL!");
				}

				if (ghoul2[0].mFileName && ghoul2[0].mFileName[0])
				{
					strcpy(fName, ghoul2[0].mFileName);
				}
				else
				{
					strcpy(fName, "None?!");
				}

				ri->Printf(PRINT_ALL, "ERROR, GHOUL2 INSTANCE BEING REMOVED BELONGS TO A REFENTITY!\nThis is in caps because it's important. Tell Rich and save the following text.\n\n");
				ri->Printf(PRINT_ALL, "Ref num: %i\nModel: %s\nFilename: %s\n", i, mName, fName);

				R_SetRNumEntities(0); //avoid recursive error
				Com_Error(ERR_DROP, "Write down or save this error message, show it to Rich\nRef num: %i\nModel: %s\nFilename: %s\n", i, mName, fName);
			}
			i++;
		}
#endif

#ifdef _G2_GORE
		G2API_ClearSkinGore(ghoul2);
#endif

#ifdef _FULL_G2_LEAK_CHECKING
		if (g_G2AllocServer)
		{
			g_G2ServerAlloc -= sizeof(*ghoul2Ptr);
		}
		else
		{
			g_G2ClientAlloc -= sizeof(*ghoul2Ptr);
		}
		g_Ghoul2Allocations -= sizeof(*ghoul2Ptr);
		G2_DEBUG_RemovePtrFromTracker(*ghoul2Ptr);
#endif

		delete* ghoul2Ptr;
		*ghoul2Ptr = nullptr;
	}
}

qboolean G2_ShouldRegisterServer(void)
{
	const vm_t* currentVM = ri->GetCurrentVM();

	if (currentVM && currentVM->slot == VM_GAME)
	{
		if (ri->Cvar_VariableIntegerValue("cl_running") &&
			ri->Com_TheHunkMarkHasBeenMade() && ShaderHashTableExists())
		{ //if the hunk has been marked then we are now loading client assets so don't load on server.
			return qfalse;
		}

		return qtrue;
	}
	return qfalse;
}

qhandle_t G2API_PrecacheGhoul2Model(const char* file_name)
{
	if (G2_ShouldRegisterServer())
		return RE_RegisterServerModel(file_name);
	return RE_RegisterModel(file_name);
}

// initialise all that needs to be on a new Ghoul II model
int G2API_InitGhoul2Model(CGhoul2Info_v** ghoul2Ptr, const char* fileName, int model_index, qhandle_t customSkin,
	qhandle_t customShader, int modelFlags, int lodBias)
{
	int				model;

	// are we actually asking for a model to be loaded.
	if (!fileName || !fileName[0])
	{
		assert(0);
		return -1;
	}

	if (!(*ghoul2Ptr))
	{
		*ghoul2Ptr = new CGhoul2Info_v;
#ifdef _FULL_G2_LEAK_CHECKING
		if (g_G2AllocServer)
		{
			g_G2ServerAlloc += sizeof(CGhoul2Info_v);
		}
		else
		{
			g_G2ClientAlloc += sizeof(CGhoul2Info_v);
		}
		g_Ghoul2Allocations += sizeof(CGhoul2Info_v);
		G2_DEBUG_ShovePtrInTracker(*ghoul2Ptr);
#endif
	}

	CGhoul2Info_v& ghoul2 = *(*ghoul2Ptr);

	// find a free spot in the list
	for (model = 0; model < ghoul2.size(); model++)
	{
		if (ghoul2[model].mModelindex == -1)
		{
			ghoul2[model] = CGhoul2Info();
			break;
		}
	}
	if (model == ghoul2.size())
	{	//init should not be used to create additional models, only the first one
		assert(ghoul2.size() < 4); //use G2API_CopySpecificG2Model to add models
		ghoul2.push_back(CGhoul2Info());
	}

	strcpy(ghoul2[model].mFileName, fileName);
	ghoul2[model].mModelindex = model;
	if (!G2_TestModelPointers(&ghoul2[model]))
	{
		ghoul2[model].mFileName[0] = 0;
		ghoul2[model].mModelindex = -1;
	}
	else
	{
		G2_Init_Bone_List(ghoul2[model].mBlist, ghoul2[model].aHeader->numBones);
		G2_Init_Bolt_List(ghoul2[model].mBltlist);
		ghoul2[model].mCustomShader = customShader;
		ghoul2[model].mCustomSkin = customSkin;
		ghoul2[model].mLodBias = lodBias;
		ghoul2[model].mAnimFrameDefault = 0;
		ghoul2[model].mFlags = 0;

		ghoul2[model].mModelBoltLink = -1;
	}
	return ghoul2[model].mModelindex;
}

qboolean G2API_SetLodBias(CGhoul2Info* ghl_info, int lod_bias)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		ghl_info->mLodBias = lod_bias;
		return qtrue;
	}
	return qfalse;
}

void G2_SetSurfaceOnOffFromSkin(CGhoul2Info* ghl_info, qhandle_t renderSkin);
qboolean G2API_SetSkin(CGhoul2Info_v& ghoul2, int model_index, qhandle_t customSkin, qhandle_t renderSkin)
{
	CGhoul2Info* ghl_info = &ghoul2[model_index];

	if (G2_SetupModelPointers(ghl_info))
	{
		ghl_info->mCustomSkin = customSkin;
		if (renderSkin)
		{//this is going to set the surfs on/off matching the skin file
			G2_SetSurfaceOnOffFromSkin(ghl_info, renderSkin);
		}

		return qtrue;
	}
	return qfalse;
}

qboolean G2API_SetShader(CGhoul2Info* ghl_info, qhandle_t custom_shader)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		ghl_info->mCustomShader = custom_shader;
		return qtrue;
	}
	return qfalse;
}

qboolean G2API_SetSurfaceOnOff(CGhoul2Info_v& ghoul2, const char* surface_name, const int flags)
{
	CGhoul2Info* ghl_info = nullptr;

	if (ghoul2.size() > 0)
	{
		ghl_info = &ghoul2[0];
	}

	if (G2_SetupModelPointers(ghl_info))
	{
		// ensure we flush the cache
		ghl_info->mMeshFrameNum = 0;
		return G2_SetSurfaceOnOff(ghl_info, ghl_info->mSlist, surface_name, flags);
	}
	return qfalse;
}

int G2API_GetSurfaceOnOff(CGhoul2Info* ghl_info, const char* surface_name)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_IsSurfaceOff(ghl_info, ghl_info->mSlist, surface_name);
	}
	return -1;
}

qboolean G2API_SetRootSurface(CGhoul2Info_v& ghoul2, const int model_index, const char* surface_name)
{
	if (G2_SetupModelPointers(ghoul2))
	{
		return G2_SetRootSurface(ghoul2, model_index, surface_name);
	}

	return qfalse;
}

int G2API_AddSurface(CGhoul2Info* ghl_info, int surface_number, int poly_number, float barycentric_i, float barycentric_j, int lod)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		// ensure we flush the cache
		ghl_info->mMeshFrameNum = 0;
		return G2_AddSurface(ghl_info, surface_number, poly_number, barycentric_i, barycentric_j, lod);
	}
	return -1;
}

qboolean G2API_RemoveSurface(CGhoul2Info* ghl_info, const int index)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		// ensure we flush the cache
		ghl_info->mMeshFrameNum = 0;
		return G2_RemoveSurface(ghl_info->mSlist, index);
	}
	return qfalse;
}

int G2API_GetParentSurface(CGhoul2Info* ghl_info, const int index)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_GetParentSurface(ghl_info, index);
	}
	return -1;
}

int G2API_GetSurfaceRenderStatus(CGhoul2Info_v& ghoul2, int model_index, const char* surface_name)
{
	CGhoul2Info* ghl_info = &ghoul2[model_index];

	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_IsSurfaceRendered(ghl_info, surface_name, ghl_info->mSlist);
	}
	return -1;
}

qboolean G2API_HasGhoul2ModelOnIndex(CGhoul2Info_v** ghlRemove, const int model_index)
{
	CGhoul2Info_v& ghl_info = **ghlRemove;

	if (!ghl_info.size() || (ghl_info.size() <= model_index) || (ghl_info[model_index].mModelindex == -1))
	{
		return qfalse;
	}

	return qtrue;
}

qboolean G2API_RemoveGhoul2Model(CGhoul2Info_v** ghlRemove, const int model_index)
{
	CGhoul2Info_v& ghl_info = **ghlRemove;

	// sanity check
	if (!ghl_info.size() || (ghl_info.size() <= model_index) || (ghl_info[model_index].mModelindex == -1))
	{
		// if we hit this assert then we are trying to delete a ghoul2 model on a ghoul2 instance that
		// one way or another is already gone.
		assert(0);
		return qfalse;
	}

	if (ghl_info.size() > model_index)
	{
#ifdef _G2_GORE
		// Cleanup the gore attached to this model
		if (ghl_info[model_index].mGoreSetTag)
		{
			DeleteGoreSet(ghl_info[model_index].mGoreSetTag);
			ghl_info[model_index].mGoreSetTag = 0;
		}
#endif

		if (ghl_info[model_index].mBoneCache)
		{
			RemoveBoneCache(ghl_info[model_index].mBoneCache);
			ghl_info[model_index].mBoneCache = nullptr;
		}

		// clear out the vectors this model used.
		ghl_info[model_index].mBlist.clear();
		ghl_info[model_index].mBltlist.clear();
		ghl_info[model_index].mSlist.clear();

		// set us to be the 'not active' state
		ghl_info[model_index].mModelindex = -1;

		int newSize = ghl_info.size();
		// now look through the list from the back and see if there is a block of -1's we can resize off the end of the list
		for (int i = ghl_info.size() - 1; i > -1; i--)
		{
			if (ghl_info[i].mModelindex == -1)
			{
				newSize = i;
			}
			// once we hit one that isn't a -1, we are done.
			else
			{
				break;
			}
		}
		// do we need to resize?
		if (newSize != ghl_info.size())
		{
			// yes, so lets do it
			ghl_info.resize(newSize);
		}

		// if we are not using any space, just delete the ghoul2 vector entirely
		if (!ghl_info.size())
		{
#ifdef _FULL_G2_LEAK_CHECKING
			if (g_G2AllocServer)
			{
				g_G2ServerAlloc -= sizeof(*ghlRemove);
			}
			else
			{
				g_G2ClientAlloc -= sizeof(*ghlRemove);
			}
			g_Ghoul2Allocations -= sizeof(*ghlRemove);
#endif
			delete* ghlRemove;
			*ghlRemove = nullptr;
		}
	}

	return qtrue;
}

qboolean G2API_RemoveGhoul2Models(CGhoul2Info_v** ghlRemove)
{//remove 'em ALL!
	CGhoul2Info_v& ghl_info = **ghlRemove;

	// sanity check
	if (!ghl_info.size())
	{// if we hit this then we are trying to delete a ghoul2 model on a ghoul2 instance that
		// one way or another is already gone.
		return qfalse;
	}

	for (int model_index = 0; model_index < ghl_info.size(); model_index++)
	{
		if (ghl_info[model_index].mModelindex == -1)
		{
			continue;
		}
#ifdef _G2_GORE
		// Cleanup the gore attached to this model
		if (ghl_info[model_index].mGoreSetTag)
		{
			DeleteGoreSet(ghl_info[model_index].mGoreSetTag);
			ghl_info[model_index].mGoreSetTag = 0;
		}
#endif

		if (ghl_info[model_index].mBoneCache)
		{
			RemoveBoneCache(ghl_info[model_index].mBoneCache);
			ghl_info[model_index].mBoneCache = nullptr;
		}

		// clear out the vectors this model used.
		ghl_info[model_index].mBlist.clear();
		ghl_info[model_index].mBltlist.clear();
		ghl_info[model_index].mSlist.clear();

		// set us to be the 'not active' state
		ghl_info[model_index].mModelindex = -1;
	}

	int newSize = ghl_info.size();
	// now look through the list from the back and see if there is a block of -1's we can resize off the end of the list
	for (int i = ghl_info.size() - 1; i > -1; i--)
	{
		if (ghl_info[i].mModelindex == -1)
		{
			newSize = i;
		}
		// once we hit one that isn't a -1, we are done.
		else
		{
			break;
		}
	}
	// do we need to resize?
	if (newSize != ghl_info.size())
	{
		// yes, so lets do it
		ghl_info.resize(newSize);
	}

	// if we are not using any space, just delete the ghoul2 vector entirely
	if (!ghl_info.size())
	{
#ifdef _FULL_G2_LEAK_CHECKING
		if (g_G2AllocServer)
		{
			g_G2ServerAlloc -= sizeof(*ghlRemove);
		}
		else
		{
			g_G2ClientAlloc -= sizeof(*ghlRemove);
		}
		g_Ghoul2Allocations -= sizeof(*ghlRemove);
#endif
		delete* ghlRemove;
		*ghlRemove = nullptr;
	}
	return qtrue;
}

//check if a bone exists on skeleton without actually adding to the bone list -rww
qboolean G2API_DoesBoneExist(CGhoul2Info_v& ghoul2, int model_index, const char* bone_name)
{
	CGhoul2Info* ghl_info = &ghoul2[model_index];

	if (G2_SetupModelPointers(ghl_info))
	{ //model is valid
		mdxaHeader_t* mdxa = ghl_info->current_model->mdxa;
		if (mdxa)
		{
			//get the skeleton data and iterate through the bones

			const mdxaSkelOffsets_t* offsets = (mdxaSkelOffsets_t*)((byte*)mdxa + sizeof(mdxaHeader_t));

			for (int i = 0; i < mdxa->numBones; i++)
			{
				const mdxaSkel_t* skel = (mdxaSkel_t*)((byte*)mdxa + sizeof(mdxaHeader_t) + offsets->offsets[i]);
				if (!Q_stricmp(skel->name, bone_name))
				{ //got it
					return qtrue;
				}
			}
		}
	}

	//guess it doesn't exist
	return qfalse;
}

//rww - RAGDOLL_BEGIN
#define		GHOUL2_RAG_STARTED						0x0010
#define		GHOUL2_RAG_FORCESOLVE					0x1000		//api-override, determine if ragdoll should be forced to continue solving even if it thinks it is settled
//rww - RAGDOLL_END

qboolean G2API_SetBoneAnimIndex(CGhoul2Info* ghl_info, const int index, const int astart_frame, const int aend_frame, const int flags, const float anim_speed, const int acurrent_time, const float aset_frame, const int blend_time)
{
	qboolean setPtrs = qfalse;
	qboolean res = qfalse;

	//rww - RAGDOLL_BEGIN
	if (ghl_info)
	{
		res = G2_SetupModelPointers(ghl_info);
		setPtrs = qtrue;

		if (res)
		{
			if (ghl_info->mFlags & GHOUL2_RAG_STARTED)
			{
				return qfalse;
			}
		}
	}
	//rww - RAGDOLL_END

	int end_frame = aend_frame;
	int start_frame = astart_frame;
	float setFrame = aset_frame;
	assert(end_frame > 0);
	assert(start_frame >= 0);
	assert(end_frame < 100000);
	assert(start_frame < 100000);
	assert(setFrame >= 0.0f || setFrame == -1.0f);
	assert(setFrame <= 100000.0f);
	if (end_frame <= 0)
	{
		end_frame = 1;
	}
	if (end_frame >= 100000)
	{
		end_frame = 1;
	}
	if (start_frame < 0)
	{
		start_frame = 0;
	}
	if (start_frame >= 100000)
	{
		start_frame = 0;
	}
	if (setFrame < 0.0f && setFrame != -1.0f)
	{
		setFrame = 0.0f;
	}
	if (setFrame > 100000.0f)
	{
		setFrame = 0.0f;
	}
	if (!setPtrs)
	{
		res = G2_SetupModelPointers(ghl_info);
	}

	if (res)
	{
		// ensure we flush the cache
		ghl_info->mSkelFrameNum = 0;
		return G2_Set_Bone_Anim_Index(ghl_info->mBlist, index, start_frame, end_frame, flags, anim_speed, acurrent_time, setFrame, blend_time, ghl_info->aHeader->num_frames);
	}
	return qfalse;
}

#define _PLEASE_SHUT_THE_HELL_UP

qboolean G2API_SetBoneAnim(CGhoul2Info_v& ghoul2, const int model_index, const char* bone_name, const int astart_frame, const int aend_frame, const int flags, const float anim_speed, const int current_time, const float aset_frame, const int blend_time)
{
	int end_frame = aend_frame;
	int start_frame = astart_frame;
	float setFrame = aset_frame;
#ifndef _PLEASE_SHUT_THE_HELL_UP
	assert(end_frame > 0);
	assert(start_frame >= 0);
	assert(end_frame < 100000);
	assert(start_frame < 100000);
	assert(setFrame >= 0.0f || setFrame == -1.0f);
	assert(setFrame <= 100000.0f);
#endif
	if (end_frame <= 0)
	{
		end_frame = 1;
	}
	if (end_frame >= 100000)
	{
		end_frame = 1;
	}
	if (start_frame < 0)
	{
		start_frame = 0;
	}
	if (start_frame >= 100000)
	{
		start_frame = 0;
	}
	if (setFrame < 0.0f && setFrame != -1.0f)
	{
		setFrame = 0.0f;
	}
	if (setFrame > 100000.0f)
	{
		setFrame = 0.0f;
	}
	if (ghoul2.size() > model_index)
	{
		CGhoul2Info* ghl_info = &ghoul2[model_index];
		qboolean setPtrs = qfalse;
		qboolean res = qfalse;

		//rww - RAGDOLL_BEGIN
		if (ghl_info)
		{
			res = G2_SetupModelPointers(ghl_info);
			setPtrs = qtrue;

			if (res)
			{
				if (ghl_info->mFlags & GHOUL2_RAG_STARTED)
				{
					return qfalse;
				}
			}
		}
		//rww - RAGDOLL_END

		if (!setPtrs)
		{
			res = G2_SetupModelPointers(ghl_info);
		}

		if (res)
		{
			// ensure we flush the cache
			ghl_info->mSkelFrameNum = 0;
			return G2_Set_Bone_Anim(ghl_info, ghl_info->mBlist, bone_name, start_frame, end_frame, flags, anim_speed, current_time, setFrame, blend_time);
		}
	}
	return qfalse;
}

qboolean G2API_GetBoneAnim(CGhoul2Info_v& ghoul2, int model_index, const char* bone_name, const int current_time, float* current_frame,
	int* start_frame, int* end_frame, int* flags, float* anim_speed, int* model_list)
{
	assert(start_frame != end_frame); //this is bad
	assert(start_frame != flags); //this is bad
	assert(end_frame != flags); //this is bad
	assert(current_frame != anim_speed); //this is bad

	CGhoul2Info* ghl_info = &ghoul2[model_index];

	if (G2_SetupModelPointers(ghl_info))
	{
		const int aCurrentTime = G2API_GetTime(current_time);
		const qboolean ret = G2_Get_Bone_Anim(ghl_info, ghl_info->mBlist, bone_name, aCurrentTime, current_frame,
			start_frame, end_frame, flags, anim_speed, model_list, ghl_info->mModelindex);
#ifdef _DEBUG
		/*
		assert(*end_frame>0);
		assert(*end_frame<100000);
		assert(*start_frame>=0);
		assert(*start_frame<100000);
		assert(*current_frame>=0.0f);
		assert(*current_frame<100000.0f);
		*/
		if (*end_frame < 1)
		{
			*end_frame = 1;
		}
		if (*end_frame > 100000)
		{
			*end_frame = 1;
		}
		if (*start_frame < 0)
		{
			*start_frame = 0;
		}
		if (*start_frame > 100000)
		{
			*start_frame = 1;
		}
		if (*current_frame < 0.0f)
		{
			*current_frame = 0.0f;
		}
		if (*current_frame > 100000)
		{
			*current_frame = 1;
		}
#endif
		return ret;
	}
	return qfalse;
}

qboolean G2API_GetAnimRange(CGhoul2Info* ghl_info, const char* bone_name, int* start_frame, int* end_frame)
{
	assert(start_frame != end_frame); //this is bad
	if (G2_SetupModelPointers(ghl_info))
	{
		const qboolean ret = G2_Get_Bone_Anim_Range(ghl_info, ghl_info->mBlist, bone_name, start_frame, end_frame);
#ifdef _DEBUG
		assert(*end_frame > 0);
		assert(*end_frame < 100000);
		assert(*start_frame >= 0);
		assert(*start_frame < 100000);
		if (*end_frame < 1)
		{
			*end_frame = 1;
		}
		if (*end_frame > 100000)
		{
			*end_frame = 1;
		}
		if (*start_frame < 0)
		{
			*start_frame = 0;
		}
		if (*start_frame > 100000)
		{
			*start_frame = 1;
		}
#endif
		return ret;
	}
	return qfalse;
}

qboolean G2API_PauseBoneAnim(CGhoul2Info* ghl_info, const char* bone_name, const int acurrent_time)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_Pause_Bone_Anim(ghl_info, ghl_info->mBlist, bone_name, acurrent_time);
	}
	return qfalse;
}

qboolean	G2API_IsPaused(CGhoul2Info* ghl_info, const char* bone_name)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_IsPaused(ghl_info->mFileName, ghl_info->mBlist, bone_name);
	}
	return qfalse;
}

qboolean G2API_StopBoneAnimIndex(CGhoul2Info* ghl_info, const int index)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_Stop_Bone_Anim_Index(ghl_info->mBlist, index);
	}
	return qfalse;
}

qboolean G2API_StopBoneAnim(CGhoul2Info* ghl_info, const char* bone_name)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_Stop_Bone_Anim(ghl_info->mFileName, ghl_info->mBlist, bone_name);
	}
	return qfalse;
}

qboolean G2API_SetBoneAnglesIndex(CGhoul2Info* ghl_info, const int index, const vec3_t angles, const int flags,
	const Eorientations yaw, const Eorientations pitch, const Eorientations roll,
	qhandle_t* model_list, int blend_time, int current_time)
{
	qboolean setPtrs = qfalse;
	qboolean res = qfalse;

	//rww - RAGDOLL_BEGIN
	if (ghl_info)
	{
		res = G2_SetupModelPointers(ghl_info);
		setPtrs = qtrue;

		if (res)
		{
			if (ghl_info->mFlags & GHOUL2_RAG_STARTED)
			{
				return qfalse;
			}
		}
	}
	//rww - RAGDOLL_END

	if (!setPtrs)
	{
		res = G2_SetupModelPointers(ghl_info);
	}

	if (res)
	{
		// ensure we flush the cache
		ghl_info->mSkelFrameNum = 0;
		return G2_Set_Bone_Angles_Index(ghl_info->mBlist, index, angles, flags, yaw, pitch, roll, model_list, ghl_info->mModelindex, blend_time, current_time);
	}
	return qfalse;
}

qboolean G2API_SetBoneAngles(CGhoul2Info_v& ghoul2, const int model_index, const char* bone_name, const vec3_t angles, const int flags,
	const Eorientations up, const Eorientations left, const Eorientations forward,
	qhandle_t* model_list, int blend_time, int current_time)
{
	if (ghoul2.size() > model_index)
	{
		CGhoul2Info* ghl_info = &ghoul2[model_index];
		qboolean setPtrs = qfalse;
		qboolean res = qfalse;

		//rww - RAGDOLL_BEGIN
		if (ghl_info)
		{
			res = G2_SetupModelPointers(ghl_info);
			setPtrs = qtrue;

			if (res)
			{
				if (ghl_info->mFlags & GHOUL2_RAG_STARTED)
				{
					return qfalse;
				}
			}
		}
		//rww - RAGDOLL_END

		if (!setPtrs)
		{
			res = G2_SetupModelPointers(ghoul2);
		}

		if (res)
		{
			// ensure we flush the cache
			ghl_info->mSkelFrameNum = 0;
			return G2_Set_Bone_Angles(ghl_info, ghl_info->mBlist, bone_name, angles, flags, up, left, forward, model_list, ghl_info->mModelindex, blend_time, current_time);
		}
	}
	return qfalse;
}

qboolean G2API_SetBoneAnglesMatrixIndex(CGhoul2Info* ghl_info, const int index, const mdxaBone_t& matrix,
	const int flags, qhandle_t* model_list, int blend_time, int current_time)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		// ensure we flush the cache
		ghl_info->mSkelFrameNum = 0;
		return G2_Set_Bone_Angles_Matrix_Index(ghl_info->mBlist, index, matrix, flags, model_list, ghl_info->mModelindex, blend_time, current_time);
	}
	return qfalse;
}

qboolean G2API_SetBoneAnglesMatrix(CGhoul2Info* ghl_info, const char* bone_name, const mdxaBone_t& matrix,
	const int flags, qhandle_t* model_list, int blend_time, int current_time)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		// ensure we flush the cache
		ghl_info->mSkelFrameNum = 0;
		return G2_Set_Bone_Angles_Matrix(ghl_info->mFileName, ghl_info->mBlist, bone_name, matrix, flags, model_list, ghl_info->mModelindex, blend_time, current_time);
	}
	return qfalse;
}

qboolean G2API_StopBoneAnglesIndex(CGhoul2Info* ghl_info, const int index)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		// ensure we flush the cache
		ghl_info->mSkelFrameNum = 0;
		return G2_Stop_Bone_Angles_Index(ghl_info->mBlist, index);
	}
	return qfalse;
}

qboolean G2API_StopBoneAngles(CGhoul2Info* ghl_info, const char* bone_name)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		// ensure we flush the cache
		ghl_info->mSkelFrameNum = 0;
		return G2_Stop_Bone_Angles(ghl_info->mFileName, ghl_info->mBlist, bone_name);
	}
	return qfalse;
}

void G2API_AbsurdSmoothing(CGhoul2Info_v& ghoul2, qboolean status)
{
	assert(ghoul2.size());
	CGhoul2Info* ghl_info = &ghoul2[0];

	if (status)
	{ //turn it on
		ghl_info->mFlags |= GHOUL2_CRAZY_SMOOTH;
	}
	else
	{ //off
		ghl_info->mFlags &= ~GHOUL2_CRAZY_SMOOTH;
	}
}

//rww - RAGDOLL_BEGIN
class CRagDollParams;
void G2_SetRagDoll(CGhoul2Info_v& ghoul2V, CRagDollParams* parms);
void G2API_SetRagDoll(CGhoul2Info_v& ghoul2, CRagDollParams* parms)
{
	G2_SetRagDoll(ghoul2, parms);
}

void G2_ResetRagDoll(CGhoul2Info_v& ghoul2V);
void G2API_ResetRagDoll(CGhoul2Info_v& ghoul2)
{
	G2_ResetRagDoll(ghoul2);
}
//rww - RAGDOLL_END

qboolean G2API_RemoveBone(CGhoul2Info_v& ghoul2, int model_index, const char* bone_name)
{
	CGhoul2Info* ghl_info = &ghoul2[model_index];

	if (G2_SetupModelPointers(ghl_info))
	{
		// ensure we flush the cache
		ghl_info->mSkelFrameNum = 0;
		return G2_Remove_Bone(ghl_info, ghl_info->mBlist, bone_name);
	}
	return qfalse;
}

//rww - RAGDOLL_BEGIN
#ifdef _DEBUG
extern int ragTraceTime;
extern int ragSSCount;
extern int ragTraceCount;
#endif

void G2API_AnimateG2ModelsRag(CGhoul2Info_v& ghoul2, int acurrent_time, CRagDollUpdateParams* params)
{
	const int current_time = G2API_GetTime(acurrent_time);

#ifdef _DEBUG
	ragTraceTime = 0;
	ragSSCount = 0;
	ragTraceCount = 0;
#endif

	// Walk the list and find all models that are active
	for (int model = 0; model < ghoul2.size(); model++)
	{
		if (ghoul2[model].mModel)
		{
			G2_Animate_Bone_List(ghoul2, current_time, model, params);
		}
	}
#ifdef _DEBUG
	/*
	if (ragTraceTime)
	{
		ri->Printf( PRINT_ALL, "Rag trace time: %i (%i STARTSOLID, %i TOTAL)\n", ragTraceTime, ragSSCount, ragTraceCount);
	}
	*/

	//keep sane limits here, if it gets too slow an assert is proper.
//	assert(ragTraceTime < 150);
//	assert(ragTraceCount < 1500);
#endif
}
//rww - RAGDOLL_END

int G2_Find_Bone_Rag(CGhoul2Info* ghl_info, const boneInfo_v& blist, const char* bone_name);
#define RAG_PCJ						(0x00001)
#define RAG_EFFECTOR				(0x00100)

static inline boneInfo_t* G2_GetRagBoneConveniently(CGhoul2Info_v& ghoul2, const char* bone_name)
{
	assert(ghoul2.size());
	CGhoul2Info* ghl_info = &ghoul2[0];

	if (!(ghl_info->mFlags & GHOUL2_RAG_STARTED))
	{ //can't do this if not in ragdoll
		return nullptr;
	}

	const int bone_index = G2_Find_Bone_Rag(ghl_info, ghl_info->mBlist, bone_name);

	if (bone_index < 0)
	{ //bad bone specification
		return nullptr;
	}

	boneInfo_t* bone = &ghl_info->mBlist[bone_index];

	if (!(bone->flags & BONE_ANGLES_RAGDOLL))
	{ //only want to return rag bones
		return nullptr;
	}

	return bone;
}

qboolean G2API_RagPCJConstraint(CGhoul2Info_v& ghoul2, const char* bone_name, vec3_t min, vec3_t max)
{
	boneInfo_t* bone = G2_GetRagBoneConveniently(ghoul2, bone_name);

	if (!bone)
	{
		return qfalse;
	}

	if (!(bone->RagFlags & RAG_PCJ))
	{ //this function is only for PCJ bones
		return qfalse;
	}

	VectorCopy(min, bone->minAngles);
	VectorCopy(max, bone->maxAngles);

	return qtrue;
}

qboolean G2API_RagPCJGradientSpeed(CGhoul2Info_v& ghoul2, const char* bone_name, const float speed)
{
	boneInfo_t* bone = G2_GetRagBoneConveniently(ghoul2, bone_name);

	if (!bone)
	{
		return qfalse;
	}

	if (!(bone->RagFlags & RAG_PCJ))
	{ //this function is only for PCJ bones
		return qfalse;
	}

	bone->overGradSpeed = speed;

	return qtrue;
}

qboolean G2API_RagEffectorGoal(CGhoul2Info_v& ghoul2, const char* bone_name, vec3_t pos)
{
	boneInfo_t* bone = G2_GetRagBoneConveniently(ghoul2, bone_name);

	if (!bone)
	{
		return qfalse;
	}

	if (!(bone->RagFlags & RAG_EFFECTOR))
	{ //this function is only for effectors
		return qfalse;
	}

	if (!pos)
	{ //go back to none in case we have one then
		bone->hasOverGoal = false;
	}
	else
	{
		VectorCopy(pos, bone->overGoalSpot);
		bone->hasOverGoal = true;
	}
	return qtrue;
}

qboolean G2API_GetRagBonePos(CGhoul2Info_v& ghoul2, const char* bone_name, vec3_t pos, vec3_t ent_angles, vec3_t ent_pos, vec3_t ent_scale)
{ //do something?
	return qfalse;
}

qboolean G2API_RagEffectorKick(CGhoul2Info_v& ghoul2, const char* bone_name, vec3_t velocity)
{
	boneInfo_t* bone = G2_GetRagBoneConveniently(ghoul2, bone_name);

	if (!bone)
	{
		return qfalse;
	}

	if (!(bone->RagFlags & RAG_EFFECTOR))
	{ //this function is only for effectors
		return qfalse;
	}

	bone->epVelocity[2] = 0;
	VectorAdd(bone->epVelocity, velocity, bone->epVelocity);
	bone->physicsSettled = false;

	return qtrue;
}

qboolean G2API_RagForceSolve(CGhoul2Info_v& ghoul2, qboolean force)
{
	assert(ghoul2.size());
	CGhoul2Info* ghl_info = &ghoul2[0];

	if (!(ghl_info->mFlags & GHOUL2_RAG_STARTED))
	{ //can't do this if not in ragdoll
		return qfalse;
	}

	if (force)
	{
		ghl_info->mFlags |= GHOUL2_RAG_FORCESOLVE;
	}
	else
	{
		ghl_info->mFlags &= ~GHOUL2_RAG_FORCESOLVE;
	}

	return qtrue;
}

qboolean G2_SetBoneIKState(CGhoul2Info_v& ghoul2, int time, const char* bone_name, int ikState, sharedSetBoneIKStateParams_t* params);
qboolean G2API_SetBoneIKState(CGhoul2Info_v& ghoul2, int time, const char* bone_name, int ikState, sharedSetBoneIKStateParams_t* params)
{
	return G2_SetBoneIKState(ghoul2, time, bone_name, ikState, params);
}

qboolean G2_IKMove(CGhoul2Info_v& ghoul2, int time, sharedIKMoveParams_t* params);
qboolean G2API_IKMove(CGhoul2Info_v& ghoul2, int time, sharedIKMoveParams_t* params)
{
	return G2_IKMove(ghoul2, time, params);
}

qboolean G2API_RemoveBolt(CGhoul2Info* ghl_info, const int index)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_Remove_Bolt(ghl_info->mBltlist, index);
	}
	return qfalse;
}

int G2API_AddBolt(CGhoul2Info_v& ghoul2, const int model_index, const char* bone_name)
{
	//	assert(ghoul2.size()>model_index);

	if (ghoul2.size() > model_index)
	{
		CGhoul2Info* ghl_info = &ghoul2[model_index];
		if (G2_SetupModelPointers(ghl_info))
		{
			return G2_Add_Bolt(ghl_info, ghl_info->mBltlist, bone_name);
		}
	}
	return -1;
}

int G2API_AddBoltSurfNum(CGhoul2Info* ghl_info, const int surf_index)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_Add_Bolt_Surf_Num(ghl_info, ghl_info->mBltlist, ghl_info->mSlist, surf_index);
	}
	return -1;
}

qboolean G2API_AttachG2Model(CGhoul2Info_v& ghoul2_from, int modelFrom, CGhoul2Info_v& ghoul2To, int toBoltIndex, int toModel)
{
	assert(toBoltIndex >= 0);
	if (toBoltIndex < 0)
	{
		return qfalse;
	}
	if (G2_SetupModelPointers(ghoul2_from) && G2_SetupModelPointers(ghoul2To))
	{
		// make sure we have a model to attach, a model to attach to, and a bolt on that model
		if ((ghoul2_from.size() > modelFrom) &&
			(ghoul2To.size() > toModel) &&
			((ghoul2To[toModel].mBltlist[toBoltIndex].boneNumber != -1) || (ghoul2To[toModel].mBltlist[toBoltIndex].surface_number != -1)))
		{
			// encode the bolt address into the model bolt link
			toModel &= MODEL_AND;
			toBoltIndex &= BOLT_AND;
			ghoul2_from[modelFrom].mModelBoltLink = (toModel << MODEL_SHIFT) | (toBoltIndex << BOLT_SHIFT);
			return qtrue;
		}
	}
	return qfalse;
}

void G2API_SetBoltInfo(CGhoul2Info_v& ghoul2, int model_index, int bolt_info)
{
	if (ghoul2.size() > model_index)
	{
		ghoul2[model_index].mModelBoltLink = bolt_info;
	}
}

qboolean G2API_DetachG2Model(CGhoul2Info* ghl_info)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		ghl_info->mModelBoltLink = -1;
		return qtrue;
	}
	return qfalse;
}

qboolean G2API_AttachEnt(int* bolt_info, CGhoul2Info_v& ghoul2, int model_index, int toBoltIndex, int ent_num, int toModelNum)
{
	CGhoul2Info* ghlInfoTo = &ghoul2[model_index];

	if (bolt_info && G2_SetupModelPointers(ghlInfoTo))
	{
		// make sure we have a model to attach, a model to attach to, and a bolt on that model
		if (ghlInfoTo->mBltlist.size() && ((ghlInfoTo->mBltlist[toBoltIndex].boneNumber != -1) || (ghlInfoTo->mBltlist[toBoltIndex].surface_number != -1)))
		{
			// encode the bolt address into the model bolt link
			toModelNum &= MODEL_AND;
			toBoltIndex &= BOLT_AND;
			ent_num &= ENTITY_AND;
			*bolt_info = (toBoltIndex << BOLT_SHIFT) | (toModelNum << MODEL_SHIFT) | (ent_num << ENTITY_SHIFT);
			return qtrue;
		}
	}
	return qfalse;
}

qboolean gG2_GBMNoReconstruct;
qboolean gG2_GBMUseSPMethod;

qboolean G2API_GetBoltMatrix_SPMethod(CGhoul2Info_v& ghoul2, const int model_index, const int bolt_index, mdxaBone_t* matrix, const vec3_t angles,
	const vec3_t position, const int frame_num, qhandle_t* model_list, const vec3_t scale)
{
	assert(ghoul2.size() > model_index);

	if ((ghoul2.size() > model_index))
	{
		CGhoul2Info* ghl_info = &ghoul2[model_index];

		//assert(bolt_index < ghl_info->mBltlist.size());

		if (ghl_info && (bolt_index < static_cast<int>(ghl_info->mBltlist.size())) && bolt_index >= 0)
		{
			// make sure we have transformed the skeleton
			if (!gG2_GBMNoReconstruct)
			{
				G2_ConstructGhoulSkeleton(ghoul2, frame_num, true, scale);
			}

			gG2_GBMNoReconstruct = qfalse;

			mdxaBone_t scaled;
			mdxaBone_t* use = &ghl_info->mBltlist[bolt_index].position;

			if (scale[0] || scale[1] || scale[2])
			{
				scaled = *use;
				use = &scaled;

				// scale the bolt position by the scale factor for this model since at this point its still in model space
				if (scale[0])
				{
					scaled.matrix[0][3] *= scale[0];
				}
				if (scale[1])
				{
					scaled.matrix[1][3] *= scale[1];
				}
				if (scale[2])
				{
					scaled.matrix[2][3] *= scale[2];
				}
			}
			// pre generate the world matrix
			G2_GenerateWorldMatrix(angles, position);

			VectorNormalize((float*)use->matrix[0]);
			VectorNormalize((float*)use->matrix[1]);
			VectorNormalize((float*)use->matrix[2]);

			Multiply_3x4Matrix(matrix, &worldMatrix, use);
			return qtrue;
		}
	}
	return qfalse;
}

#define G2ERROR(exp,m)		((void)0) //rwwFIXMEFIXME: This is because I'm lazy.
#define G2WARNING(exp,m)     ((void)0)
#define G2NOTE(exp,m)     ((void)0)
#define G2ANIM(ghl_info,m) ((void)0)
bool G2_NeedsRecalc(CGhoul2Info* ghl_info, int frame_num);
void G2_GetBoltMatrixLow(CGhoul2Info& ghoul2, int boltNum, const vec3_t scale, mdxaBone_t& ret_matrix);
void G2_GetBoneMatrixLow(CGhoul2Info& ghoul2, int bone_num, const vec3_t scale, mdxaBone_t& ret_matrix, mdxaBone_t*& retBasepose, mdxaBone_t*& retBaseposeInv);

//qboolean G2API_GetBoltMatrix(CGhoul2Info_v &ghoul2, const int model_index, const int bolt_index, mdxaBone_t *matrix, const vec3_t angles,
//							 const vec3_t position, const int AframeNum, qhandle_t *model_list, const vec3_t scale )
qboolean G2API_GetBoltMatrix(CGhoul2Info_v& ghoul2, const int model_index, const int bolt_index, mdxaBone_t* matrix, const vec3_t angles,
	const vec3_t position, const int frame_num, qhandle_t* model_list, vec3_t scale)
{
	//	G2ERROR(ghoul2.IsValid(),"Invalid ghl_info");
	G2ERROR(matrix, "NULL matrix");
	G2ERROR(model_index >= 0 && model_index < ghoul2.size(), "Invalid ModelIndex");
	const static mdxaBone_t		identityMatrix =
	{
		{
			{ 0.0f, -1.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0f }
		}
	};
	G2_GenerateWorldMatrix(angles, position);
	if (G2_SetupModelPointers(ghoul2))
	{
		if (matrix && model_index >= 0 && model_index < ghoul2.size())
		{
			int tframeNum = G2API_GetTime(frame_num);
			CGhoul2Info* ghl_info = &ghoul2[model_index];
			G2ERROR(bolt_index >= 0 && (bolt_index < ghl_info->mBltlist.size()), va("Invalid Bolt Index (%d:%s)", bolt_index, ghl_info->mFileName));

			if (bolt_index >= 0 && ghl_info && (bolt_index < static_cast<int>(ghl_info->mBltlist.size())))
			{
				mdxaBone_t bolt;

#if 0 //yeah, screw it
				if (!gG2_GBMNoReconstruct)
				{ //This should only be used when you know what you're doing.
					if (G2_NeedsRecalc(ghl_info, tframeNum))
					{
						G2_ConstructGhoulSkeleton(ghoul2, tframeNum, true, scale);
					}
				}
				else
				{
					gG2_GBMNoReconstruct = qfalse;
				}
#else
				if (G2_NeedsRecalc(ghl_info, tframeNum))
				{
					G2_ConstructGhoulSkeleton(ghoul2, tframeNum, true, scale);
				}
#endif

				G2_GetBoltMatrixLow(*ghl_info, bolt_index, scale, bolt);
				// scale the bolt position by the scale factor for this model since at this point its still in model space
				if (scale[0])
				{
					bolt.matrix[0][3] *= scale[0];
				}
				if (scale[1])
				{
					bolt.matrix[1][3] *= scale[1];
				}
				if (scale[2])
				{
					bolt.matrix[2][3] *= scale[2];
				}
				VectorNormalize((float*)&bolt.matrix[0]);
				VectorNormalize((float*)&bolt.matrix[1]);
				VectorNormalize((float*)&bolt.matrix[2]);

				Multiply_3x4Matrix(matrix, &worldMatrix, &bolt);
#if G2API_DEBUG
				for (int i = 0; i < 3; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						assert(!_isnan(matrix->matrix[i][j]));
					}
				}
#endif// _DEBUG
				G2ANIM(ghl_info, "G2API_GetBoltMatrix");

				if (!gG2_GBMUseSPMethod)
				{ //this is horribly stupid and I hate it. But lots of game code is written to assume this 90 degree offset thing.
					float ftemp = matrix->matrix[0][0];
					matrix->matrix[0][0] = -matrix->matrix[0][1];
					matrix->matrix[0][1] = ftemp;

					ftemp = matrix->matrix[1][0];
					matrix->matrix[1][0] = -matrix->matrix[1][1];
					matrix->matrix[1][1] = ftemp;

					ftemp = matrix->matrix[2][0];
					matrix->matrix[2][0] = -matrix->matrix[2][1];
					matrix->matrix[2][1] = ftemp;
				}
				else
				{ //reset it
					gG2_GBMUseSPMethod = qfalse;
				}

				return qtrue;
			}
		}
	}
	else
	{
		G2WARNING(0, "G2API_GetBoltMatrix Failed on empty or bad model");
	}
	Multiply_3x4Matrix(matrix, &worldMatrix, (mdxaBone_t*)&identityMatrix);
	return qfalse;
}

void G2API_ListSurfaces(CGhoul2Info* ghl_info)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		G2_List_Model_Surfaces(ghl_info->mFileName);
	}
}

void G2API_ListBones(CGhoul2Info* ghl_info, int frame)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		G2_List_Model_Bones(ghl_info->mFileName);
	}
}

// decide if we have Ghoul2 models associated with this ghoul list or not
qboolean G2API_HaveWeGhoul2Models(CGhoul2Info_v& ghoul2)
{
	if (ghoul2.size())
	{
		for (int i = 0; i < ghoul2.size(); i++)
		{
			if (ghoul2[i].mModelindex != -1)
			{
				return qtrue;
			}
		}
	}
	return qfalse;
}

// run through the Ghoul2 models and set each of the mModel values to the correct one from the cgs.gameModel offset lsit
void G2API_SetGhoul2ModelIndexes(CGhoul2Info_v& ghoul2, qhandle_t* model_list, qhandle_t* skin_list)
{
#if 0
	int i;
	if (&ghoul2)
	{
		for (i = 0; i < ghoul2.size(); i++)
		{
			if (ghoul2[i].mModelindex != -1)
			{
				// broken into 3 lines for debugging, STL is a pain to view...
				//
				int iModelIndex = ghoul2[i].mModelindex;
				qhandle_t mModel = model_list[iModelIndex];
				ghoul2[i].mModel = mModel;

				ghoul2[i].mSkin = skin_list[ghoul2[i].mCustomSkin];
			}
		}
	}
#endif
}

char* G2API_GetAnimFileNameIndex(qhandle_t model_index)
{
	const model_t* mod_m = R_GetModelByHandle(model_index);
	return mod_m->mdxm->animName;
}

/************************************************************************************************
 * G2API_GetAnimFileName
 *    obtains the name of a model's .gla (animation) file
 *
 * Input
 *    pointer to list of CGhoul2Info's, WraithID of specific model in that list
 *
 * Output
 *    true if a good filename was obtained, false otherwise
 *
 ************************************************************************************************/
qboolean G2API_GetAnimFileName(CGhoul2Info* ghl_info, char** filename)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_GetAnimFileName(ghl_info->mFileName, filename);
	}
	return qfalse;
}

/*
=======================
SV_QsortEntityNumbers
=======================
*/
static int QDECL QsortDistance(const void* a, const void* b) {
	const float& ea = ((CollisionRecord_t*)a)->mDistance;
	const float& eb = ((CollisionRecord_t*)b)->mDistance;

	if (ea < eb) {
		return -1;
	}
	return 1;
}

static inline bool G2_NeedRetransform(CGhoul2Info* g2, int frame_num)
{ //see if we need to do another transform
	size_t i = 0;
	bool needTrans = false;
	while (i < g2->mBlist.size())
	{
		float	time;
		boneInfo_t& bone = g2->mBlist[i];

		if (bone.pauseTime)
		{
			time = (bone.pauseTime - bone.startTime) / 50.0f;
		}
		else
		{
			time = (frame_num - bone.startTime) / 50.0f;
		}
		const int newFrame = bone.start_frame + (time * bone.anim_speed);

		if (newFrame < bone.end_frame ||
			(bone.flags & BONE_ANIM_OVERRIDE_LOOP) ||
			(bone.flags & BONE_NEED_TRANSFORM))
		{ //ok, we're gonna have to do it. bone is apparently animating.
			bone.flags &= ~BONE_NEED_TRANSFORM;
			needTrans = true;
		}
		i++;
	}

	return needTrans;
}

void G2API_CollisionDetectCache(CollisionRecord_t* collRecMap, CGhoul2Info_v& ghoul2, const vec3_t angles, const vec3_t position,
	int frameNumber, int ent_num, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, IHeapAllocator* g2_vert_space, int traceFlags, int use_lod, float fRadius)
{ //this will store off the transformed verts for the next trace - this is slower, but for models that do not animate
	//frequently it is much much faster. -rww
#if 0 // UNUSED
	int* test = ghoul2[0].mTransformedVertsArray;
#endif
	if (G2_SetupModelPointers(ghoul2))
	{
		vec3_t	transRayStart, transRayEnd;

		const int tframeNum = G2API_GetTime(frameNumber);
		// make sure we have transformed the whole skeletons for each model
		if (G2_NeedRetransform(&ghoul2[0], tframeNum) || !ghoul2[0].mTransformedVertsArray)
		{ //optimization, only create new transform space if we need to, otherwise
			//store it off!
			int i = 0;
			while (i < ghoul2.size())
			{
				CGhoul2Info& g2 = ghoul2[i];

				/*
				if ((g2.mFlags & GHOUL2_ZONETRANSALLOC) && g2.mTransformedVertsArray)
				{ //clear it out, yo.
					Z_Free(g2.mTransformedVertsArray);
					g2.mTransformedVertsArray = 0;
				}
				*/
				if (!g2.mTransformedVertsArray || !(g2.mFlags & GHOUL2_ZONETRANSALLOC))
				{ //reworked so we only alloc once!
					//if we have a pointer, but not a ghoul2_zonetransalloc flag, then that means
					//it is a miniheap pointer. Just stomp over it.
					const int iSize = g2.current_model->mdxm->numSurfaces * 4;
					g2.mTransformedVertsArray = static_cast<size_t*>(Z_Malloc(iSize, TAG_GHOUL2, qtrue));
				}

				g2.mFlags |= GHOUL2_ZONETRANSALLOC;

				i++;
			}
			G2_ConstructGhoulSkeleton(ghoul2, frameNumber, true, scale);
			g2_vert_space->ResetHeap();

			// now having done that, time to build the model
#ifdef _G2_GORE
			G2_TransformModel(ghoul2, frameNumber, scale, g2_vert_space, use_lod, false);
#else
			G2_TransformModel(ghoul2, frameNumber, scale, g2_vert_space, use_lod);
#endif

			//don't need to do this anymore now that I am using a flag for zone alloc.
			/*
			i = 0;
			while (i < ghoul2.size())
			{
				CGhoul2Info &g2 = ghoul2[i];
				int iSize = g2.current_model->mdxm->numSurfaces * 4;

				int *zoneMem = (int *)Z_Malloc(iSize, TAG_GHOUL2, qtrue);
				memcpy(zoneMem, g2.mTransformedVertsArray, iSize);
				g2.mTransformedVertsArray = zoneMem;
				g2.mFlags |= GHOUL2_ZONETRANSALLOC;
				i++;
			}
			*/
		}

		// pre generate the world matrix - used to transform the incoming ray
		G2_GenerateWorldMatrix(angles, position);

		// model is built. Lets check to see if any triangles are actually hit.
		// first up, translate the ray to model space
		TransformAndTranslatePoint(rayStart, transRayStart, &worldMatrixInv);
		TransformAndTranslatePoint(rayEnd, transRayEnd, &worldMatrixInv);

		// now walk each model and check the ray against each poly - sigh, this is SO expensive. I wish there was a better way to do this.
#ifdef _G2_GORE
		G2_TraceModels(ghoul2, transRayStart, transRayEnd, collRecMap, ent_num, traceFlags, use_lod, fRadius, 0, 0, 0, 0, nullptr, qfalse);
#else
		G2_TraceModels(ghoul2, transRayStart, transRayEnd, collRecMap, ent_num, traceFlags, use_lod, fRadius);
#endif
		int i;
		for (i = 0; i < MAX_G2_COLLISIONS && collRecMap[i].mEntityNum != -1; i++);

		// now sort the resulting array of collision records so they are distance ordered
		qsort(collRecMap, i,
			sizeof(CollisionRecord_t), QsortDistance);
	}
}

void G2API_CollisionDetect(CollisionRecord_t* collRecMap, CGhoul2Info_v& ghoul2, const vec3_t angles, const vec3_t position,
	int frameNumber, int ent_num, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, IHeapAllocator* g2_vert_space, int traceFlags, int use_lod, float fRadius)
{
	/*
	if (1)
	{
		G2API_CollisionDetectCache(collRecMap, ghoul2, angles, position, frameNumber, ent_num,
			rayStart, rayEnd, scale, g2_vert_space, traceFlags, use_lod, fRadius);
		return;
	}
	*/

	if (G2_SetupModelPointers(ghoul2))
	{
		vec3_t	transRayStart, transRayEnd;

		// make sure we have transformed the whole skeletons for each model
		G2_ConstructGhoulSkeleton(ghoul2, frameNumber, true, scale);

		// pre generate the world matrix - used to transform the incoming ray
		G2_GenerateWorldMatrix(angles, position);

		g2_vert_space->ResetHeap();

		// now having done that, time to build the model
#ifdef _G2_GORE
		G2_TransformModel(ghoul2, frameNumber, scale, g2_vert_space, use_lod, false);
#else
		G2_TransformModel(ghoul2, frameNumber, scale, g2_vert_space, use_lod);
#endif

		// model is built. Lets check to see if any triangles are actually hit.
		// first up, translate the ray to model space
		TransformAndTranslatePoint(rayStart, transRayStart, &worldMatrixInv);
		TransformAndTranslatePoint(rayEnd, transRayEnd, &worldMatrixInv);

		// now walk each model and check the ray against each poly - sigh, this is SO expensive. I wish there was a better way to do this.
#ifdef _G2_GORE
		G2_TraceModels(ghoul2, transRayStart, transRayEnd, collRecMap, ent_num, traceFlags, use_lod, fRadius, 0, 0, 0, 0, nullptr, qfalse);
#else
		G2_TraceModels(ghoul2, transRayStart, transRayEnd, collRecMap, ent_num, traceFlags, use_lod, fRadius);
#endif
		int i;
		for (i = 0; i < MAX_G2_COLLISIONS && collRecMap[i].mEntityNum != -1; i++);

		// now sort the resulting array of collision records so they are distance ordered
		qsort(collRecMap, i,
			sizeof(CollisionRecord_t), QsortDistance);
	}
}

qboolean G2API_SetGhoul2ModelFlags(CGhoul2Info* ghl_info, const int flags)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		ghl_info->mFlags &= GHOUL2_NEWORIGIN;
		ghl_info->mFlags |= flags;
		return qtrue;
	}
	return qfalse;
}

int G2API_GetGhoul2ModelFlags(CGhoul2Info* ghl_info)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return (ghl_info->mFlags & ~GHOUL2_NEWORIGIN);
	}
	return 0;
}

// given a boltmatrix, return in vec a normalised vector for the axis requested in flags
void G2API_GiveMeVectorFromMatrix(mdxaBone_t* bolt_matrix, Eorientations flags, vec3_t vec)
{
	switch (flags)
	{
	case ORIGIN:
		vec[0] = bolt_matrix->matrix[0][3];
		vec[1] = bolt_matrix->matrix[1][3];
		vec[2] = bolt_matrix->matrix[2][3];
		break;
	case POSITIVE_Y:
		vec[0] = bolt_matrix->matrix[0][1];
		vec[1] = bolt_matrix->matrix[1][1];
		vec[2] = bolt_matrix->matrix[2][1];
		break;
	case POSITIVE_X:
		vec[0] = bolt_matrix->matrix[0][0];
		vec[1] = bolt_matrix->matrix[1][0];
		vec[2] = bolt_matrix->matrix[2][0];
		break;
	case POSITIVE_Z:
		vec[0] = bolt_matrix->matrix[0][2];
		vec[1] = bolt_matrix->matrix[1][2];
		vec[2] = bolt_matrix->matrix[2][2];
		break;
	case NEGATIVE_Y:
		vec[0] = -bolt_matrix->matrix[0][1];
		vec[1] = -bolt_matrix->matrix[1][1];
		vec[2] = -bolt_matrix->matrix[2][1];
		break;
	case NEGATIVE_X:
		vec[0] = -bolt_matrix->matrix[0][0];
		vec[1] = -bolt_matrix->matrix[1][0];
		vec[2] = -bolt_matrix->matrix[2][0];
		break;
	case NEGATIVE_Z:
		vec[0] = -bolt_matrix->matrix[0][2];
		vec[1] = -bolt_matrix->matrix[1][2];
		vec[2] = -bolt_matrix->matrix[2][2];
		break;
	}
}

int G2API_CopyGhoul2Instance(CGhoul2Info_v& g2From, CGhoul2Info_v& g2To, int model_index)
{
	assert(model_index == -1); // copy individual bolted parts is not used in jk2 and I didn't want to deal with it
	// if ya want it, we will add it back correctly

//G2ERROR(ghoul2_from.IsValid(),"Invalid ghl_info");
	if (g2From.IsValid())
	{
#ifdef _DEBUG
		if (g2To.IsValid())
		{
			assert(!"Copying to a valid g2 instance?!");

			if (g2To[0].mBoneCache)
			{
				assert(!"Instance has a bonecache too.. it's gonna get stomped");
			}
		}
#endif
		g2To.DeepCopy(g2From);

#ifdef _G2_GORE //check through gore stuff then, as well.
		int model = 0;

		while (model < g2To.size())
		{
			if (g2To[model].mGoreSetTag)
			{
				CGoreSet* gore = FindGoreSet(g2To[model].mGoreSetTag);
				assert(gore);
				gore->mRefCount++;
			}

			model++;
		}
#endif
		//G2ANIM(ghoul2_from,"G2API_CopyGhoul2Instance (source)");
		//G2ANIM(ghoul2To,"G2API_CopyGhoul2Instance (dest)");
	}

	return -1;
}

void G2API_CopySpecificG2Model(CGhoul2Info_v& ghoul2_from, int modelFrom, CGhoul2Info_v& ghoul2To, int modelTo)
{
#if 0
	qboolean forceReconstruct = qtrue;
#endif //model1 was not getting reconstructed like it should for thrown sabers?
	//might have been a bug in the reconstruct checking which has since been
	//mangled and probably fixed. -rww

 // assume we actually have a model to copy from
	if (ghoul2_from.size() > modelFrom)
	{
		// if we don't have enough models on the to side, resize us so we do
		if (ghoul2To.size() <= modelTo)
		{
			assert(modelTo < 5);
			ghoul2To.resize(modelTo + 1);
#if 0
			forceReconstruct = qtrue;
#endif
		}
		// do the copy

		if (ghoul2To.IsValid() && ghoul2To.size() >= modelTo)
		{ //remove the bonecache before we stomp over this instance.
			if (ghoul2To[modelTo].mBoneCache)
			{
				RemoveBoneCache(ghoul2To[modelTo].mBoneCache);
				ghoul2To[modelTo].mBoneCache = nullptr;
			}
		}
		ghoul2To[modelTo] = ghoul2_from[modelFrom];

#if 0
		if (forceReconstruct)
		{ //rww - we should really do this shouldn't we? If we don't mark a reconstruct after this,
			//and we do a GetBoltMatrix in the same frame, it doesn't reconstruct the skeleton and returns
			//a completely invalid matrix
			ghoul2To[0].mSkelFrameNum = 0;
		}
#endif
	}
}

// This version will automatically copy everything about this model, and make a new one if necessary.
void G2API_DuplicateGhoul2Instance(CGhoul2Info_v& g2From, CGhoul2Info_v** g2To)
{
	//int ignore;

	if (*g2To)
	{	// This is bad.  We only want to do this if there is not yet a to declared.
		assert(0);
		return;
	}

	*g2To = new CGhoul2Info_v;
#ifdef _FULL_G2_LEAK_CHECKING
	if (g_G2AllocServer)
	{
		g_G2ServerAlloc += sizeof(CGhoul2Info_v);
	}
	else
	{
		g_G2ClientAlloc += sizeof(CGhoul2Info_v);
	}
	g_Ghoul2Allocations += sizeof(CGhoul2Info_v);
	G2_DEBUG_ShovePtrInTracker(*g2To);
#endif
	CGhoul2Info_v& ghoul2 = *(*g2To);

	/*ignore = */G2API_CopyGhoul2Instance(g2From, ghoul2, -1);
}

char* G2API_GetSurfaceName(CGhoul2Info_v& ghoul2, int model_index, int surf_number)
{
	static char noSurface[1] = "";
	CGhoul2Info* ghl_info = &ghoul2[model_index];

	if (G2_SetupModelPointers(ghl_info))
	{
		model_t* mod = (model_t*)ghl_info->current_model;
		mdxmSurface_t* surf = nullptr;
		mdxmSurfHierarchy_t* surfInfo = nullptr;

#ifndef FINAL_BUILD
		if (!mod || !mod->mdxm)
		{
			Com_Error(ERR_DROP, "G2API_GetSurfaceName: Bad model on instance %s.", ghl_info->mFileName);
		}
#endif

		//ok, I guess it's semi-valid for the user to be passing in surface > numSurfs because they don't know how many surfs a model
		//may have.. but how did they get that surf index to begin with? Oh well.
		if (surf_number < 0 || surf_number >= mod->mdxm->numSurfaces)
		{
			ri->Printf(PRINT_ALL, "G2API_GetSurfaceName: You passed in an invalid surface number (%i) for model %s.\n", surf_number, ghl_info->mFileName);
			return noSurface;
		}

		surf = static_cast<mdxmSurface_t*>(G2_FindSurface((void*)mod, surf_number, 0));
		if (surf)
		{
#ifndef FINAL_BUILD
			if (surf->thisSurfaceIndex < 0 || surf->thisSurfaceIndex >= mod->mdxm->numSurfaces)
			{
				Com_Error(ERR_DROP, "G2API_GetSurfaceName: Bad surf num (%i) on surf for instance %s.", surf->thisSurfaceIndex, ghl_info->mFileName);
			}
#endif
			mdxmHierarchyOffsets_t* surf_indexes = (mdxmHierarchyOffsets_t*)((byte*)mod->mdxm + sizeof(mdxmHeader_t));
			surfInfo = (mdxmSurfHierarchy_t*)((byte*)surf_indexes + surf_indexes->offsets[surf->thisSurfaceIndex]);
			return surfInfo->name;
		}
	}
	return noSurface;
}

int	G2API_GetSurfaceIndex(CGhoul2Info* ghl_info, const char* surface_name)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_GetSurfaceIndex(ghl_info, surface_name);
	}
	return -1;
}

char* G2API_GetGLAName(CGhoul2Info_v& ghoul2, int model_index)
{
	if (G2_SetupModelPointers(ghoul2))
	{
		if ((ghoul2.size() > model_index))
		{
			//model_t	*mod = R_GetModelByHandle(RE_RegisterModel(ghoul2[model_index].mFileName));
			//return mod->mdxm->animName;

			assert(ghoul2[model_index].current_model && ghoul2[model_index].current_model->mdxm);
			return ghoul2[model_index].current_model->mdxm->animName;
		}
	}
	return nullptr;
}

qboolean G2API_SetNewOrigin(CGhoul2Info_v& ghoul2, const int bolt_index)
{
	CGhoul2Info* ghl_info = nullptr;

	if (ghoul2.size() > 0)
	{
		ghl_info = &ghoul2[0];
	}

	if (G2_SetupModelPointers(ghl_info))
	{
		if (bolt_index < 0)
		{
			char modelName[MAX_QPATH];
			if (ghl_info->current_model &&
				ghl_info->current_model->name[0])
			{
				strcpy(modelName, ghl_info->current_model->name);
			}
			else
			{
				strcpy(modelName, "None?!");
			}

			Com_Error(ERR_DROP, "Bad boltindex (%i) trying to SetNewOrigin (naughty naughty!)\nModel %s\n", bolt_index, modelName);
		}

		ghl_info->mNewOrigin = bolt_index;
		ghl_info->mFlags |= GHOUL2_NEWORIGIN;
		return qtrue;
	}
	return qfalse;
}

int G2API_GetBoneIndex(CGhoul2Info* ghl_info, const char* bone_name)
{
	if (G2_SetupModelPointers(ghl_info))
	{
		return G2_Get_Bone_Index(ghl_info, bone_name);
	}
	return -1;
}

qboolean G2API_SaveGhoul2Models(CGhoul2Info_v& ghoul2, char** buffer, int* size)
{
	return G2_SaveGhoul2Models(ghoul2, buffer, size);
}

void G2API_LoadGhoul2Models(CGhoul2Info_v& ghoul2, char* buffer)
{
	G2_LoadGhoul2Model(ghoul2, buffer);
}

void G2API_FreeSaveBuffer(char* buffer)
{
	Z_Free(buffer);
}

// this is kinda sad, but I need to call the destructor in this module (exe), not the game.dll...
//
void G2API_LoadSaveCodeDestructGhoul2Info(CGhoul2Info_v& ghoul2)
{
#ifdef _G2_GORE
	G2API_ClearSkinGore(ghoul2);
#endif
	ghoul2.~CGhoul2Info_v();	// so I can load junk over it then memset to 0 without orphaning
}

//see if surfs have any shader info...
qboolean G2API_SkinlessModel(CGhoul2Info_v& ghoul2, int model_index)
{
	CGhoul2Info* g2 = &ghoul2[model_index];

	if (G2_SetupModelPointers(g2))
	{
		const model_t* mod = (model_t*)g2->current_model;

		if (mod &&
			mod->mdxm)
		{
			mdxmSurfHierarchy_t* surf = (mdxmSurfHierarchy_t*)((byte*)mod->mdxm + mod->mdxm->ofsSurfHierarchy);

			for (int i = 0; i < mod->mdxm->numSurfaces; i++)
			{
				if (surf->shader[0])
				{ //found a surface with a shader name, ok.
					return qfalse;
				}

				surf = (mdxmSurfHierarchy_t*)((byte*)surf + (intptr_t)(&static_cast<mdxmSurfHierarchy_t*>(nullptr)->childIndexes[surf->numChildren]));
			}
		}
	}

	//found nothing.
	return qtrue;
}

//#ifdef _SOF2
#ifdef _G2_GORE
void ResetGoreTag(); // put here to reduce coupling

//way of seeing how many marks are on a model currently -rww
int G2API_GetNumGoreMarks(CGhoul2Info_v& ghoul2, int model_index)
{
	const CGhoul2Info* g2 = &ghoul2[model_index];

	if (g2->mGoreSetTag)
	{
		const CGoreSet* goreSet = FindGoreSet(g2->mGoreSetTag);

		if (goreSet)
		{
			return goreSet->mGoreRecords.size();
		}
	}

	return 0;
}

void G2API_ClearSkinGore(CGhoul2Info_v& ghoul2)
{
	for (int i = 0; i < ghoul2.size(); i++)
	{
		if (ghoul2[i].mGoreSetTag)
		{
			DeleteGoreSet(ghoul2[i].mGoreSetTag);
			ghoul2[i].mGoreSetTag = 0;
		}
	}
}

int G2API_Ghoul2Size(CGhoul2Info_v& ghoul2)
{
	return ghoul2.size();
}

extern int		G2_DecideTraceLod(const CGhoul2Info& ghoul2, int use_lod);
void G2API_AddSkinGore(CGhoul2Info_v& ghoul2, SSkinGoreData& gore)
{
	if (VectorLength(gore.rayDirection) < .1f)
	{
		assert(0); // can't add gore without a shot direction
		return;
	}

	// make sure we have transformed the whole skeletons for each model
	//G2_ConstructGhoulSkeleton(ghoul2, gore.current_time, NULL, true, gore.angles, gore.position, gore.scale, false);
	G2_ConstructGhoulSkeleton(ghoul2, gore.current_time, true, gore.scale);

	// pre generate the world matrix - used to transform the incoming ray
	G2_GenerateWorldMatrix(gore.angles, gore.position);

	// first up, translate the ray to model space
	vec3_t	transRayDirection, transHitLocation;
	TransformAndTranslatePoint(gore.hitLocation, transHitLocation, &worldMatrixInv);
	TransformPoint(gore.rayDirection, transRayDirection, &worldMatrixInv);

	ResetGoreTag();
	const int lodbias = Com_Clamp(0, 2, G2_DecideTraceLod(ghoul2[0], ri->Cvar_VariableIntegerValue("r_lodbias")));
	const int maxLod = Com_Clamp(0, ghoul2[0].current_model->numLods, 3);	//limit to the number of lods the main model has
	for (int lod = lodbias; lod < maxLod; lod++)
	{
		// now having done that, time to build the model
		ri->GetG2VertSpaceServer()->ResetHeap();

		G2_TransformModel(ghoul2, gore.current_time, gore.scale, ri->GetG2VertSpaceServer(), lod, true);

		// now walk each model and compute new texture coordinates
		G2_TraceModels(ghoul2, transHitLocation, transRayDirection, nullptr, gore.ent_num, 0, lod, 0.0f, gore.SSize, gore.TSize, gore.theta, gore.shader, &gore, qtrue);
	}
}
#endif

qboolean G2_TestModelPointers(CGhoul2Info* ghl_info) // returns true if the model is properly set up
{
	G2ERROR(ghl_info, "NULL ghl_info");
	if (!ghl_info)
	{
		return qfalse;
	}
	ghl_info->mValid = false;
	if (ghl_info->mModelindex != -1)
	{
		if (ri->Cvar_VariableIntegerValue("dedicated") ||
			(G2_ShouldRegisterServer())) //supreme hackery!
		{
			ghl_info->mModel = RE_RegisterServerModel(ghl_info->mFileName);
		}
		else
		{
			ghl_info->mModel = RE_RegisterModel(ghl_info->mFileName);
		}
		ghl_info->current_model = R_GetModelByHandle(ghl_info->mModel);
		if (ghl_info->current_model)
		{
			if (ghl_info->current_model->mdxm)
			{
				if (ghl_info->currentModelSize)
				{
					if (ghl_info->currentModelSize != ghl_info->current_model->mdxm->ofsEnd)
					{
						Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
					}
				}
				ghl_info->currentModelSize = ghl_info->current_model->mdxm->ofsEnd;
				ghl_info->animModel = R_GetModelByHandle(ghl_info->current_model->mdxm->animIndex);
				if (ghl_info->animModel)
				{
					ghl_info->aHeader = ghl_info->animModel->mdxa;
					if (ghl_info->aHeader)
					{
						if (ghl_info->currentAnimModelSize)
						{
							if (ghl_info->currentAnimModelSize != ghl_info->aHeader->ofsEnd)
							{
								Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
							}
						}
						ghl_info->currentAnimModelSize = ghl_info->aHeader->ofsEnd;
						ghl_info->mValid = true;
					}
				}
			}
		}
	}
	if (!ghl_info->mValid)
	{
		ghl_info->current_model = nullptr;
		ghl_info->currentModelSize = 0;
		ghl_info->animModel = nullptr;
		ghl_info->currentAnimModelSize = 0;
		ghl_info->aHeader = nullptr;
	}
	return static_cast<qboolean>(ghl_info->mValid);
}

#ifdef G2_PERFORMANCE_ANALYSIS
#include "qcommon/timing.h"
extern timing_c G2PerformanceTimer_G2_SetupModelPointers;
extern int G2Time_G2_SetupModelPointers;
#endif

qboolean G2_SetupModelPointers(CGhoul2Info* ghl_info) // returns true if the model is properly set up
{
#ifdef G2_PERFORMANCE_ANALYSIS
	G2PerformanceTimer_G2_SetupModelPointers.Start();
#endif
	G2ERROR(ghl_info, "NULL ghl_info");
	if (!ghl_info)
	{
		return qfalse;
	}

	//	if (ghl_info->mValid && ghl_info->current_model)

	ghl_info->mValid = false;

	//	G2WARNING(ghl_info->mModelindex != -1,"Setup request on non-used info slot?");
	if (ghl_info->mModelindex != -1)
	{
		G2ERROR(ghl_info->mFileName[0], "empty ghl_info->mFileName");

		// RJ - experimental optimization!
		if (!ghl_info->mModel || 1)
		{
			if (ri->Cvar_VariableIntegerValue("dedicated") ||
				(G2_ShouldRegisterServer())) //supreme hackery!
			{
				ghl_info->mModel = RE_RegisterServerModel(ghl_info->mFileName);
			}
			else
			{
				ghl_info->mModel = RE_RegisterModel(ghl_info->mFileName);
			}
			ghl_info->current_model = R_GetModelByHandle(ghl_info->mModel);
		}

		G2ERROR(ghl_info->current_model, va("NULL Model (glm) %s", ghl_info->mFileName));
		if (ghl_info->current_model)
		{
			G2ERROR(ghl_info->current_model->mdxm, va("Model has no mdxm (glm) %s", ghl_info->mFileName));
			if (ghl_info->current_model->mdxm)
			{
				if (ghl_info->currentModelSize)
				{
					if (ghl_info->currentModelSize != ghl_info->current_model->mdxm->ofsEnd)
					{
						Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
					}
				}
				ghl_info->currentModelSize = ghl_info->current_model->mdxm->ofsEnd;
				G2ERROR(ghl_info->currentModelSize, va("Zero sized Model? (glm) %s", ghl_info->mFileName));

				ghl_info->animModel = R_GetModelByHandle(ghl_info->current_model->mdxm->animIndex);
				G2ERROR(ghl_info->animModel, va("NULL Model (gla) %s", ghl_info->mFileName));
				if (ghl_info->animModel)
				{
					ghl_info->aHeader = ghl_info->animModel->mdxa;
					G2ERROR(ghl_info->aHeader, va("Model has no mdxa (gla) %s", ghl_info->mFileName));
					if (ghl_info->aHeader)
					{
						if (ghl_info->currentAnimModelSize)
						{
							if (ghl_info->currentAnimModelSize != ghl_info->aHeader->ofsEnd)
							{
								Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
							}
						}
						ghl_info->currentAnimModelSize = ghl_info->aHeader->ofsEnd;
						G2ERROR(ghl_info->currentAnimModelSize, va("Zero sized Model? (gla) %s", ghl_info->mFileName));
						ghl_info->mValid = true;
					}
				}
			}
		}
	}
	if (!ghl_info->mValid)
	{
		ghl_info->current_model = nullptr;
		ghl_info->currentModelSize = 0;
		ghl_info->animModel = nullptr;
		ghl_info->currentAnimModelSize = 0;
		ghl_info->aHeader = nullptr;
	}

#ifdef G2_PERFORMANCE_ANALYSIS
	G2Time_G2_SetupModelPointers += G2PerformanceTimer_G2_SetupModelPointers.End();
#endif
	return static_cast<qboolean>(ghl_info->mValid);
}

qboolean G2_SetupModelPointers(CGhoul2Info_v& ghoul2) // returns true if any model is properly set up
{
	bool ret = false;
	for (int i = 0; i < ghoul2.size(); i++)
	{
		const qboolean r = G2_SetupModelPointers(&ghoul2[i]);
		ret = ret || r;
	}
	return static_cast<qboolean>(ret);
}

qboolean G2API_IsGhoul2InfovValid(CGhoul2Info_v& ghoul2)
{
	return static_cast<qboolean>(ghoul2.IsValid());
}

const char* G2API_GetModelName(CGhoul2Info_v& ghoul2, int model_index)
{
	return ghoul2[model_index].mFileName;
}