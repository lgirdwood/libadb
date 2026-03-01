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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ftplib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

#include <zlib.h>

#include "libastrodb/db.h"
#include "libastrodb/object.h"
#include "debug.h"
#include "private.h"

#define GZ_CHUNK 16384
#define CHUNK_SIZE (1024 * 32)

/**
 * @brief Inflates a gzip compressed file into a new destination file.
 *
 * @param db Database instance for logging.
 * @param path The base path where the files are located (e.g., local table path).
 * @param src_file The name of the source gzip file.
 * @param dest_file The target name for the uncompressed file.
 * @return 0 on success, negative error code on failure.
 */
static int inflate_file(struct adb_db *db, const char *path,
						const char *src_file, const char *dest_file)
{
	gzFile src;
	FILE *dest;
	char buf[GZ_CHUNK << 1], src_path[ADB_PATH_SIZE], dest_path[ADB_PATH_SIZE];
	int size, err;

	snprintf(src_path, ADB_PATH_SIZE, "%s%s", path, src_file);
	snprintf(dest_path, ADB_PATH_SIZE, "%s%s", path, dest_file);

	adb_info(db, ADB_LOG_CDS_FTP, "inflating %s to %s\n", src_path, dest_path);

	/* open gz file */
	src = gzopen(src_path, "rb");
	if (src == NULL) {
		adb_error(db, "could not open %s\n", src_path);
		return -EIO;
	}

	/* open dest file */
	dest = fopen(dest_path, "wb+");
	if (dest == NULL) {
		adb_error(db, "could not open %s\n", dest_path);
		gzclose(src);
		return -EIO;
	}

	/* inflate */
	do {
		size = gzread(src, buf, GZ_CHUNK);
		fwrite(buf, 1, size, dest);
	} while (!gzeof(src));

	gzclose(src);
	fclose(dest);

	/* delete .gz */
	err = unlink(src_path);
	if (err < 0)
		adb_error(db, "failed to delete %s\n", src_path);

	return 0;
}

/**
 * @brief Retrieves a single file from a remote FTP server.
 *
 * This function connects to the CDS FTP server anonymously and downloads
 * the specified file from the remote path to the local table path.
 *
 * @param table The database table containing local and remote path configurations.
 * @param file The name of the file to download.
 * @return 1 on success, negative error code or 0 on failure.
 */
static int ftp_get_file(struct adb_table *table, const char *file)
{
	struct adb_db *db = table->db;
	netbuf *nbuf;
	int ret;
	char dest[ADB_PATH_SIZE], src[ADB_PATH_SIZE];

	snprintf(dest, ADB_PATH_SIZE, "%s%s", table->path.local, file);
	snprintf(src, ADB_PATH_SIZE, "%s%s", table->path.remote, file);

	adb_info(db, ADB_LOG_CDS_FTP, "ftp %s to %s\n", src, dest);
	FtpInit();

	/* for some reason Ftplib return 0 for errors */
	ret = FtpConnect(table->cds.host, &nbuf);
	if (ret == 0) {
		adb_error(db, "FTP could not connect to %s\n", table->cds.host);
		return -EIO;
	}

	ret = FtpLogin("anonymous", "anonymous@", nbuf);
	if (ret == 0) {
		adb_error(db, "FTP could not login\n", ret);
		ret = -EIO;
		goto out;
	}

	//TODO: FtpDir(NULL, "/", nbuf);
	ret = FtpGet(dest, src, FTPLIB_IMAGE, nbuf);
	if (ret == 0) {
		adb_warn(db, ADB_LOG_CDS_FTP, "FTP could not get %s\n", src);
		ret = -EIO;
		unlink(dest);
	} else
		ret = 1;

out:
	FtpQuit(nbuf);
	return ret;
}

/**
 * @brief Retrieves multiple files matching a pattern from a remote FTP server.
 *
 * This function accesses the remote directory on the CDS FTP server, reads its
 * contents, and downloads all files that match the provided substring pattern.
 *
 * @param table The database table containing local and remote path configurations.
 * @param pattern The substring pattern to match file names against.
 * @return The number of successfully downloaded files, or a negative error code on failure.
 */
static int ftp_get_files(struct adb_table *table, const char *pattern)
{
	struct adb_db *db = table->db;
	netbuf *nbuf, *nbuf_dir = NULL;
	char dest[ADB_PATH_SIZE], src[ADB_PATH_SIZE], dir[ADB_PATH_SIZE];
	static char *ftp_dir[CHUNK_SIZE];
	char *file;
	int ret, count = 0, got = 0;

	snprintf(src, ADB_PATH_SIZE, "%s", table->path.remote);

	adb_info(db, ADB_LOG_CDS_FTP, "FTP files %s pattern %s\n", src, pattern);

	FtpInit();

	ret = FtpConnect(table->cds.host, &nbuf);
	if (ret == 0) {
		adb_error(db, "FTP connect to %s failed\n", table->cds.host);
		return -EIO;
	}

	ret = FtpLogin("anonymous", "anonymous@", nbuf);
	if (ret == 0) {
		adb_error(db, "FTP could not login\n");
		ret = -EIO;
		goto out;
	}

	/* open directory */
	ret = FtpAccess(src, FTPLIB_DIR, FTPLIB_ASCII, nbuf, &nbuf_dir);
	if (ret == 0) {
		adb_error(db, "FTP could not access directory %s\n", src);
		ret = -EIO;
		goto out;
	}

	/* read entire directory, file by file */
	do {
		char *space;

		/* read the directory */
		ret = FtpRead(dir, ADB_PATH_SIZE, nbuf_dir);
		if (ret == -1) {
			adb_error(db, "FTP could not read directory %s\n", src);
			ret = -EIO;
			goto out;
		}

		/* copy file name */
		space = ftp_dir[count] = strdup(dir);

		/* strip any CR/LF */
		while (!isspace(*space))
			space++;
		*space = 0;

		count++;
	} while (ret > 0);

	FtpClose(nbuf_dir);

	/* parse directory */
	count--;
	do {
		/* check for pattern */
		file = strstr(ftp_dir[count], pattern);
		adb_info(db, ADB_LOG_CDS_FTP, "found file %s\n", ftp_dir[count]);
		if (file) {
			int err;

			strncpy(dest, table->path.local, ADB_PATH_SIZE - 1);
			strncat(dest, file, ADB_PATH_SIZE - strlen(file) - 1);

			/* download file by file*/
			adb_info(db, ADB_LOG_CDS_FTP, "FTP downloading %s\n",
					 ftp_dir[count]);
			err = FtpGet(dest, ftp_dir[count], FTPLIB_IMAGE, nbuf);
			if (err == 0) {
				adb_error(db, "FTP could not get %s to %s\n", ftp_dir[count],
						  dest);
				ret = -EIO;
				goto out;
			} else
				got++;
		}
		free(ftp_dir[count]);
		count--;
	} while (count);

	ret = got;

out:
	for (; count >= 0; count--)
		free(ftp_dir[count]);
	FtpQuit(nbuf);
	return ret;
}

/**
 * @brief Appends the contents of a single local file to an open output file stream.
 *
 * Reads the input file in chunks and writes it to the output file stream `ofd`.
 *
 * @param table The database table containing the local path configuration.
 * @param dent Directory entry representing the input file to concatenate.
 * @param ofile The path of the output file being written to (for logging errors).
 * @param ofd The open file stream of the output file.
 * @return 0 on success, negative error code on failure.
 */
static int concat_file(struct adb_table *table, struct dirent *dent,
					   const char *ofile, FILE *ofd)
{
	struct adb_db *db = table->db;
	char ifile[1024];
	char buff[CHUNK_SIZE];
	FILE *ifd;
	int bytes, sect, rem, i;
	size_t size;
	int res = 0;

	/* open input file */
	sprintf(ifile, "%s%s", table->path.local, dent->d_name);
	ifd = fopen(ifile, "r");
	if (ifd == NULL) {
		adb_error(db, "Error can't open input file %s for reading\n", ifd);
		return -EIO;
	}

	/* calculate file length */
	adb_info(db, ADB_LOG_CDS_FTP, "cat %s to %s\n", ifile, ofile);
	fseek(ifd, 0, SEEK_END);
	bytes = ftell(ifd);
	sect = bytes / CHUNK_SIZE;
	rem = bytes % CHUNK_SIZE;
	fseek(ifd, 0, SEEK_SET);

	/* copy input file chunks to output file */
	for (i = 0; i < sect; i++) {
		size = fread(buff, CHUNK_SIZE, 1, ifd);
		if (size == 0) {
			adb_error(db, "Error failed to read input %s file\n", ifile);
			res = -EIO;
			goto out;
		}
		size = fwrite(buff, CHUNK_SIZE, 1, ofd);
		if (size == 0) {
			adb_error(db, "Error failed to write %s output file\n", ofile);
			res = -EIO;
			goto out;
		}
	}

	/* write remainder */
	size = fread(buff, rem, 1, ifd);
	if (size == 0)
		adb_error(db, "Error failed to read %s input file\n", ifile);

	size = fwrite(buff, rem, 1, ofd);
	if (size == 0)
		adb_error(db, "Error failed to write %s output file\n", ofile);

out:
	fclose(ifd);
	return res;
}

/**
 * @brief Concatenates split CDS data files into a single uncompressed data file.
 *
 * Scans the local directory for files matching the table's base file name,
 * skipping compressed, schema, database, or temporary files. It concatenates
 * all matching files into a temporary file and then renames it to the target name.
 *
 * @param table The database table containing the local path and base file name.
 * @param ext The extension of the target concatenated file.
 * @return 0 on success, negative error code on failure.
 */
static int table_concat_files(struct adb_table *table, const char *ext)
{
	struct adb_db *db = table->db;
	char ofile[1024], nfile[1024];
	struct dirent *dent;
	FILE *ofd;
	DIR *dir;
	int res, ret;

	/* open local directory */
	dir = opendir(table->path.local);
	if (dir == NULL) {
		adb_error(db, "can't open directory %s\n", table->path.local);
		return -EIO;
	}

	/*delete stale file */
	sprintf(nfile, "%s%s%s", table->path.local, table->path.file, ext);
	unlink(nfile);

	/* open output file */
	sprintf(ofile, "%s%s%s.tmp", table->path.local, table->path.file, ext);
	ofd = fopen(ofile, "w");
	if (ofd == NULL) {
		adb_error(db, "Error can't open output file %s for writing\n", ofile);
		return -EIO;
	}

	/* read each director entry */
	while ((dent = readdir(dir)) != NULL) {
		/* does it belong to table */
		if (!strcmp(table->path.file, dent->d_name))
			continue;

		/* discard if compressed */
		if (strstr(dent->d_name, ".gz"))
			continue;

		/* discard if schema */
		if (strstr(dent->d_name, ".schema"))
			continue;

		/* discard if db */
		if (strstr(dent->d_name, ".db"))
			continue;

		/* discard if tmp */
		if (strstr(dent->d_name, ".tmp"))
			continue;

		/* found matching file */
		if (strncmp(table->path.file, dent->d_name, strlen(table->path.file)))
			continue;

		/* concat the files */
		res = concat_file(table, dent, ofile, ofd);
		if (res < 0) {
			adb_error(db, "Failed to concat files to %s\n", ofile);
			return res;
		}
	}
	closedir(dir);
	fclose(ofd);

	/* remove .tmp from name */
	ret = rename(ofile, nfile);
	if (ret < 0)
		adb_error(db, "error: %d failed to rename tmp file %s t %s\n", -errno,
				  ofile, nfile);
	return ret;
}

/**
 * @brief Attempts to download a complete dataset from CDS via FTP.
 *
 * Uses the table's path settings to download the base file with the given extension.
 *
 * @param db Database instance for logging.
 * @param table The table containing paths to use for the download.
 * @param ext The file extension to append to the base file name.
 * @return 0 on success, -ENOENT if the file cannot be downloaded.
 */
int cds_get_dataset(struct adb_db *db, struct adb_table *table, const char *ext)
{
	int ret;
	char file[1024];

	/* now try tablet name with a .gz extension */
	sprintf(file, "%s%s", table->path.file, ext);
	adb_info(db, ADB_LOG_CDS_FTP, "Try to download %s from CDS\n", file);
	ret = ftp_get_file(table, file);
	if (ret <= 0) {
		/* give up ! */
		adb_warn(db, ADB_LOG_CDS_FTP, "couldn't download %s\n",
				 table->path.file);
		return -ENOENT;
	} else
		return 0;
}

/**
 * @brief Attempts to download split dataset files from CDS via FTP.
 *
 * Uses the base file name and extension as a pattern to match multiple files
 * on the remote server and download them all.
 *
 * @param db Database instance for logging.
 * @param table The table containing paths to use for the download.
 * @param ext The file extension pattern to look for.
 * @return 0 on success, -ENOENT if no files can be found or downloaded.
 */
int cds_get_split_dataset(struct adb_db *db, struct adb_table *table,
						  const char *ext)
{
	int ret;
	char file[1024];

	sprintf(file, "%s%s", table->path.file, ext);
	adb_info(db, ADB_LOG_CDS_FTP, "Try to download %s  splits from CDS\n",
			 file);

	ret = ftp_get_files(table, file);
	if (ret <= 0) {
		adb_error(db, "Error can't get split files %s\n", file);
		return -ENOENT;
	} else
		return 0;
}

/**
 * @brief Prepares local CDS data files for parsing.
 *
 * Scans the local directory for CDS data files matching the given extension.
 * It will automatically inflate any gzipped files it finds. If multiple split
 * files are found, it uses table_concat_files to merge them into a single file.
 *
 * @param db Database instance for logging.
 * @param table The table configuration indicating the local working directory.
 * @param ext The file extension to search for (e.g., .dat, .txt). If NULL, any file matching the table base name is counted.
 * @return The number of found and prepared data files, or a negative error code on failure.
 */
int cds_prepare_files(struct adb_db *db, struct adb_table *table,
					  const char *ext)
{
	struct dirent *dent;
	DIR *dir;
	char dest[1024], *suffix;
	int found = 0, err;

	/* open local directory */
	dir = opendir(table->path.local);
	if (dir == NULL) {
		adb_error(db, "can't open directory %s\n", table->path.local);
		return -EIO;
	}

	adb_info(db, ADB_LOG_CDS_FTP, "Searching %s for %s CDS data files\n",
			 table->path.local, ext ? ext : "");

	/* check each directory entry */
	while ((dent = readdir(dir)) != NULL) {
		/* skip if filename does not match */
		if (strncmp(table->path.file, dent->d_name, strlen(table->path.file)))
			continue;

		/* skip existing db files */
		if (strstr(dent->d_name, ".db"))
			continue;

		/* skip existing schema files */
		if (strstr(dent->d_name, ".schema"))
			continue;

		/* do we need to also check file extension */
		if (ext == NULL) {
			adb_info(db, ADB_LOG_CDS_FTP, " --> %s\n", dent->d_name);
			found++;
			continue;
		}

		if (strstr(dent->d_name, ext)) {
			adb_info(db, ADB_LOG_CDS_FTP, " --> %s\n", dent->d_name);
			found++;

			/* if the extension is .gz then inflate it */
			if (strstr(dent->d_name, ".gz")) {
				/* chop the .gz from the inflate destination file */
				sprintf(dest, "%s", dent->d_name);
				suffix = strstr(dest, ".gz");
				if (suffix)
					*suffix = 0;

				/* unzip if we got it */
				err = inflate_file(db, table->path.local, dent->d_name, dest);
				if (err < 0) {
					adb_info(db, ADB_LOG_CDS_FTP,
							 "Error: failed to inflate %s \n", dent->d_name);
					found--;
				}
			}
		}
	}

	closedir(dir);
	adb_info(db, ADB_LOG_CDS_FTP, "Found %d CDS data files\n", found);

	/* if required, concat files */
	if (found > 1)
		table_concat_files(table, ext);

	return found;
}

/**
 * @brief Locates or downloads the ReadMe file for the given table.
 *
 * First checks if a local "ReadMe" file exists for the table. If not, it attempts
 * to download it from the remote FTP server.
 *
 * @param db The database instance.
 * @param table_id The ID of the table to retrieve the ReadMe for.
 * @return 0 on success (either found locally or successfully downloaded), negative error code on failure.
 */
int cds_get_readme(struct adb_db *db, int table_id)
{
	struct adb_table *table = &db->table[table_id];
	int ret = 0;
	struct stat buf;
	char file[ADB_PATH_SIZE];

	/* check for local disk ReadMe (ASCII) copy */
	snprintf(file, ADB_PATH_SIZE, "%s%s", table->path.local, "ReadMe");
	if (stat(file, &buf) == 0) {
		adb_info(db, ADB_LOG_CDS_FTP, "Found local ReadMe version at %s\n",
				 file);
		return ret;
	}

	/* local binary or ASCII not available, so download ASCII ReadMe*/
	adb_info(db, ADB_LOG_CDS_FTP, "%s not found, using remote version\n", file);
	ret = ftp_get_file(table, "ReadMe");
	if (ret < 0)
		adb_error(db, "failed to load ReadMe %d\n", ret);

	adb_info(db, ADB_LOG_CDS_FTP, "Got CDS ReadMe version at %s\n", file);
	return ret;
}
