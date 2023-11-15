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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <libastrodb/db.h>
#include "readme.h"
#include "debug.h"
#include "private.h"

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

/* Find section header <num> and then copy section details
 * into hdr_data (if non NULL) */
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

	snprintf(readme->designation, RM_DSGN_SIZE, "%.*s", RM_DSGN_SIZE - 1, line);

	/* find next space and pad zeros */
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
		int n;

		file_info = &readme->file[files];

		end = fgets(line, README_LINE_SIZE - 2, fp) ;
		if (end == NULL || *line == '-')
			break;

		if (*line == ' ')
			continue;

		n = sscanf(line, "%s %d %d %80c", file_info->name,
				&file_info->length, &file_info->records, file_info->title);
		if (n != 4)
			continue;

		files++;

	} while (files < RM_MAX_FILES);

	adb_debug(db, ADB_LOG_CDS_PARSER, "found %d files in ReadMe\n", files);
	return files;
}

/*
 * Format, Label and Explanation line offsets vary between files.
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
		int n;

		byte_desc = &readme->file[file_id].byte_desc[desc];

		bzero(line, sizeof(line));
		end = fgets(line, README_LINE_SIZE - 2, fp) ;
		if (end == NULL || *line == '-')
			break;

		/* try "%d-%d" start-end format */
		n = sscanf(line, "%d- %d %s %s %s %128c",
				&byte_desc->start, &byte_desc->end, byte_desc->type,
				byte_desc->units, byte_desc->label, byte_desc->explanation);
		if (n == 6) {
			byte_desc->start--;
			byte_desc->end--;
			goto next;
		}

		/* try "%d %d" start-end format */
		n = sscanf(line, "%d %d %s %s %s %128c",
				&byte_desc->start, &byte_desc->end, byte_desc->type,
				byte_desc->units, byte_desc->label, byte_desc->explanation);
		if (n == 6) {
			byte_desc->start--;
			byte_desc->end--;
			goto next;
		}

		/* try "%d" start-end format */
		n = sscanf(line, "%d %s %s %s %128c",
				&byte_desc->start, byte_desc->type,
				byte_desc->units, byte_desc->label, byte_desc->explanation);
		if (n == 5) {
			byte_desc->end = byte_desc->start--;
			goto next;
		}

		/* continuation of explanation */
		byte_desc = &readme->file[file_id].byte_desc[--desc];
		sscanf(line, " %128c", cont);
		strncat(byte_desc->explanation, cont,
			RM_BYTE_EXP_SIZE - strlen(byte_desc->explanation) - 1);

next:
		adb_debug(db, ADB_LOG_CDS_PARSER, " %d...%d is %s of (%s) at %s : %s\n",
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
static int get_byte_description(struct adb_db *db, struct readme *readme,
	FILE *fp)
{
	struct cds_file_info *file_info;
	char table_name[README_LINE_SIZE];
	int file = 0;

	adb_info(db, ADB_LOG_CDS_PARSER, "Parsing byte descriptions\n");

	rewind(fp);

	do {

		if (find_header("Byte-by-byte Description of file:", fp, table_name)) {

			adb_info(db, ADB_LOG_CDS_PARSER," found description for %s\n",
				table_name);
			file = get_file_id(readme, table_name);
			if (file < 0) {
				adb_info(db, ADB_LOG_CDS_PARSER," %s not required\n",
					table_name);
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

			adb_info(db, ADB_LOG_CDS_PARSER," found description for %s\n",
				table_name);
			file = get_file_id(readme, table_name);
			if (file < 0) {
				adb_info(db, ADB_LOG_CDS_PARSER," %s not required\n",
					table_name);
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
