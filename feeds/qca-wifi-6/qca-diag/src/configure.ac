#                                               -*- Autoconf -*-

# configure.ac -- Autoconf script for diag
#

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([diag],
	1.0.0)
AM_INIT_AUTOMAKE([foreign])
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/diag_lsm.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])

DIAG_LIBRARY_NAME=diag

#release versioning
DIAG_MAJOR_VERSION=1
DIAG_MINOR_VERSION=0
DIAG_MICRO_VERSION=0

DIAG_LIBRARY_VERSION=1:0:0
AC_SUBST(DIAG_LIBRARY_VERSION)

PACKAGE=$DIAG_LIBRARY_NAME
AC_SUBST(DIAG_LIBRARY_NAME)

DIAG_VERSION=$DIAG_MAJOR_VERSION.$DIAG_MINOR_VERSION.$DIAG_MICRO_VERSION
DIAG_RELEASE=$DIAG_MAJOR_VERSION.$DIAG_MINOR_VERSION
AC_SUBST(DIAG_RELEASE)
AC_SUBST(DIAG_VERSION)

VERSION=$DIAG_VERSION

LT_INIT
AM_PROG_LIBTOOL
# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
PKG_PROG_PKG_CONFIG

AC_ARG_WITH([kernel],
      AC_HELP_STRING([--with-kernel=@<:@dir@:>@],
         [Specify the location of the Linux kernel headers]),
      [kerneldir=$withval],
      with_kernel=no)

if test "x$with_kernel" != "xno"; then
   CFLAGS="${CFLAGS} -I${kerneldir}/include -I${kerneldir}/arch/arm/include"
fi

AC_ARG_WITH([glib],
      AC_HELP_STRING([--with-glib],
         [enable glib, building HLOS systems which use glib]))

if (test "x${with_glib}" = "xyes"); then
	AC_DEFINE(ENABLE_USEGLIB, 1, [Define if HLOS systems uses glib])
	PKG_CHECK_MODULES(GTHREAD, gthread-2.0 >= 2.16, dummy=yes,
				AC_MSG_ERROR(GThread >= 2.16 is required))
	PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.16, dummy=yes,
				AC_MSG_ERROR(GLib >= 2.16 is required))
	GLIB_CFLAGS="$GLIB_CFLAGS $GTHREAD_CFLAGS"
	GLIB_LIBS="$GLIB_LIBS $GTHREAD_LIBS"

	AC_SUBST(GLIB_CFLAGS)
	AC_SUBST(GLIB_LIBS)
fi

AC_ARG_WITH([common_includes],
      AC_HELP_STRING([--with-common-includes=@<:@dir@:>@],
         [Specify the location of the common headers]),
      [common_incdir=$withval],
      with_common_includes=no)

if test "x$with_common_includes" != "xno"; then
   CFLAGS="${CFLAGS} -I${common_incdir}"
fi

AM_CONDITIONAL(USE_GLIB, test "x${with_glib}" = "xyes")

# Checks for libraries.
PKG_CHECK_MODULES([TGENOFF], [time-genoff])
AC_SUBST([TGENOFF_CFLAGS])
AC_SUBST([TGENOFF_LIBS])

AC_SUBST([CFLAGS])
AC_SUBST([CC])
AC_CONFIG_FILES([     \
	Makefile      \
	src/Makefile  \
	test/Makefile \
	klog/Makefile \
	mdlog/Makefile \
	uart_log/Makefile \
	PktRspTest/Makefile \
	dci_sample/Makefile \
	callback_sample/Makefile \
	socket_log/Makefile \
	diag.pc
	])
AC_OUTPUT

