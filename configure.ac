dnl -*- bash -*-
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)dnl		Minimum Autoconf version required.

AC_INIT(drizzle, [7.0.0], [http://bugs.launchpad.net/drizzle])
AC_CONFIG_SRCDIR([drizzled/drizzled.cc])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS([config.h])

# Save some variables and the command line options for mysqlbug
SAVE_CC="$CC"
SAVE_CXX="$CXX"
SAVE_ASFLAGS="$ASFLAGS"
SAVE_CFLAGS="$CFLAGS"
SAVE_CXXFLAGS="$CXXFLAGS"
SAVE_LDFLAGS="$LDFLAGS"
SAVE_CXXLDFLAGS="$CXXLDFLAGS"
CONF_COMMAND="$0 $ac_configure_args"
AC_SUBST(CONF_COMMAND)
AC_SUBST(SAVE_CC)
AC_SUBST(SAVE_CXX)
AC_SUBST(SAVE_ASFLAGS)
AC_SUBST(SAVE_CFLAGS)
AC_SUBST(SAVE_CXXFLAGS)
AC_SUBST(SAVE_LDFLAGS)
AC_SUBST(SAVE_CXXLDFLAGS)
AC_SUBST(CXXLDFLAGS)

AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE(-Wall -Wno-portability -Werror)

PROTOCOL_VERSION=10
DOT_FRM_VERSION=6
# See the libtool docs for information on how to do shared lib versions.
SHARED_LIB_MAJOR_VERSION=1
SHARED_LIB_VERSION=$SHARED_LIB_MAJOR_VERSION:0:0

##############################################################################
# The below section needs to be done before AC_PROG_CC
##############################################################################

if test "x${CFLAGS-}" = x ; then
  cflags_is_set=no
else
  cflags_is_set=yes
fi

if test "x${CPPFLAGS-}" = x ; then
  cppflags_is_set=no
else
  cppflags_is_set=yes
fi

if test "x${LDFLAGS-}" = x ; then
  ldflags_is_set=no
else
  ldflags_is_set=yes
fi

if test "x${enable_dependency_tracking}" = "x"
then
  enable_dependency_tracking=yes
fi

################ End of section to be done before AC_PROG_CC #################

dnl Checks for programs.
AC_PROG_CC
ifdef([AC_PROG_CC_C99],[
  dnl TODO: Need to fix this to use c99 instead of gnu99
  AC_PROG_CC_C99([],[AC_MSG_ERROR([C99 support required for compiling Drizzle])])
  C99_SUPPORT_HACK=""
],[C99_SUPPORT_HACK="-std=gnu99"])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CXX
AC_CXX_HEADER_STDCXX_98
if test "$ac_cv_cxx_stdcxx_98" = "no"
then
  AC_MSG_ERROR([C++ Compiler required to compile Drizzle])
fi
AC_PROG_CPP
AM_PROG_CC_C_O

AC_CHECK_DECL([__SUNPRO_C], [SUNCC="yes"], [SUNCC="no"])

dnl AC_CANONICAL_HOST thinks it's a good idea to just set CFLAGS to 
dnl -g -O2 if you're running gcc. We would like to use something else, thanks.
if test "x${CFLAGS}" = "x-g -O2"
then
  CFLAGS=
fi
if test "x${CXXFLAGS}" = "x-g -O2"
then
  CXXFLAGS=
fi


# Set all version vars based on $VERSION. How do we do this more elegant ?
# Remember that regexps needs to quote [ and ] since this is run through m4
# We take some made up examples
#
#  VERSION                  5.1.40sp1-alpha     5.0.34a
#  DRIZZLE_NO_DASH_VERSION    5.1.40sp1           5.0.34a
#  DRIZZLE_NUMERIC_VERSION    5.1.40              5.0.34
#  DRIZZLE_BASE_VERSION       5.1                 5.0
#  DRIZZLE_VERSION_ID         50140               50034
#
DRIZZLE_NO_DASH_VERSION=`echo $VERSION | sed -e "s|-.*$||"`
DRIZZLE_NUMERIC_VERSION=`echo $DRIZZLE_NO_DASH_VERSION | sed -e "s|[[a-z]][[a-z0-9]]*$||"`
DRIZZLE_BASE_VERSION=`echo $DRIZZLE_NUMERIC_VERSION | sed -e "s|\.[[^.]]*$||"`
DRIZZLE_VERSION_ID=`echo $DRIZZLE_NUMERIC_VERSION | \
    awk -F. '{printf "%d%0.2d%0.2d", $1, $2, $3}'`

# The port should be constant for a LONG time
DRIZZLE_TCP_PORT_DEFAULT=4427

sinclude(m4/dtrace.m4)
sinclude(m4/character_sets.m4)
sinclude(m4/gettext.m4)

AM_GNU_GETTEXT([external])

AM_CONDITIONAL([BUILD_GETTEXT],[test "x$MSGMERGE" != "x" -a "x$MSGMERGE" != "x:"])

AC_SUBST(DRIZZLE_NO_DASH_VERSION)
AC_DEFINE_UNQUOTED(DRIZZLE_BASE_VERSION,["$DRIZZLE_BASE_VERSION"],
                  [Major and minor version])
AC_SUBST(DRIZZLE_VERSION_ID)
AC_DEFINE_UNQUOTED([DRIZZLE_VERSION_ID],[$DRIZZLE_VERSION_ID],
		   [Version ID that can be easily used for numeric comparison])
AC_SUBST(DRIZZLE_PREVIOUS_BASE_VERSION)
AC_SUBST(PROTOCOL_VERSION)
AC_DEFINE_UNQUOTED([PROTOCOL_VERSION], [$PROTOCOL_VERSION],
                   [mysql client protocol version])
AC_SUBST(DOT_FRM_VERSION)
AC_DEFINE_UNQUOTED([FRM_VER], [$DOT_FRM_VERSION],
                   [Version of .frm files])
AC_DEFINE_UNQUOTED([DRIZZLE_CONFIG_NAME],["drizzled"],[Name of server config section])

AC_SUBST(SHARED_LIB_MAJOR_VERSION)
AC_SUBST(SHARED_LIB_VERSION)
AC_SUBST(AVAILABLE_LANGUAGES)

dnl Ok. This is sort of lame, but we need to be nice to plugins Makefile.am's.
AM_CONDITIONAL(BUILD_DRIZZLE,[test "x" = "x"])


# General Constants
AC_DEFINE([IO_SIZE], [4096], [Io buffer size; Must be a power of 2 and 
  a multiple of 512. May be
  smaller what the disk page size. This influences the speed of the
  isam btree library. eg to big to slow.])
AC_DEFINE([SC_MAXWIDTH],[256], [Max width of screen (for error messages)])
AC_DEFINE([FN_LEN],[256	], [Max file name len ])
AC_DEFINE([FN_EXTLEN],[20], [Max length of extension (part of FN_LEN) ])
AC_DEFINE([FN_REFLEN],[512], [Max length of full path-name ])
AC_DEFINE([FN_EXTCHAR],['.'], [File extension character])
AC_DEFINE([FN_HOMELIB],['~'], [~/ is used as abbrev for home dir ])
AC_DEFINE([FN_CURLIB],['.'], [./ is used as abbrev for current dir ])
AC_DEFINE([FN_PARENTDIR],[".."], [Parent directory; Must be a string ])

AC_DEFINE([MASTER],[1],[Compile without unireg])

AH_VERBATIM([QUOTE_ARG],[
/* Quote argument (before cpp) */
#ifndef QUOTE_ARG
# define QUOTE_ARG(x) #x
#endif
/* Quote argument, (after cpp) */
#ifndef STRINGIFY_ARG
# define STRINGIFY_ARG(x) QUOTE_ARG(x)
#endif
])

AH_VERBATIM([builtin_expect],[
/*
 * The macros below are borrowed from include/linux/compiler.h in the
 * Linux kernel. Use them to indicate the likelyhood of the truthfulness
 * of a condition. This serves two purposes - newer versions of gcc will be
 * able to optimize for branch predication, which could yield siginficant
 * performance gains in frequently executed sections of the code, and the
 * other reason to use them is for documentation
 */
#if !defined(__GNUC__)
#define __builtin_expect(x, expected_value) (x)
#endif

#define likely(x)  __builtin_expect((x),1)
#define unlikely(x)  __builtin_expect((x),0)
])

dnl  InnoDB depends on some Drizzle's internals which other plugins should not
dnl  need.  This is because of InnoDB's foreign key support, "safe" binlog
dnl  truncation, and other similar legacy features.

dnl  We define accessors for these internals unconditionally, but do not
dnl  expose them in mysql/plugin.h.  They are declared in ha_innodb.h for
dnl  InnoDB's use.

AC_DEFINE([INNODB_COMPATIBILITY_HOOKS],[1],[TODO: Remove the need for this])
dnl TODO: Make a test for when this needs to be set.
AC_DEFINE([_REENTRANT],[1],[Some thread libraries require this])

AH_VERBATIM([posix_pthread],[
/* We want posix threads */
#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS
#endif
])


# Canonicalize the configuration name.

# Check whether --with-system-type or --without-system-type was given.
AC_ARG_WITH([system-type],
    [AS_HELP_STRING([--with-system-type],
       [Set the system type, like "sun-solaris10"])],
    [SYSTEM_TYPE="$withval"],
    [SYSTEM_TYPE="$host_vendor-$host_os"])
AC_ARG_WITH([machine-type],
    [AS_HELP_STRING([--with-machine-type],
       [Set the machine type, like "powerpc"])],
    [MACHINE_TYPE="$withval"],
    [MACHINE_TYPE="$host_cpu"])
AC_SUBST(SYSTEM_TYPE)
AC_DEFINE_UNQUOTED([SYSTEM_TYPE], ["$SYSTEM_TYPE"],
                   [Name of system, eg sun-solaris])
AC_SUBST(MACHINE_TYPE)
AC_DEFINE_UNQUOTED([MACHINE_TYPE], ["$MACHINE_TYPE"],
                   [Machine type name, eg sparc])

# Detect intel x86 like processor
BASE_MACHINE_TYPE=$MACHINE_TYPE
case $MACHINE_TYPE in
  i?86) BASE_MACHINE_TYPE=i386 ;;
esac

AM_SANITY_CHECK
# This is needed is SUBDIRS is set
AC_PROG_MAKE_SET


case "$target_os" in
  *linux*)
  TARGET_LINUX="true"
  AC_SUBST(TARGET_LINUX)
  AC_DEFINE([TARGET_OS_LINUX], [1], [Whether we build for Linux])
    ;;
  *apple-darwin*)
    TARGET_OSX="true"
    AC_SUBST(TARGET_OSX)
    AC_DEFINE([TARGET_OS_OSX], [1], [Whether we build for OSX])
    ;;
  *solaris*)
    TARGET_SOLARIS="true"
    AC_SUBST(TARGET_SOLARIS)
    AC_DEFINE([TARGET_OS_SOLARIS], [1], [Whether we are building for Solaris])
    ;;
  *)
    ;;
esac

dnl AC_CANONICAL_HOST thinks it's a good idea to just set CFLAGS to 
dnl -g -O2 if you're running gcc. We would like to use something else, thanks.
if test "x${CFLAGS}" = "x-g -O2"
then
  CFLAGS=
fi
if test "x${CXXFLAGS}" = "x-g -O2"
then
  CXXFLAGS=
fi

DRIZZLE_CHECK_C_VERSION
DRIZZLE_CHECK_CXX_VERSION

AC_SYS_LARGEFILE

AC_PROG_AWK

if test "$ac_cv_c_compiler_gnu" = "yes"
then
  AS="$CC -c"
  AC_SUBST(AS)
else
  AC_PATH_PROG(AS, as, as)
fi

dnl TODO: This needs to go away and be replaced with _ISOC99_SOURCE
if test "$ac_cv_c_compiler_gnu" = "yes" -o "$target_os" = "linux-gnu"
then
  AC_DEFINE([_GNU_SOURCE],[1],[Fix problem with S_ISLNK() on Linux])
fi

dnl  Solaris 9 include file <sys/feature_tests.h> refers to X/Open document

dnl  System Interfaces and Headers, Issue 5

dnl  saying we should define _XOPEN_SOURCE=500 to get POSIX.1c prototypes,
dnl  but apparently other systems (namely FreeBSD) don't agree.

dnl  On a newer Solaris 10, the above file recognizes also _XOPEN_SOURCE=600.
dnl  Furthermore, it tests that if a program requires older standard
dnl  (_XOPEN_SOURCE<600 or _POSIX_C_SOURCE<200112L) it cannot be
dnl  run on a new compiler (that defines _STDC_C99) and issues an #error.
dnl  It's also an #error if a program requires new standard (_XOPEN_SOURCE=600
dnl  or _POSIX_C_SOURCE=200112L) and a compiler does not define _STDC_C99.

dnl  To add more to this mess, Sun Studio C compiler defines _STDC_C99 while
dnl  C++ compiler does not!

dnl TODO: Can _ISOC99_SOURCE be defined on all platforms and remove the 
dnl       Need for all of this? 
if test "$GCC" = "yes"
then
  case "$target_os" in
    *solaris*)
    CFLAGS="${CFLAGS} -D_XOPEN_SOURCE=600"
    CXXFLAGS="${CXXFLAGS} -D__C99FEATURES__"
    ;;
  esac
fi

# We use libtool
AC_PROG_LIBTOOL

# Ensure that we have --preserve-dup-deps defines, otherwise we get link
# problems of 'mysql' with CXX=g++
LIBTOOL="$LIBTOOL --preserve-dup-deps"
AC_SUBST(LIBTOOL)dnl

AC_SUBST(NM)dnl

AC_PROG_INSTALL
test -z "$INSTALL_SCRIPT" && INSTALL_SCRIPT='${INSTALL_PROGRAM}'

# Look for "(group|user)add". 
# TODO: If the programs do not exist, inform the DBA that the user
#       was not created at the end of the install routine.
AC_CHECK_PROGS(GROUPADD, groupadd addgroup)
AC_CHECK_PROGS(USERADD, useradd adduser)

dnl Needed for ltmain
AC_PROG_RANLIB

dnl Not critical since the generated file is distributed
AC_CHECK_PROGS(YACC, ['bison -y'])
if test -z "$YACC" && test -d ".bzr"
then
  AC_MSG_ERROR(["bison is required for Drizzle to build from a bzr branch"])
fi

AC_PATH_PROG(uname_prog, uname, no)

# We should go through this and put all the explictly system dependent
# stuff in one place
AC_MSG_CHECKING(operating system)
AC_CACHE_VAL(mysql_cv_sys_os,
[
if test "$uname_prog" != "no"; then
  mysql_cv_sys_os="`uname`"
else
  mysql_cv_sys_os="Not Solaris"
fi
])
AC_MSG_RESULT($mysql_cv_sys_os)


# The following is required for portable results of floating point calculations
# on PowerPC. The same must also be done for IA-64, but this options is missing
# in the IA-64 gcc backend.

if test "$GCC" = "yes"
then
  case "$host_cpu" in
    *ppc* | *powerpc*)
      AM_CFLAGS="$CFLAGS -mno-fused-madd"
      AM_CXXFLAGS="$CXXFLAGS -mno-fused-madd"
    ;;
  esac
fi
# Build optimized or debug version ?
# First check for gcc and g++
SYMBOLS_FLAGS="-g"
DEBUG_OPTIMIZE_CC=""
OPTIMIZE_CFLAGS="-O"
DEBUG_OPTIMIZE_CXX=""
OPTIMIZE_CXXFLAGS="-O"
if test "$GCC" = "yes"
then
  SYMBOLS_FLAGS="-ggdb3"
  DEBUG_OPTIMIZE_CC="-O0"
  OPTIMIZE_CFLAGS="-O3"
  DEBUG_OPTIMIZE_CXX="-O0"
  OPTIMIZE_CXXFLAGS="-O3"
fi
if test "$SUNCC" = "yes"
then
  dnl I'm cheating here and sticking C99 support in SYMBOLS_FLAGS
  SYMBOLS_FLAGS=""
  DEBUG_OPTIMIZE_CC=""
  OPTIMIZE_CFLAGS="-xO4 -xlibmil -xdepend -Xa -mt -xstrconst -D_FORTEC_"
  DEBUG_OPTIMIZE_CXX=""
  #Put back in once isnan is figured out
  OPTIMIZE_CXXFLAGS="-xO4 -xlibmil -mt -D_FORTEC_ -xlang=c99 -compat=5 -library=stlport4"
fi

dnl TODO: Remove this define once we are using 2.61 across the board.
# AX_HEADER_ASSERT
# ----------------
# Check whether to enable assertions.
AC_DEFUN([AX_HEADER_ASSERT],
[
  AC_MSG_CHECKING([whether to enable assertions])
  AC_ARG_ENABLE([assert],
    [AS_HELP_STRING([--disable-assert],
       [Turn off assertions])],
    [ac_cv_assert="no"],
    [ac_cv_assert="yes"])
  AC_MSG_RESULT([$ac_cv_assert])
])

AX_HEADER_ASSERT

CFLAGS="${SYMBOLS_FLAGS} ${CFLAGS}"
CXXFLAGS="${SYMBOLS_FLAGS} ${CXXFLAGS}"

AC_ARG_WITH([debug],
    [AS_HELP_STRING([--with-debug],
       [Add debug code/turns off optimizations (yes|no) @<:@default=no@:>@])],
    [with_debug=$withval],
    [with_debug=no])
if test "$with_debug" = "yes"
then
  # Medium debug.
  CFLAGS="$DEBUG_OPTIMIZE_CC -DDEBUG $CFLAGS ${SAVE_CFLAGS}"
  CXXFLAGS="$DEBUG_OPTIMIZE_CXX -DDEBUG $CXXFLAGS ${SAVE_CXXFLAGS}"
else
  # Optimized version. No debug
  CFLAGS="${OPTIMIZE_CFLAGS} ${CFLAGS} ${SAVE_CFLAGS}"
  CXXFLAGS="$OPTIMIZE_CXXFLAGS $CXXFLAGS ${SAVE_CXXFLAGS}"
fi

AC_SUBST(CC)
AC_SUBST(CFLAGS)
AC_SUBST(CXX)
AC_SUBST(CXXFLAGS)
AC_SUBST(ASFLAGS)
AC_SUBST(LD)
AC_SUBST(INSTALL_SCRIPT)


export CC CXX CFLAGS LD LDFLAGS AR ARFLAGS

AC_CXX_STL_HASH
AC_CXX_CSTDINT
AC_CXX_CINTTYPES
AC_CXX_CMATH

DRIZZLE_PROG_AR

# libdrizzle versioning when linked with GNU ld.
if test "x$EGREP" != "x"
then
  if test "$lt_cv_prog_gnu_ld" = "yes" -a $LD --version 2>/dev/null|${EGREP} -q GNU
  then
    LD_VERSION_SCRIPT="-Wl,--version-script=\$(top_srcdir)/libdrizzle/libdrizzle.ver"
  fi
fi
AC_SUBST(LD_VERSION_SCRIPT)

#--------------------------------------------------------------------
# Check for Google Proto Buffers
#--------------------------------------------------------------------

AC_LANG_PUSH([C++])
SEARCH_FOR_LIB(protobuf, printf, google/protobuf/message.h,
               AC_MSG_ERROR([protobuf is required for Drizzle]))

AC_PATH_PROG([PROTOC],[protoc],[no],[$PROTOBUF_PATH])
if test "x$PROTOC" = "xno"
then
  AC_MSG_ERROR([Couldn't find protoc. Try installing Google Protocol Buffer.])
fi

AC_LANG_POP()

#--------------------------------------------------------------------
# Check for libuuid
#--------------------------------------------------------------------

dnl Do this by hand instead of with SEARCH_FOR_LIB, because uuid is weird.
AC_CHECK_HEADERS(uuid/uuid.h)
if test "x$ac_cv_header_uuid_uuid_h" = "xno"
then
  AC_MSG_ERROR([Couldn't find uuid/uuid.h. Try installing libuuid development packages])
fi
AC_CHECK_LIB(uuid, uuid_generate)

#--------------------------------------------------------------------
# Check for libevent
#--------------------------------------------------------------------

SEARCH_FOR_LIB(event, event_loop, event.h, 
               AC_MSG_ERROR([libevent is required for Drizzle]))


AC_CACHE_CHECK([for bufferevent in libevent], ac_cv_libevent_works, [
  save_CPPFLAGS="$CPPFLAGS"
  save_LIBS="$LIBS"
  CPPFLAGS="$EVENT_CFLAGS"
  LIBS="$EVENT_LIBS"
  AC_TRY_LINK([
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <event.h>],[
	struct bufferevent bev;
	bufferevent_settimeout(&bev, 1, 1);
  ], ac_cv_libevent_works=yes, [
        AC_MSG_ERROR([you need to install a more recent version of libevent,
	check http://www.monkey.org/~provos/libevent/])
  ])

  CPPFLAGS="$save_CPPFLAGS"
  LIBS="$save_LIBS"
])


#--------------------------------------------------------------------
# Check for libpthread
#--------------------------------------------------------------------

AC_CHECK_HEADERS(pthread.h)
if test "x$ac_cv_header_pthread_h" != "xyes"
then
  AC_MSG_ERROR([Couldn't find pthread.h.])
fi
AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR(could not find libpthread)])

#--------------------------------------------------------------------
# Check for tcmalloc/mtmalloc
#--------------------------------------------------------------------

AC_ARG_ENABLE([tcmalloc],
    [AS_HELP_STRING([--enable-tcmalloc],
       [Enable linking with tcmalloc @<:@default=off@:>@])],
    [ac_enable_tcmalloc="$enableval"],
    [ac_enable_tcmalloc="no"])

if test "x$ac_enable_tcmalloc" != "xno"
then
  AC_CHECK_LIB(tcmalloc,malloc,[],[])
fi

if test "x$ac_cv_lib_tcmalloc_malloc" != "xyes"
then
  AC_CHECK_LIB(mtmalloc,malloc,[],[])
fi

#--------------------------------------------------------------------
# Check for libz
#--------------------------------------------------------------------

SEARCH_FOR_LIB(z, crc32, zlib.h, 
               AC_MSG_ERROR([libz is required for Drizzle]))

#--------------------------------------------------------------------
# Check for libncurses
#--------------------------------------------------------------------


SEARCH_FOR_LIB(ncurses, tgetent, ncurses/ncurses.h, [
	SEARCH_FOR_LIB(tinfo, tgetent, ncurses/ncurses.h,
		AC_MSG_ERROR([Couldn't find ncurses])
	)
	]
)

#--------------------------------------------------------------------
# Check for libreadline or compatible (libedit on Mac OS X)
#--------------------------------------------------------------------


SEARCH_FOR_LIB(readline, rl_initialize, readline/readline.h,
               AC_MSG_ERROR([libreadline is required for Drizzle]),
               [$NCURSES_LIBS]) 

save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $READLINE_CFLAGS"
AC_CHECK_HEADERS(readline/history.h)
AC_CHECK_TYPES([HIST_ENTRY], [], [], [AC_INCLUDES_DEFAULT[
#ifdef HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif
#include <readline/readline.h>
]])
AC_CHECK_DECLS([completion_matches], [], [], [AC_INCLUDES_DEFAULT[
#ifdef HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif
#include <readline/readline.h>
]])

DRIZZLE_CHECK_NEW_RL_INTERFACE
CPPFLAGS="$save_CPPFLAGS"


#--------------------------------------------------------------------
# Check for libpcre
#--------------------------------------------------------------------

AC_PATH_PROG(PKG_CONFIG, pkg-config, AC_MSG_ERROR([pkg-config wasn't found.]))
PKG_CHECK_MODULES(PCRE, [libpcre >= 3], [found_pcre="yes"],[found_pcre="no"])

if test "$found_pcre" = "no"
then
  SEARCH_FOR_LIB(pcre, pcre_compile, pcre.h, 
                 AC_MSG_ERROR([libpcre is required for Drizzle]))
fi
AC_SUBST(PCRE_LIBS)
AC_SUBST(PCRE_CFLAGS)

dnl Find paths to some shell programs
AC_PATH_PROG(LN, ln, ln)
# This must be able to take a -f flag like normal unix ln.
AC_PATH_PROG(LN_CP_F, ln, ln)

AC_PATH_PROG(MV, mv, mv)
AC_PATH_PROG(RM, rm, rm)
AC_PATH_PROG(CP, cp, cp)
AC_PATH_PROG(SED, sed, sed)
AC_PATH_PROG(CMP, cmp, cmp)
AC_PATH_PROG(CHMOD, chmod, chmod)
AC_PATH_PROG(HOSTNAME, hostname, hostname)
# Check for a GNU tar named 'gtar', or 'gnutar' (MacOS X) and
# fall back to 'tar' otherwise and hope that it's a GNU tar as well
AC_CHECK_PROGS(TAR, gnutar gtar tar)

dnl We use a path for perl so the script startup works
dnl We make sure to use perl, not perl5, in hopes that the RPMs will
dnl not depend on the perl5 binary being installed (probably a bug in RPM)
AC_PATH_PROG(PERL, perl, no)
if test "$PERL" != "no" && $PERL -e 'require 5' > /dev/null 2>&1
then
  PERL5=$PERL
else
  AC_PATH_PROG(PERL5, perl5, no)
  if test "$PERL5" != no
  then
    PERL=$PERL5
    ac_cv_path_PERL=$ac_cv_path_PERL5
  fi
fi

AC_SUBST(HOSTNAME)
AC_SUBST(PERL)
AC_SUBST(PERL5)

# icheck, used for ABI check
AC_PATH_PROG(ICHECK, icheck, no)
# "icheck" is also the name of a file system check program on Tru64.
# Verify the program found is really the interface checker.
if test "x$ICHECK" != "xno"
then
  AC_MSG_CHECKING(if $ICHECK works as expected)
  echo "int foo;" > conftest.h
  $ICHECK --canonify -o conftest.ic conftest.h 2>/dev/null
  if test -f "conftest.ic"
  then
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
    ICHECK=no
  fi
  rm -f conftest.ic conftest.h
fi
AC_SUBST(ICHECK)

# Lock for PS
AC_PATH_PROG(PS, ps, ps)
AC_MSG_CHECKING("how to check if pid exists")
PS=$ac_cv_path_PS
# Linux style
if $PS p $$ 2> /dev/null | grep `echo $0 | sed s/\-//` > /dev/null
then
  FIND_PROC="$PS p \$\$PID | grep -v grep | grep \$\$MYSQLD > /dev/null"
# Solaris
elif $PS -fp $$ 2> /dev/null | grep $0 > /dev/null
then
  FIND_PROC="$PS -p \$\$PID | grep -v grep | grep \$\$MYSQLD > /dev/null"
# BSD style
elif $PS -uaxww 2> /dev/null | grep $0 > /dev/null
then
  FIND_PROC="$PS -uaxww | grep -v grep | grep \$\$MYSQLD | grep \" \$\$PID \" > /dev/null"
# SysV style
elif $PS -ef 2> /dev/null | grep $0 > /dev/null
then
  FIND_PROC="$PS -ef | grep -v grep | grep \$\$MYSQLD | grep \" \$\$PID \" > /dev/null"
# Do anybody use this?
elif $PS $$ 2> /dev/null | grep $0 > /dev/null
then
  FIND_PROC="$PS \$\$PID | grep -v grep | grep \$\$MYSQLD > /dev/null"
else
  case $SYSTEM_TYPE in
    *freebsd*|*dragonfly*|*cygwin*)
      FIND_PROC="$PS p \$\$PID | grep -v grep | grep \$\$MYSQLD > /dev/null"
      ;;
    *darwin*)
      FIND_PROC="$PS -uaxww | grep -v grep | grep \$\$MYSQLD | grep \" \$\$PID \" > /dev/null"
      ;;
    *)
      AC_MSG_ERROR([Could not find the right ps switches. Which OS is this ?. See the Installation chapter in the Reference Manual.])
      ;;
  esac
fi
AC_SUBST(FIND_PROC)
AC_MSG_RESULT("$FIND_PROC")

# Check if a pid is valid
AC_PATH_PROG(KILL, kill, kill)
AC_MSG_CHECKING("for kill switches")
if $ac_cv_path_KILL -0 $$
then
  CHECK_PID="$ac_cv_path_KILL -0 \$\$PID > /dev/null 2> /dev/null"
elif kill -s 0 $$
then
  CHECK_PID="$ac_cv_path_KILL -s 0 \$\$PID > /dev/null 2> /dev/null"
else
  AC_MSG_WARN([kill -0 to check for pid seems to fail])
    CHECK_PID="$ac_cv_path_KILL -s SIGCONT \$\$PID > /dev/null 2> /dev/null"
fi
AC_SUBST(CHECK_PID)
AC_MSG_RESULT("$CHECK_PID")

# We need an ANSI C compiler
AM_PROG_CC_STDC

# We need an assembler, too
AM_PROG_AS
CCASFLAGS="$CCASFLAGS $ASFLAGS"

# Check if we need noexec stack for assembler
AC_CHECK_NOEXECSTACK

if test "$am_cv_prog_cc_stdc" = "no"
then
  AC_MSG_ERROR([Drizzle requires an ANSI C compiler (and a C++ compiler). Try gcc. See the Installation chapter in the Reference Manual.])
fi


AC_ARG_WITH([server-suffix],
    [AS_HELP_STRING([--with-server-suffix],
      [Append value to the version string.])],
    [ DRIZZLE_SERVER_SUFFIX=`echo "$withval" | sed -e  's/^\(...................................\)..*$/\1/'` ],
    [ DRIZZLE_SERVER_SUFFIX= ]
    )
AC_DEFINE_UNQUOTED([DRIZZLE_SERVER_SUFFIX],[$DRIZZLE_SERVER_SUFFIX],
                   [Append value to the version string])

# Force use of a curses libs
AC_ARG_WITH([named-curses-libs],
    [AS_HELP_STRING([--with-named-curses-libs=ARG],
            [Use specified curses libraries instead of those
		automatically found by configure.])],
    [ with_named_curses=$withval ],
    [ with_named_curses=no ]
    )

AC_ARG_WITH([tcp-port],
    [AS_HELP_STRING([--with-tcp-port=port-number],
            [Which port to use for Drizzle services @<:@default=4427@:>@])],
    [ DRIZZLE_TCP_PORT=$withval ],
    [ DRIZZLE_TCP_PORT=$DRIZZLE_TCP_PORT_DEFAULT
      # if we actually defaulted (as opposed to the pathological case of
      # --with-tcp-port=<DRIZZLE_TCP_PORT_DEFAULT> which might in theory
      # happen if whole batch of servers was built from a script), set
      # the default to zero to indicate that; we don't lose information
      # that way, because 0 obviously indicates that we can get the
      # default value from DRIZZLE_TCP_PORT. this seems really evil, but
      # testing for DRIZZLE_TCP_PORT==DRIZZLE_TCP_PORT_DEFAULT would make a
      # a port of DRIZZLE_TCP_PORT_DEFAULT magic even if the builder did not
      # intend it to mean "use the default, in fact, look up a good default
      # from /etc/services if you can", but really, really meant 4427 when
      # they passed in 4427. When they pass in a specific value, let them
      # have it; don't second guess user and think we know better, this will
      # just make people cross.  this makes the the logic work like this
      # (which is complicated enough):
      #
      # - if a port was set during build, use that as a default.
      #
      # - otherwise, try to look up a port in /etc/services; if that fails,
      #   use DRIZZLE_TCP_PORT_DEFAULT (at the time of this writing 4427)
      #
      # - allow the DRIZZLE_TCP_PORT environment variable to override that.
      #
      # - allow command-line parameters to override all of the above.
      #
      # the top-most DRIZZLE_TCP_PORT_DEFAULT is read from win/configure.js,
      # so don't mess with that.
      DRIZZLE_TCP_PORT_DEFAULT=0 ]
    )
AC_SUBST(DRIZZLE_TCP_PORT)
# We might want to document the assigned port in the manual.
AC_SUBST(DRIZZLE_TCP_PORT_DEFAULT)
AC_DEFINE_UNQUOTED([DRIZZLE_PORT],[$DRIZZLE_TCP_PORT],
                   [Drizzle port to use])
AC_DEFINE_UNQUOTED([DRIZZLE_PORT_DEFAULT],[$DRIZZLE_TCP_PORT_DEFAULT],
		   [If we defaulted to DRIZZLE_PORT, then this will be zero])

# Use this to set the place used for unix socket used to local communication.
AC_ARG_WITH([drizzled-user],
    [AS_HELP_STRING([--with-drizzled-user=username],
            [What user the drizzled daemon shall be run as.
		@<:@default=drizzle@:>@])],
    [ DRIZZLED_USER=$withval ],
    [ DRIZZLED_USER=drizzle ]
    )
AC_SUBST(DRIZZLED_USER)

# If we should allow LOAD DATA LOCAL
AC_MSG_CHECKING(If we should should enable LOAD DATA LOCAL by default)
AC_ARG_ENABLE(local-infile,
    [  --enable-local-infile   Enable LOAD DATA LOCAL INFILE (default: disabled)],
    [ ENABLED_LOCAL_INFILE=$enableval ],
    [ ENABLED_LOCAL_INFILE=no ]
    )
if test "$ENABLED_LOCAL_INFILE" = "yes"
then
  AC_MSG_RESULT([yes])
  AC_DEFINE([ENABLED_LOCAL_INFILE], [1],
            [If LOAD DATA LOCAL INFILE should be enabled by default])
else
  AC_MSG_RESULT([no])
fi

# Types that must be checked AFTER large file support is checked
AC_TYPE_SIZE_T

#--------------------------------------------------------------------
# Check for system header files
#--------------------------------------------------------------------

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_STDBOOL
AC_CHECK_HEADERS(fcntl.h float.h fpu_control.h ieeefp.h)
AC_CHECK_HEADERS(limits.h pwd.h select.h linux/config.h)
AC_CHECK_HEADERS(sys/fpu.h utime.h sys/utime.h )
AC_CHECK_HEADERS(synch.h sys/mman.h sys/socket.h)
AC_CHECK_HEADERS([curses.h term.h],[],[],
[[#ifdef HAVE_CURSES_H
# include <curses.h>
#endif
]])
AC_CHECK_HEADERS(termio.h termios.h sched.h alloca.h)
AC_CHECK_HEADERS(sys/prctl.h ieeefp.h)
AC_CHECK_HEADERS(execinfo.h)

#--------------------------------------------------------------------
# Check for system libraries. Adds the library to $LIBS
# and defines HAVE_LIBM etc
#--------------------------------------------------------------------

AC_CHECK_LIB(m, floor, [], AC_CHECK_LIB(m, __infinity))

AC_CHECK_FUNC(setsockopt, [], [AC_CHECK_LIB(socket, setsockopt)])
AC_CHECK_FUNC(yp_get_default_domain, [],
  [AC_CHECK_LIB(nsl, yp_get_default_domain)])
AC_CHECK_FUNC(p2open, [], [AC_CHECK_LIB(gen, p2open)])
# This may get things to compile even if bind-8 is installed
AC_CHECK_FUNC(bind, [], [AC_CHECK_LIB(bind, bind)])
# Check if crypt() exists in libc or libcrypt, sets LIBS if needed
AC_SEARCH_LIBS(crypt, crypt, AC_DEFINE(HAVE_CRYPT, 1, [crypt]))

# Check rt for aio_read
AC_CHECK_LIB(rt, aio_read)

# For the sched_yield() function on Solaris
AC_CHECK_FUNC(sched_yield, [],
  [AC_CHECK_LIB(posix4, [sched_yield],
    [AC_DEFINE(HAVE_SCHED_YIELD) LIBS="$LIBS -lposix4"])])

if test "$ac_cv_header_termio_h" = "no" -a "$ac_cv_header_termios_h" = "no"
then
  AC_CHECK_FUNC(gtty, [], [AC_CHECK_LIB(compat, gtty)])
fi

AC_CHECK_TYPES([fp_except], [], [], [
#include <sys/types.h>
#include <ieeefp.h>
])

my_save_LIBS="$LIBS"
LIBS=""
AC_CHECK_LIB(dl,dlopen)
AC_CHECK_FUNCS(dlopen)
if test "$ac_cv_func_dlopen" != "yes"
then
  AC_MSG_ERROR([Drizzle requires dlopen])
fi
LIBDL_LIBS="$LIBS"
LIBS="$my_save_LIBS"
AC_SUBST(LIBDL_LIBS)

AC_CHECK_FUNCS(strtok_r)



AC_ARG_WITH([fast-mutexes],
    [AS_HELP_STRING([--with-fast-mutexes],
	    [Compile with fast mutexes  @<:@default=off@:>@])],
    [with_fast_mutexes=$withval],
    [with_fast_mutexes=no])

if test "$with_fast_mutexes" != "no"
then
	AC_DEFINE([MY_PTHREAD_FASTMUTEX], [1], 
			[Define to 1 if you want to use fast mutexes])
fi

AM_CONDITIONAL(BUILD_FAST_MUTEX,[test "$with_fast_mutexes" != "no"])

AC_ARG_WITH([comment],
    [AS_HELP_STRING([--with-comment],
            [Comment about compilation environment. @<:@default=off@:>@])],
    [with_comment=$withval],
    [with_comment=no])
if test "$with_comment" != "no"
then
  COMPILATION_COMMENT=$with_comment
else
  COMPILATION_COMMENT="Source distribution"
fi
AC_DEFINE_UNQUOTED([COMPILATION_COMMENT],["$COMPILATION_COMMENT"],
                   [Comment about compilation environment])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_TYPE_OFF_T
AC_HEADER_TIME
AC_STRUCT_TM
# off_t is not a builtin type
AC_CHECK_SIZEOF(off_t, 4)
if test "$ac_cv_sizeof_off_t" -eq 0
then
  AC_MSG_ERROR("Drizzle needs a off_t type.")
fi

dnl
dnl check if time_t is unsigned
dnl

DRIZZLE_CHECK_TIME_T


# This always gives a warning. Ignore it unless you are cross compiling
AC_C_BIGENDIAN
#---START: Used in for client configure
# Check base type of last arg to accept
DRIZZLE_TYPE_ACCEPT
#---END:
# Figure out what type of struct rlimit to use with setrlimit
DRIZZLE_TYPE_STRUCT_RLIMIT
# Find where the stack goes
DRIZZLE_STACK_DIRECTION
# We want to skip alloca on irix unconditionally. It may work on some version..
DRIZZLE_FUNC_ALLOCA
# Do struct timespec have members tv_sec or ts_sec
DRIZZLE_TIMESPEC_TS
# Do we have the tzname variable
DRIZZLE_TZNAME
AC_CHECK_TYPES([sigset_t, off_t], [], [], [#include <sys/types.h>])
AC_CHECK_TYPES([uint, ulong])

DRIZZLE_PTHREAD_YIELD


dnl Checks for header files.
AC_CHECK_HEADERS(malloc.h)

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL

AC_CHECK_FUNCS(issetugid)

# from old readline settting:

MAKE_SHELL=/bin/sh
AC_SUBST(MAKE_SHELL)

# Already-done: stdlib.h string.h unistd.h termios.h
AC_CHECK_HEADERS(stdarg.h dirent.h locale.h ndir.h sys/dir.h \
 sys/ndir.h sys/select.h \
 sys/mman.h termcap.h termio.h asm/termbits.h grp.h \
 paths.h)

# Already-done: strcasecmp
AC_CHECK_FUNCS(lstat select)

AC_HEADER_STAT
DRIZZLE_SIGNAL_CHECK
DRIZZLE_CHECK_GETPW_FUNCS
DRIZZLE_HAVE_TIOCGWINSZ
DRIZZLE_HAVE_TIOCSTAT
DRIZZLE_TYPE_SIGHANDLER
if test "$with_named_curses" = "no"
then
  DRIZZLE_CHECK_LIB_TERMCAP
else
  TERMCAP_LIBS="$with_named_curses"
fi
AC_SUBST(TERMCAP_LIBS)

# End of readline/libedit stuff
#########################################################################

dnl Checks for library functions.

#
# The following code disables intrinsic function support while we test for
# library functions.  This is to avoid configure problems with Intel ecc
# compiler

ORG_CFLAGS="$CFLAGS"
if test "$GCC" != "yes"; then
  AC_SYS_COMPILER_FLAG(-nolib_inline,nolib_inline,CFLAGS,[],[])
fi

#AC_FUNC_MMAP
AC_TYPE_SIGNAL
DRIZZLE_TYPE_QSORT
AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF

AC_CHECK_FUNCS(fcntl)
if test "x$ac_cv_func_fcntl" != "xyes"
then
  AC_MSG_ERROR("Drizzle requires fcntl.")
fi

AC_CONFIG_LIBOBJ_DIR([mystrings])

AC_CHECK_FUNCS( \
  cuserid fchmod \
  fdatasync fpresetsticky fpsetmask fsync \
  getpassphrase getpwnam \
  getpwuid getrlimit getrusage index initgroups isnan \
  localtime_r gethrtime gmtime_r \
  madvise \
  mkstemp mlockall poll pread pthread_attr_create mmap mmap64 \
  pthread_attr_getstacksize pthread_attr_setprio pthread_attr_setschedparam \
  pthread_attr_setstacksize pthread_condattr_create pthread_getsequence_np \
  pthread_key_delete pthread_rwlock_rdlock pthread_setprio \
  pthread_setprio_np pthread_setschedparam pthread_sigmask readlink \
  realpath rename rwlock_init setupterm \
  sigaction \
  sigthreadmask \
  snprintf strpbrk \
  tell tempnam \
  backtrace backtrace_symbols backtrace_symbols_fd)

AC_LANG_PUSH(C++)
# Test whether madvise() is declared in C++ code -- it is not on some
# systems, such as Solaris
AC_CHECK_DECLS([madvise], [], [], [AC_INCLUDES_DEFAULT[
#if HAVE_SYS_MMAN_H
#include <sys/types.h>
#include <sys/mman.h>
#endif
]])
AC_LANG_POP()


AM_CONDITIONAL(BUILD_THR_RWLOCK,[test "$ac_cv_func_rwlock_init" -a "$ac_cv_funn_pthread_rwlock_rdlock"])


CFLAGS="$ORG_CFLAGS"

# Sanity check: We chould not have any fseeko symbol unless
# large_file_support=yes
AC_CHECK_FUNC(fseeko,
[if test "$large_file_support" = no -a "x$TARGET_LINUX" = "xtrue";
then
  AC_MSG_ERROR("Found fseeko symbol but large_file_support is not enabled!")
fi]
)

# Check definition of pthread_getspecific
AC_CACHE_CHECK([args to pthread_getspecific], [mysql_cv_getspecific_args],
  [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if !defined(_REENTRANT)
#define _REENTRANT
#endif
#ifndef _POSIX_PTHREAD_SEMANTICS 
#define _POSIX_PTHREAD_SEMANTICS 
#endif
#include <pthread.h> ]], [[ void *pthread_getspecific(pthread_key_t key);
pthread_getspecific((pthread_key_t) NULL); ]])],
    [mysql_cv_getspecific_args=POSIX],
    [mysql_cv_getspecific_args=other])])
  if test "$mysql_cv_getspecific_args" = "other"
  then
    AC_DEFINE([HAVE_NONPOSIX_PTHREAD_GETSPECIFIC], [1],
              [For some non posix threads])
  fi

  # Check definition of pthread_mutex_init
  AC_CACHE_CHECK([args to pthread_mutex_init], [mysql_cv_mutex_init_args],
    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS 
#endif
#include <pthread.h> ]], [[ 
  pthread_mutexattr_t attr;
  pthread_mutex_t mp;
  pthread_mutex_init(&mp,&attr); ]])],
      [mysql_cv_mutex_init_args=POSIX],
      [mysql_cv_mutex_init_args=other])])
  if test "$mysql_cv_mutex_init_args" = "other"
  then
    AC_DEFINE([HAVE_NONPOSIX_PTHREAD_MUTEX_INIT], [1],
              [For some non posix threads])
  fi
#---END:

#---START: Used in for client configure
# Check definition of readdir_r
AC_CACHE_CHECK([args to readdir_r], [mysql_cv_readdir_r],
  [AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifndef _POSIX_PTHREAD_SEMANTICS 
#define _POSIX_PTHREAD_SEMANTICS 
#endif
#include <pthread.h>
#include <dirent.h>]], [[ int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
readdir_r((DIR *) NULL, (struct dirent *) NULL, (struct dirent **) NULL); ]])],
    [mysql_cv_readdir_r=POSIX],
    [mysql_cv_readdir_r=other])])
if test "$mysql_cv_readdir_r" = "POSIX"
then
  AC_DEFINE([HAVE_READDIR_R], [1], [POSIX readdir_r])
fi

# Check definition of posix sigwait()
AC_CACHE_CHECK([style of sigwait], [mysql_cv_sigwait],
  [AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS 
#endif
#include <pthread.h>
#include <signal.h>
      ]], [[
#ifndef _AIX
sigset_t set;
int sig;
sigwait(&set,&sig);
#endif
      ]])],
    [mysql_cv_sigwait=POSIX],
    [mysql_cv_sigwait=other])])
if test "$mysql_cv_sigwait" = "POSIX"
then
  AC_DEFINE([HAVE_SIGWAIT], [1], [POSIX sigwait])
fi

if test "$mysql_cv_sigwait" != "POSIX"
then
unset mysql_cv_sigwait
# Check definition of posix sigwait()
AC_CACHE_CHECK([style of sigwait], [mysql_cv_sigwait],
  [AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS 
#endif
#include <pthread.h>
#include <signal.h>
      ]], [[
sigset_t set;
int sig;
sigwait(&set);
      ]])],
    [mysql_cv_sigwait=NONPOSIX],
    [mysql_cv_sigwait=other])])
if test "$mysql_cv_sigwait" = "NONPOSIX"
then
  AC_DEFINE([HAVE_NONPOSIX_SIGWAIT], [1], [sigwait with one argument])
fi
fi
#---END:

# Check if pthread_attr_setscope() exists
AC_CACHE_CHECK([for pthread_attr_setscope], [mysql_cv_pthread_attr_setscope],
  [AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifndef _REENTRANT
#define _REENTRANT
#endif
#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS 
#endif
#include <pthread.h>
      ]], [[
pthread_attr_t thr_attr;
pthread_attr_setscope(&thr_attr,0);
      ]])],
    [mysql_cv_pthread_attr_setscope=yes],
    [mysql_cv_pthread_attr_setscope=no])])
if test "$mysql_cv_pthread_attr_setscope" = "yes"
then
  AC_DEFINE([HAVE_PTHREAD_ATTR_SETSCOPE], [1], [pthread_attr_setscope])
fi

AC_LANG_PUSH([C++])
AC_CHECK_HEADERS(cxxabi.h)
AC_CACHE_CHECK([checking for abi::__cxa_demangle], mysql_cv_cxa_demangle,
[AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <cxxabi.h>]], [[
  char *foo= 0; int bar= 0;
  foo= abi::__cxa_demangle(foo, foo, 0, &bar);
]])],[mysql_cv_cxa_demangle=yes],[mysql_cv_cxa_demangle=no])])
AC_LANG_POP([])

if test "x$mysql_cv_cxa_demangle" = xyes; then
  AC_DEFINE(HAVE_ABI_CXA_DEMANGLE, 1,
            [Define to 1 if you have the `abi::__cxa_demangle' function.])
fi

#--------------------------------------------------------------------
# Check for requested features
#--------------------------------------------------------------------

DRIZZLE_CHECK_MAX_INDEXES
DRIZZLE_CHECK_VIO

#--------------------------------------------------------------------
# Declare our plugin modules
# Has to be done late, as the plugin may need to check for existence of
# functions tested above
#--------------------------------------------------------------------

DRIZZLE_CONFIGURE_PLUGINS([none])

AC_SUBST(mysql_plugin_dirs)
AC_SUBST(mysql_plugin_libs)
AC_SUBST(mysql_plugin_defs)


AC_ARG_ENABLE([profiling],
    [AS_HELP_STRING([--enable-profiling],
       [Toggle profiling @<:@default=off@:>@])],
    [ac_profiling="$enableval"],
    [ac_profiling="no"])

AC_ARG_ENABLE([coverage],
    [AS_HELP_STRING([--enable-coverage],
       [Toggle coverage @<:@default=off@:>@])],
    [ac_coverage="$enableval"],
    [ac_coverage="no"])

AC_ARG_ENABLE([pedantic-warnings],
    [AS_HELP_STRING([--disable-pedantic-warnings],
       [Toggle pedanticness @<:@default=on@:>@])],
    [ac_warn_pedantic="$enableval"],
    [ac_warn_pedantic="yes"])

AC_ARG_ENABLE([fail],
    [AS_HELP_STRING([--disable-fail],
       [Turn warnings into failures @<:@default=on@:>@])],
    [ac_warn_fail="$enableval"],
    [ac_warn_fail="yes"])

AC_ARG_ENABLE([unreachable],
    [AS_HELP_STRING([--enable-unreachable],
       [Enable warnings about unreachable code @<:@default=off@:>@])],
    [ac_warn_unreachable="$enableval"],
    [ac_warn_unreachable="no"])

AC_ARG_ENABLE([longlong-warnings],
    [AS_HELP_STRING([--enable-longlong-warnings],
       [Enable warnings about longlong in C++ @<:@default=off@:>@])],
    [ac_warn_longlong="$enableval"],
    [ac_warn_longlong="no"])

AC_ARG_ENABLE([strict-aliasing],
    [AS_HELP_STRING([--enable-strict-aliasing],
       [Enable warnings about stict-aliasing @<:@default=off@:>@])],
    [ac_warn_strict_aliasing="$enableval"],
    [ac_warn_strict_aliasing="no"])

AC_ARG_ENABLE([cast-warnings],
    [AS_HELP_STRING([--enable-cast-warnings],
       [Enable warnings about use of old C-style casts @<:@default=off@:>@])],
    [ac_warn_cast="$enableval"],
    [ac_warn_cast="no"])

AC_ARG_ENABLE([effective-style],
    [AS_HELP_STRING([--enable-effective-style],
       [Enable warnings violating Effective C++ Style Guidelines @<:@default=off@:>@])],
    [ac_warn_effc="$enableval"],
    [ac_warn_effc="no"])

AC_ARG_ENABLE([go-crazy],
    [AS_HELP_STRING([--enable-go-crazy],
       [Enables extra little warnings that might be too much @<:@default=off@:>@])],
    [ac_warn_go_crazy="$enableval"],
    [ac_warn_go_crazy="no"])


if test "$GCC" = "yes"
then


  if test "$ac_warn_fail" = "yes"
  then
    W_FAIL="-Werror"
  fi
  BASE_WARNINGS="-W -Wall -Wextra ${W_FAIL}"

  if test "$ac_warn_longlong" = "yes"
  then
    W_LONGLONG="-Wlong-long"
  else
    W_LONGLONG="-Wno-long-long"
  fi

  if test "$ac_warn_strict_aliasing" = "yes"
  then
    W_STRICT_ALIASING="-Wstrict-aliasing"
  else
    W_STRICT_ALIASING="-Wno-strict-aliasing"
  fi

  if test "$ac_profiling" = "yes"
  then
    GPROF_PROFILING="-pg"
  else
    GPROF_PROFILING=" "
  fi

  if test "$ac_coverage" = "yes"
  then
    GPROF_COVERAGE="-fprofile-arcs -ftest-coverage"
  else
    GPROF_COVERAGE=" "
  fi

  if test "$ac_warn_pedantic" = "yes"
  then
    save_CXXFLAGS="${CXXFLAGS}"
    CXXFLAGS="${CXXFLAGS} ${W_FAIL} -Wredundant-decls"
    AC_CACHE_CHECK([whether it is safe to use -Wredundant-decls],
      [ac_cv_safe_to_use_Wredundant_decls_],
      [AC_LANG_PUSH(C++)
       AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([
template <typename E> struct C { void foo(); };
template <typename E> void C<E>::foo() { }
template <> void C<int>::foo();
            AC_INCLUDES_DEFAULT])],
          [ac_cv_safe_to_use_Wredundant_decls_=yes],
          [ac_cv_safe_to_use_Wredundant_decls_=no])
        AC_LANG_POP()])
    if test $ac_cv_safe_to_use_Wredundant_decls_ = yes
    then
      GXX_W_REDUNDANT_DECLS="-Wredundant-decls"
    else
      GXX_W_REDUNDANT_DECLS="-Wno-redundant-decls"
    fi
    
    GCC_PEDANTIC="-pedantic -Wundef -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls ${W_STRICT_ALIASING}"
    GXX_PEDANTIC="-pedantic -Wundef ${GXX_W_REDUNDANT_DECLS} ${W_LONGLONG} ${W_STRICT_ALIASING}"

    AC_CACHE_CHECK([whether __attribute__ visibility "hidden" is supported],
      [ac_cv_can_use_hidden_],
      [AC_LANG_PUSH(C++)
       AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([
          AC_INCLUDES_DEFAULT
__attribute__((visibility ("hidden")))
void testme() {  };],[
     testme();])],  
         [ac_cv_can_use_hidden_=yes],
         [ac_cv_can_use_hidden_=no])
       AC_LANG_POP()])
    if test "$ac_cv_can_use_hidden_" = "yes"
    then
      AC_DEFINE(HAVE_ATTR_HIDDEN, 1,
                [Define to 1 if you have support for __attribute__((visibility("hidden")))])
    fi

    CXXFLAGS="${save_CXXFLAGS}"
  fi

  if test "$ac_warn_unreachable" = "yes"
  then
    W_UNREACHABLE="-Wunreachable-code"
  fi
  if test "$ac_warn_cast" = "yes"
  then
    W_CAST="-Wold-style-cast"
  fi

  if test "$ac_warn_effc" = "yes"
  then
    W_EFFC="-Weffc++"
  fi

  if test "$ac_warn_gocrazy" = "yes"
  then
    W_CRAZY="-Wshadow -Wconversion -Winvalid-pch"
  fi

  CC_WARNINGS="${C99_SUPPORT_HACK} ${BASE_WARNINGS} ${GCC_PEDANTIC} ${W_UNREACHABLE} ${GPROF_PROFILING} ${GPROF_COVERAGE} ${W_CRAZY}"
  CXX_WARNINGS="${BASE_WARNINGS} ${GXX_PEDANTIC} ${W_UNREACHABLE} ${GPROF_PROFILING} ${GPROF_COVERAGE} ${W_CAST} ${W_EFFC} ${W_CRAZY}"

  NO_EXCEPTIONS="-fno-exceptions"
  W_EXCEPTIONS="-fexceptions"
  NO_UNUSED_MACROS="-Wno-unused-macros"
  NO_REDUNDANT_DECLS="-Wno-redundant-decls"
  # Disable exceptions as they seams to create problems with gcc and threads.
  # drizzled doesn't use run-time-type-checking, so we disable it.
  AM_CXXFLAGS="${AM_CXXFLAGS} -fno-rtti"
  CPPFLAGS="${CPPFLAGS} -fpch-deps"
fi
if test "$SUNCC" = "yes"
then
  CC_WARNINGS="-v -xc99=all -errtags=yes"
  CXX_WARNINGS="+w +w2 -xport64 -errtags=yes -erroff=attrskipunsup -compat=5"
  NO_EXCEPTIONS="-features=no%except"
  W_EXCEPTIONS="-features=except"
fi
AM_CXXFLAGS="${CXX_WARNINGS} ${NO_EXCEPTIONS}  ${AM_CXXFLAGS}"
AM_CFLAGS="${CC_WARNINGS} ${AM_CFLAGS}"

AC_SUBST(NO_EXCEPTIONS)
AC_SUBST(W_EXCEPTIONS)
AC_SUBST(NO_UNUSED_MACROS)
AC_SUBST(NO_REDUNDANT_DECLS)

AC_SUBST([GLOBAL_CPPFLAGS],['-I$(top_srcdir) -I$(top_builddir)'])
AC_SUBST([AM_CPPFLAGS],['${GLOBAL_CPPFLAGS}'])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])

# Some usefull subst
AC_SUBST(CC)
AC_SUBST(GXX)

# Set configuration options for make_binary_distribution
case $SYSTEM_TYPE in
  *netware*)
    MAKE_BINARY_DISTRIBUTION_OPTIONS="$MAKE_BINARY_DISTRIBUTION_OPTIONS --no-strip"
    ;;
  *)
    : # no change for other platforms yet
    ;;
esac
AC_SUBST(MAKE_BINARY_DISTRIBUTION_OPTIONS)

dnl GCC Precompiled Header Support
dnl re-enable later
dnl AM_CONDITIONAL([BUILD_GCC_PCH],[test "$GCC" = "yes"])
AM_CONDITIONAL([BUILD_GCC_PCH],[test "no" = "yes"])

AC_CONFIG_FILES(Makefile extra/Makefile mysys/Makefile dnl
 mystrings/Makefile storage/Makefile dnl
 po/Makefile.in dnl
 libdrizzle/Makefile client/Makefile dnl
 drizzled/Makefile dnl
 drizzled/serialize/Makefile dnl
 drizzled/function/Makefile dnl
 drizzled/function/str/Makefile dnl
 drizzled/function/time/Makefile dnl
 drizzled/sql_builtin.cc dnl
 support-files/Makefile dnl
 tests/Makefile tests/install_test_db dnl
 plugin/Makefile dnl
 drizzled/drizzled_safe support-files/libdrizzle.pc dnl
 support-files/drizzle.server support-files/drizzle-log-rotate)

AC_CONFIG_COMMANDS([default], , test -z "$CONFIG_HEADERS" || echo timestamp > stamp-h)

# Ensure that table handlers gets all modifications to CFLAGS/CXXFLAGS
AC_CONFIG_COMMANDS_POST(ac_configure_args="$ac_configure_args CFLAGS='$CFLAGS' CXXFLAGS='$CXXFLAGS' AM_CFLAGS='$AM_CFLAGS' AM_CXXFLAGS='$AM_CXXFLAGS'")

AC_OUTPUT

echo "---"
echo "Configuration summary for $PACKAGE_NAME version $VERSION"
echo ""
echo "   * Installation prefix:       $prefix"
echo "   * System type:               $SYSTEM_TYPE"
echo "   * Host CPU:                  $host_cpu"
echo "   * C Compiler:                $CC_VERSION"
echo "   * C++ Compiler:              $CXX_VERSION"
echo "   * Build auth_pam:            $ac_cv_header_security_pam_appl_h"
echo "   * Assertions enabled:        $ac_cv_assert"
echo "   * Debug enabled:             $with_debug"
echo "   * Profiling enabled:         $ac_profiling"
echo "   * Coverage enabled:          $ac_coverage"
echo "   * Warnings as failure:       $ac_warn_fail"
echo "   * C++ cstdint location:      $ac_cv_cxx_cstdint"
echo "   * C++ hash_map location:     $ac_cv_cxx_hash_map"
echo "   * C++ hash namespace:        $ac_cv_cxx_hash_namespace"
echo "   * C++ cmath location:        $ac_cv_cxx_cmath"
echo "   * C++ cmath namespace:       $ac_cv_cxx_cmath_namespace"
echo ""
echo "---"
