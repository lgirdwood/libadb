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

/*! \mainpage libadb
* <A>General Purpose Astronomical Database</A>
*
* \section intro Introduction
* Astrodb is a general purpose astronomical database designed to give very fast
* access to <A href="http://cdsweb.u-strasbg.fr/">CDS</A> astronomical catalog
* data. It is designed to be independent of any underlying catalog data formats
* and will import most CDS data records providing the catalog ships with a
* "ReadMe" file that follows the CDS <A href="http://vizier.u-strasbg.fr/doc/catstd.htx">formatting specifications</A>.
* Astrodb provides a simple C API for catalog access.
*
* The intended audience of libastrodb is C / C++ programmers, astronomers and
* anyone working with large astronomical catalogs.
*
* \section features Features
* Libastrodb currently supports :-
*
* - Import and download CDS catalog object data.
* - Objects stored in hierarchical triangular mesh (HTM) and a K-D tree for
*   fast access.
* - Objects can be queried based on :-
* 		- position.
*		- position and magnitude.
*		- hashed object ID's.
*		- proximity to other objects.
* - Search queries can be constructed using RPN to search the catalog data with
*   multiple conditions on multiple object fields.
* - Image solver that can return catalog objects found in images.
*
* \section docs Documentation
* API documentation for libastrodb is included in the source.
*
* \section licence Licence
* libastrodb is released under the <A href="http://www.gnu.org">GNU</A> LGPL.
*
* \section authors Authors
* libastrodb is maintained by <A href="mailto:lgirdwood@gmail.com">Liam Girdwood</A>
*
*/

#ifndef __LIBADB_H
#define __LIBADB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <libastrodb/db.h>
#include <libastrodb/db-import.h>
#include <libastrodb/object.h>
#include <libastrodb/solve.h>
#include <libastrodb/search.h>

#ifdef __cplusplus
};
#endif

#endif
