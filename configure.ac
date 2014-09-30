AC_PREREQ([2.69])

AC_INIT([libastrodb],[m4_esyscmd(./git-version-gen .tarball-version)],[lgirdwood (at) gmail (dot) com])

AC_CONFIG_MACRO_DIR([m4])

# libtool versioning
LIBASTRODB_CURRENT_VERSION=0
LIBASTRODB_RELEASE_VERSION=2
LIBASTRODB_AGE_VERSION=0
LT_VERSION=$LIBASTRODB_CURRENT_VERSION:$LIBASTRODB_RELEASE_VERSION:$LIBASTRODB_AGE_VERSION
AC_SUBST(LT_VERSION)

LIBASTRODB_VERSION=0.9
AC_SUBST(LIBASTRODB_VERSION)
PACKAGE=libastrodb

AM_INIT_AUTOMAKE([foreign 1.11 -Wall -Wno-portability silent-rules tar-pax])

# Specify a configuration file
AC_CONFIG_HEADERS([config.h])

dnl Initialize libtool
LT_PREREQ(2.2)
LT_INIT

dnl Initialize maintainer mode
AM_MAINTAINER_MODE([enable])

CFLAGS="-Wall"

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([bzero gettimeofday memmove mkdir strdup strstr strtol], , [AC_MSG_ERROR([Missing core functions])])
AC_CHECK_LIB([m], [sqrtf], , [AC_MSG_ERROR([Need sqrtf])])

# Checks for libraries.
AC_CHECK_LIB([ftp], [FtpInit], , [AC_MSG_ERROR([Missing ftplib-dev])])
AC_CHECK_LIB([z], [gzopen], , [AC_MSG_ERROR([Missing zlib1g-dev])])

AC_DEFINE([_GNU_SOURCE], [], [Description])


# Debug symbol support
AC_ARG_ENABLE(debug, [AS_HELP_STRING([--enable-debug],[enable debug symbols])],
	have_debug=$enableval, have_debug=no)
if test "$have_debug" = "yes"; then
	AC_DEFINE(HAVE_DEBUG,1,[Define to enable debug symbols.])
	CFLAGS+=" -g"
else
	CFLAGS+=" -O3"
fi

# AVX support
AC_ARG_ENABLE(avx, [AS_HELP_STRING([--enable-avx],[enable AVX optimizations])], have_avx=$enableval, have_avx=no)
if test "$have_avx" = "yes"; then
	AC_DEFINE(HAVE_AVX,1,[Define to enable AVX optimizations.])
fi
AM_CONDITIONAL(HAVE_AVX, test "$have_avx" = "yes")

if test "$have_avx" = "yes" -a "x$AVX_CFLAGS" = x; then
	AX_CHECK_COMPILE_FLAG(-mavx, [AVX_CFLAGS="-mavx"],
		[AC_MSG_ERROR([Need a version of gcc with -mavx])])
fi

AC_SUBST(AVX_CFLAGS)

# OpenMP support
AC_ARG_ENABLE(openmp, [AS_HELP_STRING([--enable-openmp],[enable OpenMP])], have_openmp=$enableval, have_openmp=no)
if test "$have_openmp" = "yes"; then
	AC_DEFINE(HAVE_OPENMP,1,[Define to enable OpenMP])
fi
AM_CONDITIONAL(HAVE_OPENMP, test "$have_openmp" = "yes")

if test "$have_openmp" = "yes" -a "x$OPENMP_CFLAGS" = x; then
	AX_CHECK_COMPILE_FLAG(-fopenmp, [OPENMP_CFLAGS="-fopenmp"],
		[AC_MSG_ERROR([Need a version of gcc with -fopenmp])])
fi

AC_SUBST(OPENMP_CFLAGS)

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	src/libastrodb/Makefile
	examples/Makefile
	doc/Makefile
	m4/Makefile
	libastrodb.pc
])

AC_OUTPUT

AS_IF([test "$have_avx" = "yes"], ENABLE_AVX=yes, ENABLE_AVX=no)
AS_IF([test "$have_openmp" = "yes"], ENABLE_OPENMP=yes, ENABLE_OPENMP=no)


echo "
---{ $PACKAGE_NAME $VERSION }---

prefix:                        ${prefix}
Compiler:                      ${CC}
CFLAGS:                        ${CFLAGS}
LIBS:                          ${LIBS}

Enable AVX:                    ${ENABLE_AVX}
Enable OpenMP:    	       ${ENABLE_OPENMP}
"