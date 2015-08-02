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

#ifndef __ADB_LIB_H
#define __ADB_LIB_H

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include "cds.h"
#include "table.h"
#include "import.h"
#include "private.h"

/*
 * The library container.
 */
struct adb_library {
	char *local;	/*!< local repository and cache */
	char *remote;	/*!< remote repository */
	char *host;
	unsigned int err; /*!< last error */
};

/*
 * HTM Database with N tables.
 */
struct adb_db {

	struct adb_library *lib;	/*!< catalog parent library n:1 */
	struct htm *htm;

	/* tables */
	struct adb_table table[ADB_MAX_TABLES];   /*!< Catalog datasets */
	int table_in_use[ADB_MAX_TABLES];

	/* logging */
	enum adb_msg_level msg_level;
	int msg_flags;
};

#endif

#endif
