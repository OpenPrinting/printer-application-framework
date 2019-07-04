#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Printer Applications Framework], [0.0.1], [https://github.com/dheeraj135/Printer-Applications-Framework])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

######################################################################
# Directory
AC_PREFIX_DEFAULT(/)

if test "$prefix" = "NONE"; then
    prefix="/"
fi

if test "$exec_prefix" = "NONE";then
    if test "$prefix" = "/";then
        exec_prefix="/usr"
    else
        exec_prefix="$prefix"
    fi
fi

# echo $datarootdir
# echo "x\${datarootdir}"

#if test "x$datarootdir" = "x";then
    datarootdir="$exec_prefix/share"
    # echo "${datarootdir}"
#fi

if test "x$datadir" = "x\${datarootdir}";then
    datadir="${datarootdir}/cups"
fi

if test "x$cachedir" = "x";then
    cachedir="$prefix/var/cache"
fi

if test "x$serverbin" = "x";then
    serverbin="$exec_prefix/lib/cups"
fi

if test "x$tmpdir" = "x";then
    tmpdir="$prefix/var/tmp/"
fi

bindir="$exec_prefix/bin"
# sbindir="$exec_prefix"
# AC_DEFINE([EXEC_PREFIX],[],[EXEC prefix])
# AC_DEFINE([PREFIX],[],[prefix])
# AC_DEFINE([DATADIR],[],[datadir])
# AC_DEFINE([CACHEDIR],[],[cachedir])
# AC_DEFINE([SERVERBIN],[],[serverbin])
# AC_DEFINE([BINDIR],[],[bindir])
# AC_DEFINE([TMPDIR],[],[tmpdir])

AC_DEFINE_UNQUOTED([EXEC_PREFIX],["$exec_prefix"],[exec prefix])
AC_DEFINE_UNQUOTED([PREFIX],["$prefix"],[prefix])
AC_DEFINE_UNQUOTED([DATADIR],["$datadir"],[datadir])
AC_DEFINE_UNQUOTED([CACHEDIR],["$cachedir"],[cachedir])
AC_DEFINE_UNQUOTED([SERVERBIN],["$serverbin"],[serverbin])
AC_DEFINE_UNQUOTED([BINDIR],["$bindir"],[bindir])
AC_DEFINE_UNQUOTED([TMPDIR],["$tmpdir"],[temp dir])

# echo "$serverbin"
AC_SUBST([serverbin])
AC_SUBST([tmpdir])
######################################################################
# cups-config
AC_ARG_WITH([cups-config],
	[AS_HELP_STRING([--with-cups-config=path], [Specify path to cups-config executable.])],
	[with_cups_config="$withval"],
	[with_cups_config=system]
)

AS_IF([test "x$with_cups_config" != "xsystem"], [
	CUPSCONFIG=$with_cups_config
], [
	AC_PATH_TOOL(CUPSCONFIG, [cups-config])
	AS_IF([test -z "$CUPSCONFIG"], [
		AC_MSG_ERROR([Required cups-config is missing. Please install CUPS developer packages.])
	])
])
cups_serverbin=`$CUPSCONFIG --serverbin`
CUPS_DATADIR="`$CUPSCONFIG --datadir`"
CUPS_SERVERBIN="$cups_serverbin"
AC_SUBST(CUPS_SERVERBIN)
AC_SUBST(CUPS_DATADIR)
# AC_SUBST(LIB_CUPS)
# AC_SUBST(LDFLAGS_CUPS)
AC_DEFINE_UNQUOTED([CUPS_SERVERBIN],["$CUPS_SERVERBIN"],[Cups serverbin directory])
######################################################################
# CUPS-CONFIG
CUPS_API_VERSION=`$CUPSCONFIG --api-version`
CUPS_BUILD=`$CUPSCONFIG --build`
CUPS_CFLAGS=`$CUPSCONFIG --cflags`
CUPS_LDFLAGS=`$CUPSCONFIG --ldflags`
CUPS_LIBS=`$CUPSCONFIG --libs`
CUPS_STATIC=`$CUPSCONFIG --static --libs`
CUPS_VERSION=`$CUPSCONFIG --version`
CUPS_SERVERROOT=`$CUPSCONFIG --serverroot`

AC_SUBST(CUPS_API_VERSION)
AC_SUBST(CUPS_BUILD)
AC_SUBST(CUPS_LDFLAGS)
AC_SUBST(CUPS_LIBS)
AC_SUBST(CUPS_STATIC)
AC_SUBST(CUPS_CFLAGS)
AC_SUBST(CUPS_VERSION)
AC_SUBST(CUPS_SERVERROOT)
######################################################################
# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_CHECK_PROG(IPPEVEPRINTER,ippeveprinter,yes,no)
if test x"$IPPEVEPRINTER" != x"yes";then
    AC_MSG_ERROR([Please install ippeveprinter from cups package and try again.])
fi
# AC_CHECK_PROG(CUPS_DRIVERD,cups-driverd,yes,no,[$cups_serverbin/daemon/])
# if test x"$CUPS_DRIVERD" != x"yes";then
#     AC_MSG_ERROR([Please install cups-driverd from cups package and try again.])
# fi
# Checks for libraries.
AC_CHECK_LIB([cups],[cupsAddDest])
# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/socket.h unistd.h])
AC_CHECK_HEADERS([cups/cups.h cups/array.h cups/dir.h cups/file.h cups/ppd.h])
######################################################################
# Check for avahi
PKG_CHECK_MODULES([libavahi],[avahi-client],[have_avahi=1],[have_avahi=0])
# echo "HAVE_AVAHI="$have_avahi
if test x"$have_avahi" = x"1"; then
    AC_CHECK_HEADERS([avahi-client/client.h avahi-client/lookup.h avahi-common/simple-watch.h\
    avahi-common/malloc.h avahi-common/error.h],[],[have_avahi=0])
fi
AC_DEFINE_UNQUOTED([HAVE_AVAHI],[$have_avahi],[Do we have Avahi?])
######################################################################
# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_TYPE_UINT16_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([dup2 gettimeofday memmove memset socket strcasecmp strdup strerror strncasecmp strstr])

AC_CONFIG_FILES([Makefile
                 server/Makefile])

# This is a fake cups-config file allowing driver packages to detect this file and work accordingly.
AC_CONFIG_FILES([cups-config],[chmod +x cups-config])
AC_OUTPUT
