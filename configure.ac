dnl -*- bash -*-
dnl Process this file with autoconf to produce a configure script.

dnl  Copyright (C) 2009 Sun Microsystems
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
AC_INIT([drizzle],[7],[http://bugs.launchpad.net/drizzle])
AC_CONFIG_SRCDIR([drizzled/drizzled.cc])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

PANDORA_CANONICAL_TARGET(less-warnings, warnings-always-on, use-gnulib, require-cxx, force-gcc42)
PANDORA_VC_VERSION


AC_PATH_PROG(GPERF, gperf)
AS_IF([test "x$GPERF" = "x"],
      AC_MSG_ERROR("Drizzle requires gperf to build."))

AC_CHECK_PROGS(YACC, ['bison -y'])
AS_IF([test "x$YACC" = "x" -a "$ac_cv_building_from_bzr" = "yes"],[
  AC_MSG_ERROR(["bison is required for Drizzle to build from a bzr branch"])
])


PANDORA_WITH_GETTEXT

PANDORA_REQUIRE_LIBPROTOBUF
PANDORA_PROTOBUF_REQUIRE_VERSION([2.1.0])
PANDORA_REQUIRE_PROTOC

AC_LANG_PUSH(C++)
PANDORA_REQUIRE_PTHREAD
PANDORA_REQUIRE_LIBUUID
PANDORA_REQUIRE_LIBDRIZZLE
PANDORA_REQUIRE_LIBZ
PANDORA_REQUIRE_LIBPCRE
PANDORA_REQUIRE_LIBREADLINE
PANDORA_REQUIRE_LIBDL
AC_LANG_POP

PANDORA_USE_BETTER_MALLOC

PANDORA_DRIZZLE_BUILD


AC_ARG_WITH([server-suffix],
    [AS_HELP_STRING([--with-server-suffix],
      [Append value to the version string.])],
    [ DRIZZLE_SERVER_SUFFIX=`echo "$withval" | sed -e  's/^\(...................................\)..*$/\1/'` ],
    [ DRIZZLE_SERVER_SUFFIX= ]
    )
AC_DEFINE_UNQUOTED([DRIZZLE_SERVER_SUFFIX],[$DRIZZLE_SERVER_SUFFIX],
                   [Append value to the version string])


# The port that was assigned by IANA.
DRIZZLE_TCP_PORT_DEFAULT=4427


AC_ARG_WITH([tcp-port],
    [AS_HELP_STRING([--with-tcp-port=port-number],
            [Which port to use for Drizzle TCP services @<:@default=4427@:>@])],
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
AC_SUBST(DRIZZLE_TCP_PORT_DEFAULT)
AC_DEFINE_UNQUOTED([DRIZZLE_TCP_PORT],[$DRIZZLE_TCP_PORT],
                   [Drizzle port to use])
AC_DEFINE_UNQUOTED([DRIZZLE_TCP_PORT_DEFAULT],[$DRIZZLE_TCP_PORT_DEFAULT],
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


AC_ARG_WITH([max-indexes],
            AS_HELP_STRING([--with-max-indexes=N],
                           [Sets the maximum number of indexes per table, default 64]),
            [max_indexes="$withval"],
            [max_indexes=64])
AC_MSG_CHECKING([max indexes per table])
AC_DEFINE_UNQUOTED([MAX_INDEXES], [$max_indexes],
                   [Maximum number of indexes per table])
AC_MSG_RESULT([$max_indexes])


#########################################################################


dnl Has to be done late, as the plugin may need to check for existence of
dnl functions tested above
PANDORA_PLUGINS([drizzled/plugin/config.h])

dnl GCC Precompiled Header Support
dnl re-enable later
dnl AM_CONDITIONAL([BUILD_GCC_PCH],[test "$GCC" = "yes"])
AM_CONDITIONAL([BUILD_GCC_PCH],[test "no" = "yes"])

AC_CONFIG_FILES(Makefile dnl
 gnulib/Makefile dnl
 po/Makefile.in dnl
 drizzled/hash/Makefile dnl
 drizzled/message/Makefile dnl
 drizzled/Makefile dnl
 support-files/Makefile dnl
 tests/Makefile tests/install_test_db dnl
 support-files/drizzle.server support-files/drizzle-log-rotate
 support-files/smf/Makefile dnl
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
echo "   * Host CPU:                  $host_cpu"
echo "   * C Compiler:                $CC_VERSION"
echo "   * C++ Compiler:              $CXX_VERSION"
echo "   * Build auth_pam:            $ac_cv_libpam"
echo "   * Assertions enabled:        $ac_cv_assert"
echo "   * Debug enabled:             $with_debug"
echo "   * Profiling enabled:         $ac_profiling"
echo "   * Coverage enabled:          $ac_coverage"
echo "   * Warnings as failure:       $ac_cv_warnings_as_errors"
echo "   * C++ cstdint location:      $ac_cv_cxx_cstdint"
echo "   * C++ hash_map location:     $ac_cv_cxx_hash_map"
echo "   * C++ hash namespace:        $ac_cv_cxx_hash_namespace"
echo "   * C++ shared_ptr namespace:  $ac_cv_shared_ptr_namespace"
echo ""
echo "---"

dnl libtoolize scans configure.ac  and needs to see some text
m4_define([LIBTOOLIZE_AC_INIT], [])
