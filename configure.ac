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


AC_PREREQ(2.59)dnl		Minimum Autoconf version required.
dnl Version from the AC_INIT call below is overridden later
dnl So don't update it here
AC_INIT([drizzle7],[7],[http://bugs.launchpad.net/drizzle])
AC_CONFIG_SRCDIR([drizzled/drizzled.cc])
AC_CONFIG_AUX_DIR([config])

PANDORA_CANONICAL_TARGET(less-warnings, require-cxx, force-gcc42, version-from-vc) 

LIBDRIZZLE_LIBRARY_VERSION=3:0:0
#                          | | |
#                   +------+ | +---+
#                   |        |     |
#                current:revision:age
#                   |        |     |
#                   |        |     +- increment if interfaces have been added
#                   |        |        set to zero if interfaces have been
#                   |        |        removed or changed
#                   |        +- increment if source code has changed
#                   |           set to zero if current is incremented
#                   +- increment if interfaces have been added, removed or
#                      changed
AC_SUBST(LIBDRIZZLE_LIBRARY_VERSION)
OLDLIBDRIZZLE_LIBRARY_VERSION=2:0:1
#                             | | |
#                      +------+ | +---+
#                      |        |     |
#                   current:revision:age
#                      |        |     |
#                      |        |     +- increment if interfaces have been added
#                      |        |        set to zero if interfaces have been
#                      |        |        removed or changed
#                      |        +- increment if source code has changed
#                      |           set to zero if current is incremented
#                      +- increment if interfaces have been added, removed or
#                         changed
AC_SUBST(OLDLIBDRIZZLE_LIBRARY_VERSION)

libdrizzleincludedir='${includedir}/libdrizzle-2.0'
libdrizzleinstalldir='${libdrizzleincludedir}/libdrizzle'
AC_SUBST(libdrizzleinstalldir)
AC_SUBST(libdrizzleincludedir)

oldlibdrizzleincludedir='${includedir}/libdrizzle-1.0'
oldlibdrizzleinstalldir='${oldlibdrizzleincludedir}/libdrizzle'
AC_SUBST(oldlibdrizzleinstalldir)
AC_SUBST(oldlibdrizzleincludedir)

# libdrizzle versioning when linked with GNU ld.
AS_IF([test "$lt_cv_prog_gnu_ld" = "yes"],[
  LD_VERSION_SCRIPT="-Wl,--version-script=\$(top_srcdir)/config/drizzle.ver"
  ])
AC_SUBST(LD_VERSION_SCRIPT)

dnl Set the default datadir location to /usr/local/var/drizzle. If a
dnl localstatedir is explicitly given, just use that.
AS_IF([test "x${localstatedir}" = 'x${prefix}/var'],[
  localstatedir='${prefix}/var/drizzle'
])

AC_ARG_WITH([server],
    [AS_HELP_STRING([--without-server],
      [Do not build the drizzled server.])],
    [ pandora_cv_skip_requires="$withval" ]
    [ pandora_cv_skip_requires=no ])

AS_IF([test "x${pandora_cv_skip_requires}" != "xno"],[

  AC_PATH_PROG(GPERF, gperf)
  AS_IF([test "x$GPERF" = "x"],
    AC_MSG_ERROR("Drizzle requires gperf to build."))

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

AC_LANG_PUSH(C++)
PANDORA_REQUIRE_PTHREAD
PANDORA_REQUIRE_LIBUUID
PANDORA_REQUIRE_LIBZ
PANDORA_REQUIRE_LIBPCRE
PANDORA_REQUIRE_LIBREADLINE
PANDORA_REQUIRE_LIBDL
AC_LANG_POP

PANDORA_DRIZZLE_BUILD
PANDORA_HAVE_BOOST_TEST
PANDORA_HAVE_LIBSQLITE3



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
 support-files/drizzle7.pc dnl
 support-files/libdrizzle-1.0.pc dnl
 support-files/libdrizzle-2.0.pc dnl
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
echo "   * C++ Compiler:              $CXX_VERSION"
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
