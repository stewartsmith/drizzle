dnl -*- bash -*-
dnl Process this file with autoconf to produce a configure script.

dnl  Copyright (C) 2009 Sun Microsystems, Inc.
dnl
dnl  This program is free software; you can redistribute it and/or modify
dnl  it under the terms of the GNU General Public License as published by
dnl  the Free Software Foundation; version 2 of the License.
dnl
dnl  This program is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl  GNU General Public License for more details.
dnl
dnl  You should have received a copy of the GNU General Public License
dnl  along with this program; if not, write to the Free Software
dnl  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


AC_INIT([drizzle],[7.2],[http://bugs.launchpad.net/drizzle],[drizzle],[http://drizzle.org/])

AC_CONFIG_AUX_DIR([config])

AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability subdir-objects foreign tar-ustar])
AC_PREREQ(2.59)dnl		Minimum Autoconf version required.

AC_CANONICAL_HOST
AC_CANONICAL_BUILD
AC_ARG_PROGRAM
AC_USE_SYSTEM_EXTENSIONS

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([config.h])

AC_CONFIG_SRCDIR([drizzled/drizzled.cc])

PANDORA_CANONICAL_TARGET([less-warnings, require-cxx, force-gcc42, version-from-vc]) 

m4_include([m4/libdrizzle_version.m4])

LT_PREREQ([2.2])
LT_INIT
LT_LANG([C++])


gl_VISIBILITY
AX_ASSERT

dnl Set the default datadir location to /usr/local/var/drizzle. If a
dnl localstatedir is explicitly given, just use that.
AS_IF([test "x${localstatedir}" = 'x${prefix}/var'],[
  localstatedir='${prefix}/var/drizzle'
])

AS_IF([test "x${pandora_cv_skip_requires}" != "xno"],[

  AC_PATH_PROG([GPERF],[gperf],[AC_MSG_ERROR([Drizzle requires gperf to build.])])

  # libexecinfo provides backtrace_symbols on FreeBSD
  AC_SEARCH_LIBS(backtrace_symbols, [execinfo])

  AC_FUNC_STRERROR_R

  DRIZZLE_STACK_DIRECTION

  PANDORA_USE_BETTER_MALLOC
  AC_CHECK_FUNC(inet_ntoa, [], [AC_CHECK_LIB(nsl, inet_ntoa)])


  AC_ARG_WITH([server-suffix],
      [AS_HELP_STRING([--with-server-suffix],
        [Append value to the version string.])],
      [ DRIZZLE_SERVER_SUFFIX="$withval" ]
      [ DRIZZLE_SERVER_SUFFIX= ])

  AC_DEFINE_UNQUOTED([DRIZZLE_SERVER_SUFFIX],[$DRIZZLE_SERVER_SUFFIX],
    [Append value to the version string])

  AC_DEFINE([BUILDING_DRIZZLED],[1],
    [Define indicating we are building and not consuming drizzle.])


  AC_ARG_WITH([drizzled-user],
    [AS_HELP_STRING([--with-drizzled-user=username],
      [What user the drizzled daemon shall be run as.
      @<:@default=drizzle@:>@])],
    [ DRIZZLED_USER="$withval" ],
    [ DRIZZLED_USER=drizzle ])
  AC_SUBST(DRIZZLED_USER)


  AC_ARG_WITH([comment],
    [AS_HELP_STRING([--with-comment],
      [Comment about compilation environment. @<:@default=off@:>@])],
    [with_comment=$withval],
    [with_comment=no])
  AS_IF([test "$with_comment" != "no"],[
    COMPILATION_COMMENT=$with_comment
    ],[
    COMPILATION_COMMENT="Source distribution (${PANDORA_RELEASE_COMMENT})"
    ])
  AC_DEFINE_UNQUOTED([COMPILATION_COMMENT],["$COMPILATION_COMMENT"],
    [Comment about compilation environment])
])

PANDORA_REQUIRE_LIBPROTOBUF
PANDORA_PROTOBUF_REQUIRE_VERSION([2.1.0])
PANDORA_REQUIRE_PROTOC

AC_DEFUN([DRIZZLE_UUID],[
          AC_REQUIRE([AX_UUID_GENERATE_TIME_SAFE])dnl
          AS_IF([ test "$ax_libuuid" = yes ],,[AC_MSG_ERROR([Drizzle requires uuid/uuid.h to build.])])
          AS_IF([ test "$ax_cv_uuid_generate_time_safe" = yes ],,[AC_MSG_ERROR([Drizzle requires a uuid/uuid.h with uuid_generate_time_safe() to build.])])
          ])
DRIZZLE_UUID

AC_LANG_PUSH(C++)
PANDORA_REQUIRE_LIBZ
PANDORA_REQUIRE_LIBSSL
PANDORA_REQUIRE_LIBPCRE
PANDORA_REQUIRE_LIBREADLINE
PANDORA_REQUIRE_LIBDL
AC_LANG_POP

AM_CONDITIONAL([HAVE_LIBSQLITE3],[false])

PANDORA_DRIZZLE_BUILD
BOOST_REQUIRE([1.46])
BOOST_PROGRAM_OPTIONS
BOOST_THREADS
BOOST_DATE_TIME
BOOST_FILESYSTEM
BOOST_IOSTREAMS
BOOST_REGEX
BOOST_TEST

AM_CONDITIONAL([HAVE_BOOST_TEST],[ test "x$ac_cv_header_boost_test_unit_test_hpp" = "xyes" ])

AX_PTHREAD([ 
            AM_CFLAGS="${PTHREAD_CFLAGS} ${AM_CFLAGS}"
            AM_CXXFLAGS="${PTHREAD_CFLAGS} ${AM_CXXFLAGS}"
            AM_LDFLAGS="${PTHREAD_LIBS} ${AM_LDFLAGS}"
            LIBS="${PTHREAD_LIBS} ${LIBS}"
            ], [AC_MSG_ERROR([Drizzle requires pthreads])])

# Specialty checks

# Checks for programs.

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([sys/socket.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_C_RESTRICT

AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_SIZE_T
AC_SYS_LARGEFILE
PANDORA_CLOCK_GETTIME

# off_t is not a builtin type
AC_CHECK_SIZEOF(off_t, 4)
AS_IF([test "$ac_cv_sizeof_off_t" -eq 0],[
       PANDORA_MSG_ERROR("${PACKAGE} needs an off_t type.")
       ])

AC_CHECK_SIZEOF(size_t)
AS_IF([test "$ac_cv_sizeof_size_t" -eq 0],[
       PANDORA_MSG_ERROR("${PACKAGE} needs an size_t type.")
       ])

AC_DEFINE_UNQUOTED([SIZEOF_SIZE_T],[$ac_cv_sizeof_size_t],[Size of size_t as computed by sizeof()])
AC_CHECK_SIZEOF(long long)
AC_DEFINE_UNQUOTED([SIZEOF_LONG_LONG],[$ac_cv_sizeof_long_long],[Size of long long as computed by sizeof()])




# Checks for library functions.
AC_CHECK_FUNC([setsockopt],[],[AC_CHECK_LIB(socket, setsockopt)])
AC_CHECK_FUNC([bind],[],[AC_CHECK_LIB(bind, bind)])

AC_CHECK_PROGS([DOXYGEN],[doxygen])
AX_AM_JOBSERVER([yes])

#########################################################################


dnl Has to be done late, as the plugin may need to check for existence of
dnl functions tested above
PANDORA_PLUGINS([drizzled/module/load_list.h])

dnl GCC Precompiled Header Support
dnl re-enable later
dnl AM_CONDITIONAL([BUILD_GCC_PCH],[test "$GCC" = "yes"])
AM_CONDITIONAL([BUILD_GCC_PCH],[test "no" = "yes"])

AS_IF([test "$lt_cv_prog_gnu_ld" = "yes"],[
  LDFLAGS="${LDFLAGS} ${LD_VERSION_SCRIPT}"
  ])

AS_IF([test "x${ac_gcc_profile_mode}" = "xyes" -a "x${pandora_cv_boost_profile}" = "xyes"],[
    AC_DEFINE([_GLIBCXX_PROFILE],[1],[Enable GCC Profile Mode])
    ])


AC_CONFIG_FILES(Makefile dnl
 tests/Makefile dnl
 drizzled/plugin/version.h dnl
 support-files/rpm/SPECS/drizzle.spec dnl
 libdrizzle-1.0/version.h dnl
 support-files/drizzle.pc dnl
 support-files/libdrizzle.pc dnl
 support-files/libdrizzle-1.0.pc dnl
 support-files/smf/install.sh dnl
 support-files/smf/drizzle.xml dnl
 support-files/smf/drizzle)

scheduling_plugins_available="
  pool_of_threads 
  single_thread
"

for sched_plugin in $scheduling_plugins_available
do
  varname="\${with_plugin_${sched_plugin}}"
  result=`eval "echo $varname"`
  if test "x$result" = "xyes"
  then
    scheduling_plugins="$sched_plugin $scheduling_plugins"
  fi
done

AC_OUTPUT

echo "---"
echo "Configuration summary for $PACKAGE_NAME version $VERSION $PANDORA_RELEASE_COMMENT"
echo ""
echo "   * Installation prefix:       $prefix"
echo "   * System type:               $host_vendor-$host_os"
echo "   * pandora-build version:     PANDORA_CANONICAL_VERSION"
echo "   * Host CPU:                  $host_cpu"
echo "   * C Compiler:                $CC_VERSION"
echo "   * C Flags:                   $CFLAGS"
echo "   * C AM Flags                 $AM_CFLAGS"
echo "   * C++ Compiler:              $CXX_VERSION"
echo "   * C++ Flags:                 $CXXFLAGS"
echo "   * C++ AM Flags:              $AM_CXXFLAGS"
echo "   * CPP Flags:                 $CPPFLAGS"
echo "   * CPP AM Flags:              $AM_CPPFLAGS"
echo "   * Assertions enabled:        $ac_cv_assert"
echo "   * Debug enabled:             $with_debug"
echo "   * Profiling enabled:         $ac_profiling"
echo "   * Coverage enabled:          $ac_coverage"
echo "   * Warnings as failure:       $ac_cv_warnings_as_errors"
echo ""
echo "---"

case "$host_os" in
  *freebsd*)
    echo "*****"
    echo "*"
    echo "*  NOTE: You are on FreeBSD. BSD make will not work."
    echo "*"
    echo "*   use 'gmake' To build Drizzle"
    echo "*"
    echo "*  And ensure that /usr/local/lib/gcc44 is in your LD_LIBRARY_PATH"
    echo "*" 
    echo "*****"
  ;;
esac


dnl libtoolize scans configure.ac  and needs to see some text
m4_define([LIBTOOLIZE_AC_INIT], [])
m4_define([UNUSED_MACRO], [
  LT_INIT
  AC_CONFIG_MACRO_DIR([m4])
])
