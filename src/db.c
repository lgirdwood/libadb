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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "debug.h"
#include "libastrodb/db.h"
#include "libastrodb/object.h"

static const char *dirs[] = {
	"/I", "/II", "/III", "/IV", "/V", "/VI", "/VII", "/VIII", "/IX", "/B",
};

/**
 * @brief Creates local subdirectories required for the library repository.
 *
 * Checks if the local base directory exists, creating it if not. Then iterates
 * through the standard CDS sub-catalog directories (/I, /B, etc.) and creates
 * any that are missing to establish the local repository structure.
 *
 * @param lib The active library object (used for logging errors).
 * @param location The base local file path of the repository.
 * @return 0 on success, or a positive errno value on failure.
 */
static int create_lib_local_dirs(struct adb_library *lib, const char *location)
{
	char dir[ADB_PATH_SIZE];
	struct stat stat_info;
	int i, ret = 0;

	/* create <path>/ */
	snprintf(dir, ADB_PATH_SIZE, "%s", location);
	if (stat(location, &stat_info) < 0) {
		if ((ret = mkdir(dir, S_IRWXU | S_IRWXG)) < 0) {
			astrolib_error(lib, "failed to create directory %s %d\n", dir,
						   -errno);
			return errno;
		}
	}

	/* create sub dirs */
	for (i = 0; i < adb_size(dirs); i++) {
		snprintf(dir, ADB_PATH_SIZE, "%s%s", location, dirs[i]);
		if (stat(dir, &stat_info) < 0) {
			if ((ret = mkdir(dir, S_IRWXU | S_IRWXG)) < 0) {
				astrolib_error(lib, "failed to create directory %s %d\n", dir,
							   -errno);
				return errno;
			}
		}
	}

	return ret;
}

/**
 * @brief Open and initialize a CDS library local repository.
 *
 * Initializes a CDS library structure and ensures the local directory hierarchy
 * (I through IX, B) is created at the specified local path.
 * This typically only needs to be called once per program execution.
 *
 * @param host Host URL or name
 * @param remote Remote repository location/URL
 * @param local Local library repository location on disk
 * @return A pointer to the newly allocated adb_library object, or NULL on
 * failure
 */

struct adb_library *adb_open_library(const char *host, const char *remote,
									 const char *local)
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
	free(lib->host);
	free(lib->local);
	free(lib->remote);
	free(lib);
	return NULL;
}

/**
 * @brief Close a library and release its resources.
 *
 * Frees all memory associated with the library path configurations and the
 * library structure itself.
 *
 * @param lib Library object to free
 */
void adb_close_library(struct adb_library *lib)
{
	free(lib->remote);
	free(lib->local);
	free(lib->host);
	free(lib);
}

/**
 * @brief Sets the global message logging level for the database.
 *
 * Updates both the main database object and its underlying HTM index context
 * with the new message verbosity level.
 *
 * @param db The active catalog database instance.
 * @param level The desired message level enum (e.g., ADB_MSG_INFO).
 */
void adb_set_msg_level(struct adb_db *db, enum adb_msg_level level)
{
	db->msg_level = level;
	db->htm->msg_level = level;
}

/**
 * @brief Configures specific log verbosity feature flags.
 *
 * Sets the bitmask indicating which subsystems should log messages to the console
 * or specified output.
 *
 * @param db The active catalog database instance.
 * @param log Bitmask of features to log (e.g. ADB_LOG_SEARCH | ADB_LOG_SOLVE).
 */
void adb_set_log_level(struct adb_db *db, unsigned int log)
{
	db->msg_flags = log;
	db->htm->msg_flags = log;
}

/**
 * @brief Create a new database catalog instance.
 *
 * Allocates a new database structure associated with the specified library.
 * It also initializes the underlying HTM (Hierarchical Triangular Mesh) index
 * structure with the specified depth and table capacity.
 *
 * @param lib Library repository to bind the database to
 * @param depth HTM resolution depth
 * @param tables Number of tables
 * @return A pointer to the newly allocated adb_db object, or NULL on failure
 */
struct adb_db *adb_create_db(struct adb_library *lib, int depth, int tables)
{
	struct adb_db *db;

	db = (struct adb_db *)calloc(1, sizeof(struct adb_db));
	if (db == NULL)
		return NULL;
	db->lib = lib;
	db->msg_level = ADB_MSG_INFO;
	db->msg_flags = ADB_LOG_SEARCH | ADB_LOG_SOLVE;

	db->htm = htm_new(depth, tables);
	if (db->htm == NULL) {
		astrolib_error(lib,
					   "failed to create DB with HTM depth of"
					   "%d degrees and %d tables\n",
					   depth, tables);
		free(db);
		return NULL;
	}

	return db;
}

/**
 * @brief Free an active catalog database.
 *
 * Frees all resources allocated by the catalog database, including its
 * underlying HTM structures.
 *
 * @param db Catalog database to free
 */
void adb_db_free(struct adb_db *db)
{
	// TODO: free tables and htm
	htm_free(db->htm);
	free(db);
}

/**
 * @brief Retrieve the library version string.
 *
 * Returns the compile-time version tag of libastrodb.
 *
 * @return A constant string representing the libastrodb version number.
 */
const char *adb_get_version(void)
{
	return VERSION;
}
