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
 *  Copyright (C) 2005 Liam Girdwood
 */

#ifndef __LNC_README_H
#define __LNC_README_H

#define RM_BYTE_TYPE_SIZE		8
#define RM_BYTE_UNIT_SIZE		8
#define RM_BYTE_LABEL_SIZE		16
#define RM_BYTE_EXP_SIZE			128

struct adb_db;

/*! \struct adb_table_column_info
 * \brief Dataset byte description.
 *
 * Describes a field from a CDS dataset.
 *
 * <ul>
 * <li><b>  Start</b>			<i>Field start pos</i></li> 
 * <li><b>  End</b>			<i>Field end pos</i></li>
 * <li><b>  Type</b>			<i>Field type</i></li>
 * <li><b>  Units</b>			<i>Units</i></li>
 * <li><b>  Label</b>			<i>CDS label</i></li>
 * <li><b>  Explanation</b>	<i>Explanation</i></li>
 * </ul>
 */
struct cds_byte_desc { 
	int start;        							/*!< start byte */
	int end;          							/*!< end byte */
	char type[RM_BYTE_TYPE_SIZE];			/*!< field type */
	char units[RM_BYTE_UNIT_SIZE];			/*!< field units */
	char label[RM_BYTE_LABEL_SIZE];		/*!< field label */
	char explanation[RM_BYTE_EXP_SIZE];	/*!< field explanation */
};

#define RM_FILE_NAME_SIZE		32
#define RM_FILE_TITLE_SIZE		80
#define RM_FILE_MAX_DESC		128

/*! \struct adb_table_info
 * \brief Dataset information.
 *
 * Describes a dataset from a CDS catalog ReadMe file_name.
 *
 * <ul>
 * <li><b>  Name</b>		<i>Dataset name</i></li> 
 * <li><b>  Records</b>	<i>Number of records in dataset</i></li>
 * <li><b>  Length</b>	<i>Length of dataset records</i></li>
 * <li><b>  Title</b>		<i>Dataset title</i></li>
 * <li><b>  Byte Desc</b>	<i>Dataset byte description (in dlist)</i></li>
 * </ul>
 */
struct cds_file_info {
	char name[RM_FILE_NAME_SIZE];  		/*!< filename dos 8.3 */
	int records;							/*!< number of records */
	int length;							/*!< maximum line length */
	char title[RM_FILE_TITLE_SIZE];			/*!< short title (80 chars max) */
	struct cds_byte_desc byte_desc[RM_FILE_MAX_DESC];	/*!< Byte by byte desc 9c */
	int num_desc;
};


#define RM_MAX_FILES		128
#define RM_DSGN_SIZE		16
#define RM_TITLE_SIZE		80
#define RM_KEYWORD_SIZE	80
#define RM_DESC_SIZE		2048
#define RM_NOTE_SIZE		1024

struct readme {
	/* catalog description info from ReadMe */
	char designation[RM_DSGN_SIZE];		/*!< Catalog designation 1c*/
	char title[RM_TITLE_SIZE];				/*!< Catalog titles 2c */
	char keywords[RM_KEYWORD_SIZE];		/*!< Catalog keywords 3c */
	char description[RM_DESC_SIZE];			/*!< Catalog description 4c */
										/*!< Observed objects 5o */
	struct cds_file_info file[RM_MAX_FILES];	/*!< Catalog data files 6c */
	int num_files;
										/*!< Related catalogs 7o */
										/*!< Nomenclature 8o */
	/* under files */       						/*!< Byte by byte desc 9c */
	char notes[RM_NOTE_SIZE];				/*!< Global notes 10o */
	int num_notes;
										/*!< History/Acks 11o */
										/*!< References 12o */
										/*!< End (date) */

	int label_offset;
	int explain_offset;
};

struct readme *readme_parse(struct adb_db *db, char* file);
void readme_free(struct readme* info);

int cds_get_readme(struct adb_db *db, int table_id);

int cds_get_split_dataset(struct adb_db *db, struct adb_table *table);

int cds_get_dataset(struct adb_db *db, struct adb_table *table);

int cds_prepare_files(struct adb_db *db, struct adb_table *table,
	const char *ext);

int table_parse_readme(struct adb_db *db, int table_id);

#endif
