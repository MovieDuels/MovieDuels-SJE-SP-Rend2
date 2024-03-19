#include "cg_local.h"

extern int BG_SiegeGetPairedValue(const char* buf, char* key, char* outbuf);
extern int BG_SiegeGetValueGroup(const char* buf, char* group, char* outbuf);

stringID_table_t holsterTypeTable[] =
{
	ENUM2STRING(HLR_NONE),
	ENUM2STRING(HLR_SINGLESABER_1), //first single saber
	ENUM2STRING(HLR_SINGLESABER_2), //second single saber
	ENUM2STRING(HLR_STAFFSABER), //staff saber
	ENUM2STRING(HLR_PISTOL_L), //left hip blaster pistol
	ENUM2STRING(HLR_PISTOL_R), //right hip blaster pistol
	ENUM2STRING(HLR_BLASTER_L), //left hip blaster rifle
	ENUM2STRING(HLR_BLASTER_R), //right hip blaster rifle
	ENUM2STRING(HLR_BRYARPISTOL_L), //left hip bryer pistol
	ENUM2STRING(HLR_BRYARPISTOL_R), //right hip bryer pistol
	ENUM2STRING(HLR_BOWCASTER), //bowcaster
	ENUM2STRING(HLR_ROCKET_LAUNCHER), //rocket launcher
	ENUM2STRING(HLR_DEMP2), //demp2
	ENUM2STRING(HLR_CONCUSSION), //concussion
	ENUM2STRING(HLR_REPEATER), //repeater
	ENUM2STRING(HLR_FLECHETTE), //flechette
	ENUM2STRING(HLR_DISRUPTOR), //disruptor
	ENUM2STRING(MAX_HOLSTER)
};

stringID_table_t holsterBoneTable[] =
{
	ENUM2STRING(HOLSTER_NONE),
	ENUM2STRING(HOLSTER_UPPERBACK),
	ENUM2STRING(HOLSTER_LOWERBACK),
	ENUM2STRING(HOLSTER_LEFTHIP),
	ENUM2STRING(HOLSTER_RIGHTHIP)
};

void InitHolsterData(clientInfo_t* ci)
{
	//initialize holster data with the premade defaults.
	for (int i = 0; i < MAX_HOLSTER; i++)
	{
		ci->holsterData[i].bone_index = HOLSTER_NONE;
		VectorCopy(vec3_origin, ci->holsterData[i].posOffset);
		VectorCopy(vec3_origin, ci->holsterData[i].angOffset);
	}
}

extern char* BG_GetNextValueGroup(char* inbuf, char* outbuf);

void CG_LoadHolsterData(clientInfo_t* ci)
{
	//adjusts the manual holster positional data based on the holster_mp.cfg file associated with the model or simply
	//use the default values

	fileHandle_t f;
	int f_len;
	char file_buffer[MAX_HOLSTER_INFO_SIZE];
	char holster_type_group[MAX_HOLSTER_INFO_SIZE];

	InitHolsterData(ci);

	if (!ci->skinName || !Q_stricmp("default", ci->skinName))
	{
		//try default holster_mp.cfg first
		f_len = trap->FS_Open(va("models/players/%s/holster_mp.cfg", ci->modelName), &f, FS_READ);

		if (!f)
		{
			//no file, use kyle's then.
			f_len = trap->FS_Open("models/players/kyle/holster_mp.cfg", &f, FS_READ);
		}
	}
	else
	{
		//use the holster_mp.cfg associated with this skin
		f_len = trap->FS_Open(va("models/players/%s/holster_mp%s.cfg", ci->modelName, ci->skinName), &f, FS_READ);
		if (!f)
		{
			//fall back to default holster_mp.cfg
			f_len = trap->FS_Open(va("models/players/%s/holster_mp.cfg", ci->modelName), &f, FS_READ);
		}

		if (!f)
		{
			//still no dice, use kyle's then.
			f_len = trap->FS_Open("models/players/kyle/holster_mp.cfg", &f, FS_READ);
		}
	}

	if (!f || !f_len)
	{
		//couldn't open file or it was empty, just use the defaults
		return;
	}

	if (f_len >= MAX_HOLSTER_INFO_SIZE)
	{
		trap->FS_Close(f);
		return;
	}

	trap->FS_Read(file_buffer, f_len, f);

	trap->FS_Close(f);

	char* s = file_buffer;

	//parse file
	while ((s = BG_GetNextValueGroup(s, holster_type_group)) != NULL)
	{
		vec3_t vector_data;
		char holster_type_value[MAX_QPATH];
		if (!BG_SiegeGetPairedValue(holster_type_group, "holsterType", holster_type_value))
		{
			//couldn't find holster type in group
			continue;
		}

		const int i = GetIDForString(holsterTypeTable, holster_type_value);

		if (i == -1)
		{
			//bad holster type
			continue;
		}

		if (BG_SiegeGetPairedValue(holster_type_group, "bone_index", holster_type_value))
		{
			//have bone index data for this holster type, use it
			if (!Q_stricmp(holster_type_value, "disabled"))
			{
				//disable the rendering of this holster type on this model
				ci->holsterData[i].bone_index = HOLSTER_NONE;
			}
			else
			{
				ci->holsterData[i].bone_index = GetIDForString(holsterBoneTable, holster_type_value);
			}
		}

		if (BG_SiegeGetPairedValue(holster_type_group, "posOffset", holster_type_value))
		{
			//parsing positional offset data
			sscanf(holster_type_value, "%f, %f, %f", &vector_data[0], &vector_data[1], &vector_data[2]);
			VectorCopy(vector_data, ci->holsterData[i].posOffset);
		}

		if (BG_SiegeGetPairedValue(holster_type_group, "angOffset", holster_type_value))
		{
			//parsing angular offset
			sscanf(holster_type_value, "%f, %f, %f", &vector_data[0], &vector_data[1], &vector_data[2]);
			VectorCopy(vector_data, ci->holsterData[i].angOffset);
		}
	}
#ifdef _DEBUG
	Com_Printf("Holstered Weapon Data Loaded for %s.\n", ci->modelName);
#endif
}