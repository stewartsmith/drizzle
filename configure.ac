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
AC_INIT([drizzle],[RELEASE_VERSION],[http://bugs.launchpad.net/drizzle])
AC_CONFIG_SRCDIR([drizzled/drizzled.cc])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

PANDORA_CANONICAL_TARGET(less-warnings warnings-always-on use-gnulib require-cxx)

# Version with no dots
AC_DEFINE([DRIZZLE_VERSION_ID],[RELEASE_ID],
	  [Version ID that can be easily used for numeric comparison])

AC_DEFINE([_BACKWARD_BACKWARD_WARNING_H],[1],[Hack to disable deprecation warning in gcc])

# The port that was assigned by IANA.
DRIZZLE_TCP_PORT_DEFAULT=4427

m4_include(m4/character_sets.m4)
AC_SUBST(AVAILABLE_LANGUAGES)


# Set this for plugins to use
ac_build_drizzle="yes"


# Canonicalize the configuration name.

AC_DEFINE_UNQUOTED([HOST_VENDOR], ["$host_vendor"],[Vendor of Build System])
AC_DEFINE_UNQUOTED([HOST_OS], ["$host_os"], [OS of Build System])
AC_DEFINE_UNQUOTED([HOST_CPU], ["$host_cpu"], [CPU of Build System])

AC_DEFINE_UNQUOTED([TARGET_VENDOR], ["$target_vendor"],[Vendor of Target System])
AC_DEFINE_UNQUOTED([TARGET_OS], ["$target_os"], [OS of Target System])
AC_DEFINE_UNQUOTED([TARGET_CPU], ["$target_cpu"], [CPU of Target System])


case "$target_os" in
  *linux*)
  TARGET_LINUX="true"
  AC_SUBST(TARGET_LINUX)
  AC_DEFINE([TARGET_OS_LINUX], [1], [Whether we build for Linux])
    ;;
  *darwin*)
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

AC_PATH_PROG(GPERF, gperf)
AS_IF([test "x$GPERF" = "x"],
      AC_MSG_ERROR("Drizzle requires gperf to build."))

AC_PATH_PROG(LCOV, lcov)
AC_PATH_PROG(GENHTML, genhtml)

AM_CONDITIONAL(HAVE_LCOV,[test "x$LCOV" != "x"])

AC_CHECK_PROGS(YACC, ['bison -y'])
if test -z "$YACC" && test "$building_from_bzr" = "yes"
then
  AC_MSG_ERROR(["bison is required for Drizzle to build from a bzr branch"])
fi


AM_GNU_GETTEXT(external, need-formatstring-macros)
if test "x$MSGMERGE" = "x" -o "x$MSGMERGE" = "x:"
then
  AM_PATH_PROG_WITH_TEST(GMSGMERGE, gmsgmerge,
    [$ac_dir/$ac_word --update -q /dev/null /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1], :)
  MSGMERGE="${GMSGMERGE}"
fi
AM_CONDITIONAL([BUILD_GETTEXT],[test "x$MSGMERGE" != "x" -a "x$MSGMERGE" != "x:"])


AC_CXX_STL_HASH
AC_CXX_CSTDINT
AC_CXX_CINTTYPES

AC_LIB_PREFIX

#--------------------------------------------------------------------
# Check for libpthread
#--------------------------------------------------------------------

ACX_PTHREAD(,AC_MSG_ERROR(could not find libpthread))
LIBS="${PTHREAD_LIBS} ${LIBS}"
AM_CFLAGS="${PTHREAD_CFLAGS} ${AM_CFLAGS}"
CC="$PTHREAD_CC"

#--------------------------------------------------------------------
# Check for Google Proto Buffers
#--------------------------------------------------------------------

AC_LANG_PUSH([C++])
AC_LIB_HAVE_LINKFLAGS(protobuf,pthread,
[#include <google/protobuf/descriptor.h>
],
[google::protobuf::FileDescriptor* file;],system)
AS_IF([test x$ac_cv_libprotobuf = xno],
      AC_MSG_ERROR([protobuf is required for Drizzle. On Debian this can be found in libprotobuf-dev. On RedHat this can be found in protobuf-devel.]))

AC_CACHE_CHECK([if protobuf is recent enough], [drizzle_cv_protobuf_recent],
  [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <google/protobuf/descriptor.h>
#if GOOGLE_PROTOBUF_VERSION < 2000002
# error Your version of Protobuf is too old
#endif
    ]])],
    [drizzle_cv_protobuf_recent=yes],
    [drizzle_cv_protobuf_recent=no])])
if test "$drizzle_cv_protobuf_recent" = "no"
then
  AC_MSG_ERROR([Your version of Google Protocol Buffers is too old. Drizzle requires at least version 2.0.2])
fi

AC_PATH_PROG([PROTOC],[protoc],[no],[$LIBPROTOBUF_PREFIX/bin:$PATH])
if test "x$PROTOC" = "xno"
then
  AC_MSG_ERROR([Couldn't find the protoc compiler. On Debian this can be found in protobuf-compiler. On RedHat this can be found in protobuf-compiler.])
fi

AC_LANG_POP()

#--------------------------------------------------------------------
# Check for libuuid
#--------------------------------------------------------------------

dnl Do this by hand. Need to check for uuid/uuid.h, but uuid may or may
dnl not be a lib is weird.

AC_CHECK_HEADERS(uuid/uuid.h)
if test "x$ac_cv_header_uuid_uuid_h" = "xno"
then
  AC_MSG_ERROR([Couldn't find uuid/uuid.h. On Debian this can be found in uuid-dev. On Redhat this can be found in e2fsprogs-devel.])
fi
AC_LIB_HAVE_LINKFLAGS(uuid,,
[
#include <uuid/uuid.h>
],
[
  uuid_t uout;
  uuid_generate(uout);
])




#--------------------------------------------------------------------
# Check for tcmalloc/mtmalloc
#--------------------------------------------------------------------

case "$target_os" in
  *linux*)
    AC_ARG_ENABLE([tcmalloc],
      [AS_HELP_STRING([--enable-tcmalloc],
         [Enable linking with tcmalloc @<:@default=off@:>@])],
      [ac_enable_tcmalloc="$enableval"],
      [ac_enable_tcmalloc="no"])

    if test "x$ac_enable_tcmalloc" != "xno"
    then
      AC_CHECK_LIB(tcmalloc-minimal,malloc,[],[])

      if test "x$ac_cv_lib_tcmalloc_minimal_malloc" != "xyes"
      then
        AC_CHECK_LIB(tcmalloc,malloc,[],[])
      fi
    fi

    ;;
  *solaris*)
    AC_ARG_ENABLE([mtmalloc],
      [AS_HELP_STRING([--disable-mtmalloc],
         [Enable linking with mtmalloc @<:@default=on@:>@])],
      [ac_enable_mtmalloc="$enableval"],
      [ac_enable_mtmalloc="yes"])

    if test "x$ac_enable_mtmalloc" != "xno"
    then
      AC_CHECK_LIB(mtmalloc,malloc,[],[])
    fi
    ;;
esac


#--------------------------------------------------------------------
# Check for libdrizzle
#--------------------------------------------------------------------

AC_LANG_PUSH(C++)
AC_LIB_HAVE_LINKFLAGS(drizzle,,
[#include <libdrizzle/drizzle.h>],
[
  const char *version= drizzle_version()
])
AS_IF([test x$ac_cv_libdrizzle = xno],
      AC_MSG_ERROR([libdrizzle is required for Drizzle]))
AC_LANG_POP


#--------------------------------------------------------------------
# Check for libz
#--------------------------------------------------------------------

AC_LIB_HAVE_LINKFLAGS(z,,
[#include <zlib.h>],
[
  crc32(0, Z_NULL, 0);
])
AS_IF([test x$ac_cv_libz = xno],
      AC_MSG_ERROR([libz is required for Drizzle. On Debian this can be found in zlib1g-dev. On RedHat this can be found in zlib-devel.]))

#--------------------------------------------------------------------
# Check for TBB
#--------------------------------------------------------------------

AC_LANG_PUSH(C++)
AC_LIB_HAVE_LINKFLAGS(tbb,,
[#include <tbb/atomic.h>
 #include <stdint.h>
],
[
  tbb::atomic<uint64_t> x;
  tbb::atomic<uint8_t> y;
  x=0;
  y=0;
  x++;
  y++;
])
AC_LANG_POP()

#--------------------------------------------------------------------
# Check for libreadline or compatible (libedit on Mac OS X)
#--------------------------------------------------------------------

save_LIBS="${LIBS}"
LIBS=""
VL_LIB_READLINE
AS_IF([test "x$vl_cv_lib_readline" = "xno"],
      AC_MSG_ERROR([libreadline is required for Drizzle. On Debian this can be found in libreadline5-dev. On RedHat this can be found in readline-devel.]))
READLINE_LIBS="${LIBS}"
LIBS="${save_LIBS}"
AC_SUBST(READLINE_LIBS)

DRIZZLE_CHECK_NEW_RL_INTERFACE

#--------------------------------------------------------------------
# Check for libpcre
#--------------------------------------------------------------------

AC_LIB_HAVE_LINKFLAGS(pcre,,
[#include <pcre.h>],
[
  pcre *re= NULL;
  pcre_version();
])
AS_IF([test "x$ac_cv_libpcre" = "xno"],
[
  unset ac_cv_libpcre
  unset HAVE_LIBPCRE
  unset LIBPCRE
  unset LIBPCRE_PREFIX
  unset LTLIBPCRE
  AC_LIB_HAVE_LINKFLAGS(pcre,,
  [#include <pcre/pcre.h>],
  [
    pcre *re= NULL;
    pcre_version();
  ])
  AS_IF([test "x$ac_cv_libpcre" = "xno"],
  [
    AC_MSG_ERROR([libpcre is required for Drizzle. On Debian this can be found in libpcre3-dev. On RedHat this can be found in pcre-devel.])
  ],[
    AC_DEFINE(PCRE_HEADER,[<pcre/pcre.h>],[Location of pcre header])
  ])
],[
  AC_DEFINE(PCRE_HEADER,[<pcre.h>],[Location of pcre header])
])


AC_PATH_PROG(PERL, perl, no)


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
    [AC_DEFINE(HAVE_SCHED_YIELD, 1, [Have sched_yield function]) LIBS="$LIBS -lposix4"])])

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


AC_ARG_WITH([atomic-ops],
    [AS_HELP_STRING([--with-atomic-ops=rwlocks|smp|up],
       [Implement atomic operations using pthread rwlocks or atomic CPU
        instructions for multi-processor or uniprocessor
        configuration. By default gcc built-in sync functions are used,
        if available and 'smp' configuration otherwise.])],
    [with_atomic_ops="$withval"],
    [with_atomic_ops=smp])

case "$with_atomic_ops" in
  "up") AC_DEFINE([MY_ATOMIC_MODE_DUMMY], [1],
                  [Assume single-CPU mode, no concurrency]) ;;
  "rwlocks") AC_DEFINE([MY_ATOMIC_MODE_RWLOCKS], [1],
                  [Use pthread rwlocks for atomic ops]) ;;
  "smp") 
    AC_CACHE_CHECK(
      [whether the compiler provides atomic builtins],
      [ac_cv_gcc_atomic_builtins],
      [AC_RUN_IFELSE(
        [AC_LANG_PROGRAM([],[[
          int foo= -10; int bar= 10;
          if (!__sync_fetch_and_add(&foo, bar) || foo)
            return -1;
          bar= __sync_lock_test_and_set(&foo, bar);
          if (bar || foo != 10)
            return -1;
          bar= __sync_val_compare_and_swap(&bar, foo, 15);
          if (bar)
            return -1;
          return 0;
        ]])],
       [ac_cv_gcc_atomic_builtins=yes],
       [ac_cv_gcc_atomic_builtins=no])])

    if test "x$ac_cv_gcc_atomic_builtins" = "xyes"; then
      AC_DEFINE(HAVE_GCC_ATOMIC_BUILTINS, 1,
                [Define to 1 if compiler provides atomic builtins.])
    fi
   ;;
   *) AC_MSG_ERROR(["$with_atomic_ops" is not a valid value for --with-atomic-ops]) ;;
esac


AC_ARG_WITH([comment],
    [AS_HELP_STRING([--with-comment],
            [Comment about compilation environment. @<:@default=off@:>@])],
    [with_comment=$withval],
    [with_comment=no])
if test "$with_comment" != "no"
then
  COMPILATION_COMMENT=$with_comment
else
  COMPILATION_COMMENT="Source distribution (RELEASE_COMMENT)"
fi
AC_DEFINE_UNQUOTED([COMPILATION_COMMENT],["$COMPILATION_COMMENT"],
                   [Comment about compilation environment])

dnl Checks for typedefs, structures, and compiler characteristics.

AC_STRUCT_TM
# off_t is not a builtin type
AC_CHECK_SIZEOF(off_t, 4)
if test "$ac_cv_sizeof_off_t" -eq 0
then
  AC_MSG_ERROR("Drizzle needs a off_t type.")
fi
AC_CHECK_SIZEOF(size_t)
AC_DEFINE_UNQUOTED([SIZEOF_SIZE_T],[$ac_cv_sizeof_size_t],[Size of size_t as computed by sizeof()])
AC_CHECK_SIZEOF(long long)
AC_DEFINE_UNQUOTED(SIZEOF_LONG_LONG,[$ac_cv_sizeof_long_long],[Size of long long as computed by sizeof()])

dnl
dnl check if time_t is unsigned
dnl

DRIZZLE_CHECK_TIME_T


# We want to skip alloca on irix unconditionally. It may work on some version..
DRIZZLE_FUNC_ALLOCA
DRIZZLE_STACK_DIRECTION

AC_CHECK_TYPES([sigset_t, off_t], [], [], [#include <sys/types.h>])
AC_CHECK_TYPES([uint, ulong])

DRIZZLE_PTHREAD_YIELD


dnl Checks for header files.
AC_CHECK_HEADERS(malloc.h)

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_PROG_GCC_TRADITIONAL


AC_CHECK_FUNCS(issetugid)

# Already-done: stdlib.h string.h unistd.h termios.h
AC_CHECK_HEADERS(stdarg.h dirent.h locale.h ndir.h sys/dir.h \
 sys/ndir.h sys/select.h \
 sys/mman.h termcap.h termio.h asm/termbits.h grp.h \
 paths.h)

# Already-done: strcasecmp
AC_CHECK_FUNCS(lstat select)

AC_HEADER_STAT
DRIZZLE_HAVE_TIOCGWINSZ
DRIZZLE_HAVE_TIOCSTAT

#########################################################################

dnl Checks for library functions.

AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF

AC_CHECK_FUNCS(fcntl)
if test "x$ac_cv_func_fcntl" != "xyes"
then
  AC_MSG_ERROR("Drizzle requires fcntl.")
fi

AC_CONFIG_LIBOBJ_DIR([gnulib])

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
#include <pthread.h>
   ]], [[
void *pthread_getspecific(pthread_key_t key);
pthread_getspecific((pthread_key_t) NULL);
   ]])],
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

#--------------------------------------------------------------------
# Declare our plugin modules
# Has to be done late, as the plugin may need to check for existence of
# functions tested above
#--------------------------------------------------------------------

m4_include(config/plugin.ac)
dnl Add code here to read set plugin lists and  set drizzled_default_plugin_list
AC_DEFINE_UNQUOTED([DRIZZLED_PLUGIN_LIST],[$drizzled_default_plugin_list],
                   [List of plugins that should be loaded on startup if no
                    value is given for --plugin-load])

drizzled_builtin_list=`echo $drizzled_builtin_list | sed 's/, *$//'`
AC_DEFINE_UNQUOTED([DRIZZLED_BUILTIN_LIST],[$drizzled_builtin_list],
                   [List of plugins to be loaded in sql_builtin.cc])
AC_SUBST(drizzled_plugin_libs)

AC_SUBST(mysql_plugin_dirs)
AC_SUBST(mysql_plugin_libs)
drizzled_plugin_defs=`echo $drizzled_plugin_defs | sed 's/, *$//'`
AC_SUBST(drizzled_plugin_defs)
AC_SUBST(DRIZZLED_PLUGIN_DEP_LIBS)

dnl Must be done once we turn on warnings and such
AC_CACHE_CHECK(
  [whether __attribute__ visibility "hidden" is supported],
  [ac_cv_can_use_hidden_],[
    AC_LANG_PUSH(C++)
    save_CXXFLAGS="${CXXFLAGS}"
    CXXFLAGS="${AM_CXXFLAGS}"
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM(
        [[
__attribute__((visibility ("hidden")))
void testme() {  }
        ]],[[testme()]]
      )],  
      [ac_cv_can_use_hidden_=yes],
      [ac_cv_can_use_hidden_=no])
    CXXFLAGS="${save_CXXFLAGS}"
    AC_LANG_POP()])
AS_IF([test "$ac_cv_can_use_hidden_" = "yes"],[
  AC_DEFINE(HAVE_ATTR_HIDDEN, 1,
  [Define to 1 if you have support for __attribute__((visibility("hidden")))])
])


AC_SUBST(pkgplugindir,"\$(pkglibdir)/plugin")

dnl GCC Precompiled Header Support
dnl re-enable later
dnl AM_CONDITIONAL([BUILD_GCC_PCH],[test "$GCC" = "yes"])
AM_CONDITIONAL([BUILD_GCC_PCH],[test "no" = "yes"])

AC_CONFIG_FILES(Makefile dnl
 gnulib/Makefile dnl
 po/Makefile.in dnl
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
echo "Configuration summary for $PACKAGE_NAME version $VERSION RELEASE_COMMENT"
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
