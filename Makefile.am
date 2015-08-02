## Process this file with automake to produce Makefile.in
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	git-version-gen \
	libastrodb.pc

SUBDIRS = src doc examples m4

MAINTAINERCLEANFILES =
noinst_DATA =
DISTCLEANFILES =

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libastrodb.pc

.PHONY: distcleancheck

# see git-version-gen
BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@

doc-dummy:

doc: doc-dummy
	$(MAKE) -C doc doc