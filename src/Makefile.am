SUBDIRS = libastrodb

AM_CFLAGS = $(AVX_CFLAGS) $(OPENMP_CFLAGS)

lib_LTLIBRARIES = libastrodb.la

libastrodb_la_SOURCES = \
	cds_file.c \
	cds_parse.c \
	db.c \
	htm_core.c \
	htm_file.c \
	htm_get.c \
	import.c \
	htm_insert.c \
	htm_import.c \
	kd_tree.c \
	schema.c \
	search.c \
	table.c \
	hash.c \
	solve.c

libastrodb_la_LDFLAGS = \
	-version-info $(LT_VERSION) \
	-release $(LT_RELEASE) 
	-export-dynamic
