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
 *  Copyright (C) 2008, 2012 Liam Girdwood
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#include <libastrodb/db.h>
#include <libastrodb/table.h>
#include <libastrodb/adbstdio.h>


/* import object into trixel in magnitude order -
 * lowest (brightest) -> highest (faintest)
 */
static inline void htm_insert_object_ascending(struct htm *htm,
	struct adb_table *table, struct htm_trixel *trixel,
	struct adb_object *new_object, unsigned int object_count)
{
	int table_id = table->id;

	trixel->data[table_id].objects = new_object;
	trixel->data[table_id].num_objects = object_count;
}

#if 0
/* import object into trixel in size order -
 * largest -> smallest */
static inline void htm_insert_object_descending(struct htm *htm,
	struct htm_trixel *trixel, struct adb_object *new_object, int table_id)
{
	struct adb_object *head;

	head = trixel->data[table_id].objects;

	/* is object first in trixel ?, then make it HEAD */
	if (head == NULL) {
		trixel->data[table_id].objects = new_object;
		trixel->data[table_id].num_objects = 1;
		trixel->data[table_id].tail = new_object;
		return;
	}

	/* object is faintest, so import object into TAIL */
	trixel->data[table_id].tail->next = new_object;
	trixel->data[table_id].tail = new_object;

	trixel->data[table_id].num_objects++;
}
#endif

/* used by file reader */
int htm_table_insert_object(struct htm *htm, struct adb_table *table,
	struct adb_object *object, unsigned int object_count, 
	unsigned int trixel_id)
{
	struct htm_trixel *trixel;

	if (!htm_trixel_valid(trixel_id)) {
		adb_htm_error(htm, "invalid trixel ID %x\n", trixel_id);
		return -EINVAL;
	}

	trixel = htm_get_trixel(htm, trixel_id);
	if (!trixel) {
		adb_htm_error(htm, "no trixel at %x\n", trixel_id);
		return -EINVAL;
	}

	/* insert obejcts */
	htm_insert_object_ascending(htm, table, trixel, object, object_count);

	return 1;
}

#if 0
/* from client */
int table_insert_object(struct adb_db *db, int table_id,
		struct adb_object *object)
{
	struct htm_vertex vertex;
	struct htm_trixel *trixel;
	struct adb_table *table;
	struct htm *htm = db->htm;
	int depth;

	table = &db->table[table_id];

	depth = htm_get_depth_from_magnitude(htm, adb_object_vmag(object));
	if (depth > table->max_depth) {
		adb_error(db, "object depth %d too deep for table\n", depth);
		return -EINVAL;
	}

	vertex.ra =  adb_object_ra(object);
	vertex.dec = adb_object_dec(object);
	adb_vdebug(db, ADB_LOG_HTM_INSERT, 
		"inserting object at RA %f DEC %f at depth %d\n",
		vertex.ra * R2D, vertex.dec *R2D, depth);

	trixel = htm_get_home_trixel(db, &vertex, depth);
	if (trixel == NULL) {
		adb_error(db, " invalid trixel at %3.3f:%3.3f depth %d\n",
					vertex.ra * R2D, vertex.dec * R2D, depth);
		return -EINVAL;
	}

	htm_insert_object_ascending(htm, trixel, object, table_id);
	return 0;
}
#endif
