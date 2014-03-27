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
 *  Copyright (C) 2005 - 2014 Liam Girdwood
 */

#ifndef __LIBADB_OBJECT_H
#define __LIBADB_OBJECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADB_OBJECT_NAME_SIZE	16

#define adb_offset(x,y) (long)(&((x*)0)->y) 		/* offset in struct */
#define adb_sizeof(x,y) sizeof(((x*)0)->y) 		/* size in struct */
#define adb_size(x) (sizeof(x)/sizeof(x[0]))		/* array size */

struct adb_db;

struct adb_posn_mag {
	double ra;
	double dec;
	float key;
};

struct adb_kd_tree {
	/* KD tree - indexs */
	int32_t child[2], parent, index;
};

struct adb_import {
	/* next object (in Vmag or size) or NULL */
	struct adb_object *next;
	struct adb_kd_tree *kd;
};

struct adb_object {
	/* primary keys for object hash based access */
	union {
		unsigned long id;
		char designation[ADB_OBJECT_NAME_SIZE];
	};
	/* primary keys for object attribute based access */
	struct adb_posn_mag posn_mag;

	union {
		struct adb_import import; /* only used for importing */
		struct adb_kd_tree kd;	/* only used for imported data */
	};

	/* remainder of object here - variable size and type */
};


/****************** Table Clipping ********************************************/

/*! \fn void adb_table_clip_on_fov (adb_table *table, double ra, double dec,
					double clip_fov, double faint_mag,
					double bright_mag);
 * \brief Set dataset clipping area based on field of view
 * \ingroup dataset
 */
struct adb_object_set *adb_table_set_new(struct adb_db *db,
	int table_id);

int adb_table_set_constraints(struct adb_object_set *set,
				double ra, double dec,
				double fov, double min_Z,
				double max_Z);

/*! \fn void adb_table_unclip (adb_table *table);
 * \brief Unclip dataset clipping area to full boundaries
 * \ingroup dataset
 */
void adb_table_set_free(struct adb_object_set *set);



/******************* Table Object Get *****************************************/

#define adb_object_ra(object) object->posn_mag.ra
#define adb_object_dec(object) object->posn_mag.dec
#define adb_object_keyval(object) object->posn_mag.key

struct adb_object_head {
	const void *objects;
	unsigned int count;
};

int adb_table_set_get_objects(struct adb_object_set *set);

/*! \fn void* adb_table_get_object (adb_table *table, char *id, char *field);
 * \brief Get object from catalog based on ID
 * \return pointer to object or NULL
 * \ingroup dataset
 */
int adb_table_set_get_object(struct adb_object_set *set,
	const void *id, const char *field, const struct adb_object **object);

const struct adb_object *adb_table_set_get_nearest_on_object(
	struct adb_object_set *set, const struct adb_object *object);

const struct adb_object *adb_table_set_get_nearest_on_pos(
	struct adb_object_set *set, double ra, double dec);

struct adb_object_head *adb_set_get_head(struct adb_object_set *set);
int adb_set_get_count(struct adb_object_set *set);


#ifdef __cplusplus
};
#endif

#endif
