/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

#include "cm_local.h"

class CPoint
{
public:
	float x, y, z;

	CPoint(const float _x, const float _y, const float _z) :
		x(_x),
		y(_y),
		z(_z)
	{
	}

	bool operator==(const CPoint& p) const { return x == p.x && y == p.y && z == p.z; }
};

/*
class CPointComparator
{
public:
	bool operator()(const CPoint& _A,const CPoint& _B) const {return((_A.x==_B.x)&&(_A.y==_B.y)&&(_A.z==_B.z));}
};
*/

// Fixed memory version of pointToLeaf that doesn't use STL
// cuts down on memory fragmentation
struct PointAndLeaf
{
	// Default constructor for array construction below
	PointAndLeaf() : point(0, 0, 0), leaf(0)
	{
	}

	CPoint point;
	int leaf;
};

// I think it is a pathologically bad idea to do a 64 item linear search for a cache,
// so I reduced this to something more manageable.
// hopefully getting rid of water checks on maps with no water will leave us less
// reliant on this cache. -gwg

#define MAX_POINTS_TO_LEAVES	16

static PointAndLeaf pointToLeaf[MAX_POINTS_TO_LEAVES];
static int oldestPointToLeaf = 0, sizePointToLeaf = 0;

//static hlist<pair<CPoint,int> >				pointToLeaf;
//static hlist<pair<CPoint,int> >				pointToContents;

void CM_CleanLeafCache()
{
	oldestPointToLeaf = sizePointToLeaf = 0;
	//	pointToLeaf.clear();
#if 0 // VVFIXME
	hlist<pair<CPoint, int> >::iterator l;
	for (l = pointToLeaf.begin(); l != pointToLeaf.end(); l++)
	{
		pointToLeaf.erase(l);
	}
#endif
	/*
		for(l=pointToContents.begin();l!=pointToContents.end();l++)
		{
			pointToContents.erase(l);
		}
	*/
}

/*
==================
CM_PointLeafnum_r

==================
*/
int CM_PointLeafnum_r(const vec3_t p, int num, const clipMap_t* local)
{
	float d;

	while (num >= 0)
	{
		const cNode_t* node = local->nodes + num;
		const cplane_t* plane = node->plane;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct(plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	c_pointcontents++; // optimize counter

	return -1 - num;
}

int CM_PointLeafnum(const vec3_t p)
{
	if (!cmg.numNodes)
	{
		// map not loaded
		return 0;
	}
	return CM_PointLeafnum_r(p, 0, &cmg);
}

/*
======================================================================

LEAF LISTING

======================================================================
*/

void CM_StoreLeafs(leafList_t* ll, const int nodenum)
{
	const int leaf_num = -1 - nodenum;

	// store the lastLeaf even if the list is overflowed
	if (cmg.leafs[leaf_num].cluster != -1)
	{
		ll->lastLeaf = leaf_num;
	}

	if (ll->count >= ll->maxcount)
	{
		ll->overflowed = qtrue;
		return;
	}
	ll->list[ll->count++] = leaf_num;
}

void CM_StoreBrushes(leafList_t* ll, const int nodenum)
{
	int i;

	const int leafnum = -1 - nodenum;

	cLeaf_t* leaf = &cmg.leafs[leafnum];

	for (int k = 0; k < leaf->numLeafBrushes; k++)
	{
		const int brushnum = cmg.leafbrushes[leaf->firstLeafBrush + k];
		cbrush_t* b = &cmg.brushes[brushnum];
		if (b->checkcount == cmg.checkcount)
		{
			continue; // already checked this brush in another leaf
		}
		b->checkcount = cmg.checkcount;
		for (i = 0; i < 3; i++)
		{
			if (b->bounds[0][i] >= ll->bounds[1][i] || b->bounds[1][i] <= ll->bounds[0][i])
			{
				break;
			}
		}
		if (i != 3)
		{
			continue;
		}
		if (ll->count >= ll->maxcount)
		{
			ll->overflowed = qtrue;
			return;
		}
		reinterpret_cast<cbrush_t**>(ll->list)[ll->count++] = b;
	}
#if 0
	// store patches?
	for (k = 0; k < leaf->numLeafSurfaces; k++) {
		patch = cmg.surfaces[cmg.leafsurfaces[leaf->firstleafsurface + k]];
		if (!patch) {
			continue;
		}
	}
#endif
}

/*
=============
CM_BoxLeafnums

Fills in a list of all the leafs touched
=============
*/
void CM_BoxLeafnums_r(leafList_t* ll, int nodenum)
{
	while (true)
	{
		if (nodenum < 0)
		{
			ll->storeLeafs(ll, nodenum);
			return;
		}

		const cNode_t* node = &cmg.nodes[nodenum];
		const cplane_t* plane = node->plane;

		const int s = BoxOnPlaneSide(ll->bounds[0], ll->bounds[1], plane);
		if (s == 1)
		{
			nodenum = node->children[0];
		}
		else if (s == 2)
		{
			nodenum = node->children[1];
		}
		else
		{
			// go down both
			CM_BoxLeafnums_r(ll, node->children[0]);
			nodenum = node->children[1];
		}
	}
}

/*
==================
CM_BoxLeafnums
==================
*/
int CM_BoxLeafnums(const vec3_t mins, const vec3_t maxs, int* box_list, const int listsize, int* last_leaf)
{
	leafList_t ll{};

	cmg.checkcount++;

	VectorCopy(mins, ll.bounds[0]);
	VectorCopy(maxs, ll.bounds[1]);
	ll.count = 0;
	ll.maxcount = listsize;
	ll.list = box_list;
	ll.storeLeafs = CM_StoreLeafs;
	ll.lastLeaf = 0;
	ll.overflowed = qfalse;

	CM_BoxLeafnums_r(&ll, 0);

	*last_leaf = ll.lastLeaf;
	return ll.count;
}

//====================================================================

/*
==================
CM_PointContents

==================
*/

#if 1

int CM_PointContents(const vec3_t p, const clipHandle_t model)
{
	int i;
	cLeaf_t* leaf;
	clipMap_t* local;

	if (!cmg.numNodes)
	{
		// map not loaded
		return 0;
	}

	if (model)
	{
		cmodel_t* clipm = CM_ClipHandleToModel(model, &local);
		leaf = &clipm->leaf;
	}
	else
	{
		int leafnum = 0;
		local = &cmg;
		const CPoint pt(p[0], p[1], p[2]);

		int l = 0;
		for (; l < sizePointToLeaf; ++l)
		{
			if (pointToLeaf[l].point == pt)
			{
				leafnum = pointToLeaf[l].leaf;
				break;
			}
		}

		if (l == sizePointToLeaf)
		{
			// Didn't find it
			if (sizePointToLeaf < MAX_POINTS_TO_LEAVES)
			{
				// We're adding a new one, rather than replacing
				sizePointToLeaf++;
			}
			else
			{
				// Put it in the "oldest" slot
				l = oldestPointToLeaf++;
				oldestPointToLeaf &= MAX_POINTS_TO_LEAVES - 1;
			}

			leafnum = CM_PointLeafnum_r(p, 0, local);
			pointToLeaf[l].leaf = leafnum;
			pointToLeaf[l].point = pt;
		}
		leaf = &local->leafs[leafnum];
	}

	int contents = 0;
	for (int k = 0; k < leaf->numLeafBrushes; k++)
	{
		const int brushnum = local->leafbrushes[leaf->firstLeafBrush + k];
		const cbrush_t* b = &local->brushes[brushnum];

		// see if the point is in the brush
		for (i = 0; i < b->numsides; i++)
		{
			const float d = DotProduct(p, b->sides[i].plane->normal);

			// FIXME test for Cash
			//			if ( d >= b->sides[i].plane->dist ) {
			if (d > b->sides[i].plane->dist)
			{
				break;
			}
		}

		if (i == b->numsides)
		{
			contents |= b->contents;
		}
	}

	return contents;
}

#else

int CM_PointContents(const vec3_t p, clipHandle_t model) {
	int			leafnum = 0;
	int			i, k;
	int			brushnum;
	cLeaf_t* leaf;
	cbrush_t* b;
	int			contents;
	float		d;
	cmodel_t* clipm;

	if (!cmg.numNodes) {	// map not loaded
		return 0;
	}

	CPoint pt(p[0], p[1], p[2]);
	if (model)
	{
		clipm = CM_ClipHandleToModel(model);
		leaf = &clipm->leaf;
	}
	else
	{
		hlist<pair<CPoint, int> >::iterator l;
		for (l = pointToContents.begin(); l != pointToContents.end(); l++)
		{
			if ((*l).first == pt)
			{
				// Breakout early.
				return((*l).second);
			}
		}

		leafnum = CM_PointLeafnum_r(p, 0);
		leaf = &cmg.leafs[leafnum];
	}

	contents = 0;
	for (k = 0; k < leaf->numLeafBrushes; k++)
	{
		brushnum = cmg.leafbrushes[leaf->firstLeafBrush + k];
		b = &cmg.brushes[brushnum];

		// see if the point is in the brush
		for (i = 0; i < b->numsides; i++)
		{
			d = DotProduct(p, b->sides[i].plane->normal);
			// FIXME test for Cash
//			if ( d >= b->sides[i].plane->dist ) {
			if (d > b->sides[i].plane->dist)
			{
				break;
			}
		}

		if (i == b->numsides)
		{
			contents |= b->contents;
		}
	}

	// Cache the result for next time.
	if (!model)
	{
		if (pointToContents.size() >= 64)
		{
			pointToContents.pop_back();
		}
		pointToContents.push_front(pair<CPoint, int>(pt, contents));
	}

	return contents;
}

#endif
/*
==================
CM_TransformedPointContents

Handles offseting and rotation of the end points for moving and
rotating entities
==================
*/
int CM_TransformedPointContents(const vec3_t p, const clipHandle_t model, const vec3_t origin, const vec3_t angles)
{
	vec3_t p_l;

	// subtract origin offset
	VectorSubtract(p, origin, p_l);

	// rotate start and end into the models frame of reference
	if (model != BOX_MODEL_HANDLE &&
		(angles[0] || angles[1] || angles[2]))
	{
		vec3_t up;
		vec3_t right;
		vec3_t forward;
		vec3_t temp;
		AngleVectors(angles, forward, right, up);

		VectorCopy(p_l, temp);
		p_l[0] = DotProduct(temp, forward);
		p_l[1] = -DotProduct(temp, right);
		p_l[2] = DotProduct(temp, up);
	}

	return CM_PointContents(p_l, model);
}

/*
===============================================================================

PVS

===============================================================================
*/

byte* CM_ClusterPVS(const int cluster)
{
	if (cluster < 0 || cluster >= cmg.numClusters || !cmg.vised)
	{
		return cmg.visibility;
	}

	return cmg.visibility + cluster * cmg.clusterBytes;
}

/*
===============================================================================

AREAPORTALS

===============================================================================
*/

void CM_FloodArea_r(const int area_num, const int floodnum, clipMap_t& cm)
{
	cArea_t* area = &cmg.areas[area_num];

	if (area->floodvalid == cmg.floodvalid)
	{
		if (area->floodnum == floodnum)
			return;
		Com_Error(ERR_DROP, "FloodArea_r: reflooded");
	}

	area->floodnum = floodnum;
	area->floodvalid = cmg.floodvalid;
	const int* con = cmg.areaPortals + area_num * cmg.numAreas;
	for (int i = 0; i < cmg.numAreas; i++)
	{
		if (con[i] > 0)
		{
			CM_FloodArea_r(i, floodnum, cm);
		}
	}
}

/*
====================
CM_FloodAreaConnections

====================
*/
void CM_FloodAreaConnections(clipMap_t& cm)
{
	// all current floods are now invalid
	cmg.floodvalid++;
	int floodnum = 0;

	for (int i = 0; i < cmg.numAreas; i++)
	{
		const cArea_t* area = &cmg.areas[i];
		if (area->floodvalid == cmg.floodvalid)
		{
			continue; // already flooded into
		}
		floodnum++;
		CM_FloodArea_r(i, floodnum, cm);
	}
}

/*
====================
CM_AdjustAreaPortalState

====================
*/
void CM_AdjustAreaPortalState(const int area1, const int area2, const qboolean open)
{
	if (area1 < 0 || area2 < 0)
	{
		return;
	}

	if (area1 >= cmg.numAreas || area2 >= cmg.numAreas)
	{
		Com_Error(ERR_DROP, "CM_ChangeAreaPortalState: bad area number");
	}

	if (open)
	{
		cmg.areaPortals[area1 * cmg.numAreas + area2]++;
		cmg.areaPortals[area2 * cmg.numAreas + area1]++;
	}
	else
	{
		cmg.areaPortals[area1 * cmg.numAreas + area2]--;
		cmg.areaPortals[area2 * cmg.numAreas + area1]--;
		if (cmg.areaPortals[area2 * cmg.numAreas + area1] < 0)
		{
			Com_Error(ERR_DROP, "CM_AdjustAreaPortalState: negative reference count");
		}
	}

	CM_FloodAreaConnections(cmg);
}

/*
====================
CM_AreasConnected

====================
*/
qboolean CM_AreasConnected(const int area1, const int area2)
{
#ifndef BSPC
	if (cm_noAreas->integer)
	{
		return qtrue;
	}
#endif

	if (area1 < 0 || area2 < 0)
	{
		return qfalse;
	}

	if (area1 >= cmg.numAreas || area2 >= cmg.numAreas)
	{
		Com_Error(ERR_DROP, "area >= cmg.numAreas");
	}

	if (cmg.areas[area1].floodnum == cmg.areas[area2].floodnum)
	{
		return qtrue;
	}
	return qfalse;
}

/*
=================
CM_WriteAreaBits

Writes a bit vector of all the areas
that are in the same flood as the area parameter
Returns the number of bytes needed to hold all the bits.

The bits are OR'd in, so you can CM_WriteAreaBits from multiple
viewpoints and get the union of all visible areas.

This is used to cull non-visible entities from snapshots
=================
*/
int CM_WriteAreaBits(byte* buffer, int area)
{
	const int bytes = (cmg.numAreas + 7) >> 3;

#ifndef BSPC
	if (cm_noAreas->integer || area == -1)
#else
	if (area == -1)
#endif
	{
		// for debugging, send everything
		memset(buffer, 255, bytes);
	}
	else
	{
		const int floodnum = cmg.areas[area].floodnum;
		for (int i = 0; i < cmg.numAreas; i++)
		{
			if (cmg.areas[i].floodnum == floodnum || area == -1)
				buffer[i >> 3] |= 1 << (i & 7);
		}
	}

	return bytes;
}

void CM_SnapPVS(vec3_t origin, byte* buffer)
{
	const int leafnum = CM_PointLeafnum(origin);
	const int clientarea = CM_LeafArea(leafnum);

	// calculate the visible areas
	memset(buffer, 0, MAX_MAP_AREA_BYTES);
	CM_WriteAreaBits(buffer, clientarea);
	for (int i = 0; i < MAX_MAP_AREA_BYTES / 4; i++)
	{
		reinterpret_cast<int*>(buffer)[i] = reinterpret_cast<int*>(buffer)[i] ^ -1;
	}
}