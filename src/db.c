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

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <libastrodb/object.h>
#include <libastrodb/db.h>
#include "debug.h"
#include "config.h"

static const char *dirs[] = {
	"/I",
	"/II",
	"/III",
	"/IV",
	"/V",
	"/VI",
	"/VII",
	"/VIII",
	"/IX",
	"/B",
};

static int create_lib_local_dirs(struct adb_library *lib, const char *location)
{
	char dir[ADB_PATH_SIZE];
	struct stat stat_info;
	int i, ret = 0;

	/* create <path>/ */
	snprintf(dir, ADB_PATH_SIZE, "%s", location);
	if (stat(location, &stat_info) < 0) {
		if ((ret = mkdir(dir, S_IRWXU | S_IRWXG)) < 0) {
			astrolib_error(lib, "failed to create directory %s %d\n",
						dir, -errno);
			return errno;
		}
	}

	/* create sub dirs */
	for (i = 0; i < adb_size(dirs); i++) {
		snprintf(dir, ADB_PATH_SIZE, "%s%s", location, dirs[i]);
		if (stat(dir, &stat_info) < 0) {
			if ((ret = mkdir(dir, S_IRWXU | S_IRWXG)) < 0) {
				astrolib_error(lib, "failed to create directory %s %d\n",
						dir, -errno);
				return errno;
			}
		}
	}

	return ret;
}

/*! \fn adb_library* adb_open_library(char* remote, char* local)
 * \param local Local library repository location
 * \param remote Remote repository location
 * \returns A adb_library object or NULL on failure
 *
 * Initialises a CDS library structure on disk at location specified.
 *
 * This typically only needs to be called once per program.
 */

struct adb_library *adb_open_library(const char *host,
	const char *remote, const char *local)
{
	struct adb_library *lib;
	int err;

	if (local == NULL)
		return NULL;

	lib = calloc(1, sizeof(struct adb_library));
	if (lib == NULL)
		return NULL;

	err = create_lib_local_dirs(lib, local);
	if (err < 0)
		goto err;

	lib->local = strdup(local);
	if (lib->local == NULL)
		goto err;
	lib->host = strdup(host);
	if (lib->host == NULL)
		goto err;
	lib->remote = strdup(remote);
	if (lib->remote == NULL)
		goto err;
	return lib;

err:
	free(lib->local);
	free(lib->remote);
	free(lib);
	return NULL;
}


/*! \fn void adb_close_library(adb_library* lib)
 * \param lib Library to free
 *
 * Free's all library resources.
 */
void adb_close_library(struct adb_library * lib)
{
	free(lib->remote);
	free(lib->local);
	free(lib);
}

void adb_set_msg_level(struct adb_db *db, enum adb_msg_level level)
{
	db->msg_level = level;
	db->htm->msg_level = level;
}

void adb_set_log_level(struct adb_db *db, unsigned int log)
{
	db->msg_flags = log;
	db->htm->msg_flags = log;
}

/*! \fn adb_db* adb_create_db(struct adb_library* lib,
 *
 */
struct adb_db *adb_create_db(struct adb_library *lib,
		int depth, int tables)
{
	struct adb_db *db;

	db = (struct adb_db *) calloc(1, sizeof(struct adb_db));
	if (db == NULL)
		return NULL;
	db->lib = lib;
	db->msg_level = ADB_MSG_NONE;
	db->msg_flags = 0;

	db->htm = htm_new(depth, tables);
	if (db->htm == NULL) {
		astrolib_error(lib, "failed to create DB with HTM depth of"
			"%d degrees and %d tables\n", depth, tables);
		free(db);
		return NULL;
	}

	return db;
}

/*! \fn void adb_db_free (adb_db *db)
 * \param db Catalog
 *
 * Free's all catalog resources
 */
void adb_db_free(struct adb_db *db)
{
	// TODO: free tables and htm
	htm_free(db->htm);
	free(db);
}

/*! \fn const char* adb_get_version(void);
 * \return libastrodb version
 *
 * Get the libastrodb version number.
 */
const char *adb_get_version(void)
{
	return VERSION;
}
