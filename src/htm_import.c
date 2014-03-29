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
 *  Copyright (C) 2008 - 2014 Liam Girdwood
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#include <libastrodb/db.h>
#include <libastrodb/db-import.h>
#include <libastrodb/object.h>
#include "table.h"
#include "debug.h"

struct kd_elem {
	uint32_t id;
	uint32_t child[2];
	struct adb_object *object;
};

void htm_import_object_ascending(struct adb_db *db,
	struct htm_trixel *trixel, struct adb_object *new_object,
	struct adb_table *table)
{
	struct adb_object *last, *current, *head;
	int table_id = table->id;

	head = trixel->data[table_id].objects;

	/* is object first in trixel ?, then make it HEAD */
	if (head == NULL) {
		trixel->data[table_id].objects = new_object;
		trixel->data[table_id].num_objects++;
		return;
	}
	last = current = head;

	/* import object into trixel, brightest at HEAD -> faintest at TAIL */
	while (current) {

		/* is new object lower than HEAD object ? */
		if (adb_object_keyval(new_object) <= adb_object_keyval(current)) {

			/* is object new HEAD ?*/
			if (current == head) {
				/* import current as new HEAD */
				trixel->data[table_id].objects = new_object;
				new_object->import.next = head;
			} else {
				/* import new object */
				new_object->import.next = current;
				last->import.next = new_object;
			}
			goto out;
		}

		/* goto next object in list */
		last = current;
		current = current->import.next;
	}

	/* object is faintest, so import object into TAIL */
	last->import.next = new_object;

out:
	trixel->data[table_id].num_objects++;
	adb_vdebug(db, ADB_LOG_CDS_IMPORT,
		"imported object at RA %3.3f DEC %3.3f size %f"
		" -> %sQ%dD%dP%x with objs %d\n",
		adb_object_ra(new_object) * R2D, adb_object_dec(new_object) * R2D,
		adb_object_keyval(new_object),
		trixel->hemisphere ? "S" : "N", trixel->quadrant, trixel->depth,
		trixel->position, trixel->data[table_id].num_objects);
}

void htm_import_object_descending(struct adb_db *db,
	struct htm_trixel *trixel, struct adb_object *new_object,
	struct adb_table *table)
{
	struct adb_object *last, *current, *head;
	int table_id = table->id;

	head = trixel->data[table_id].objects;

	/* is object first in trixel ?, then make it HEAD */
	if (head == NULL) {
		trixel->data[table_id].objects = new_object;
		trixel->data[table_id].num_objects++;
		return;
	}
	last = current = head;

	/* import object into trixel, brightest at HEAD -> faintest at TAIL */
	while (current) {

		/* is new object higher than HEAD object ? */
		if (adb_object_keyval(new_object) >= adb_object_keyval(current)) {

			/* is object new HEAD ?*/
			if (current == head) {
				/* import current as new HEAD */
				trixel->data[table_id].objects = new_object;
				new_object->import.next = head;
			} else {
				/* import new object */
				new_object->import.next = current;
				last->import.next = new_object;
			}
			goto out;
		}

		/* goto next object in list */
		last = current;
		current = current->import.next;
	}

	/* object is faintest, so import object into TAIL */
	last->import.next = new_object;

out:
	trixel->data[table_id].num_objects++;
	adb_vdebug(db, ADB_LOG_CDS_IMPORT,
		"imported object at RA %3.3f DEC %3.3f size %f"
		" -> %sQ%dD%dP%x with objs %d\n",
		adb_object_ra(new_object) * R2D, adb_object_dec(new_object) * R2D,
		adb_object_keyval(new_object),
		trixel->hemisphere ? "S" : "N", trixel->quadrant, trixel->depth,
		trixel->position, trixel->data[table_id].num_objects);
}

