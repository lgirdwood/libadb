/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Copyright (C) 2010, 2012 Liam Girdwood
 */

#ifndef __ADB_TABLE_HTM_H
#define __ADB_TABLE_HTM_H

#include <stdlib.h>
#include <math.h>
#include <libastrodb/astrodb.h>

#define INSIDE_UP_LIMIT		-1.0e-5
#define HTM_TRIXEL_UP			0
#define HTM_TRIXEL_DOWN	1
#define HTM_HEMI_NORTH		0
#define HTM_HEMI_SOUTH		1

#define HTM_MAX_TABLES			8

#define TRIXEL_UP	0
#define TRIXEL_DOWN	1

#define HEMI_NORTH	0
#define HEMI_SOUTH	1

#define TRIXELS_PER_VERTEX		6
#define NUM_CHILD_TRIXELS			4

#define HTM_MAX_DEPTH		12

/*
 * Pixel ID is 32 bit filed as follows :-
 *  31....24  23....16  15.....8  7......0
 *  1SQQDDDD  T0T1T2T3  T4T5T6T7  T8T9T1Tb
 */

#define HTM_ID_VALID_SHIFT		31
#define HTM_ID_HEMI_SHIFT		30
#define HTM_ID_QUAD_SHIFT		28
#define HTM_ID_QUAD_MASK		0x3
#define HTM_ID_POS_MASK		0x3
#define HTM_ID_DEPTH_SHIFT	24
#define HTM_ID_DEPTH_MASK	0xf

#define htm_trixel_valid(id)		(id & (1 << HTM_ID_VALID_SHIFT))
#define htm_trixel_north(id)		(!(id & (1 << HTM_ID_HEMI_SHIFT)))
#define htm_trixel_south(id)		(id & (1 << HTM_ID_HEMI_SHIFT))

#define htm_trixel_quadrant(id) \
	((id >> HTM_ID_QUAD_SHIFT) & HTM_ID_QUAD_MASK)

#define htm_trixel_depth(id) \
	((id >> HTM_ID_DEPTH_SHIFT) & HTM_ID_DEPTH_MASK)

#define htm_trixel_position(id, depth) \
		((id >> (depth << 1)) & HTM_ID_POS_MASK)

#define htm_for_each_trixel_object(trixel, table_id, object)		\
	for (object = trixel->data[table_id].objects; object;  \
		object = object->next)

#define htm_for_each_clipped_trixel(htm, trixel)		\
	for (trixel = htm->clip.trixels[0]; trixel!= NULL; trixel++)

struct htm_trixel;
struct htm;
struct adb_table;
struct adb_object_set;

/*! \struct struct htm_vertex
 *
 *  Hierarchical Triangular Mesh Vertex.
 */
struct htm_vertex {
	/* spherical coords */
	float ra;
	float dec;

	/* HTM (polyhedron) coords */
	float x;
	float y;
	float z;

	unsigned int depth; /* depth we are 1st used at */
	struct htm_trixel **trixel;
};

struct htm_trixel_data {
	struct adb_object *objects;	/*!< object catalog data */
	union {
		int num_objects;
		float v;
	};
};

/*
 * /struct htm_trixel
 */
struct htm_trixel {
	/* geometry */
	struct htm_vertex *a,*b,*c;		/* verticies */
	struct htm_trixel *parent;			/* parent trixel */
	struct htm_trixel *child;		/* child trixels - 0,1,2,3 */

	struct htm_trixel_data data[8]; 	/*!< object or flux data */

	/* flags */
	unsigned int visible:2;		/* visible in query - partial or full */
	unsigned int orientation:1;	/* up or down */
	unsigned int num_datasets:4;

	/* ID */
	unsigned int hemisphere:1;	/* north of south */
	unsigned int quadrant:4;		/* quadrant */
	unsigned int depth:4;			/* depth this trixel is at */
	unsigned int position;		/* trixel position */
};

/*! \struct dec_domain
 *
 * DEC vertex domain containing all vertices in DEC stripes.
 */
struct dec_strip {
	int half_size; /* at max depth - calc this at startup */
	int vertex_count;
	float width;
	struct htm_vertex *vertex; /* variable len array of vertices per quad */
};

struct htm_depth_map {
	float boundary;
	int object_count[ADB_MAX_TABLES];
};

struct htm {
	/* mesh specifics */
	struct htm_trixel N[4];
	struct htm_trixel S[4];
	int depth;

	/* domain */
	float dec_step;
	struct dec_strip *dec;

	/* mesh stats */
	int trixel_count;
	int vertex_count;
	int dec_strip_count;

	/* logging */
	enum adb_msg_level msg_level;
	int msg_flags;
};

/* create and init new HTM */
struct htm *htm_new(int depth, int tables);

/* free HTM and resources */
void htm_free(struct htm *htm);

int htm_clip(struct htm *htm, struct adb_object_set *set, 
	float ra, float dec, float fov,
	float min_depth, float max_depth);

//int htm_unclip(struct htm *htm);

int htm_get_trixels(struct htm *htm, struct adb_object_set *set);

int htm_get_depth_from_resolution(float resolution);

int htm_get_object_depth_more(struct htm *htm, float value);
int htm_get_object_depth_less(struct htm *htm, float value);

int htm_get_depth_from_magnitude(struct htm *htm, float mag);

struct htm_trixel *htm_get_home_trixel(struct htm *htm,
		struct htm_vertex *point, int depth);

int htm_get_object_depth(struct htm *htm, float value);

struct htm_trixel *htm_get_trixel(struct htm *htm, unsigned int id);

int htm_table_insert_object(struct htm *htm, struct adb_table *table,
	struct adb_object *object, unsigned int object_count, 
	unsigned int trixel_id);

void htm_import_object_ascending(struct adb_db *db,
	struct htm_trixel *trixel, struct adb_object *new_object,
	struct adb_table *table);

void htm_import_object_descending(struct adb_db *db,
	struct htm_trixel *trixel, struct adb_object *new_object,
	struct adb_table *table);

static inline unsigned int htm_trixel_id(struct htm_trixel *trixel)
{
	return 1 << HTM_ID_VALID_SHIFT |
			trixel->hemisphere << HTM_ID_HEMI_SHIFT |
			trixel->quadrant << HTM_ID_QUAD_SHIFT |
			trixel->depth << HTM_ID_DEPTH_SHIFT |
			trixel->position;
}

static inline void htm_vertex_update_unit(struct htm_vertex *v)
{
	float cos_dec = cosf(v->dec);

	/* spherical XYZ on unit vector */
	v->x = cos_dec * sinf(v->ra);
	v->y = sinf(v->dec);
	v->z = cos_dec * cosf(v->ra);

	/* spherical to octohedron */
	if (v->x < 0)
		v->x *= -v->x;
	else
		v->x *= v->x;

	if (v->y < 0)
		v->y *= -v->y;
	else
		v->y *= v->y;

	if (v->z < 0)
		v->z *= -v->z;
	else
		v->z *= v->z;
}

#define htm_dump_trixel(htm, trixel) \
{ \
	adb_htm_debug(htm, ADB_LOG_HTM_GET, "H:%s Q:%d D:%d P:%x\n", \
		trixel->hemisphere ? "S" : "N", trixel->quadrant, \
		trixel->depth, trixel->position); \
}

#endif /* TABLE_HTM_H_ */
