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

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifndef __ADB_CDS_H
#define __ADB_CDS_H

/*
 * An object representing a local catalog repository.
 *
 * A local library is a mirror of a remote CDS mirror in
 * structure. Catalogs can then be downloaded in part or in whole
 * on a need by need basis to populate the library.
 *
 * CDS directory structure mirrored:-
 *
 *  - I/number  		Astrometric Catalogues
 *  - II/number 		Photometric Catalogues (except Radio)
 *  - III/number	 	Spectroscopic Catalogues
 *  - IV/number 		Cross-Identifications
 *  - V/number 			Combined Data
 *  - VI/number 		Miscellaneous Catalogues
 *  - VII/number 		Non-stellar Objects
 *  - VIII/number 		Radio Catalogues
 *  - IX/number 		High Energy Catalogues
 */

struct table_cds {
	char *cat_class; /*!< catalog class */
	char *index; /*!< catalog number (in repo) */
	char *host; /*!< remote host */
	char *name; /*!< table CDS file name */
};

struct table_path {
	/* file paths */
	char *local; /*!< local catalog path */
	char *remote; /*!< remote catalog path */
	char *file; /*!< table local filename */
};

#endif

#endif
