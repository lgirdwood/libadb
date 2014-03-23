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
 *  Copyright (C) 2008,2012 Liam Girdwood
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <libastrodb/db.h>
#include <libastrodb/astrodb.h>
#include <libastrodb/readme.h>
#include <libastrodb/adbstdio.h>
#include <libastrodb/private.h>

#define README_LINE_SIZE		(80 + 4)
#define FORMAT_SIZE			32

static inline void skiplines(FILE *fp, int lines)
{
	char line[README_LINE_SIZE], *end;
	
	while (lines--) {
		end = fgets(line, README_LINE_SIZE - 2, fp) ;
		if (end == NULL)
			break;
	}
} 

static inline int get_string_at_pos(char *src, int pos, char *dest, int len)
{
	/* skip to pos n*/
	const char * _src = src + pos;
	
	/* skip white space */
	while (*_src == ' ' ) 
		_src++;

	/* copy word at offset */
	while (*_src != 0 && len--) {
		*dest = *_src;
		dest++;
		_src++;
	}
	*dest = 0;
	
	return 1;	
}

static inline int get_word_at_pos(char *src, int pos, char *dest, int len)
{
	/* skip to pos n*/
	const char * _src = src + pos;

	/* some word fields our out of position by being 1 char early */
	if (pos > 0) {
		if (*(_src -1) != ' ' && *_src != ' ')
			_src--;
	}

	/* skip white space */
	while (*_src == ' ') 
		_src++;
	
	/* copy word at offset */
	while (*_src != ' ' && len--) {
		*dest = *_src;
		dest++;
		_src++;
	}
	*dest = 0;
	
	return 1;
}

static inline int get_int_at_pos(char *src, int pos, int *dest, int len)
{
	/* skip to pos n*/
	const char * _src = src + pos;

	*dest = atoi(_src);
	return 1;
} 


 /* Find section header <num> - copy section details into hdr_data (if non NULL)  */
static int find_header(char *header, FILE *fp, char *hdr_data)
{
	char line[README_LINE_SIZE], *end;

	do {
		/* read in line */
		end = fgets(line, README_LINE_SIZE - 2, fp) ;
		if (end == NULL)
			break;

		/* is it our header */ 
		if (strncmp(header, line, strlen(header)))
			continue;

		/* Is the header at the correct position ? */
		if (hdr_data) {
			/* copy back header data field (if any) */
			strncpy(hdr_data, line + strlen(header) + 1, 80);
			*(hdr_data + strlen(hdr_data) - 1) = 0;	/* remove cr */
		}
		return 1;

	} while (1);
	
	return 0;
}

/* Get catalog designation */
static int get_designation(struct readme *readme, FILE *fp)
{
	char line[README_LINE_SIZE], *end;
	
	/* designation is always first line of ReadMe */
	rewind(fp);
	
	end = fgets(line, README_LINE_SIZE - 2, fp) ;
	if (end == NULL)
		return 0;

	strncpy(readme->designation, line, 6);
	return  0;
}

static int get_titles(struct readme *info, FILE *f)
{
	
	return 0;
}

static int get_keywords(struct readme *info, FILE *fp)
{
	
	return 0;
}


static int get_description(struct readme *info, FILE *fp)
{
	
	return 0;
}

#define FILE_NAME_OFFSET			0
#define FILE_LENGTH_OFFSET			13
#define FILE_RECORDS_OFFSET			21
#define FILE_EXPLANATION_OFFSET		31

/* read all table files from readMe */
static int get_files(struct adb_db *db, struct readme *readme, FILE *fp)
{
	char line[README_LINE_SIZE], *end;
	struct cds_file_info *file_info;
	int files = 0;

	rewind(fp);
	
	if (find_header("File Summary:", fp, NULL) < 0) {
		adb_error(db, "failed to find File Summary\n");
		return -ENODATA;
	}
	
	/* read and skip in 3 lines of header */
	skiplines(fp, 3);
	
	do {
		file_info = &readme->file[files];		
	
		end = fgets(line, README_LINE_SIZE - 2, fp) ;
		if (end == NULL || *line == '-')
			break;

		if (*line == ' ')
			continue;

		/* get name */
		get_word_at_pos(line, FILE_NAME_OFFSET,
				file_info->name, sizeof(line));
		/* get length */
		get_int_at_pos(line, FILE_LENGTH_OFFSET,
				&file_info->length, sizeof(line));
		/* get records */
		get_int_at_pos(line, FILE_RECORDS_OFFSET,
				&file_info->records, sizeof(line));
		/* get explanation */
		get_string_at_pos(line, FILE_EXPLANATION_OFFSET,
				file_info->title, sizeof(line));
		files++;

	} while (files < RM_MAX_FILES);

	adb_debug(db, ADB_LOG_CDS_PARSER, "found %d files in ReadMe\n", files);
	return files;
}

#define BYTE_START_OFFSET			0
#define BYTE_END_OFFSET				5
#define BYTE_TYPE_OFFSET				10
#define BYTE_UNITS_OFFSET			15
#define BYTE_LABEL_OFFSET			23
#define BYTE_EXPLANATION_OFFSET		32

/*
 * Format, Label  and Explanation line offsets vary between files.
 */
static int get_byte_desc_offset(struct adb_db *db,
	struct readme *readme, int file_id,  FILE *fp)
{
	char line[README_LINE_SIZE], * end, *offset;

	bzero(line, sizeof(line));

	end = fgets(line, README_LINE_SIZE - 2, fp) ;
	if (end == NULL)
		return -EINVAL;

	offset = strstr(line, "Label");
	if (offset == NULL) {
		adb_error(db, "cant find Label in byte offset\n");
		return -EINVAL;
	}
	readme->label_offset = offset - line;

	offset = strstr(line, "Explanation");
	if (offset == NULL) {
		adb_error(db, "cant find Explanation in byte offset\n");
		return -EINVAL;
	}
	readme->explain_offset = offset - line;

	return 0;
}

/* Parse a single byte by byte description */
static int get_byte_desc(struct adb_db *db, struct readme *readme,
	int file_id,  FILE *fp)
{
	char line[README_LINE_SIZE], cont[README_LINE_SIZE], *end;
	struct cds_byte_desc *byte_desc;
	int desc = 0,  err;
	
	cont[0] = ' ';
	
	/* read and skip in 1 lines of header */
	skiplines(fp, 1);

	err = get_byte_desc_offset(db, readme, file_id, fp);
	if (err < 0)
		return err;

	/* read and skip in 1 lines of header */
	skiplines(fp, 1);

	do {
		byte_desc = &readme->file[file_id].byte_desc[desc];
			
		bzero(line, sizeof(line));
		end = fgets(line, README_LINE_SIZE - 2, fp) ;
		if (end == NULL || *line == '-')
			break;
	
		get_int_at_pos(line, BYTE_START_OFFSET, &byte_desc->start, sizeof(line));
		get_int_at_pos(line, BYTE_END_OFFSET, &byte_desc->end, sizeof(line));

		if (byte_desc->start == 0 && byte_desc->end == 0) {

			/* continuation */
			byte_desc = &readme->file[file_id].byte_desc[--desc];
			get_string_at_pos(line, 0, cont + 1, sizeof(cont) - 1);
			strncat(byte_desc->explanation, cont, 
				RM_BYTE_EXP_SIZE - strlen(byte_desc->explanation) - 1);

		} else if (byte_desc->end > byte_desc->start) {

			/* multiple bytes */
			get_word_at_pos(line, BYTE_TYPE_OFFSET, 
							byte_desc->type, sizeof(line));
			get_word_at_pos(line, BYTE_UNITS_OFFSET, 
							byte_desc->units, sizeof(line));
			get_word_at_pos(line, readme->label_offset,
							byte_desc->label, sizeof(line));
			get_string_at_pos(line,readme->explain_offset,
							byte_desc->explanation, sizeof(line));
							
			/* subtract 1 from start and end to align */
			byte_desc->start --;
			byte_desc->end --;

		} else {

			/* single byte */
			get_word_at_pos(line, BYTE_TYPE_OFFSET , 
							byte_desc->type, sizeof(line));
			get_word_at_pos(line, BYTE_UNITS_OFFSET, 
							byte_desc->units, sizeof(line));
			get_word_at_pos(line, BYTE_LABEL_OFFSET, 
							byte_desc->label, sizeof(line));
			get_string_at_pos(line, BYTE_EXPLANATION_OFFSET, 
							byte_desc->explanation, sizeof(line));
			
			/* align end */
			byte_desc->end = byte_desc->start--;
		}
		
		adb_debug(db, ADB_LOG_CDS_PARSER, " %d...%d is %s of (%s) at %s : %s",
			byte_desc->start, byte_desc->end, byte_desc->type,
			byte_desc->units, byte_desc->label, byte_desc->explanation);
		
		desc++;
	} while (desc < RM_MAX_FILES);
	
	adb_debug(db, ADB_LOG_CDS_PARSER, "read %d descriptors\n", desc);
	return desc;
}

/* match table file name to array position */
static inline int get_file_id(struct readme *readme, char *table_name)
{
	int i;
	
	for (i = 0; i< readme->num_files; i++) {
		if (strstr(table_name, readme->file[i].name))
			return i;
	}
	return -1;
}

/* parse all ReadMe byte-by-byte descriptions */
static int get_byte_description(struct adb_db *db, struct readme *readme, FILE *fp)
{
	struct cds_file_info *file_info;
	char table_name[README_LINE_SIZE];
	int file = 0;
	
	adb_info(db, ADB_LOG_CDS_PARSER, "Parsing byte descriptions\n");

	rewind(fp);
	
	do {

		if (find_header("Byte-by-byte Description of file:", fp, table_name)) {

			adb_info(db, ADB_LOG_CDS_PARSER," found description for %s\n", table_name);
			file = get_file_id(readme, table_name);
			if (file < 0) {
				adb_info(db, ADB_LOG_CDS_PARSER," %s not required\n", table_name);
				continue;
			}

			file_info = &readme->file[file];
			file_info->num_desc = get_byte_desc(db, readme, file, fp);

		} else
			break;
	} while (1);

	rewind(fp);
	do {
 		if (find_header("Byte-per-byte Description of file:", fp, table_name)) {

			adb_info(db, ADB_LOG_CDS_PARSER," found description for %s\n", table_name);
			file = get_file_id(readme, table_name);
			if (file < 0) {
				adb_info(db, ADB_LOG_CDS_PARSER," %s not required\n", table_name);
				continue;
			}

			file_info = &readme->file[file];
			file_info->num_desc = get_byte_desc(db, readme, file, fp);

		} else
			break;
	} while (1);

	adb_info(db, ADB_LOG_CDS_PARSER,"Parsing byte descriptions done\n");
	return 0;
}

struct readme *readme_parse(struct adb_db *db, char *file)
{
	struct readme *readme;
	FILE *fp;

	adb_debug(db, ADB_LOG_CDS_PARSER, "about to parse %s\n", file);

	fp = fopen(file, "r");
	if (fp == NULL) {
		adb_error(db, "failed to open %s\n", file);
		return NULL;
	}

	readme = (struct readme *) 
		calloc(1, sizeof(struct readme));
	if (readme == NULL) {
		fclose(fp);
		return NULL;
	}

	get_designation(readme, fp);
	get_titles(readme, fp);
	get_keywords(readme, fp);
	get_description(readme, fp);

	readme->num_files = get_files(db, readme, fp);
	if (readme->num_files < 0) {
		adb_error(db, "failed to find any data files\n");
		free(readme);
		readme = NULL;
		goto out;
	}

	get_byte_description(db, readme, fp);

out:
	fclose(fp);
	return readme;
}

void readme_free(struct readme *readme)
{
	free(readme);
}

int table_parse_readme(struct adb_db *db, int table_id)
{
	struct adb_table *table = &db->table[table_id];
	char file[ADB_PATH_SIZE];

	snprintf(file, ADB_PATH_SIZE, "%s%s", table->path.local, "ReadMe");
	adb_info(db, ADB_LOG_CDS_PARSER,"Parsing catalog ReadMe: %s\n", file);

	table->import.readme = readme_parse(db, file);
	if (table->import.readme == NULL) {
		adb_error(db, "failed to parse ReadMe\n");
		return -EINVAL;
	}

	return 0;
}
