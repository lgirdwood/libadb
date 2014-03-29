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
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <libastrodb/db.h>
#include <libastrodb/object.h>
#include "htm.h"
#include "table.h"
#include "debug.h"

struct trixel_hdr {
	u_int32_t id;
	u_int32_t num_objects;
} __attribute__((packed));

static int read_trixel(struct adb_db *db, struct adb_table *table,
	struct adb_object *objects, FILE *f, struct trixel_hdr *hdr)
{
	size_t size;

	size = fread(objects, table->object.bytes, hdr->num_objects, f);
	if (size != hdr->num_objects) {
		adb_error(db, "read %d objects expected %d\n", size,
			hdr->num_objects);
		return -EINVAL;
	}

	/* insert objects into HTM */
	htm_table_insert_object(db->htm, table, objects,
			hdr->num_objects, hdr->id);
	return hdr->num_objects;
}

static int read_trixels(struct adb_db *db, struct adb_table *table,
	void *objects, FILE *f)
{
	struct trixel_hdr hdr;
	size_t size;
	int ret, count = 0;

	/* read in trixel hdr */
	size = fread(&hdr, sizeof(hdr), 1, f);
	while (size > 0) {

		adb_vdebug(db, ADB_LOG_HTM_FILE,
			"read trixel %sQ%dD%dP%x with objs %d\n",
			htm_trixel_north(hdr.id) ? "N" : "S",
			htm_trixel_quadrant(hdr.id),
			htm_trixel_depth(hdr.id),
			htm_trixel_position(hdr.id, htm_trixel_depth(hdr.id)),
			hdr.num_objects);

		if (!htm_trixel_valid(hdr.id)) {
			adb_error(db, "Error invalid trixel ID %x\n", hdr.id);
			return -EINVAL;
		}

		ret = read_trixel(db, table, objects, f, &hdr);
		if (ret < 0) {
			adb_error(db, "failed to read trixel %x\n", hdr.id);
			return ret;
		}

		count += ret;
		objects += hdr.num_objects * table->object.bytes;

		size = fread(&hdr, sizeof(hdr), 1, f);
	}

	return count;
}

static int write_trixel(struct adb_db *db, struct adb_table *table,
		struct htm_trixel *trixel, FILE *file, int table_id)
{
	struct adb_object *object, *object_next;
	struct adb_kd_tree *kd;
	struct trixel_hdr hdr;
	int count = 0, _count;
	size_t size;

	if (!trixel)
		return 0;

	/* write children if this trixel has no objects */
	if (!trixel->data[table_id].num_objects)
		goto children;

	/* write trixel header */
	hdr.id = 1 << HTM_ID_VALID_SHIFT |
			trixel->hemisphere << HTM_ID_HEMI_SHIFT |
			trixel->quadrant << HTM_ID_QUAD_SHIFT |
			trixel->depth << HTM_ID_DEPTH_SHIFT |
			trixel->position;
	hdr.num_objects = trixel->data[table_id].num_objects;

	size = fwrite(&hdr, sizeof(struct trixel_hdr), 1, file);
	if (size == 0)
		return 0;

	/* write objects */
	object = trixel->data[table_id].objects;
	while (object) {
		kd = object->import.kd;
		object_next = object->import.next;

		/* copy KD tree data */
		memcpy(&object->kd, object->import.kd, sizeof(struct adb_kd_tree));
		free(kd);

		/* write object + KD data to file */
		size = fwrite(object, table->object.bytes, 1, file);
		if (size == 0)
			return 0;

		object = object_next;
		count++;
	}
	count = trixel->data[table_id].num_objects;

	if (count != trixel->data[table_id].num_objects) {
		adb_error(db, "wrote %d expected %d ", count,
			trixel->data[table_id].num_objects);
		adb_error(db, "for trixel %x\n", hdr.id);
		return -EINVAL;
	}

children:
	if (!trixel->child)
		return count;

	_count = write_trixel(db, table, &trixel->child[0], file, table_id);
	if (_count < 0)
		return _count;
	count += _count;
	_count = write_trixel(db, table, &trixel->child[1], file, table_id);
	if (_count < 0)
		return _count;
	count += _count;
	_count = write_trixel(db, table, &trixel->child[2], file, table_id);
	if (_count < 0)
		return _count;
	count += _count;
	_count = write_trixel(db, table, &trixel->child[3], file, table_id);
	if (_count < 0)
		return _count;
	count += _count;
	return count;
}

int table_read_trixels(struct adb_db *db, struct adb_table *table,
	int table_id)
{
	struct adb_object *objects;
	int count;
	char file[ADB_PATH_SIZE];
	FILE *f;

	if (table->object.bytes <= 0) {
		adb_error(db, "Error invalid object size\n", table->object.bytes);
		return -EINVAL;
	}

	/* allocate object buffer */
	objects = calloc(table->object.count, table->object.bytes);
	if (objects == NULL)
		return -ENOMEM;

	sprintf(file, "%s%s%s", table->path.local, table->path.file, ".db");
	adb_info(db, ADB_LOG_HTM_FILE, "Reading table objects from %s\n", file);

	/* open DB table file for reading */
	f = fopen(file, "r");
	if (f == NULL) {
		adb_error(db, "Error can't open table file %s for reading\n", file);
		free(objects);
		return -EIO;
	}

	adb_info(db, ADB_LOG_HTM_FILE,
		"Reading %d objects from %s with object size %d bytes\n",
		table->object.count, file, table->object.bytes);

	/* read in table rows */
	count = read_trixels(db, table, objects, f);
	fclose(f);

	adb_info(db, ADB_LOG_HTM_FILE, "Read and inserted %d objects\n", count);
	table->objects = objects;
	return count;
}

int table_write_trixels(struct adb_db *db, struct adb_table *table,
	int table_id)
{
	struct htm *htm = db->htm;
	int count = 0, count_;
	char file[ADB_PATH_SIZE];
	FILE *f;

	sprintf(file, "%s%s%s", table->path.local, table->path.file, ".db");
	adb_info(db, ADB_LOG_HTM_FILE, "Writing table objects to %s\n", file);

	f = fopen(file, "w+");
	if (f == NULL) {
		adb_error(db, "Error can't open table file %s for writing\n", file);
		return -EIO;
	}

	count_ = write_trixel(db, table, &htm->N[0], f, table_id);
	if (count_ < 0)
		goto err;
	adb_info(db, ADB_LOG_HTM_FILE, " wrote %d N0 objects\n", count_);
	count += count_;
	count_= write_trixel(db, table, &htm->N[1], f, table_id);
	if (count_ < 0)
		goto err;
	adb_info(db, ADB_LOG_HTM_FILE, " wrote %d N1 objects\n", count_);
	count += count_;
	count_ = write_trixel(db, table, &htm->N[2], f, table_id);
	if (count_ < 0)
		goto err;
	adb_info(db, ADB_LOG_HTM_FILE, " wrote %d N2 objects\n", count_);
	count += count_;
	count_ = write_trixel(db, table, &htm->N[3], f, table_id);
	if (count_ < 0)
		goto err;
	adb_info(db, ADB_LOG_HTM_FILE, " wrote %d N3 objects\n", count_);
	count += count_;
	count_ = write_trixel(db, table, &htm->S[0], f, table_id);
	if (count_ < 0)
		goto err;
	adb_info(db, ADB_LOG_HTM_FILE, " wrote %d S0 objects\n", count_);
	count += count_;
	count_ = write_trixel(db, table, &htm->S[1], f, table_id);
	if (count_ < 0)
		goto err;
	adb_info(db, ADB_LOG_HTM_FILE, " wrote %d S1 objects\n", count_);
	count += count_;
	count_ = write_trixel(db, table, &htm->S[2], f, table_id);
	if (count_ < 0)
		goto err;
	adb_info(db, ADB_LOG_HTM_FILE, " wrote %d S2 objects\n", count_);
	count += count_;
	count_ = write_trixel(db, table, &htm->S[3], f, table_id);
	if (count_ < 0)
		goto err;
	adb_info(db, ADB_LOG_HTM_FILE, " wrote %d S3 objects\n", count_);
	count += count_;

	if (count != table->object.count)
		adb_error(db, "Error wrote %d objects, expected %d\n",
					count, table->object.count);
	fclose(f);
	adb_info(db, ADB_LOG_HTM_FILE, "Wrote %d objects\n", count);
	return count;

err:
	fclose(f);
	unlink(file);
	return count_;
}
