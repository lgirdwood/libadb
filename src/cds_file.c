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
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

#include <zlib.h>

#include <libastrodb/db.h>
#include "debug.h"
#include "private.h"

#define GZ_CHUNK 	16384
#define CHUNK_SIZE	(1024 * 32)

static int inflate_file(struct adb_db *db, const char *path,
	const char *src_file, const char *dest_file)
{
	gzFile src;
	FILE *dest;
	char buf[GZ_CHUNK << 1], src_path[ADB_PATH_SIZE], dest_path[ADB_PATH_SIZE];
	int size, err;

	snprintf(src_path, ADB_PATH_SIZE, "%s%s", path, src_file);
	snprintf(dest_path, ADB_PATH_SIZE, "%s%s", path, dest_file);

	adb_info(db, ADB_LOG_CDS_FTP, "inflating %s to %s\n",
		src_path, dest_path);

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

/* FTP single file */
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
		adb_error(db, "FTP could not connect to %s\n",
			table->cds.host);
		return -EIO;
	}

	ret = FtpLogin("anonymous", "lgirdwood@gmail.com", nbuf);
	if (ret == 0) {
		adb_error(db, "FTP could not login\n", ret);
		ret = -EIO;
		goto out;
	}

	//TODO: FtpDir(NULL, "/", nbuf);
	ret = FtpGet(dest, src, FTPLIB_IMAGE, nbuf);
	if (ret == 0) {
		adb_error(db, "FTP could not get %s\n", src);
		ret = -EIO;
		unlink(dest);
		goto out;
	}

	ret = 0;

out:
	FtpQuit(nbuf);
	return ret;
}

/* FTP multiple files based on pattern */
static int ftp_get_files(struct adb_table *table, const char *pattern)
{
	struct adb_db *db = table->db;
	netbuf *nbuf, *nbuf_dir = NULL;
	char dest[ADB_PATH_SIZE], src[ADB_PATH_SIZE], dir[ADB_PATH_SIZE];
	static char *ftp_dir[CHUNK_SIZE];
	char *file;
	int ret, count = 0;

	snprintf(src, ADB_PATH_SIZE, "%s", table->path.remote);

	adb_info(db, ADB_LOG_CDS_FTP, "FTP files %s pattern %s\n",
		src, pattern);

	FtpInit();

	ret = FtpConnect(table->cds.host, &nbuf);
	if (ret == 0) {
		adb_error(db, "FTP connect to %s failed\n",
		table->cds.host);
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
			adb_error(db, "FTP could not read directory %s\n",
				src);
			ret = -EIO;
			goto out;
		}

		/* copy file name */
		space = ftp_dir[count] = strdup(dir);

		/* strip any CR/LF */
		while (!isspace(*space))
			space++;
		*space = 0;

		count ++;
	} while (ret > 0);

	FtpClose(nbuf_dir);

	/* parse directory */
	count--;
	do {
		/* check for pattern */
		file = strstr(ftp_dir[count], pattern);
		if (file) {
			int err;

			strncpy(dest, table->path.local, ADB_PATH_SIZE);
			strncat(dest, file, ADB_PATH_SIZE - strlen(file) - 1);

			/* download file by file*/
			adb_info(db, ADB_LOG_CDS_FTP, "FTP downloading %s\n",
				ftp_dir[count]);
			err = FtpGet(dest, ftp_dir[count], FTPLIB_IMAGE, nbuf);
			if (err == 0) {
				adb_error(db, "FTP could not get %s to %s\n",
					ftp_dir[count], dest);
				ret = -EIO;
				goto out;
			}
		}
		free(ftp_dir[count]);
		count--;
	} while (count);

	ret = 0;

out:
	for (;count >= 0; count--)
		free(ftp_dir[count]);
	FtpQuit(nbuf);
	return ret;
}

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
		adb_error(db, "Error can't open input file %s for reading\n",
			ifd);
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

/* concat split CDS data files into signgle file */
static int table_concat_files(struct adb_table *table, const char *ext)
{
	struct adb_db *db = table->db;
	char ofile[1024];
	struct dirent *dent;
	FILE *ofd;
	DIR *dir;
	int res;

	/* open local directory */
	dir = opendir(table->path.local);
	if (dir == NULL) {
		adb_error(db, "can't open directory %s\n", table->path.local);
		return -EIO;
	}

	/* open output file */
	sprintf(ofile, "%s%s%s", table->path.local, table->path.file, ext);
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

	return 0;
}

int cds_get_dataset(struct adb_db *db, struct adb_table *table,
	const char *ext)
{
	int ret;
	char file[1024];

	/* now try tablet name with a .gz extension */
	sprintf(file, "%s%s", table->path.file, ext);
	adb_info(db, ADB_LOG_CDS_FTP, "Try to download %s from CDS\n", file);
	ret = ftp_get_file(table, file);
	if (ret != 0)
		/* give up ! */
		adb_warn(db, ADB_LOG_CDS_FTP, "couldn't download %s\n", table->path.file);
	return ret;
}

int cds_get_split_dataset(struct adb_db *db, struct adb_table *table,
	const char *ext)
{
	int ret;
	char file[1024];

	sprintf(file, "%s%s", table->path.file, ext);
	adb_info(db, ADB_LOG_CDS_FTP, "Try to download %s  splits from CDS\n",
		file);

	ret = ftp_get_files(table, file);
	if (ret < 0)
		adb_error(db, "Error can't get split files %s\n", file);
	return ret;
}

/* search the local directory for table CDS (ASCII) data files with extension */
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

	adb_info(db, ADB_LOG_CDS_FTP,"Searching %s for %s CDS data files\n",
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
	adb_info(db, ADB_LOG_CDS_FTP, "%s not found, using remote version\n",
		file);
	ret = ftp_get_file(table, "ReadMe");
	if (ret < 0)
		adb_error(db, "failed to load ReadMe %d\n", ret);

	adb_info(db, ADB_LOG_CDS_FTP, "Got CDS ReadMe version at %s\n", file);
	return ret;
}
