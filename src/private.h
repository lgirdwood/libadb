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
 *  Copyright (C) 2010 - 2012 Liam Girdwood
 */

#ifndef PRIVATE_H_
#define PRIVATE_H_

#ifndef DOXYGEN_SHOULD_SKIP_THIS

/*
 * libastrodb file format version
 */
#define ADB_IDX_VERSION		3

/* magic numbers */
#define ADB_CAT_MAGIC		0x0  /*!< magic number for binary data format */

#define D2R  (1.7453292519943295769e-2)  /* deg->radian */
#define R2D  (5.7295779513082320877e1)   /* radian->deg */

#define ADB_FLOAT_SIZE		'6'  /* CDS F string size and below for float else double */
#define ADB_PATH_SIZE		1024
#define ADB_MAX_TABLES		16
#define ADB_IMPORT_LINE_SIZE	1024

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#endif /* PRIVATE_H_ */

#endif
