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
 *  Copyright (C) 2010 - 2014 Liam Girdwood
 */

#ifndef __ADB_TABLE_HTM_H
#define __ADB_TABLE_HTM_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <stdlib.h>
#include <math.h>

#include "libastrodb/db.h"
#include "private.h"

/*! \defgroup htm HTM
 *
 * \brief Hierarchical Triangular Mesh routines.
 *
 * Spherically partitions coordinate space into trixel structures, 
 * offering fast overlap filtering and depth-tiered object groupings 
 * optimized for field-of-view queries.
 */

#define INSIDE_UP_LIMIT -1.0e-5
#define HTM_TRIXEL_UP 0
#define HTM_TRIXEL_DOWN 1
#define HTM_HEMI_NORTH 0
#define HTM_HEMI_SOUTH 1

#define HTM_MAX_TABLES 8

#define TRIXEL_UP 0
#define TRIXEL_DOWN 1

#define HEMI_NORTH 0
#define HEMI_SOUTH 1

#define TRIXELS_PER_VERTEX 6
#define NUM_CHILD_TRIXELS 4

#define HTM_MAX_DEPTH 12

/*
 * Pixel ID is 32 bit filed as follows :-
 *  31....24  23....16  15.....8  7......0
 *  1SQQDDDD  T0T1T2T3  T4T5T6T7  T8T9T1Tb
 */

#define HTM_ID_VALID_SHIFT 31
#define HTM_ID_HEMI_SHIFT 30
#define HTM_ID_QUAD_SHIFT 28
#define HTM_ID_QUAD_MASK 0x3
#define HTM_ID_POS_MASK 0x3
#define HTM_ID_DEPTH_SHIFT 24
#define HTM_ID_DEPTH_MASK 0xf

#define htm_trixel_valid(id) (id & (1 << HTM_ID_VALID_SHIFT))
#define htm_trixel_north(id) (!(id & (1 << HTM_ID_HEMI_SHIFT)))
#define htm_trixel_south(id) (id & (1 << HTM_ID_HEMI_SHIFT))

#define htm_trixel_quadrant(id) ((id >> HTM_ID_QUAD_SHIFT) & HTM_ID_QUAD_MASK)

#define htm_trixel_depth(id) ((id >> HTM_ID_DEPTH_SHIFT) & HTM_ID_DEPTH_MASK)

#define htm_trixel_position(id, depth) ((id >> (depth << 1)) & HTM_ID_POS_MASK)

#define htm_for_each_trixel_object(trixel, table_id, object) \
	for (object = trixel->data[table_id].objects; object; object = object->next)

#define htm_for_each_clipped_trixel(htm, trixel) \
	for (trixel = htm->clitrixels[0]; trixel != NULL; trixel++)

struct htm_trixel;
struct htm;
struct adb_table;
struct adb_object_set;

/*! \struct struct htm_vertex
 * \ingroup htm
 *
 *  Hierarchical Triangular Mesh Vertex.
 */
struct htm_vertex {
	/* spherical coords */
	double ra;
	double dec;

	/* HTM (polyhedron) coords */
	double x;
	double y;
	double z;

	unsigned int depth; /* depth we are 1st used at */
	struct htm_trixel **trixel;
};

/*! \struct htm_trixel_data
 * \brief Array element managing HTM trixel objects associated natively
 * \ingroup htm
 */
struct htm_trixel_data {
	struct adb_object *objects; /*!< object catalog data linked list head */
	int num_objects; /*!< length of the linked object layout chunk */
};

/*! \struct htm_trixel
 * \brief Hierarchical Triangular Mesh leaf node
 * \ingroup htm
 */
struct htm_trixel {
	/* geometry */
	struct htm_vertex *a, *b, *c; /*!< verticies bounding area */
	struct htm_trixel *parent; /*!< parent trixel link */
	struct htm_trixel *child; /*!< base array to 4 child trixels */

	struct htm_trixel_data data[ADB_MAX_TABLES]; /*!< object data */

	/* flags */
	unsigned int visible : 2; /*!< visible in query - partial or full */
	unsigned int orientation : 1; /*!< up or down configuration */
	unsigned int num_datasets : 4; /*!< tracked datasets intersecting */

	/* ID */
	unsigned int hemisphere : 1; /*!< north or south pole */
	unsigned int quadrant : 4; /*!< mapped quadrant area */
	unsigned int depth : 4; /*!< structural depth context limit */
	unsigned int
		position; /*!< numerical location index matching pattern offset */
};

/*! \struct dec_domain
 * \ingroup htm
 *
 * DEC vertex domain containing all vertices in DEC stripes.
 */
struct dec_strip {
	int half_size; /* at max depth - calc this at startup */
	int vertex_count;
	double width;
	struct htm_vertex *vertex; /* variable len array of vertices per quad */
};

/*! \struct htm_depth_map
 * \brief Depth map limits for a dataset table
 * \ingroup htm
 */
struct htm_depth_map {
	double boundary; /*!< Boundary depth */
	int object_count[ADB_MAX_TABLES]; /*!< Count of objects per table */
};

/*! \struct htm
 * \brief Main Hierarchical Triangular Mesh context
 * \ingroup htm
 */
struct htm {
	/* mesh specifics */
	struct htm_trixel N[4]; /*!< Northern hemisphere root trixels */
	struct htm_trixel S[4]; /*!< Southern hemisphere root trixels */
	int depth; /*!< Maximum HTM depth */

	/* domain */
	double dec_step;
	struct dec_strip *dec;

	/* mesh stats */
	int trixel_count; /*!< Total allocated trixels */
	int vertex_count; /*!< Total allocated vertices */
	int dec_strip_count;

	/* logging */
	enum adb_msg_level msg_level;
	int msg_flags;
};

/**
 * \brief create and init new HTM
 * \ingroup htm
 * \param depth Maximum depth for this HTM mesh
 * \param tables Maximum number of tables to support
 * \return pointer to new htm context, or NULL on failure
 */
struct htm *htm_new(int depth, int tables);

/**
 * \brief free HTM and resources
 * \ingroup htm
 * \param htm Context to free
 */
void htm_free(struct htm *htm);

/**
 * \brief Clip an HTM area to a specific object set boundary
 * \ingroup htm
 * \param htm The HTM context
 * \param set The object set defining the area
 * \param ra Center RA in radians
 * \param dec Center DEC in radians
 * \param fov Field of view radius (radians)
 * \param min_depth Minimum HTM depth level constraint
 * \param max_depth Maximum HTM depth level constraint
 * \return 0 on success, negative error on failure
 */
int htm_clip(struct htm *htm, struct adb_object_set *set, double ra, double dec,
			 double fov, double min_depth, double max_depth);

/**
 * \brief Get trixels constrained within the clipped object set
 * \ingroup htm
 * \param htm The HTM context
 * \param set The object set
 * \return Number of trixels found, or negative error code
 */
int htm_get_trixels(struct htm *htm, struct adb_object_set *set);

/**
 * \brief Find the minimum required HTM depth to represent a specific resolution
 * \ingroup htm
 * \param resolution Angular resolution in radians
 * \return Depth level required (integer)
 */
int htm_get_depth_from_resolution(double resolution);

/**
 * \brief Get object depth more
 * \ingroup htm
 * \param htm HTM context
 * \param value Search value
 * \return Depth level
 */
int htm_get_object_depth_more(struct htm *htm, double value);

/**
 * \brief Get object depth less
 * \ingroup htm
 * \param htm HTM context
 * \param value Search value
 * \return Depth level
 */
int htm_get_object_depth_less(struct htm *htm, double value);

/**
 * \brief Get starting depth calculated from optical magnitude
 * \ingroup htm
 * \param htm HTM context
 * \param mag Apparent magnitude
 * \return Depth level
 */
int htm_get_depth_from_magnitude(struct htm *htm, double mag);

/**
 * \brief Retrieve the trixel encapsulating a specific spatial point
 * \ingroup htm
 * \param htm HTM context
 * \param point HTM vertex representing RA/DEC
 * \param depth Constraints target search bounding depth
 * \return Trixel containing the point, or NULL
 */
struct htm_trixel *htm_get_home_trixel(struct htm *htm,
									   struct htm_vertex *point, int depth);

/**
 * \brief Retrieve depth for specific value matching
 * \ingroup htm
 * \param htm HTM context
 * \param value Lookup parameter 
 * \return The resulting depth correlation
 */
int htm_get_object_depth(struct htm *htm, double value);

/**
 * \brief Fetch a specific HTM trixel globally by ID
 * \ingroup htm
 * \param htm HTM context
 * \param id Binary encoded ID for the target trixel
 * \return Found trixel or NULL
 */
struct htm_trixel *htm_get_trixel(struct htm *htm, unsigned int id);

/**
 * \brief Insert an astronomical object record directly into the mesh structure
 * \ingroup htm
 * \param htm HTM context
 * \param table Reference dataset
 * \param object Object structure
 * \param object_count Total elements currently cataloged
 * \param trixel_id The designated root node or ID for insertion
 * \return 0 on success, negative on error
 */
int htm_table_insert_object(struct htm *htm, struct adb_table *table,
							struct adb_object *object,
							unsigned int object_count, unsigned int trixel_id);

/**
 * \brief Importer helper placing items in ascending order distribution
 * \ingroup htm
 * \param db Database catalog
 * \param trixel Working trixel 
 * \param new_object Parsed object
 * \param table Target dataset table
 */
void htm_import_object_ascending(struct adb_db *db, struct htm_trixel *trixel,
								 struct adb_object *new_object,
								 struct adb_table *table);

/**
 * \brief Importer helper placing items in descending order distribution
 * \ingroup htm
 * \param db Database catalog
 * \param trixel Working trixel 
 * \param new_object Parsed object
 * \param table Target dataset table
 */
void htm_import_object_descending(struct adb_db *db, struct htm_trixel *trixel,
								  struct adb_object *new_object,
								  struct adb_table *table);

/**
 * \brief Formulate the structural 32-bit ID mask indexing a particular trixel
 * \ingroup htm
 * \param trixel Source trixel data
 * \return 32-bit integer packed ID
 */
static inline unsigned int htm_trixel_id(struct htm_trixel *trixel)
{
	return 1 << HTM_ID_VALID_SHIFT | trixel->hemisphere << HTM_ID_HEMI_SHIFT |
		   trixel->quadrant << HTM_ID_QUAD_SHIFT |
		   trixel->depth << HTM_ID_DEPTH_SHIFT | trixel->position;
}

/**
 * \brief Resolve coordinates mapping spherical space to internal octohedron HTM structure mapped unit representation
 * \ingroup htm
 * \param v Vertex containing raw `ra` and `dec` targets, calculates and replaces `x, y, z` fields
 */
static inline void htm_vertex_update_unit(struct htm_vertex *v)
{
	double cos_dec = cos(v->dec);

	/* spherical XYZ on unit vector */
	v->x = cos_dec * sin(v->ra);
	v->y = sin(v->dec);
	v->z = cos_dec * cos(v->ra);

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

#define htm_dump_trixel(htm, trixel)                                    \
	{                                                                   \
		adb_htm_debug(htm, ADB_LOG_HTM_GET, "H:%s Q:%d D:%d P:%x\n",    \
					  trixel->hemisphere ? "S" : "N", trixel->quadrant, \
					  trixel->depth, trixel->position);                 \
	}

#define htm_dump_trixel_objects(htm, trixel, idx)                       \
	{                                                                   \
		adb_htm_debug(htm, ADB_LOG_HTM_GET,                             \
					  "H:%s Q:%d D:%d P:%x objects %d\n",               \
					  trixel->hemisphere ? "S" : "N", trixel->quadrant, \
					  trixel->depth, trixel->position,                  \
					  trixel->data[idx].num_objects);                   \
	}

#endif /* TABLE_HTM_H_ */

#endif
