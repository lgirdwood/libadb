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
#include <ctype.h> // IWYU pragma: keep
#include <errno.h> // IWYU pragma: keep

#include "libastrodb/db-import.h"
#include "libastrodb/db.h"
#include "libastrodb/object.h"
#include "readme.h"
#include "debug.h"
#include "private.h"

#define README_LINE_SIZE (80 + 4)
#define FORMAT_SIZE 32

/**
 * @brief Skips a specified number of lines in a file.
 *
 * @param fp The file pointer to read from.
 * @param lines The number of lines to skip.
 */
static inline void skiplines(FILE *fp, int lines)
{
	char line[README_LINE_SIZE], *end;

	while (lines--) {
		end = fgets(line, README_LINE_SIZE - 2, fp);
		if (end == NULL)
			break;
	}
}

/**
 * @brief Finds a specific section header in the ReadMe file.
 *
 * Scans the file line by line until it finds a line starting with the given header.
 * If `hdr_data` is not NULL, it copies the rest of the line (after the header) into it.
 *
 * @param header The string to search for at the beginning of a line.
 * @param fp The file pointer to the ReadMe.
 * @param hdr_data Optional buffer to store the data following the header on the same line.
 * @return 1 if the header is found, 0 if EOF is reached without finding it.
 */
static int find_header(char *header, FILE *fp, char *hdr_data)
{
	char line[README_LINE_SIZE], *end;

	do {
		/* read in line */
		end = fgets(line, README_LINE_SIZE - 2, fp);
		if (end == NULL)
			break;

		/* is it our header */
		if (strncmp(header, line, strlen(header)))
			continue;

		/* Is the header at the correct position ? */
		if (hdr_data) {
			/* copy back header data field (if any) */
			strncpy(hdr_data, line + strlen(header) + 1, 80);
			*(hdr_data + strlen(hdr_data) - 1) = 0; /* remove cr */
		}
		return 1;

	} while (1);

	return 0;
}

/**
 * @brief Extracts the catalog designation from the ReadMe file.
 *
 * The designation is always expected to be the first line of the ReadMe file.
 *
 * @param readme The readme structure to populate.
 * @param fp The file pointer to the ReadMe.
 * @return 0 on success.
 */
static int get_designation(struct readme *readme, FILE *fp)
{
	char line[README_LINE_SIZE], *end;

	/* designation is always first line of ReadMe */
	rewind(fp);

	end = fgets(line, README_LINE_SIZE - 2, fp);
	if (end == NULL)
		return 0;

	snprintf(readme->designation, RM_DSGN_SIZE, "%.*s", RM_DSGN_SIZE - 1, line);

	/* find next space and pad zeros */
	return 0;
}

/**
 * @brief Extracts the title(s) from the ReadMe file. (Currently a stub)
 *
 * @param info The readme structure to populate.
 * @param f The file pointer to the ReadMe.
 * @return 0 unconditionally.
 */
static int get_titles(struct readme *info, FILE *f)
{
	return 0;
}

/**
 * @brief Extracts keywords from the ReadMe file. (Currently a stub)
 *
 * @param info The readme structure to populate.
 * @param fp The file pointer to the ReadMe.
 * @return 0 unconditionally.
 */
static int get_keywords(struct readme *info, FILE *fp)
{
	return 0;
}

/**
 * @brief Extracts the description from the ReadMe file. (Currently a stub)
 *
 * @param info The readme structure to populate.
 * @param fp The file pointer to the ReadMe.
 * @return 0 unconditionally.
 */
static int get_description(struct readme *info, FILE *fp)
{
	return 0;
}

/**
 * @brief Parses the 'File Summary' section to extract table file details.
 *
 * Locates the 'File Summary:' section and parses the name, length, record count,
 * and title for each file listed.
 *
 * @param db Database instance for logging.
 * @param readme The readme structure to populate with file info.
 * @param fp The file pointer to the ReadMe.
 * @return The number of files found, or -ENODATA if the section is missing.
 */
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

		end = fgets(line, README_LINE_SIZE - 2, fp);
		if (end == NULL || *line == '-')
			break;

		if (*line == ' ')
			continue;

		n = sscanf(line, "%s %d %d %80c", file_info->name, &file_info->length,
				   &file_info->records, file_info->title);
		if (n != 4)
			continue;

		files++;

	} while (files < RM_MAX_FILES);

	adb_debug(db, ADB_LOG_CDS_PARSER, "found %d files in ReadMe\n", files);
	return files;
}

/**
 * @brief Determines the text column offsets for Label and Explanation fields.
 *
 * Format, Label, and Explanation line offsets can vary between different ReadMe files.
 * This function reads the header line of the byte-by-byte description section
 * to find the exact byte offsets for the 'Label' and 'Explanation' columns.
 *
 * @param db Database instance for logging.
 * @param readme The readme structure to store offsets.
 * @param file_id The index of the file being processed.
 * @param fp The file pointer to the ReadMe.
 * @return 0 on success, -EINVAL if headers cannot be found.
 */
static int get_byte_desc_offset(struct adb_db *db, struct readme *readme,
								int file_id, FILE *fp)
{
	char line[README_LINE_SIZE], *end, *offset;

	bzero(line, sizeof(line));

	end = fgets(line, README_LINE_SIZE - 2, fp);
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

/**
 * @brief Parses a single byte-by-byte description section.
 *
 * Reads the layout definition for a file, extracting the start/end bytes,
 * format type, units, label, and explanation for each column. Handles various
 * formats for the byte range (e.g., "1- 4", "1 4", "1"). Also handles continuation
 * lines for the description parameter.
 *
 * @param db Database instance for logging.
 * @param readme The readme structure to populate.
 * @param file_id The index of the file whose description is being parsed.
 * @param fp The file pointer to the ReadMe.
 * @return The number of byte descriptors read, or a negative error code.
 */
static int get_byte_desc(struct adb_db *db, struct readme *readme, int file_id,
						 FILE *fp)
{
	char line[README_LINE_SIZE], cont[README_LINE_SIZE], *end;
	struct cds_byte_desc *byte_desc;
	int desc = 0, err;

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
		end = fgets(line, README_LINE_SIZE - 2, fp);
		if (end == NULL || *line == '-')
			break;

		/* try "%d-%d" start-end format */
		n = sscanf(line, "%d- %d %7s %7s %15s %127[^\n\r]", &byte_desc->start,
				   &byte_desc->end, byte_desc->type, byte_desc->units,
				   byte_desc->label, byte_desc->explanation);
		if (n == 6) {
			byte_desc->start--;
			byte_desc->end--;
			goto next;
		}

		/* try "%d %d" start-end format */
		n = sscanf(line, "%d %d %7s %7s %15s %127[^\n\r]", &byte_desc->start,
				   &byte_desc->end, byte_desc->type, byte_desc->units,
				   byte_desc->label, byte_desc->explanation);
		if (n == 6) {
			byte_desc->start--;
			byte_desc->end--;
			goto next;
		}

		/* try "%d" start-end format */
		n = sscanf(line, "%d %7s %7s %15s %127[^\n\r]", &byte_desc->start,
				   byte_desc->type, byte_desc->units, byte_desc->label,
				   byte_desc->explanation);
		if (n == 5) {
			byte_desc->end = byte_desc->start--;
			goto next;
		}

		/* continuation of explanation */
		byte_desc = &readme->file[file_id].byte_desc[--desc];
		sscanf(line, " %83c", cont);
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

/**
 * @brief Finds the array index of a file given its name.
 *
 * Matches a table name against the names of files already parsed into the readme structure.
 *
 * @param readme The populated readme structure.
 * @param table_name The name of the table/file to find.
 * @return The 0-based index of the file, or -1 if not found.
 */
static inline int get_file_id(struct readme *readme, char *table_name)
{
	int i;

	for (i = 0; i < readme->num_files; i++) {
		if (strstr(table_name, readme->file[i].name))
			return i;
	}
	return -1;
}

/**
 * @brief Parses all byte-by-byte descriptions in the ReadMe file.
 *
 * Searches the file for sections starting with "Byte-by-byte Description of file:"
 * or "Byte-per-byte Description of file:" and parses the layout descriptors
 * into the corresponding `cds_file_info` struct.
 *
 * @param db Database instance for logging.
 * @param readme The readme structure to populate.
 * @param fp The file pointer to the ReadMe.
 * @return 0 on success.
 */
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
			adb_info(db, ADB_LOG_CDS_PARSER, " found description for %s\n",
					 table_name);
			file = get_file_id(readme, table_name);
			if (file < 0) {
				adb_info(db, ADB_LOG_CDS_PARSER, " %s not required\n",
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
			adb_info(db, ADB_LOG_CDS_PARSER, " found description for %s\n",
					 table_name);
			file = get_file_id(readme, table_name);
			if (file < 0) {
				adb_info(db, ADB_LOG_CDS_PARSER, " %s not required\n",
						 table_name);
				continue;
			}

			file_info = &readme->file[file];
			file_info->num_desc = get_byte_desc(db, readme, file, fp);

		} else
			break;
	} while (1);

	adb_info(db, ADB_LOG_CDS_PARSER, "Parsing byte descriptions done\n");
	return 0;
}

/**
 * @brief Parses a CDS ReadMe file completely.
 *
 * Opens the file and extracts all metadata, file summaries, and byte-by-byte
 * table descriptors into a newly allocated `readme` structure.
 *
 * @param db Database instance for logging.
 * @param file The path to the ReadMe file to parse.
 * @return A completely populated readme structure, or NULL on error.
 */
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

	readme = (struct readme *)calloc(1, sizeof(struct readme));
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

/**
 * @brief Frees a previously allocated readme structure.
 *
 * @param readme The readme structure to free.
 */
void readme_free(struct readme *readme)
{
	free(readme);
}

/**
 * @brief Parses the ReadMe file associated with a specific table.
 *
 * Constructs the path to the local ReadMe file for the given table and parses it.
 *
 * @param db Database instance containing the tables.
 * @param table_id The ID of the table whose ReadMe should be parsed.
 * @return 0 on success, -EINVAL if parsing fails.
 */
int table_parse_readme(struct adb_db *db, int table_id)
{
	struct adb_table *table = &db->table[table_id];
	char file[ADB_PATH_SIZE];

	snprintf(file, ADB_PATH_SIZE, "%s%s", table->path.local, "ReadMe");
	adb_info(db, ADB_LOG_CDS_PARSER, "Parsing catalog ReadMe: %s\n", file);

	table->import.readme = readme_parse(db, file);
	if (table->import.readme == NULL) {
		adb_error(db, "failed to parse ReadMe\n");
		return -EINVAL;
	}

	return 0;
}
