#include "server/exe_headers.h"

#include "ghoul2/G2.h"
#include "ghoul2/ghoul2_gore.h" // ragdoll
#include "qcommon/q_shared.h"
#include "qcommon/MiniHeap.h"
#include "rd-common/tr_common.h"
#include "tr_local.h"
#include <set>

#ifdef _MSC_VER
#pragma warning (push, 3)	//go back down to 3 for the stl include
#endif
#include <list>
#ifdef _MSC_VER
#pragma warning (pop)
#endif

extern mdxaBone_t worldMatrix;
extern mdxaBone_t worldMatrixInv;

#define NUM_G2T_TIME (2)
#ifndef REND2_SP
// must be a power of two
#define MAX_G2_MODELS (1024) //jacesolaris was 512
#define G2_MODEL_BITS (10)   //jacesolaris was 9
#else
#define MAX_G2_MODELS (1024) //jacesolaris was 512
#define G2_MODEL_BITS (10)   //jacesolaris was 9
#endif
#define G2_INDEX_MASK (MAX_G2_MODELS-1)
// rww - RAGDOLL_BEGIN
#define GHOUL2_RAG_STARTED						0x0010
#define GHOUL2_RAG_FORCESOLVE					0x1000		//api-override, determine if ragdoll should be forced to continue solving even if it thinks it is settled
// rww - RAGDOLL_END

#if G2API_DEBUG

#define G2ERROR(exp,m) (void)( (exp) || (G2APIError().Error(m,0,__FILE__,__LINE__), 0) )
#define G2WARNING(exp,m) (void)( (exp) || (G2APIError().Error(m,1,__FILE__,__LINE__), 0) )
#define G2NOTE(exp,m) (void)( (exp) || (G2APIError().Error(m,2,__FILE__,__LINE__), 0) )
#define G2ANIM(ghlInfo,m) (void)((G2APIError().AnimTest(ghlInfo,m,__FILE__,__LINE__), 0) )

#else

#define G2ERROR(exp,m) ((void)0)
#define G2WARNING(exp,m) ((void)0)
#define G2NOTE(exp,m) ((void)0)
#define G2ANIM(ghlInfo,m) ((void)0)

#endif

// get/set time
static int G2TimeBases[NUM_G2T_TIME];

// used in tr_init.cpp
qboolean gG2_GBMNoReconstruct;
qboolean gG2_GBMUseSPMethod;

// Local functions pre-definition
qboolean G2_TestModelPointers(CGhoul2Info* ghlInfo);
void CL_InitRef(void);

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
		Com_Printf("g2 leak tracker was never initialized!\n");
		return;
	}

	while (i < MAX_TRACKED_ALLOC)
	{
		if (g_G2AllocTrack[i])
		{
			Com_Printf("Bad guy found in slot %i, attempting to access...", i);
			CGhoul2Info_v& g2v = *g_G2AllocTrack[i];
			CGhoul2Info& g2 = g2v[0];

			if (g2v.IsValid() && g2.mFileName && g2.mFileName[0])
			{
				Com_Printf("Bad guy's filename is %s\n", g2.mFileName);
			}
			else
			{
				Com_Printf("He's not valid! BURN HIM!\n");
			}
		}
		i++;
	}
}

static void G2_DEBUG_ShovePtrInTracker(CGhoul2Info_v* g2)
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

	if (g2v[0].currentModel && g2v[0].currentModel->name && g2v[0].currentModel->name[0])
	{
		Com_Printf("%s could not be fit into g2 debug instance tracker.\n", g2v[0].currentModel->name);
	}
	else
	{
		Com_Printf("Crap g2 instance passed to instance tracker (in).\n");
	}
}

static void G2_DEBUG_RemovePtrFromTracker(CGhoul2Info_v* g2)
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

	if (g2v[0].currentModel && g2v[0].currentModel->name && g2v[0].currentModel->name[0])
	{
		Com_Printf("%s not in g2 debug instance tracker.\n", g2v[0].currentModel->name);
	}
	else
	{
		Com_Printf("Crap g2 instance passed to instance tracker (out).\n");
	}
}
#endif

// RAGDOLL_BEGIN
void G2API_SetTime(const int current_time, const int clock)
{
	assert(clock >= 0 && clock < NUM_G2T_TIME);
#if G2_DEBUG_TIME
	Com_Printf("Set Time: before c%6d  s%6d", G2TimeBases[1], G2TimeBases[0]);
#endif
	G2TimeBases[clock] = current_time;
	if (G2TimeBases[1] > G2TimeBases[0] + 200)
	{
		G2TimeBases[1] = 0; // use server time instead
		return;
	}
#if G2_DEBUG_TIME
	Com_Printf(" after c%6d  s%6d\n", G2TimeBases[1], G2TimeBases[0]);
#endif
}

int	G2API_GetTime(int argTime) // this may or may not return arg depending on ghoul2_time cvar
{
	int ret = G2TimeBases[1];
	if (!ret)
	{
		ret = G2TimeBases[0];
	}

	return ret;
}
//rww - RAGDOLL_END

static void G2API_AttachInstanceToEntNum(CGhoul2Info_v& ghoul2, int entityNum, qboolean server)
{ //Assign the pointers in the arrays
#ifdef _G2_LISTEN_SERVER_OPT
	if (server)
	{
		ghoul2[0].entityNum = entityNum;
	}
	else
	{
		g2ClientAttachments[entityNum] = &ghoul2;
	}
#endif
}

static void G2API_ClearAttachedInstance(int entityNum)
{
#ifdef _G2_LISTEN_SERVER_OPT
	g2ClientAttachments[entityNum] = NULL;
#endif
}

static void G2API_CleanEntAttachments(void)
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

qboolean G2API_OverrideServerWithClientData(CGhoul2Info_v& ghoul2, int modelIndex)
{
#ifndef _G2_LISTEN_SERVER_OPT
	return qfalse;
#else
	CGhoul2Info* serverInstance = &ghoul2[modelIndex];
	CGhoul2Info* clientInstance;

	if (ri.Cvar_VariableIntegerValue("dedicated"))
	{ //No client to get from!
		return qfalse;
	}

	if (!g2ClientAttachments[serverInstance->entityNum])
	{ //No clientside instance is attached to this entity
		return qfalse;
	}

	CGhoul2Info_v& g2Ref = *g2ClientAttachments[serverInstance->entityNum];
	clientInstance = &g2Ref[0];

	int frameNum = G2API_GetTime(0);

	if (clientInstance->mSkelFrameNum != frameNum)
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
	serverInstance->currentModel = clientInstance->currentModel;
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

void RemoveBoneCache(CBoneCache* boneCache);

static size_t GetSizeOfGhoul2Info(const CGhoul2Info& g2Info)
{
	size_t size = 0;

	// This is pretty ugly, but we don't want to save everything in the CGhoul2Info object.
	size += offsetof(CGhoul2Info, mTransformedVertsArray) - offsetof(CGhoul2Info, mModelindex);

	// Surface vector + size
	size += sizeof(int);
	size += g2Info.mSlist.size() * sizeof(surfaceInfo_t);

	// Bone vector + size
	size += sizeof(int);
	size += g2Info.mBlist.size() * sizeof(boneInfo_t);

	// Bolt vector + size
	size += sizeof(int);
	size += g2Info.mBltlist.size() * sizeof(boltInfo_t);

	return size;
}

static size_t SerializeGhoul2Info(char* buffer, const CGhoul2Info& g2Info)
{
	char* base = buffer;
	size_t blockSize;

	// Oh the ugliness...
	blockSize = offsetof(CGhoul2Info, mTransformedVertsArray) - offsetof(CGhoul2Info, mModelindex);
	memcpy(buffer, &g2Info.mModelindex, blockSize);
	buffer += blockSize;

	// Surfaces vector + size
	*(int*)buffer = g2Info.mSlist.size();
	buffer += sizeof(int);

	blockSize = g2Info.mSlist.size() * sizeof(surfaceInfo_t);
	memcpy(buffer, g2Info.mSlist.data(), g2Info.mSlist.size() * sizeof(surfaceInfo_t));
	buffer += blockSize;

	// Bones vector + size
	*(int*)buffer = g2Info.mBlist.size();
	buffer += sizeof(int);

	blockSize = g2Info.mBlist.size() * sizeof(boneInfo_t);
	memcpy(buffer, g2Info.mBlist.data(), g2Info.mBlist.size() * sizeof(boneInfo_t));
	buffer += blockSize;

	// Bolts vector + size
	*(int*)buffer = g2Info.mBltlist.size();
	buffer += sizeof(int);

	blockSize = g2Info.mBltlist.size() * sizeof(boltInfo_t);
	memcpy(buffer, g2Info.mBltlist.data(), g2Info.mBltlist.size() * sizeof(boltInfo_t));
	buffer += blockSize;

	return static_cast<size_t>(buffer - base);
}

static size_t DeserializeGhoul2Info(const char* buffer, CGhoul2Info& g2Info) // vanilla sp
{
	const char* base = buffer;

	size_t size = offsetof(CGhoul2Info, mTransformedVertsArray) - offsetof(CGhoul2Info, mModelindex);
	memcpy(&g2Info.mModelindex, buffer, size);
	buffer += size;

	// Surfaces vector
	size = *(int*)buffer;
	buffer += sizeof(int);

	g2Info.mSlist.assign((surfaceInfo_t*)buffer, (surfaceInfo_t*)buffer + size);
	buffer += sizeof(surfaceInfo_t) * size;

	// Bones vector
	size = *(int*)buffer;
	buffer += sizeof(int);

	g2Info.mBlist.assign((boneInfo_t*)buffer, (boneInfo_t*)buffer + size);
	buffer += sizeof(boneInfo_t) * size;

	// Bolt vector
	size = *(int*)buffer;
	buffer += sizeof(int);

	g2Info.mBltlist.assign((boltInfo_t*)buffer, (boltInfo_t*)buffer + size);
	buffer += sizeof(boltInfo_t) * size;

	return static_cast<size_t>(buffer - base);
}

class Ghoul2InfoArray : public IGhoul2InfoArray
{
	std::vector<CGhoul2Info> m_infos_[MAX_G2_MODELS];
	int m_ids_[MAX_G2_MODELS];
	std::list<int> m_free_indecies_;
	void DeleteLow(int idx)
	{
		for (size_t model = 0; model < m_infos_[idx].size(); model++)
		{
			if (m_infos_[idx][model].mBoneCache)
			{
				RemoveBoneCache(m_infos_[idx][model].mBoneCache);
				m_infos_[idx][model].mBoneCache = 0;
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
		int i;
		for (i = 0; i < MAX_G2_MODELS; i++)
		{
			m_ids_[i] = MAX_G2_MODELS + i;
			m_free_indecies_.push_back(i);
		}
	}

	size_t GetSerializedSize() const
	{
		size_t size = 0;

		size += sizeof(int); // size of m_free_indecies_ linked list
		size += m_free_indecies_.size() * sizeof(int);

		size += sizeof(m_ids_);

		for (size_t i = 0; i < MAX_G2_MODELS; i++)
		{
			size += sizeof(int); // size of the m_infos_[i] vector

			for (size_t j = 0; j < m_infos_[i].size(); j++)
			{
				size += GetSizeOfGhoul2Info(m_infos_[i][j]);
			}
		}

		return size;
	}

	size_t Serialize(char* buffer) const
	{
		char* base = buffer;

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

	size_t Deserialize(const char* buffer, size_t size)
	{
		const char* base = buffer;
		size_t count;

		// Free indices
		count = *(int*)buffer;
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

	~Ghoul2InfoArray()
	{
		if (m_free_indecies_.size() < MAX_G2_MODELS)
		{
#if G2API_DEBUG
			char mess[1000];
			sprintf(mess, "************************\nLeaked %ld ghoul2info slots\n", MAX_G2_MODELS - m_free_indecies_.size());
			Com_DPrintf(mess);
#endif
			for (int i = 0; i < MAX_G2_MODELS; i++)
			{
				std::list<int>::iterator j;
				for (j = m_free_indecies_.begin(); j != m_free_indecies_.end(); ++j)
				{
					if (*j == i)
						break;
				}
				if (j == m_free_indecies_.end())
				{
#if G2API_DEBUG
					sprintf(mess, "Leaked Info idx=%d id=%d sz=%ld\n", i, m_ids_[i], m_infos_[i].size());
					Com_DPrintf(mess);
					if (m_infos_[i].size())
					{
						sprintf(mess, "%s\n", m_infos_[i][0].mFileName);
						Com_DPrintf(mess);
					}
#endif
					DeleteLow(i);
				}
			}
		}
#if G2API_DEBUG
		else
		{
			Com_DPrintf("No ghoul2 info slots leaked\n");
		}
#endif
	}

	int New()
	{
		if (m_free_indecies_.empty())
		{
			assert(0);
			Com_Error(ERR_FATAL, "Out of ghoul2 info slots");
		}
		// gonna pull from the front, doing a
		int idx = *m_free_indecies_.begin();
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
		assert(handle > 0); //null handle
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
	std::vector<CGhoul2Info>& GetDebug(int handle)
	{
		// Validate handle
		if (handle <= 0)
		{
#ifdef _DEBUG
			Com_Printf("^1ERROR: GetDebug called with invalid handle %d\n", handle);
#endif
			static std::vector<CGhoul2Info> dummy;
			return dummy;
		}

		const int index = (handle & G2_INDEX_MASK);

		// Validate index range
		if (index < 0 || index >= MAX_G2_MODELS)
		{
#ifdef _DEBUG
			Com_Printf("^1ERROR: GetDebug index %d out of range (MAX %d)\n",
				index, MAX_G2_MODELS);
#endif
			static std::vector<CGhoul2Info> dummy;
			return dummy;
		}

		// Validate handle matches stored ID
		if (m_ids_[index] != handle)
		{
#ifdef _DEBUG
			Com_Printf("^1ERROR: GetDebug handle mismatch: got %d, expected %d\n",
				handle, m_ids_[index]);
#endif
			static std::vector<CGhoul2Info> dummy;
			return dummy;
		}

		// All good
		return m_infos_[index];
	}
	void TestAllAnims()
	{
		int j;
		for (j = 0; j < MAX_G2_MODELS; j++)
		{
			vector<CGhoul2Info>& ghoul2 = m_infos_[j];
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

static Ghoul2InfoArray* singleton = NULL;
IGhoul2InfoArray& TheGhoul2InfoArray()
{
	if (!singleton) {
		singleton = new Ghoul2InfoArray;
	}
	return *singleton;
}

#if G2API_DEBUG
std::vector<CGhoul2Info>& DebugG2Info(int handle)
{
	return ((Ghoul2InfoArray*)(&TheGhoul2InfoArray()))->GetDebug(handle);
}

static CGhoul2Info& DebugG2InfoI(int handle, int item)
{
	return ((Ghoul2InfoArray*)(&TheGhoul2InfoArray()))->GetDebug(handle)[item];
}

static void TestAllGhoul2Anims()
{
	((Ghoul2InfoArray*)(&TheGhoul2InfoArray()))->TestAllAnims();
}
#endif

#define PERSISTENT_G2DATA "g2infoarray"

void RestoreGhoul2InfoArray()
{
	if (singleton == nullptr)
	{
		// Create the ghoul2 info array
		TheGhoul2InfoArray();

		size_t size;
		const void* data = ri.PD_Load(PERSISTENT_G2DATA, &size);
		if (data == nullptr)
		{
			return;
		}

#ifdef _DEBUG
		size_t read =
#endif // _DEBUG
			singleton->Deserialize((const char*)data, size);
		R_Free((void*)data);
#ifdef _DEBUG
		assert(read == size);
#endif
	}
}

void SaveGhoul2InfoArray()
{
	size_t size = singleton->GetSerializedSize();
	void* data = R_Malloc(size, TAG_GHOUL2, qfalse);
	size_t written = singleton->Serialize((char*)data);
#ifdef _DEBUG
	assert(written == size);
#endif // _DEBUG

	if (!ri.PD_Store(PERSISTENT_G2DATA, data, size))
	{
		Com_Printf(S_COLOR_RED "ERROR: Failed to store persistent renderer data.\n");
	}
}

static void Ghoul2InfoArray_Free(void)
{
	if (singleton) {
		delete singleton;
		singleton = NULL;
	}
}

// this is the ONLY function to read entity states directly
void G2API_CleanGhoul2Models(CGhoul2Info_v& ghoul2)
{
#ifdef _G2_GORE
	G2API_ClearSkinGore(ghoul2);
#endif
	ghoul2.~CGhoul2Info_v();
}

qhandle_t G2API_PrecacheGhoul2Model(const char* fileName)
{
	return RE_RegisterModel(fileName);
}

// initialise all that needs to be on a new Ghoul II model
int G2API_InitGhoul2Model(CGhoul2Info_v& ghoul2, const char* fileName, const int modelIndex, const qhandle_t customSkin, const qhandle_t customShader, const int model_flags, const int lod_bias)
{
	int model;

	G2ERROR(fileName && fileName[0], "NULL filename");

	if (!fileName || !fileName[0])
	{
		assert(fileName[0]);
		return -1;
	}

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
	{
		assert(model < 8); //arb, just catching run-away models
		CGhoul2Info info;
		Q_strncpyz(info.mFileName, fileName, sizeof info.mFileName);
		info.mModelindex = 0;
		if (G2_TestModelPointers(&info))
		{
			ghoul2.push_back(CGhoul2Info());
		}
		else
		{
			return -1;
		}
	}

	Q_strncpyz(ghoul2[model].mFileName, fileName, sizeof ghoul2[model].mFileName);
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
		ghoul2[model].mLodBias = lod_bias;
		ghoul2[model].mAnimFrameDefault = 0;
		ghoul2[model].mFlags = 0;

		ghoul2[model].mModelBoltLink = -1;
	}
	return ghoul2[model].mModelindex;
}

qboolean G2API_SetLodBias(CGhoul2Info* ghlInfo, const int lod_bias)
{
	G2ERROR(ghlInfo, "G2API_SetLodBias: NULL ghlInfo");
	if (G2_SetupModelPointers(ghlInfo))
	{
		ghlInfo->mLodBias = lod_bias;
		return qtrue;
	}
	return qfalse;
}

qboolean G2API_SetSkin(CGhoul2Info* ghlInfo, const qhandle_t customSkin, const qhandle_t render_skin)
{
	G2ERROR(ghlInfo, "G2API_SetSkin: NULL ghlInfo");
	if (G2_SetupModelPointers(ghlInfo))
	{
		ghlInfo->mCustomSkin = customSkin;
#ifndef JK2_MODE
		if (render_skin)
		{
			// this is going to set the surfs on/off matching the skin file
			// header moved to tr_local.h, implemented in G2_surface.cpp
			G2_SetSurfaceOnOffFromSkin(ghlInfo, render_skin);
		}
#endif
		return qtrue;
	}
	return qfalse;
}

qboolean G2API_SetShader(CGhoul2Info* ghlInfo, const qhandle_t customShader)
{
	G2ERROR(ghlInfo, "G2API_SetShader: NULL ghlInfo");
	if (ghlInfo)
	{
		ghlInfo->mCustomShader = customShader;
		return qtrue;
	}
	return qfalse;
}

qboolean G2API_SetSurfaceOnOff(CGhoul2Info* ghlInfo, const char* surfaceName, const int flags)
{
	G2ERROR(ghlInfo, "G2API_SetSurfaceOnOff: NULL ghlInfo");
	if (G2_SetupModelPointers(ghlInfo))
	{
		G2ERROR(!(flags & ~(G2SURFACEFLAG_OFF | G2SURFACEFLAG_NODESCENDANTS)), "G2API_SetSurfaceOnOff Illegal Flags");
		// ensure we flush the cache
		ghlInfo->mMeshFrameNum = 0;
		return G2_SetSurfaceOnOff(ghlInfo, surfaceName, flags);
	}
	return qfalse;
}

qboolean G2API_SetRootSurface(CGhoul2Info_v& ghlInfo, const int modelIndex, const char* surfaceName)
{
	G2ERROR(ghlInfo.IsValid(), "Invalid ghlInfo");
	G2ERROR(surfaceName, "Invalid surfaceName");
	if (G2_SetupModelPointers(ghlInfo))
	{
		G2ERROR(modelIndex >= 0 && modelIndex < ghlInfo.size(), "Bad Model Index");
		if (modelIndex >= 0 && modelIndex < ghlInfo.size())
		{
			return G2_SetRootSurface(ghlInfo, modelIndex, surfaceName);
		}
	}
	return qfalse;
}

int G2API_AddSurface(CGhoul2Info* ghlInfo, const int surface_number, const int poly_number, const float barycentric_i, const float barycentric_j, const int lod)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		// ensure we flush the cache
		ghlInfo->mMeshFrameNum = 0;
		return G2_AddSurface(ghlInfo, surface_number, poly_number, barycentric_i, barycentric_j, lod);
	}
	return -1;
}

qboolean G2API_RemoveSurface(CGhoul2Info* ghlInfo, const int index)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		// ensure we flush the cache
		ghlInfo->mMeshFrameNum = 0;
		return G2_RemoveSurface(ghlInfo->mSlist, index);
	}
	return qfalse;
}

int G2API_GetParentSurface(CGhoul2Info* ghlInfo, const int index)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		return G2_GetParentSurface(ghlInfo, index);
	}
	return -1;
}

int G2API_GetSurfaceRenderStatus(CGhoul2Info* ghlInfo, const char* surfaceName)
{
	G2ERROR(surfaceName, "G2API_GetSurfaceRenderStatus: Invalid surfaceName");
	if (G2_SetupModelPointers(ghlInfo))
	{
		return G2_IsSurfaceRendered(ghlInfo, surfaceName, ghlInfo->mSlist);
	}
	return -1;
}

qboolean G2API_RemoveGhoul2Model(CGhoul2Info_v& ghlInfo, const int modelIndex)
{
	// sanity check
	if (!ghlInfo.size() || (ghlInfo.size() <= modelIndex) || modelIndex < 0 || (ghlInfo[modelIndex].mModelindex < 0))
	{
		// if we hit this assert then we are trying to delete a ghoul2 model on a ghoul2 instance that
		// one way or another is already gone.
		assert(0);
		return qfalse;
	}

	if (modelIndex < ghlInfo.size())
	{
#ifdef _G2_GORE
		// Cleanup the gore attached to this model
		if (ghlInfo[modelIndex].mGoreSetTag)
		{
			DeleteGoreSet(ghlInfo[modelIndex].mGoreSetTag);
			ghlInfo[modelIndex].mGoreSetTag = 0;
		}
#endif

		if (ghlInfo[modelIndex].mBoneCache)
		{
			RemoveBoneCache(ghlInfo[modelIndex].mBoneCache);
			ghlInfo[modelIndex].mBoneCache = 0;
		}

		// clear out the vectors this model used.
		ghlInfo[modelIndex].mBlist.clear();
		ghlInfo[modelIndex].mBltlist.clear();
		ghlInfo[modelIndex].mSlist.clear();

		// set us to be the 'not active' state
		ghlInfo[modelIndex].mModelindex = -1;
		ghlInfo[modelIndex].mFileName[0] = 0;
		ghlInfo[modelIndex] = CGhoul2Info();

		int newSize = ghlInfo.size();

		// ! the part below doesn't exist in SP Vanilla, but it makes sence !

		// now look through the list from the back and see if there is a block of -1's we can resize off the end of the list
		for (int i = ghlInfo.size() - 1; i > -1; i--)
		{
			if (ghlInfo[i].mModelindex == -1)
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
		if (newSize != ghlInfo.size())
		{
			// yes, so lets do it
			ghlInfo.resize(newSize);
		}
	}

	return qtrue;
}

int G2API_GetAnimIndex(const CGhoul2Info* ghlInfo)
{
	return (ghlInfo)
		? ghlInfo->animModelIndexOffset
		: 0;
}

qboolean G2API_SetAnimIndex(CGhoul2Info* ghlInfo, const int index)
{
	// Is This A Valid G2 Model?
	if (ghlInfo)
	{
		// Is This A New Anim Index?
		if (ghlInfo->animModelIndexOffset != index)
		{
			ghlInfo->animModelIndexOffset = index;
			// Clear anim size so SetupModelPointers recalcs
			ghlInfo->currentAnimModelSize = 0;

			// Kill All Existing Animation, Blending, Etc.
			//---------------------------------------------
			for (size_t index = 0; index < ghlInfo->mBlist.size(); index++)
			{
				ghlInfo->mBlist[index].flags &= ~(BONE_ANIM_TOTAL);
				ghlInfo->mBlist[index].flags &= ~(BONE_ANGLES_TOTAL);
			}
		}
		return qtrue;
	}
	return qfalse;
}

//check if a bone exists on skeleton without actually adding to the bone list -rww
static qboolean G2API_DoesBoneExist(CGhoul2Info_v& ghoul2, const int modelIndex, const char* boneName)
{
	CGhoul2Info* ghlInfo = &ghoul2[modelIndex];

	if (G2_SetupModelPointers(ghlInfo))
	{ //model is valid
		mdxaHeader_t* mdxa = ghlInfo->currentModel->data.gla;
		if (mdxa)
		{ //get the skeleton data and iterate through the bones
			int i;
			mdxaSkel_t* skel;
			mdxaSkelOffsets_t* offsets;

			offsets = (mdxaSkelOffsets_t*)((byte*)mdxa + sizeof(mdxaHeader_t));

			for (i = 0; i < mdxa->numBones; i++)
			{
				skel = (mdxaSkel_t*)((byte*)mdxa + sizeof(mdxaHeader_t) + offsets->offsets[i]);
				if (!Q_stricmp(skel->name, boneName))
				{ //got it
					return qtrue;
				}
			}
		}
	}

	//guess it doesn't exist
	return qfalse;
}

qboolean G2API_SetBoneAnimIndex(CGhoul2Info* ghlInfo, const int index, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int acurrent_time, const float set_frame, const int blend_time)
{
	//rww - RAGDOLL_BEGIN
	if (ghlInfo && ghlInfo->mFlags & GHOUL2_RAG_STARTED)
	{
		return qfalse;
	}
	//rww - RAGDOLL_END

	qboolean ret = qfalse;
	if (G2_SetupModelPointers(ghlInfo))
	{
		G2ERROR(startFrame >= 0, "startframe<0");
		G2ERROR(startFrame < ghlInfo->aHeader->numFrames, "startframe>=numframes");
		G2ERROR(endFrame > 0, "endframe<=0");
		G2ERROR(endFrame <= ghlInfo->aHeader->numFrames, "endframe>numframes");
		G2ERROR(set_frame < ghlInfo->aHeader->numFrames, "setframe>=numframes");
		G2ERROR(set_frame == -1.0f || set_frame >= 0.0f, "setframe<0 but not -1");
		if (startFrame < 0 || startFrame >= ghlInfo->aHeader->numFrames)
		{
			*const_cast<int*>(&startFrame) = 0; // cast away const
		}
		if (endFrame <= 0 || endFrame > ghlInfo->aHeader->numFrames)
		{
			*const_cast<int*>(&endFrame) = 1;
		}
		if (set_frame != -1.0f && (set_frame < 0.0f || set_frame >= static_cast<float>(ghlInfo->aHeader->numFrames)))
		{
			*const_cast<float*>(&set_frame) = 0.0f;
		}
		ghlInfo->mSkelFrameNum = 0;
		G2ERROR(index >= 0 && index < (int)ghlInfo->mBlist.size(),
			va("Out of Range Bone Index (%s)", ghlInfo->mFileName));
		if (index >= 0 && index < static_cast<int>(ghlInfo->mBlist.size()))
		{
			G2ERROR(ghlInfo->mBlist[index].boneNumber >= 0, va("Bone Index is not Active (%s)", ghlInfo->mFileName));
			const int current_time = G2API_GetTime(acurrent_time);
#if 0
			/*
			if ( ge->ValidateAnimRange( startFrame, endFrame, animSpeed ) == -1 )
			{
				int wtf = 1;
			}
			*/
#endif
			ret = G2_Set_Bone_Anim_Index(ghlInfo->mBlist, index, startFrame, endFrame, flags, animSpeed, current_time,
				set_frame, blend_time, ghlInfo->aHeader->numFrames);
			G2ANIM(ghlInfo, "G2API_SetBoneAnimIndex");
		}
	}
	G2WARNING(ret, va("G2API_SetBoneAnimIndex Failed (%s)", ghlInfo->mFileName));
	return ret;
}

qboolean G2API_SetBoneAnim(CGhoul2Info* ghlInfo, const char* boneName, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int acurrent_time, const float set_frame, const int blend_time)
{
	if (ghlInfo && ghlInfo->mFlags & GHOUL2_RAG_STARTED)
	{
		return qfalse;
	}

	qboolean ret = qfalse;
	G2ERROR(boneName, "NULL boneName");
	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		G2ERROR(startFrame >= 0, "startframe<0");
		G2ERROR(startFrame < ghlInfo->aHeader->numFrames, "startframe>=numframes");
		G2ERROR(endFrame > 0, "endframe<=0");
		G2ERROR(endFrame <= ghlInfo->aHeader->numFrames, "endframe>numframes");
		G2ERROR(set_frame < ghlInfo->aHeader->numFrames, "setframe>=numframes");
		G2ERROR(set_frame == -1.0f || set_frame >= 0.0f, "setframe<0 but not -1");
		if (startFrame < 0 || startFrame >= ghlInfo->aHeader->numFrames)
		{
			*const_cast<int*>(&startFrame) = 0; // cast away const
		}
		if (endFrame <= 0 || endFrame > ghlInfo->aHeader->numFrames)
		{
			*const_cast<int*>(&endFrame) = 1;
		}
		if (set_frame != -1.0f && (set_frame < 0.0f || set_frame >= static_cast<float>(ghlInfo->aHeader->numFrames)))
		{
			*const_cast<float*>(&set_frame) = 0.0f;
		}
		ghlInfo->mSkelFrameNum = 0;
		const int current_time = G2API_GetTime(acurrent_time);
		ret = G2_Set_Bone_Anim(ghlInfo, ghlInfo->mBlist, boneName, startFrame, endFrame, flags, animSpeed, current_time, set_frame, blend_time);
		G2ANIM(ghlInfo, "G2API_SetBoneAnim");
	}
	G2WARNING(ret, "G2API_SetBoneAnim Failed");
	return ret;
}

qboolean G2API_GetBoneAnim(CGhoul2Info* ghlInfo, const char* boneName, const int acurrent_time, float* current_frame, int* startFrame, int* endFrame, int* flags, float* animSpeed, qhandle_t* model_list)
{
	G2ERROR(boneName, "G2API_GetBoneAnim: NULL boneName");

	assert(startFrame != endFrame); //this is bad
	assert(startFrame != flags); //this is bad
	assert(endFrame != flags); //this is bad
	assert(current_frame != animSpeed); //this is bad

	if (G2_SetupModelPointers(ghlInfo))
	{
		int a_CurrentTime = G2API_GetTime(acurrent_time);
#ifdef _DEBUG

		if (*endFrame < 1)
		{
			*endFrame = 1;
		}
		if (*endFrame > 100000)
		{
			*endFrame = 1;
		}
		if (*startFrame < 0)
		{
			*startFrame = 0;
		}
		if (*startFrame > 100000)
		{
			*startFrame = 1;
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

		qboolean ret = G2_Get_Bone_Anim(ghlInfo, ghlInfo->mBlist, boneName, a_CurrentTime, current_frame, startFrame, endFrame, flags, animSpeed);
		return ret;
	}

	G2WARNING(ret, "G2API_GetBoneAnim Failed");
	return qfalse;
}

qboolean G2API_GetBoneAnimIndex(CGhoul2Info* ghlInfo, const int iBoneIndex, const int acurrent_time, float* current_frame, int* startFrame, int* endFrame, int* flags, float* animSpeed, qhandle_t* model_list)
{
	qboolean ret = qfalse;
	if (G2_SetupModelPointers(ghlInfo))
	{
		const int current_time = G2API_GetTime(acurrent_time);
		G2NOTE(iBoneIndex >= 0 && iBoneIndex < (int)ghlInfo->mBlist.size(), va("Bad Bone Index (%d:%s)", iBoneIndex, ghlInfo->mFileName));

		if (iBoneIndex >= 0 && iBoneIndex < static_cast<int>(ghlInfo->mBlist.size()))
		{
			G2NOTE(ghlInfo->mBlist[iBoneIndex].flags & (BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE),
				"GetBoneAnim on non-animating bone.");
			if (ghlInfo->mBlist[iBoneIndex].flags & (BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE))
			{
				int sf, ef;
				ret = G2_Get_Bone_Anim_Index(ghlInfo->mBlist, // boneInfo_v &blist,
					iBoneIndex, // const int index,
					current_time, // const int current_time,
					current_frame, // float *current_frame,
					&sf, // int *startFrame,
					&ef, // int *endFrame,
					flags, // int *flags,
					animSpeed, // float *retAnimSpeed,
					ghlInfo->aHeader->numFrames
				);
				G2ERROR(sf >= 0, "returning startframe<0");
				G2ERROR(sf < ghlInfo->aHeader->numFrames, "returning startframe>=numframes");
				G2ERROR(ef > 0, "returning endframe<=0");
				G2ERROR(ef <= ghlInfo->aHeader->numFrames, "returning endframe>numframes");
				if (current_frame)
				{
					G2ERROR(*current_frame >= 0.0f, "returning currentframe<0");
					G2ERROR(((int)(*current_frame)) < ghlInfo->aHeader->numFrames, "returning currentframe>=numframes");
				}
				if (endFrame)
				{
					*endFrame = ef;
				}
				if (startFrame)
				{
					*startFrame = sf;
				}
				G2ANIM(ghlInfo, "G2API_GetBoneAnimIndex");
			}
		}
	}
	if (!ret)
	{
		if (endFrame) *endFrame = 1;
		if (startFrame) *startFrame = 0;
		if (flags) *flags = 0;
		if (current_frame) *current_frame = 0.0f;
		if (animSpeed) *animSpeed = 1.0f;
	}
	G2NOTE(ret, "G2API_GetBoneAnimIndex Failed");
	return ret;
}

qboolean G2API_GetAnimRange(CGhoul2Info* ghlInfo, const char* boneName, int* startFrame, int* endFrame)
{
	G2ERROR(boneName, "G2API_GetAnimRange: NULL boneName");
	assert(startFrame != endFrame); //this is bad

	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		qboolean ret = G2_Get_Bone_Anim_Range(ghlInfo, ghlInfo->mBlist, boneName, startFrame, endFrame);
		G2ANIM(ghlInfo, "G2API_GetAnimRange");
#ifdef _DEBUG
		assert(*endFrame > 0);
		assert(*endFrame < 100000);
		assert(*startFrame >= 0);
		assert(*startFrame < 100000);
#endif
		return ret;
	}
	return qfalse;
}

qboolean G2API_GetAnimRangeIndex(CGhoul2Info* ghlInfo, const int bone_index, int* startFrame, int* endFrame)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		G2ERROR(bone_index >= 0 && bone_index < (int)ghlInfo->mBlist.size(), "Bad Bone Index");
		if (bone_index >= 0 && bone_index < (int)ghlInfo->mBlist.size())
		{
			return G2_Get_Bone_Anim_Range_Index(ghlInfo->mBlist, bone_index, startFrame, endFrame);
		}
	}
	return qfalse;
}

qboolean G2API_PauseBoneAnim(CGhoul2Info* ghlInfo, const char* boneName, const int acurrent_time)
{
	qboolean ret = qfalse;
	G2ERROR(boneName, "NULL boneName");
	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		const int current_time = G2API_GetTime(acurrent_time);
		ret = G2_Pause_Bone_Anim(ghlInfo, ghlInfo->mBlist, boneName, current_time);
		G2ANIM(ghlInfo, "G2API_PauseBoneAnim");
	}
	G2NOTE(ret, "G2API_PauseBoneAnim Failed");
	return ret;
}

qboolean G2API_PauseBoneAnimIndex(CGhoul2Info* ghlInfo, const int bone_index, const int acurrent_time)
{
	qboolean ret = qfalse;
	if (G2_SetupModelPointers(ghlInfo))
	{
		const int current_time = G2API_GetTime(acurrent_time);
		G2ERROR(bone_index >= 0 && bone_index < (int)ghlInfo->mBlist.size(), "Bad Bone Index");
		if (bone_index >= 0 && bone_index < static_cast<int>(ghlInfo->mBlist.size()))
		{
			ret = G2_Pause_Bone_Anim_Index(ghlInfo->mBlist, bone_index, current_time, ghlInfo->aHeader->numFrames);
			G2ANIM(ghlInfo, "G2API_PauseBoneAnimIndex");
		}
	}
	G2WARNING(ret, "G2API_PauseBoneAnimIndex Failed");
	return ret;
}

qboolean G2API_IsPaused(CGhoul2Info* ghlInfo, const char* boneName)
{
	qboolean ret = qfalse;
	G2ERROR(boneName, "NULL boneName");
	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		ret = G2_IsPaused(ghlInfo, ghlInfo->mBlist, boneName);
	}
	G2WARNING(ret, "G2API_IsPaused Failed");
	return ret;
}

qboolean G2API_StopBoneAnimIndex(CGhoul2Info* ghlInfo, const int index)
{
	qboolean ret = qfalse;
	G2ERROR(ghlInfo, "NULL ghlInfo");
	if (G2_SetupModelPointers(ghlInfo))
	{
		G2ERROR(index >= 0 && index < (int)ghlInfo->mBlist.size(), "Bad Bone Index");
		if (index >= 0 && index < static_cast<int>(ghlInfo->mBlist.size()))
		{
			ret = G2_Stop_Bone_Anim_Index(ghlInfo->mBlist, index);
			G2ANIM(ghlInfo, "G2API_StopBoneAnimIndex");
		}
	}
	return ret;
}

qboolean G2API_StopBoneAnim(CGhoul2Info* ghlInfo, const char* boneName)
{
	qboolean ret = qfalse;
	G2ERROR(boneName, "NULL boneName");
	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		ret = G2_Stop_Bone_Anim(ghlInfo, ghlInfo->mBlist, boneName);
		G2ANIM(ghlInfo, "G2API_StopBoneAnim");
	}
	G2WARNING(ret, "G2API_StopBoneAnim Failed");
	return ret;
}

static qboolean G2API_SetBoneAnglesOffsetIndex(CGhoul2Info* ghlInfo, const int index, const vec3_t angles, const int flags, const Eorientations yaw, const Eorientations pitch, const Eorientations roll, qhandle_t* model_list, const int blend_time, const int acurrent_time, const vec3_t offset)
{
	//rww - RAGDOLL_BEGIN
	if (ghlInfo && ghlInfo->mFlags & GHOUL2_RAG_STARTED)
	{
		return qfalse;
	}
	//rww - RAGDOLL_END

	qboolean ret = qfalse;
	if (G2_SetupModelPointers(ghlInfo))
	{
		const int current_time = G2API_GetTime(acurrent_time);
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		G2ERROR(index >= 0 && index < (int)ghlInfo->mBlist.size(), "G2API_SetBoneAnglesIndex:Invalid bone index");
		if (index >= 0 && index < static_cast<int>(ghlInfo->mBlist.size()))
		{
			ret = G2_Set_Bone_Angles_Index(ghlInfo, ghlInfo->mBlist, index, angles, flags, yaw, pitch, roll, blend_time, current_time, offset);
		}
	}
	G2WARNING(ret, "G2API_SetBoneAnglesIndex Failed");
	return ret;
}

qboolean G2API_SetBoneAnglesIndex(CGhoul2Info* ghlInfo, const int index, const vec3_t angles, const int flags, const Eorientations yaw, const Eorientations pitch, const Eorientations roll, qhandle_t* model_list, const int blend_time, const int acurrent_time)
{
	return G2API_SetBoneAnglesOffsetIndex(ghlInfo, index, angles, flags, yaw, pitch, roll, 0, blend_time, acurrent_time, nullptr);
}

qboolean G2API_SetBoneAnglesOffset(CGhoul2Info* ghlInfo, const char* boneName, const vec3_t angles, const int flags, const Eorientations up, const Eorientations left, const Eorientations forward, qhandle_t* model_list, const int blend_time, const int acurrent_time, const vec3_t offset)
{
	//rww - RAGDOLL_BEGIN
	if (ghlInfo && ghlInfo->mFlags & GHOUL2_RAG_STARTED)
	{
		return qfalse;
	}
	//rww - RAGDOLL_END

	qboolean ret = qfalse;
	G2ERROR(boneName, "NULL boneName");
	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		const int current_time = G2API_GetTime(acurrent_time);
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		ret = G2_Set_Bone_Angles(ghlInfo, ghlInfo->mBlist, boneName, angles, flags, up, left, forward, blend_time, current_time, offset);
	}
	G2WARNING(ret, "G2API_SetBoneAngles Failed");
	return ret;
}

qboolean G2API_SetBoneAngles(CGhoul2Info* ghlInfo, const char* boneName, const vec3_t angles, const int flags, const Eorientations up, const Eorientations left, const Eorientations forward, qhandle_t* model_list, const int blend_time, const int acurrent_time)
{
	return G2API_SetBoneAnglesOffset(ghlInfo, boneName, angles, flags, up, left, forward, nullptr, blend_time, acurrent_time, nullptr);
}

qboolean G2API_SetBoneAnglesMatrixIndex(CGhoul2Info* ghlInfo, const int index, const mdxaBone_t& matrix, const int flags, qhandle_t* model_list, const int blend_time, const int acurrent_time)
{
	qboolean ret = qfalse;

	if (G2_SetupModelPointers(ghlInfo))
	{
		const int current_time = G2API_GetTime(acurrent_time);
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		G2ERROR(index >= 0 && index < (int)ghlInfo->mBlist.size(), "Bad Bone Index");
		if (index >= 0 && index < static_cast<int>(ghlInfo->mBlist.size()))
		{
			ret = G2_Set_Bone_Angles_Matrix_Index(ghlInfo->mBlist, index, matrix, flags, blend_time, current_time);
		}
	}
	G2WARNING(ret, "G2API_SetBoneAnglesMatrixIndex Failed");
	return ret;
}

qboolean G2API_SetBoneAnglesMatrix(CGhoul2Info* ghlInfo, const char* boneName, const mdxaBone_t& matrix, const int flags, qhandle_t* model_list, const int blend_time, const int acurrent_time)
{
	qboolean ret = qfalse;
	G2ERROR(boneName, "NULL boneName");
	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		const int current_time = G2API_GetTime(acurrent_time);
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		ret = G2_Set_Bone_Angles_Matrix(ghlInfo, ghlInfo->mBlist, boneName, matrix, flags, blend_time, current_time);
	}
	G2WARNING(ret, "G2API_SetBoneAnglesMatrix Failed");
	return ret;
}

qboolean G2API_StopBoneAnglesIndex(CGhoul2Info* ghlInfo, const int index)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		G2ERROR(index >= 0 && index < (int)ghlInfo->mBlist.size(), "Bad Bone Index");
		if (index >= 0 && index < (int)ghlInfo->mBlist.size())
		{
			return G2_Stop_Bone_Angles_Index(ghlInfo->mBlist, index);
		}
	}
	return qfalse;
}

qboolean G2API_StopBoneAngles(CGhoul2Info* ghlInfo, const char* boneName)
{
	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		return G2_Stop_Bone_Angles(
			ghlInfo, ghlInfo->mBlist, boneName);
	}
	return qfalse;
}

static void G2API_AbsurdSmoothing(CGhoul2Info_v& ghoul2, qboolean status)
{
	assert(ghoul2.size());
	CGhoul2Info* ghlInfo = &ghoul2[0];
#ifndef REND2_SP
	if (status)
	{ //turn it on
		ghlInfo->mFlags |= GHOUL2_CRAZY_SMOOTH;
	}
	else
	{ //off
		ghlInfo->mFlags &= ~GHOUL2_CRAZY_SMOOTH;
	}
#endif
}

//rww - RAGDOLL_BEGIN
class CRagDollParams;
void G2_SetRagDoll(CGhoul2Info_v& ghoul2_v, CRagDollParams* parms);
void G2API_SetRagDoll(CGhoul2Info_v& ghoul2, CRagDollParams* parms)
{
	G2_SetRagDoll(ghoul2, parms);
}

void G2_ResetRagDoll(CGhoul2Info_v& ghoul2_v);
static void G2API_ResetRagDoll(CGhoul2Info_v& ghoul2)
{
	G2_ResetRagDoll(ghoul2);
}
//rww - RAGDOLL_END

qboolean G2API_RemoveBone(CGhoul2Info* ghlInfo, const char* boneName)
{
	G2ERROR(boneName, "G2API_RemoveBone: NULL boneName");

	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		return G2_Remove_Bone(ghlInfo, ghlInfo->mBlist, boneName);
	}
	return qfalse;
}

// This is G2API_AnimateG2Models
void G2API_AnimateG2ModelsRag(CGhoul2Info_v& ghoul2, const int acurrent_time, CRagDollUpdateParams* params)
{
	int model;
	int current_time = G2API_GetTime(acurrent_time);

	// Walk the list and find all models that are active
	for (model = 0; model < ghoul2.size(); model++)
	{
		if (ghoul2[model].mModel)
		{
			G2_Animate_Bone_List(ghoul2, current_time, model, params);
		}
	}
}
// rww - RAGDOLL_END

int G2_Find_Bone_Rag(const CGhoul2Info* ghlInfo, const boneInfo_v& blist, const char* boneName);
#define RAG_PCJ						(0x00001)
#define RAG_EFFECTOR				(0x00100)

static boneInfo_t* G2_GetRagBoneConveniently(CGhoul2Info_v& ghoul2, const char* boneName)
{
	assert(ghoul2.size());
	CGhoul2Info* ghlInfo = &ghoul2[0];

	if (!(ghlInfo->mFlags & GHOUL2_RAG_STARTED))
	{ // can't do this if not in ragdoll
		return NULL;
	}

	int bone_index = G2_Find_Bone_Rag(ghlInfo, ghlInfo->mBlist, boneName);

	if (bone_index < 0)
	{ // bad bone specification
		return NULL;
	}

	boneInfo_t* bone = &ghlInfo->mBlist[bone_index];

	if (!(bone->flags & BONE_ANGLES_RAGDOLL))
	{ // only want to return rag bones
		return NULL;
	}

	return bone;
}

qboolean G2API_RagPCJConstraint(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t min, vec3_t max)
{
	boneInfo_t* bone = G2_GetRagBoneConveniently(ghoul2, boneName);

	if (!bone)
	{
		return qfalse;
	}

	if (!(bone->RagFlags & RAG_PCJ))
	{ // this function is only for PCJ bones
		return qfalse;
	}

	VectorCopy(min, bone->minAngles);
	VectorCopy(max, bone->maxAngles);

	return qtrue;
}

qboolean G2API_RagPCJGradientSpeed(CGhoul2Info_v& ghoul2, const char* boneName, const float speed)
{
	boneInfo_t* bone = G2_GetRagBoneConveniently(ghoul2, boneName);

	if (!bone)
	{
		return qfalse;
	}

	if (!(bone->RagFlags & RAG_PCJ))
	{ // this function is only for PCJ bones
		return qfalse;
	}

	bone->overGradSpeed = speed;

	return qtrue;
}

qboolean G2API_RagEffectorGoal(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t pos)
{
	boneInfo_t* bone = G2_GetRagBoneConveniently(ghoul2, boneName);

	if (!bone)
	{
		return qfalse;
	}

	if (!(bone->RagFlags & RAG_EFFECTOR))
	{ // this function is only for effectors
		return qfalse;
	}

	if (!pos)
	{ // go back to none in case we have one then
		bone->hasOverGoal = false;
	}
	else
	{
		VectorCopy(pos, bone->overGoalSpot);
		bone->hasOverGoal = true;
	}
	return qtrue;
}

qboolean G2API_GetRagBonePos(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t pos, vec3_t entAngles, vec3_t ent_pos, vec3_t entScale)
{
	return qfalse;
}

qboolean G2API_RagEffectorKick(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t velocity)
{
	boneInfo_t* bone = G2_GetRagBoneConveniently(ghoul2, boneName);

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

qboolean G2API_RagForceSolve(CGhoul2Info_v& ghoul2, const qboolean force)
{
	assert(ghoul2.size());
	CGhoul2Info* ghlInfo = &ghoul2[0];

	if (!(ghlInfo->mFlags & GHOUL2_RAG_STARTED))
	{ //can't do this if not in ragdoll
		return qfalse;
	}

	if (force)
	{
		ghlInfo->mFlags |= GHOUL2_RAG_FORCESOLVE;
	}
	else
	{
		ghlInfo->mFlags &= ~GHOUL2_RAG_FORCESOLVE;
	}

	return qtrue;
}

qboolean G2_SetBoneIKState(CGhoul2Info_v& ghoul2, const int time, const char* boneName, const int ikState, sharedSetBoneIKStateParams_t* params);

qboolean G2API_SetBoneIKState(CGhoul2Info_v& ghoul2, int time, const char* boneName, int ikState, sharedSetBoneIKStateParams_t* params)
{
	return G2_SetBoneIKState(ghoul2, time, boneName, ikState, params);
}

qboolean G2_IKMove(CGhoul2Info_v& ghoul2, int time, sharedIKMoveParams_t* params);

qboolean G2API_IKMove(CGhoul2Info_v& ghoul2, int time, sharedIKMoveParams_t* params)
{
	return G2_IKMove(ghoul2, time, params);
}

qboolean G2API_RemoveBolt(CGhoul2Info* ghlInfo, const int index)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		return G2_Remove_Bolt(ghlInfo->mBltlist, index);
	}
	return qfalse;
}

int G2API_AddBolt(CGhoul2Info* ghlInfo, const char* boneName)
{
	//	assert(ghoul2.size()>modelIndex);
	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		return G2_Add_Bolt(ghlInfo, ghlInfo->mBltlist, ghlInfo->mSlist, boneName);
	}
	return -1;
}

int G2API_AddBoltSurfNum(CGhoul2Info* ghlInfo, const int surf_index)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		return G2_Add_Bolt_Surf_Num(
			ghlInfo, ghlInfo->mBltlist, ghlInfo->mSlist, surf_index);
	}
	return -1;
}

qboolean G2API_AttachG2Model(CGhoul2Info* ghlInfo, CGhoul2Info* ghlInfoTo, int toBoltIndex, int to_model)
{
	qboolean ret = qfalse;

	if (G2_SetupModelPointers(ghlInfo) && G2_SetupModelPointers(ghlInfoTo))
	{
		G2ERROR(toBoltIndex >= 0 && toBoltIndex < (int)ghlInfoTo->mBltlist.size(), "Invalid Bolt Index");
		G2ERROR(ghlInfoTo->mBltlist.size() > 0, "Empty Bolt List");

		assert(toBoltIndex >= 0);

		if (toBoltIndex >= 0 && ghlInfoTo->mBltlist.size())
		{
			// make sure we have a model to attach, a model to attach to, and a bolt on that model
			if (((ghlInfoTo->mBltlist[toBoltIndex].boneNumber != -1) || (ghlInfoTo->mBltlist[toBoltIndex].surface_number != -1)))
			{
				// encode the bolt address into the model bolt link
				to_model &= MODEL_AND;
				toBoltIndex &= BOLT_AND;
				ghlInfo->mModelBoltLink = (to_model << MODEL_SHIFT) | (toBoltIndex << BOLT_SHIFT);
				ret = qtrue;
			}
		}
	}
	G2WARNING(ret, "G2API_AttachG2Model Failed");
	return ret;
}

static void G2API_SetBoltInfo(CGhoul2Info_v& ghoul2, int modelIndex, int boltInfo)
{
	if (ghoul2.size() > modelIndex)
	{
		ghoul2[modelIndex].mModelBoltLink = boltInfo;
	}
}

qboolean G2API_DetachG2Model(CGhoul2Info* ghlInfo)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		ghlInfo->mModelBoltLink = -1;
		return qtrue;
	}
	return qfalse;
}

qboolean G2API_AttachEnt(int* boltInfo, CGhoul2Info* ghlInfoTo, int toBoltIndex, int entNum, int to_model_num)
{
	if (boltInfo && G2_SetupModelPointers(ghlInfoTo))
	{
		// make sure we have a model to attach, a model to attach to, and a
		// bolt on that model
		if (ghlInfoTo->mBltlist.size() &&
			((ghlInfoTo->mBltlist[toBoltIndex].boneNumber != -1) ||
				(ghlInfoTo->mBltlist[toBoltIndex].surface_number != -1)))
		{
			// encode the bolt address into the model bolt link
			to_model_num &= MODEL_AND;
			toBoltIndex &= BOLT_AND;
			entNum &= ENTITY_AND;
			*boltInfo = (toBoltIndex << BOLT_SHIFT) |
				(to_model_num << MODEL_SHIFT) |
				(entNum << ENTITY_SHIFT);
			return qtrue;
		}
	}
	return qfalse;
}

void G2API_DetachEnt(int* boltInfo)
{
	G2ERROR(boltInfo, "G2API_DetachEnt: NULL boltInfo");
	if (boltInfo)
	{
		*boltInfo = 0;
	}
}

bool G2_NeedsRecalc(CGhoul2Info* ghlInfo, int frameNum);

qboolean G2API_GetBoltMatrix(CGhoul2Info_v& ghoul2, const int modelIndex, const int bolt_index, mdxaBone_t* matrix, const vec3_t angles, const vec3_t position, const int aframe_num, qhandle_t* model_list, const vec3_t scale)
{
	G2ERROR(ghoul2.IsValid(), "Invalid ghlInfo");
	G2ERROR(matrix, "NULL matrix");
	G2ERROR(modelIndex >= 0 && modelIndex < ghoul2.size(), "Invalid ModelIndex");
	constexpr static mdxaBone_t identity_matrix =
	{
		{
			{0.0f, -1.0f, 0.0f, 0.0f},
			{1.0f, 0.0f, 0.0f, 0.0f},
			{0.0f, 0.0f, 1.0f, 0.0f}
		}
	};

	G2_GenerateWorldMatrix(angles, position);
	if (G2_SetupModelPointers(ghoul2))
	{
		if (matrix && modelIndex >= 0 && modelIndex < ghoul2.size())
		{
			const int frameNum = G2API_GetTime(aframe_num);
			CGhoul2Info* ghlInfo = &ghoul2[modelIndex];
			G2ERROR(bolt_index >= 0 && (bolt_index < ghlInfo->mBltlist.size()), va("Invalid Bolt Index (%d:%s)", bolt_index, ghlInfo->mFileName));

			if (bolt_index >= 0 && ghlInfo && bolt_index < static_cast<int>(ghlInfo->mBltlist.size()))
			{
				mdxaBone_t bolt;

				if (G2_NeedsRecalc(ghlInfo, frameNum))
				{
					G2_ConstructGhoulSkeleton(ghoul2, frameNum, true, scale);
				}

				G2_GetBoltMatrixLow(*ghlInfo, bolt_index, scale, bolt);
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
				VectorNormalize(reinterpret_cast<float*>(&bolt.matrix[0]));
				VectorNormalize(reinterpret_cast<float*>(&bolt.matrix[1]));
				VectorNormalize(reinterpret_cast<float*>(&bolt.matrix[2]));

				Mat3x4_Multiply(matrix, &worldMatrix, &bolt);
#if G2API_DEBUG
				for (int i = 0; i < 3; i++)
				{
					for (int j = 0; j < 4; j++)
					{
						assert(!Q_isnan(matrix->matrix[i][j]));
					}
				}
#endif // _DEBUG
				G2ANIM(ghlInfo, "G2API_GetBoltMatrix");
				return qtrue;
			}
		}
	}
	else
	{
		G2WARNING(0, "G2API_GetBoltMatrix Failed on empty or bad model");
	}
	Mat3x4_Multiply(matrix, &worldMatrix, (mdxaBone_t*)&identity_matrix);
	return qfalse;
}

void G2API_ListSurfaces(CGhoul2Info* ghlInfo)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		G2_List_Model_Surfaces(ghlInfo->mFileName);
	}
}

void G2API_ListBones(CGhoul2Info* ghlInfo, const int frame)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		G2_List_Model_Bones(ghlInfo->mFileName);
	}
}

// decide if we have Ghoul2 models associated with this ghoul list or not
qboolean G2API_HaveWeGhoul2Models(const CGhoul2Info_v& ghoul2)
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

// run through the Ghoul2 models and set each of the mModel values to the
// correct one from the cgs.gameModel offset lsit
void G2API_SetGhoul2ModelIndexes(CGhoul2Info_v& ghoul2, qhandle_t* model_list, const qhandle_t* skin_list)
{
	G2ERROR(ghoul2.IsValid(), "Invalid ghlInfo");
	for (int i = 0; i < ghoul2.size(); i++)
	{
		if (ghoul2[i].mModelindex != -1)
		{
			ghoul2[i].mSkin = skin_list[ghoul2[i].mCustomSkin];
		}
	}
}

char* G2API_GetAnimFileNameIndex(const qhandle_t modelIndex)
{
	model_t* mod_m = R_GetModelByHandle(modelIndex);

	G2ERROR(mod_m && mod_m->data.glm, "G2API_GetAnimFileNameIndex: Bad Model");
	if (mod_m && mod_m->data.glm && mod_m->data.glm->header)
	{
		return mod_m->data.glm->header->animName;
	}
	return "";
}

// As above, but gets the internal embedded name, not the name of the disk file.
// This is needed for some unfortunate jiggery-hackery to do with frameskipping & the animevents.cfg file
char* G2API_GetAnimFileInternalNameIndex(const qhandle_t modelIndex)
{
	model_t* mod_a = R_GetModelByHandle(modelIndex);

	G2ERROR(mod_a && mod_a->data.gla, "G2API_GetAnimFileInternalNameIndex: Bad Model");
	if (mod_a && mod_a->data.gla)
	{
		return mod_a->data.gla->name;
	}
	return "";
}

qboolean G2API_GetAnimFileName(CGhoul2Info* ghlInfo, char** filename)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		return G2_GetAnimFileName(ghlInfo->mFileName, filename);
	}
	return qfalse;
}

/*
=======================
SV_QsortEntityNumbers
=======================
*/
static int QDECL QsortDistance(const void* a, const void* b)
{
	const float& ea = ((CCollisionRecord*)a)->mDistance;
	const float& eb = ((CCollisionRecord*)b)->mDistance;
	return (ea < eb) ? -1 : 1;
}

void G2API_CollisionDetect(CCollisionRecord* collRecMap, CGhoul2Info_v& ghoul2, const vec3_t angles, const vec3_t position, int aframe_number, int entNum, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, CMiniHeap* G2VertSpace, EG2_Collision eG2TraceType, int useLod, float fRadius)
{
	// not exactly correct, but doesn't matter
	G2ERROR(ghoul2.IsValid(), "G2API_CollisionDetect: Invalid ghlInfo");
	G2ERROR(collRecMap, "G2API_CollisionDetect: NULL Collision Rec");

	if (G2_SetupModelPointers(ghoul2) && collRecMap)
	{
		aframe_number = G2API_GetTime(aframe_number);
		vec3_t transRayStart, transRayEnd;

		// make sure we have transformed the whole skeletons for each model
		G2_ConstructGhoulSkeleton(ghoul2, aframe_number, true, scale);

		// pre generate the world matrix - used to transform the incoming ray
		G2_GenerateWorldMatrix(angles, position);

		ri.GetG2VertSpaceServer()->ResetHeap();

		// now having done that, time to build the model
#ifdef _G2_GORE
		G2_TransformModel(ghoul2, aframe_number, scale, ri.GetG2VertSpaceServer(), useLod, false);
#else
		G2_TransformModel(ghoul2, frameNumber, scale, ri.GetG2VertSpaceServer(), useLod);
#endif

		// model is built. Lets check to see if any triangles are actually hit.
		// first up, translate the ray to model space
		TransformAndTranslatePoint(rayStart, transRayStart, &worldMatrixInv);
		TransformAndTranslatePoint(rayEnd, transRayEnd, &worldMatrixInv);

		// now walk each model and check the ray against each poly - sigh, this is SO expensive. I wish there was a better way to do this.
#ifdef _G2_GORE
		G2_TraceModels(ghoul2, transRayStart, transRayEnd, collRecMap, entNum, eG2TraceType, useLod, fRadius, 0, 0, 0, 0, 0, qfalse);
#else
		G2_TraceModels(ghoul2, transRayStart, transRayEnd, collRecMap, entNum, eG2TraceType, useLod, fRadius);
#endif

		ri.GetG2VertSpaceServer()->ResetHeap();
		// now sort the resulting array of collision records so they are distance ordered
		qsort(collRecMap, MAX_G2_COLLISIONS, sizeof(CCollisionRecord), QsortDistance);
		G2ANIM(ghoul2, "G2API_CollisionDetect");
	}
}

qboolean G2API_SetGhoul2ModelFlags(CGhoul2Info* ghlInfo, const int flags)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		ghlInfo->mFlags &= GHOUL2_NEWORIGIN;
		ghlInfo->mFlags |= flags;
		return qtrue;
	}
	return qfalse;
}

int G2API_GetGhoul2ModelFlags(CGhoul2Info* ghlInfo)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		return (ghlInfo->mFlags & ~GHOUL2_NEWORIGIN);
	}
	return 0;
}

// given a boltmatrix, return in vec a normalised vector for the axis requested in flags
void G2API_GiveMeVectorFromMatrix(mdxaBone_t& bolt_matrix, const Eorientations flags, vec3_t& vec)
{
	switch (flags)
	{
	case ORIGIN:
		vec[0] = bolt_matrix.matrix[0][3];
		vec[1] = bolt_matrix.matrix[1][3];
		vec[2] = bolt_matrix.matrix[2][3];
		break;
	case POSITIVE_Y:
		vec[0] = bolt_matrix.matrix[0][1];
		vec[1] = bolt_matrix.matrix[1][1];
		vec[2] = bolt_matrix.matrix[2][1];
		break;
	case POSITIVE_X:
		vec[0] = bolt_matrix.matrix[0][0];
		vec[1] = bolt_matrix.matrix[1][0];
		vec[2] = bolt_matrix.matrix[2][0];
		break;
	case POSITIVE_Z:
		vec[0] = bolt_matrix.matrix[0][2];
		vec[1] = bolt_matrix.matrix[1][2];
		vec[2] = bolt_matrix.matrix[2][2];
		break;
	case NEGATIVE_Y:
		vec[0] = -bolt_matrix.matrix[0][1];
		vec[1] = -bolt_matrix.matrix[1][1];
		vec[2] = -bolt_matrix.matrix[2][1];
		break;
	case NEGATIVE_X:
		vec[0] = -bolt_matrix.matrix[0][0];
		vec[1] = -bolt_matrix.matrix[1][0];
		vec[2] = -bolt_matrix.matrix[2][0];
		break;
	case NEGATIVE_Z:
		vec[0] = -bolt_matrix.matrix[0][2];
		vec[1] = -bolt_matrix.matrix[1][2];
		vec[2] = -bolt_matrix.matrix[2][2];
		break;
	}
}

// copy a model from one ghoul2 instance to another, and reset the root surface on the new model if need be
// NOTE if modelIndex = -1 then copy all the models
void G2API_CopyGhoul2Instance(const CGhoul2Info_v& ghoul2_from, CGhoul2Info_v& ghoul2_to, int modelIndex)
{
	G2ERROR(ghoul2From.IsValid(), "G2API_CopyGhoul2Instance: Invalid ghlInfo");

	if (ghoul2_from.IsValid())
	{
		ghoul2_to.DeepCopy(ghoul2_from);

#ifdef _G2_GORE //check through gore stuff then, as well.
		int model = 0;

		//(since we are sharing this gore set with the copied instance we will have to increment
		//the reference count - if the goreset is "removed" while the refcount is > 0, the refcount
		//is decremented to avoid giving other instances an invalid pointer -rww)
		while (model < ghoul2_to.size())
		{
			if (ghoul2_to[model].mGoreSetTag)
			{
				CGoreSet* gore = FindGoreSet(ghoul2_to[model].mGoreSetTag);
				assert(gore);
				if (gore)
				{
					gore->mRefCount++;
				}
			}

			model++;
		}
#endif
		G2ANIM(ghoul2From, "G2API_CopyGhoul2Instance (source)");
		G2ANIM(ghoul2To, "G2API_CopyGhoul2Instance (dest)");
	}
}

char* G2API_GetSurfaceName(CGhoul2Info* ghlInfo, const int surfNumber)
{
	static char noSurface[1] = "";

	if (G2_SetupModelPointers(ghlInfo))
	{
		model_t* mod = (model_t*)ghlInfo->currentModel;
		mdxmSurface_t* surf = 0;
		mdxmSurfHierarchy_t* surfInfo = 0;
		mdxmHeader_t* mdxm;

		mdxm = mod->data.glm->header;

		//ok, I guess it's semi-valid for the user to be passing in surface > numSurfs because they don't know how many surfs a model
		//may have.. but how did they get that surf index to begin with? Oh well.
		if (surfNumber < 0 || surfNumber >= mdxm->numSurfaces)
		{
			Com_Printf("G2API_GetSurfaceName: You passed in an invalid surface number (%i) for model %s.\n", surfNumber, ghlInfo->mFileName);
			return noSurface;
		}

		surf = (mdxmSurface_t*)G2_FindSurface(mod, surfNumber, 0);
		if (surf)
		{
			mdxmHierarchyOffsets_t* surf_indexes = (mdxmHierarchyOffsets_t*)((byte*)ghlInfo->currentModel->data.glm->header + sizeof(mdxmHeader_t));
			surfInfo = (mdxmSurfHierarchy_t*)((byte*)surf_indexes + surf_indexes->offsets[surf->thisSurfaceIndex]);
			return surfInfo->name;
		}
	}
	return noSurface;
}

int	G2API_GetSurfaceIndex(CGhoul2Info* ghlInfo, const char* surfaceName)
{
	G2ERROR(surfaceName, "G2API_GetSurfaceIndex: NULL surfaceName");

	if (G2_SetupModelPointers(ghlInfo))
	{
		return G2_GetSurfaceIndex(ghlInfo, surfaceName);
	}
	return -1;
}

char* G2API_GetGLAName(CGhoul2Info* ghlInfo)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		assert(ghlInfo && ghlInfo->currentModel->data.glm);
		return (char*)ghlInfo->aHeader->name;
	}
	return NULL;
}

qboolean G2API_SetNewOrigin(CGhoul2Info* ghlInfo, const int bolt_index)
{
	if (G2_SetupModelPointers(ghlInfo))
	{
		// check for valid boltIndex before applying new origin
 		if (bolt_index >= 0 && bolt_index < (int)ghlInfo->mBltlist.size())
		{
			ghlInfo->mNewOrigin = bolt_index;
			ghlInfo->mFlags |= GHOUL2_NEWORIGIN;
		}
		return qtrue;
	}
	return qfalse;
}

int G2API_GetBoneIndex(CGhoul2Info* ghlInfo, const char* boneName, const qboolean bAddIfNotFound)
{
	G2ERROR(boneName, "G2API_GetBoneIndex: NULL boneName");
	if (boneName && G2_SetupModelPointers(ghlInfo))
	{
		return G2_Get_Bone_Index(ghlInfo, boneName, bAddIfNotFound);
	}
	return -1;
}

void G2API_SaveGhoul2Models(CGhoul2Info_v& ghoul2)
{
	G2_SaveGhoul2Models(ghoul2);
	G2ANIM(ghoul2, "G2API_SaveGhoul2Models");
}

void G2API_LoadGhoul2Models(CGhoul2Info_v& ghoul2, char* buffer)
{
	G2_LoadGhoul2Model(ghoul2, buffer);
	G2ANIM(ghoul2, "G2API_LoadGhoul2Models");
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
static qboolean G2API_SkinlessModel(CGhoul2Info_v& ghoul2, int modelIndex)
{
	CGhoul2Info* g2 = &ghoul2[modelIndex];

	if (G2_SetupModelPointers(g2))
	{
		model_t* mod = (model_t*)g2->currentModel;

		if (mod &&
			mod->data.glm &&
			mod->data.glm->header)
		{
			mdxmSurfHierarchy_t* surf;
			int i;
			mdxmHeader_t* mdxm = mod->data.glm->header;

			surf = (mdxmSurfHierarchy_t*)((byte*)mdxm + mdxm->ofsSurfHierarchy);

			for (i = 0; i < mdxm->numSurfaces; i++)
			{
				if (surf->shader[0])
				{ //found a surface with a shader name, ok.
					return qfalse;
				}

				surf = (mdxmSurfHierarchy_t*)((byte*)surf + (intptr_t)(&((mdxmSurfHierarchy_t*)0)->childIndexes[surf->numChildren]));
			}
		}
	}

	//found nothing.
	return qtrue;
}

static int G2API_Ghoul2Size(const CGhoul2Info_v& ghoul2)
{
	return ghoul2.size();
}

//#ifdef _SOF2
#ifdef _G2_GORE
void ResetGoreTag(); // put here to reduce coupling

//way of seeing how many marks are on a model currently -rww
static int G2API_GetNumGoreMarks(CGhoul2Info_v& ghoul2, int modelIndex)
{
	CGhoul2Info* g2 = &ghoul2[modelIndex];

	if (g2->mGoreSetTag)
	{
		CGoreSet* goreSet = FindGoreSet(g2->mGoreSetTag);

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

extern int G2_DecideTraceLod(CGhoul2Info& ghoul2, int useLod);
void G2API_AddSkinGore(CGhoul2Info_v& ghoul2, SSkinGoreData& gore)
{
	if (VectorLength(gore.rayDirection) < .1f)
	{
		assert(0); // can't add gore without a shot direction
		return;
	}

	// make sure we have transformed the whole skeletons for each model
	G2_ConstructGhoulSkeleton(ghoul2, gore.current_time, true, gore.scale);

	// pre generate the world matrix - used to transform the incoming ray
	G2_GenerateWorldMatrix(gore.angles, gore.position);

	// first up, translate the ray to model space
	vec3_t	transRayDirection, transHitLocation;
	TransformAndTranslatePoint(gore.hitLocation, transHitLocation, &worldMatrixInv);
	TransformPoint(gore.rayDirection, transRayDirection, &worldMatrixInv);
	if (!gore.useTheta)
	{
		vec3_t t;
		VectorCopy(gore.uaxis, t);
		TransformPoint(t, gore.uaxis, &worldMatrixInv);
	}

	int lod;
	ResetGoreTag();
	const int lodbias = Com_Clamp(0, 2, G2_DecideTraceLod(ghoul2[0], r_lodbias->integer));
	const int maxLod = Com_Clamp(0, ghoul2[0].currentModel->numLods, 3);	//limit to the number of lods the main model has

	for (lod = lodbias; lod < maxLod; lod++)
	{
		// now having done that, time to build the model
		ri.GetG2VertSpaceServer()->ResetHeap();

		G2_TransformModel(ghoul2, gore.current_time, gore.scale, ri.GetG2VertSpaceServer(), lod, true, &gore);

		// now walk each model and compute new texture coordinates
		G2_TraceModels(ghoul2, transHitLocation, transRayDirection, 0, gore.entNum, G2_NOCOLLIDE, lod, 1.0f, gore.SSize, gore.TSize, gore.theta, gore.shader, &gore, qtrue);
	}
}
#else

void G2API_ClearSkinGore(CGhoul2Info_v& ghoul2) {}
void G2API_AddSkinGore(CGhoul2Info_v& ghoul2, SSkinGoreData& gore)
{
}

#endif

qboolean G2_TestModelPointers(CGhoul2Info* ghlInfo) // returns true if the model is properly set up
{
	G2ERROR(ghlInfo, "G2_TestModelPointers: NULL ghlInfo");
	if (!ghlInfo)
	{
		return qfalse;
	}
	ghlInfo->mValid = false;
	if (ghlInfo->mModelindex != -1)
	{
		ghlInfo->mModel = RE_RegisterModel(ghlInfo->mFileName);
		ghlInfo->currentModel = R_GetModelByHandle(ghlInfo->mModel);
		if (ghlInfo->currentModel &&
			ghlInfo->currentModel->type == MOD_MDXM) //Rend2 - data is a union now, so we need to make sure it's also a glm that is loaded
		{
			if (ghlInfo->currentModel->data.glm &&
				ghlInfo->currentModel->data.glm->header)
			{
				mdxmHeader_t* mdxm = ghlInfo->currentModel->data.glm->header;
				if (ghlInfo->currentModelSize)
				{
					if (ghlInfo->currentModelSize != mdxm->ofsEnd)
					{
						Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
					}
				}

				ghlInfo->currentModelSize = mdxm->ofsEnd;
				ghlInfo->animModel = R_GetModelByHandle(mdxm->animIndex + ghlInfo->animModelIndexOffset);

				if (ghlInfo->animModel)
				{
					ghlInfo->aHeader = ghlInfo->animModel->data.gla;
					if (ghlInfo->aHeader)
					{
						if (ghlInfo->currentAnimModelSize)
						{
							if (ghlInfo->currentAnimModelSize != ghlInfo->aHeader->ofsEnd)
							{
								Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
							}
						}
						ghlInfo->currentAnimModelSize = ghlInfo->aHeader->ofsEnd;
						ghlInfo->mValid = true;
					}
				}
			}
		}
	}
	if (!ghlInfo->mValid)
	{
		ghlInfo->currentModel = NULL;
		ghlInfo->currentModelSize = 0;
		ghlInfo->animModel = NULL;
		ghlInfo->currentAnimModelSize = 0;
		ghlInfo->aHeader = NULL;
	}
	return (qboolean)ghlInfo->mValid;
}

#ifdef G2_PERFORMANCE_ANALYSIS
#include "qcommon/timing.h"
extern timing_c G2PerformanceTimer_G2_SetupModelPointers;
extern int G2Time_G2_SetupModelPointers;
#endif

bool G2_SetupModelPointers(CGhoul2Info* ghlInfo) // returns true if the model is properly set up
{
#ifdef G2_PERFORMANCE_ANALYSIS
	G2PerformanceTimer_G2_SetupModelPointers.Start();
#endif
	G2ERROR(ghlInfo, "G2_SetupModelPointers: NULL ghlInfo");
	if (!ghlInfo)
	{
		return qfalse;
	}

	ghlInfo->mValid = false;

	if (ghlInfo->mModelindex != -1)
	{
		G2ERROR(ghlInfo->mFileName[0], "empty ghlInfo->mFileName");

		// RJ - experimental optimization!
		ghlInfo->mModel = RE_RegisterModel(ghlInfo->mFileName);
		ghlInfo->currentModel = R_GetModelByHandle(ghlInfo->mModel);

		G2ERROR(ghlInfo->currentModel, va("NULL Model (glm) %s", ghlInfo->mFileName));
		if (ghlInfo->currentModel)
		{
			G2ERROR(ghlInfo->currentModel->modelData, va("Model has no mdxm (glm) %s", ghlInfo->mFileName));

			if (ghlInfo->currentModel->data.glm && ghlInfo->currentModel->data.glm->header)
			{
				mdxmHeader_t* mdxm = ghlInfo->currentModel->data.glm->header;
				if (ghlInfo->currentModelSize)
				{
					if (ghlInfo->currentModelSize != mdxm->ofsEnd)
					{
						Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
					}
				}
				ghlInfo->currentModelSize = mdxm->ofsEnd;
				G2ERROR(ghlInfo->currentModelSize, va("Zero sized Model? (glm) %s", ghlInfo->mFileName));

				ghlInfo->animModel = R_GetModelByHandle(mdxm->animIndex + ghlInfo->animModelIndexOffset);
				G2ERROR(ghlInfo->animModel, va("NULL Model (gla) %s", ghlInfo->mFileName));

				if (ghlInfo->animModel)
				{
					ghlInfo->aHeader = ghlInfo->animModel->data.gla;
					G2ERROR(ghlInfo->aHeader, va("Model has no mdxa (gla) %s", ghlInfo->mFileName));
					if (ghlInfo->aHeader)
					{
						if (ghlInfo->currentAnimModelSize)
						{
							if (ghlInfo->currentAnimModelSize != ghlInfo->aHeader->ofsEnd)
							{
								Com_Error(ERR_DROP, "Ghoul2 model was reloaded and has changed, map must be restarted.\n");
							}
						}

						ghlInfo->currentAnimModelSize = ghlInfo->aHeader->ofsEnd;
						G2ERROR(ghlInfo->currentAnimModelSize, va("Zero sized Model? (gla) %s", ghlInfo->mFileName));
						ghlInfo->mValid = true;
					}
				}
			}
		}
	}
	if (!ghlInfo->mValid)
	{
		ghlInfo->currentModel = NULL;
		ghlInfo->currentModelSize = 0;
		ghlInfo->animModel = NULL;
		ghlInfo->currentAnimModelSize = 0;
		ghlInfo->aHeader = NULL;
	}

#ifdef G2_PERFORMANCE_ANALYSIS
	G2Time_G2_SetupModelPointers += G2PerformanceTimer_G2_SetupModelPointers.End();
#endif
	return (qboolean)ghlInfo->mValid;
}

bool G2_SetupModelPointers(CGhoul2Info_v& ghoul2) // returns true if any model is properly set up
{
	bool ret = false;
	for (int i = 0; i < ghoul2.size(); i++)
	{
		const bool r = G2_SetupModelPointers(&ghoul2[i]);
		ret = ret || r;
	}
	return ret;
}

static qboolean G2API_IsGhoul2InfovValid(CGhoul2Info_v& ghoul2)
{
	return (qboolean)ghoul2.IsValid();
}

static const char* G2API_GetModelName(CGhoul2Info_v& ghoul2, int modelIndex)
{
	return ghoul2[modelIndex].mFileName;
}