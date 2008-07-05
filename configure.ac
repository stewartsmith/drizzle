dnl -*- ksh -*-
dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)dnl		Minimum Autoconf version required.

AC_INIT
AC_CONFIG_SRCDIR([sql/mysqld.cc])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADERS([include/config.h:config.h.in])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE(mysql, 7.0.0, no-define)

PROTOCOL_VERSION=10
DOT_FRM_VERSION=6
# See the libtool docs for information on how to do shared lib versions.
SHARED_LIB_MAJOR_VERSION=16
SHARED_LIB_VERSION=$SHARED_LIB_MAJOR_VERSION:0:0



# Set all version vars based on $VERSION. How do we do this more elegant ?
# Remember that regexps needs to quote [ and ] since this is run through m4
# We take some made up examples
#
#  VERSION                  5.1.40sp1-alpha     5.0.34a
#  MYSQL_NO_DASH_VERSION    5.1.40sp1           5.0.34a
#  MYSQL_NUMERIC_VERSION    5.1.40              5.0.34
#  MYSQL_BASE_VERSION       5.1                 5.0
#  MYSQL_VERSION_ID         50140               50034
#
MYSQL_NO_DASH_VERSION=`echo $VERSION | sed -e "s|-.*$||"`
MYSQL_NUMERIC_VERSION=`echo $MYSQL_NO_DASH_VERSION | sed -e "s|[[a-z]][[a-z0-9]]*$||"`
MYSQL_BASE_VERSION=`echo $MYSQL_NUMERIC_VERSION | sed -e "s|\.[[^.]]*$||"`
MYSQL_VERSION_ID=`echo $MYSQL_NUMERIC_VERSION | \
    awk -F. '{printf "%d%0.2d%0.2d", $1, $2, $3}'`

# The port should be constant for a LONG time
MYSQL_TCP_PORT_DEFAULT=3306

dnl Include m4 
sinclude(config/ac-macros/alloca.m4)
sinclude(config/ac-macros/check_cpu.m4)
sinclude(config/ac-macros/character_sets.m4)
sinclude(config/ac-macros/compiler_flag.m4)
sinclude(config/ac-macros/dtrace.m4)
sinclude(config/ac-macros/plugins.m4)
sinclude(config/ac-macros/large_file.m4)
sinclude(config/ac-macros/misc.m4)
sinclude(config/ac-macros/readline.m4)
sinclude(config/ac-macros/ssl.m4)

# Remember to add a directory sql/share/LANGUAGE
AVAILABLE_LANGUAGES="\
czech danish dutch english estonian french german greek hungarian \
italian japanese korean norwegian norwegian-ny polish portuguese \
romanian russian serbian slovak spanish swedish ukrainian"

#####
#####

AC_SUBST(MYSQL_NO_DASH_VERSION)
AC_SUBST(MYSQL_BASE_VERSION)
AC_SUBST(MYSQL_VERSION_ID)
AC_SUBST(MYSQL_PREVIOUS_BASE_VERSION)
AC_SUBST(PROTOCOL_VERSION)
AC_DEFINE_UNQUOTED([PROTOCOL_VERSION], [$PROTOCOL_VERSION],
                   [mysql client protocol version])
AC_SUBST(DOT_FRM_VERSION)
AC_DEFINE_UNQUOTED([DOT_FRM_VERSION], [$DOT_FRM_VERSION],
                   [Version of .frm files])
AC_SUBST(SHARED_LIB_MAJOR_VERSION)
AC_SUBST(SHARED_LIB_VERSION)
AC_SUBST(AVAILABLE_LANGUAGES)


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

AM_SANITY_CHECK
# This is needed is SUBDIRS is set
AC_PROG_MAKE_SET

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

################ End of section to be done before AC_PROG_CC #################

# The following hack should ensure that configure doesn't add optimizing
# or debugging flags to CFLAGS or CXXFLAGS
# C_EXTRA_FLAGS are flags that are automaticly added to both
# CFLAGS and CXXFLAGS
CFLAGS="$CFLAGS $C_EXTRA_FLAGS "
CXXFLAGS="$CXXFLAGS $C_EXTRA_FLAGS "

dnl Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AM_PROG_CC_C_O

# Print version of CC and CXX compiler (if they support --version)
case $SYSTEM_TYPE in
  *netware*)
CC_VERSION=`$CC -version | grep -i version`
    ;;
  *)
CC_VERSION=`$CC --version | sed 1q`
    ;;
esac
if test $? -eq "0"
then
  AC_MSG_CHECKING("C Compiler version")
  AC_MSG_RESULT("$CC $CC_VERSION")
else
CC_VERSION=""
fi
AC_SUBST(CC_VERSION)
MYSQL_CHECK_CXX_VERSION

if test "$ac_cv_c_compiler_gnu" = "yes"
then
  AS="$CC -c"
  AC_SUBST(AS)
else
  AC_PATH_PROG(AS, as, as)
fi

# Still need ranlib for readline; local static use only so no libtool.
AC_PROG_RANLIB
# We use libtool
#AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

# Ensure that we have --preserve-dup-deps defines, otherwise we get link
# problems of 'mysql' with CXX=g++
LIBTOOL="$LIBTOOL --preserve-dup-deps"
AC_SUBST(LIBTOOL)dnl

AC_SUBST(NM)dnl

AC_PROG_INSTALL
test -z "$INSTALL_SCRIPT" && INSTALL_SCRIPT='${INSTALL_PROGRAM}'

# Not critical since the generated file is distributed
AC_CHECK_PROGS(YACC, ['bison -y -p MYSQL'])

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

# This should be rewritten to use $target_os
case "$target_os" in
  *solaris*)
    TARGET_SOLARIS="true"
    AC_DEFINE([TARGET_OS_SOLARIS], [1], [Whether we are building for Solaris])
    AC_SUBST(TARGET_SOLARIS)
  ;;
esac

# The following is required for portable results of floating point calculations
# on PowerPC. The same must also be done for IA-64, but this options is missing
# in the IA-64 gcc backend.

if test "$GCC" = "yes"
then
  case "$host_cpu" in
    *ppc* | *powerpc*)
      CFLAGS="$CFLAGS -mno-fused-madd"
      CXXFLAGS="$CXXFLAGS -mno-fused-madd"
    ;;
  esac
fi

AC_SUBST(CC)
AC_SUBST(CFLAGS)
AC_SUBST(CXX)
AC_SUBST(CXXFLAGS)
AC_SUBST(ASFLAGS)
AC_SUBST(LD)
AC_SUBST(INSTALL_SCRIPT)


export CC CXX CFLAGS LD LDFLAGS AR ARFLAGS

if test "$GCC" = "yes"
then
  # mysqld requires -fno-implicit-templates.
  # Disable exceptions as they seams to create problems with gcc and threads.
  # mysqld doesn't use run-time-type-checking, so we disable it.
  # We should use -Wno-invalid-offsetof flag to disable some warnings from gcc
  # regarding offset() usage in C++ which are done in a safe manner in the
  # server
  CXXFLAGS="$CXXFLAGS ${GCC_WARNINGS} -fno-implicit-templates -fno-exceptions -fno-rtti"
  CFLAGS="$CFLAGS ${GCC_WARNINGS} "
  AC_DEFINE([HAVE_EXPLICIT_TEMPLATE_INSTANTIATION],
    [1], [Defined by configure. Use explicit template instantiation.])
fi

MYSQL_PROG_AR

# libmysqlclient versioning when linked with GNU ld.
if $LD --version 2>/dev/null|grep -q GNU; then
  LD_VERSION_SCRIPT="-Wl,--version-script=\$(top_builddir)/libmysql/libmysql.ver"
  AC_CONFIG_FILES(libmysql/libmysql.ver)
fi
AC_SUBST(LD_VERSION_SCRIPT)


# Avoid bug in fcntl on some versions of linux
AC_MSG_CHECKING([if we should use 'skip-external-locking' as default for $target_os])
# Any variation of Linux
if expr "$target_os" : "[[Ll]]inux.*" > /dev/null
then
  MYSQLD_DEFAULT_SWITCHES="--skip-external-locking"
  TARGET_LINUX="true"
  AC_MSG_RESULT([yes])
  AC_DEFINE([TARGET_OS_LINUX], [1], [Whether we build for Linux])
else
  MYSQLD_DEFAULT_SWITCHES=""
  TARGET_LINUX="false"
  AC_MSG_RESULT([no])
fi
AC_SUBST(MYSQLD_DEFAULT_SWITCHES)
AC_SUBST(TARGET_LINUX)

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
    *freebsd*|*dragonfly*)
      FIND_PROC="$PS p \$\$PID | grep -v grep | grep \$\$MYSQLD > /dev/null"
      ;;
    *darwin*)
      FIND_PROC="$PS -uaxww | grep -v grep | grep \$\$MYSQLD | grep \" \$\$PID \" > /dev/null"
      ;;
    *)
      AC_MSG_ERROR([Could not find the right ps switches. Which OS is this ?. See the Installation chapter in the Reference Manual.])
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
  AC_MSG_ERROR([MySQL requires an ANSI C compiler (and a C++ compiler). Try gcc. See the Installation chapter in the Reference Manual.])
fi

static_nss=""
STATIC_NSS_FLAGS=""
OTHER_LIBC_LIB=""

#
# Check if we are using Linux and a glibc compiled with static nss
# (this is true on the MySQL build machines to avoid NSS problems)
#

if test "$TARGET_LINUX" = "true" -a "$static_nss" = ""
then
  tmp=`nm /usr/lib*/libc.a  | grep _nss_files_getaliasent_r`
  if test -n "$tmp"
  then
     STATIC_NSS_FLAGS="-lc -lnss_files -lnss_dns -lresolv"
     STATIC_NSS_FLAGS="$STATIC_NSS_FLAGS $STATIC_NSS_FLAGS"
     static_nss=1
  fi
fi

AC_ARG_WITH([server-suffix],
    [AS_HELP_STRING([--with-server-suffix],
      [Append value to the version string.])],
    [ MYSQL_SERVER_SUFFIX=`echo "$withval" | sed -e  's/^\(...................................\)..*$/\1/'` ],
    [ MYSQL_SERVER_SUFFIX= ]
    )
AC_SUBST(MYSQL_SERVER_SUFFIX)

# Force use of a curses libs
AC_ARG_WITH([named-curses-libs],
    [AS_HELP_STRING([--with-named-curses-libs=ARG],
            [Use specified curses libraries instead of those
		automatically found by configure.])],
    [ with_named_curses=$withval ],
    [ with_named_curses=no ]
    )

# compile with strings functions in assembler
AC_ARG_ENABLE([assembler],
    [AS_HELP_STRING([--enable-assembler],
	    [Use assembler versions of some string functions if available.])],
    [ ENABLE_ASSEMBLER=$enableval ],
    [ ENABLE_ASSEMBLER=no ]
    )

AC_MSG_CHECKING(if we should use assembler functions)
# For now we only support assembler on i386 and sparc systems
AM_CONDITIONAL(ASSEMBLER_x86, test "$ENABLE_ASSEMBLER" = "yes" -a "$BASE_MACHINE_TYPE" = "i386" && $AS strings/strings-x86.s -o checkassembler >/dev/null 2>&1 && test -f checkassembler && (rm -f checkassembler; exit 0;))
AM_CONDITIONAL(ASSEMBLER_sparc32, test "$ENABLE_ASSEMBLER" = "yes" -a "$BASE_MACHINE_TYPE" = "sparc")
AM_CONDITIONAL(ASSEMBLER_sparc64, test "$ENABLE_ASSEMBLER" = "yes" -a "$BASE_MACHINE_TYPE" = "sparcv9")
AM_CONDITIONAL(ASSEMBLER, test "$ASSEMBLER_x86_TRUE" = "" -o "$ASSEMBLER_sparc32_TRUE" = "")

if test "$ASSEMBLER_TRUE" = ""
then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

# Add query profiler
AC_MSG_CHECKING(if SHOW PROFILE should be enabled.)
AC_ARG_ENABLE([profiling],
    [AS_HELP_STRING([--enable-profiling], 
	    [Build a version with query profiling code])],
    [ ENABLED_PROFILING=$enableval ],
    [ ENABLED_PROFILING=no ])

if test "$ENABLED_PROFILING" = "yes"
then
  AC_DEFINE([ENABLED_PROFILING], [1],
            [If SHOW PROFILE should be enabled])
  AC_MSG_RESULT([yes]) 
else
  AC_MSG_RESULT([no])
fi

AC_ARG_WITH([tcp-port],
    [AS_HELP_STRING([--with-tcp-port=port-number],
            [Which port to use for MySQL services @<:@default=3306@:>@])],
    [ MYSQL_TCP_PORT=$withval ],
    [ MYSQL_TCP_PORT=$MYSQL_TCP_PORT_DEFAULT
      # if we actually defaulted (as opposed to the pathological case of
      # --with-tcp-port=<MYSQL_TCP_PORT_DEFAULT> which might in theory
      # happen if whole batch of servers was built from a script), set
      # the default to zero to indicate that; we don't lose information
      # that way, because 0 obviously indicates that we can get the
      # default value from MYSQL_TCP_PORT. this seems really evil, but
      # testing for MYSQL_TCP_PORT==MYSQL_TCP_PORT_DEFAULT would make a
      # a port of MYSQL_TCP_PORT_DEFAULT magic even if the builder did not
      # intend it to mean "use the default, in fact, look up a good default
      # from /etc/services if you can", but really, really meant 3306 when
      # they passed in 3306. When they pass in a specific value, let them
      # have it; don't second guess user and think we know better, this will
      # just make people cross.  this makes the the logic work like this
      # (which is complicated enough):
      #
      # - if a port was set during build, use that as a default.
      #
      # - otherwise, try to look up a port in /etc/services; if that fails,
      #   use MYSQL_TCP_PORT_DEFAULT (at the time of this writing 3306)
      #
      # - allow the MYSQL_TCP_PORT environment variable to override that.
      #
      # - allow command-line parameters to override all of the above.
      #
      # the top-most MYSQL_TCP_PORT_DEFAULT is read from win/configure.js,
      # so don't mess with that.
      MYSQL_TCP_PORT_DEFAULT=0 ]
    )
AC_SUBST(MYSQL_TCP_PORT)
# We might want to document the assigned port in the manual.
AC_SUBST(MYSQL_TCP_PORT_DEFAULT)

# Use this to set the place used for unix socket used to local communication.
AC_ARG_WITH([mysqld-user],
    [AS_HELP_STRING([--with-mysqld-user=username],
            [What user the mysqld daemon shall be run as.
		@<:@default=mysql@:>@])],
    [ MYSQLD_USER=$withval ],
    [ MYSQLD_USER=mysql ]
    )
AC_SUBST(MYSQLD_USER)

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

MYSQL_SYS_LARGEFILE

# Types that must be checked AFTER large file support is checked
AC_TYPE_SIZE_T

#--------------------------------------------------------------------
# Check for system header files
#--------------------------------------------------------------------

AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_STDBOOL
AC_CHECK_HEADERS(fcntl.h float.h floatingpoint.h fpu_control.h ieeefp.h \
 limits.h memory.h pwd.h select.h \
 stdlib.h stddef.h stdint.h sys/fpu.h \
 strings.h string.h synch.h sys/mman.h sys/socket.h netinet/in.h arpa/inet.h \
 sys/timeb.h sys/types.h sys/un.h sys/vadvise.h sys/wait.h term.h \
 unistd.h utime.h sys/utime.h termio.h termios.h sched.h crypt.h alloca.h \
 sys/ioctl.h malloc.h sys/malloc.h sys/ipc.h sys/shm.h linux/config.h \
 sys/prctl.h sys/resource.h sys/param.h port.h ieeefp.h \
 execinfo.h)

AC_CHECK_HEADERS([xfs/xfs.h])
#--------------------------------------------------------------------
# Check for libevent
#--------------------------------------------------------------------

AC_CHECK_LIB(event, event_loop, [], [AC_MSG_ERROR(could not find libevent)])

#--------------------------------------------------------------------
# Check for libpthread
#--------------------------------------------------------------------

AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR(could not find libpthread)])

#--------------------------------------------------------------------
# Check for system libraries. Adds the library to $LIBS
# and defines HAVE_LIBM etc
#--------------------------------------------------------------------

AC_CHECK_LIB(m, floor, [], AC_CHECK_LIB(m, __infinity))
AC_CHECK_FUNCS(log2)

AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))
AC_CHECK_FUNC(yp_get_default_domain, ,
  AC_CHECK_LIB(nsl, yp_get_default_domain))
AC_CHECK_FUNC(p2open, , AC_CHECK_LIB(gen, p2open))
# This may get things to compile even if bind-8 is installed
AC_CHECK_FUNC(bind, , AC_CHECK_LIB(bind, bind))
# Check if crypt() exists in libc or libcrypt, sets LIBS if needed
AC_SEARCH_LIBS(crypt, crypt, AC_DEFINE(HAVE_CRYPT, 1, [crypt]))

# Check rt for aio_read
AC_CHECK_LIB(rt, aio_read)

# For the sched_yield() function on Solaris
AC_CHECK_FUNC(sched_yield, , AC_CHECK_LIB(posix4, sched_yield,
[AC_DEFINE(HAVE_SCHED_YIELD) LIBS="$LIBS -lposix4"]))

if test "$ac_cv_header_termio_h" = "no" -a "$ac_cv_header_termios_h" = "no"
then
  AC_CHECK_FUNC(gtty, , AC_CHECK_LIB(compat, gtty))
fi

AC_CHECK_TYPES([int8, uint8, int16, uint16, int32, uint32, int64, uint64,
                uchar, uint, ulong],[],[], [
#include <sys/types.h>
])
AC_CHECK_TYPES([fp_except], [], [], [
#include <sys/types.h>
#include <ieeefp.h>
])


my_save_LIBS="$LIBS"
LIBS=""
AC_CHECK_LIB(dl,dlopen)
LIBDL=$LIBS
LIBS="$my_save_LIBS"
AC_SUBST(LIBDL)

my_save_LIBS="$LIBS"
LIBS="$LIBS $LIBDL"
AC_CHECK_FUNCS(dlopen dlerror)
LIBS="$my_save_LIBS"

AC_CHECK_FUNCS(strtok_r)


# System characteristics
case $SYSTEM_TYPE in
  *)
AC_SYS_RESTARTABLE_SYSCALLS
    ;;
esac

# Build optimized or debug version ?
# First check for gcc and g++
if test "$ac_cv_c_compiler_gnu" = "yes"
then
  DEBUG_CFLAGS="-g"
  DEBUG_OPTIMIZE_CC="-O"
  OPTIMIZE_CFLAGS="$MAX_C_OPTIMIZE"
else
  DEBUG_CFLAGS="-g"
  DEBUG_OPTIMIZE_CC=""
  OPTIMIZE_CFLAGS="-O"
fi
if test "$ac_cv_prog_cxx_g" = "yes"
then
  DEBUG_CXXFLAGS="-g"
  DEBUG_OPTIMIZE_CXX="-O"
  OPTIMIZE_CXXFLAGS="$MAX_CXX_OPTIMIZE"
else
  DEBUG_CXXFLAGS="-g"
  DEBUG_OPTIMIZE_CXX=""
  OPTIMIZE_CXXFLAGS="-O"
fi

# If the user specified CFLAGS, we won't add any optimizations
if test -n "$SAVE_CFLAGS"
then
  OPTIMIZE_CFLAGS=""
  DEBUG_OPTIMIZE_CC=""
fi
# Ditto for CXXFLAGS
if test -n "$SAVE_CXXFLAGS"
then
  OPTIMIZE_CXXFLAGS=""
  DEBUG_OPTIMIZE_CXX=""
fi

AC_ARG_WITH([debug],
    [AS_HELP_STRING([--with-debug],
            [Add debug code (yes|no|full) @<:@default=no@:>@ 
	    Full adds memory checked, very slow.])],
    [with_debug=$withval],
    [with_debug=no])
if test "$with_debug" = "yes"
then
  # Medium debug.
  AC_DEFINE([DBUG_ON], [1], [Use libdbug])
  CFLAGS="$DEBUG_CFLAGS $DEBUG_OPTIMIZE_CC $CFLAGS"
  CXXFLAGS="$DEBUG_CXXFLAGS $DEBUG_OPTIMIZE_CXX $CXXFLAGS"
elif test "$with_debug" = "full"
then
  # Full debug. Very slow in some cases
  AC_DEFINE([DBUG_ON], [1], [Use libdbug])
  CFLAGS="$DEBUG_CFLAGS $CFLAGS"
  CXXFLAGS="$DEBUG_CXXFLAGS $CXXFLAGS"
else
  # Optimized version. No debug
  AC_DEFINE([DBUG_OFF], [1], [Dont use libdbug])
  CFLAGS="$OPTIMIZE_CFLAGS $CFLAGS"
  CXXFLAGS="$OPTIMIZE_CXXFLAGS $CXXFLAGS"
fi

# If we should allow error injection tests
AC_ARG_WITH([error-inject],
    [AS_HELP_STRING([--with-error-inject],
	    [Enable error injection in MySQL Server @<:@default=off@:>@])],
    [ with_error_inject=$withval ],
    [ with_error_inject=no ])

if test $with_debug != "no"
then
  if test "$with_error_inject" = "yes"
  then
    AC_DEFINE([ERROR_INJECT_SUPPORT], [1],
              [Enable error injection in MySQL Server])
  fi
fi

AC_ARG_WITH([fast-mutexes],
    [AS_HELP_STRING([--with-fast-mutexes],
	    [Compile with fast mutexes  @<:@default=off@:>@])],
    [with_fast_mutexes=$withval],
    [with_fast_mutexes=no])

if test "$with_fast_mutexes" != "no"
then
  if test "$with_debug" != "no"
  then
    AC_MSG_WARN(['--with-fast-mutexes' ignored when '--with-debug' is given])
  else
    AC_DEFINE([MY_PTHREAD_FASTMUTEX], [1], 
	      [Define to 1 if you want to use fast mutexes])
  fi
fi

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
AC_SUBST(COMPILATION_COMMENT)

AC_MSG_CHECKING("need of special linking flags")
if test "$TARGET_LINUX" = "true" -a "$ac_cv_c_compiler_gnu" = "yes" -a "$all_is_static" != "yes"
then
  LDFLAGS="$LDFLAGS -rdynamic"
  AC_MSG_RESULT("-rdynamic")
else
  case "$SYSTEM_TYPE$with_mysqld_ldflags " in
  *freebsd*"-all-static "*|*dragonfly*"-all-static "*)
    AC_MSG_RESULT("none")
    ;;
  *freebsd*|*dragonfly*)
    MYSQLD_EXTRA_LDFLAGS="$MYSQLD_EXTRA_LDFLAGS -export-dynamic"
    AC_MSG_RESULT("-export-dynamic")
    ;;
  *)
    AC_MSG_RESULT("none")
    ;;
  esac
fi

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_HEADER_TIME
AC_STRUCT_TM
MYSQL_NEEDS_MYSYS_NEW
# AC_CHECK_SIZEOF return 0 when it does not find the size of a
# type. We want a error instead.
AC_CHECK_SIZEOF(char, 1)
if test "$ac_cv_sizeof_char" -eq 0
then
  AC_MSG_ERROR([No size for char type.
A likely cause for this could be that there isn't any
static libraries installed. You can verify this by checking if you have libm.a
in /lib, /usr/lib or some other standard place.  If this is the problem,
install the static libraries and try again.  If this isn't the problem,
examine config.log for possible errors.  If you want to report this, use
'scripts/mysqlbug' and include at least the last 20 rows from config.log!])
fi
AC_CHECK_SIZEOF(char*, 4)
AC_CHECK_SIZEOF(short, 2)
AC_CHECK_SIZEOF(int, 4)
if test "$ac_cv_sizeof_int" -eq 0
then
  AC_MSG_ERROR("No size for int type.")
fi
AC_CHECK_SIZEOF(long, 4)
if test "$ac_cv_sizeof_long" -eq 0
then
  AC_MSG_ERROR("No size for long type.")
fi
AC_CHECK_SIZEOF(long long, 8)
if test "$ac_cv_sizeof_long_long" -eq 0
then
  AC_MSG_ERROR("MySQL needs a long long type.")
fi
# off_t is not a builtin type
AC_CHECK_SIZEOF(off_t, 4)
if test "$ac_cv_sizeof_off_t" -eq 0
then
  AC_MSG_ERROR("MySQL needs a off_t type.")
fi

dnl
dnl check if time_t is unsigned
dnl

MYSQL_CHECK_TIME_T


# do we need #pragma interface/#pragma implementation ?
# yes if it's gcc 2.x, and not icc pretending to be gcc, and not cygwin
AC_MSG_CHECKING(the need for @%:@pragma interface/implementation)
# instead of trying to match SYSTEM_TYPE and CC_VERSION (that doesn't
# follow any standard), we'll use well-defined preprocessor macros:
AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
#if !defined(__CYGWIN__) && !defined(__INTEL_COMPILER) && defined(__GNUC__) && (__GNUC__ < 3)
#error USE_PRAGMA_IMPLEMENTATION
#endif
]])],[AC_MSG_RESULT(no) ],[AC_MSG_RESULT(yes) ; CXXFLAGS="$CXXFLAGS -DUSE_PRAGMA_IMPLEMENTATION"])

# This always gives a warning. Ignore it unless you are cross compiling
AC_C_BIGENDIAN
#---START: Used in for client configure
# Check base type of last arg to accept
MYSQL_TYPE_ACCEPT
#---END:
# Figure out what type of struct rlimit to use with setrlimit
MYSQL_TYPE_STRUCT_RLIMIT
# Find where the stack goes
MYSQL_STACK_DIRECTION
# We want to skip alloca on irix unconditionally. It may work on some version..
MYSQL_FUNC_ALLOCA
# Do struct timespec have members tv_sec or ts_sec
MYSQL_TIMESPEC_TS
# Do we have the tzname variable
MYSQL_TZNAME
# Do the c++ compiler have a bool type
MYSQL_CXX_BOOL
AC_CHECK_TYPES([sigset_t, off_t], [], [], [#include <sys/types.h>])
AC_CHECK_TYPES([size_t], [], [], [#include <stdio.h>])
AC_CHECK_TYPES([u_int32_t])

MYSQL_PTHREAD_YIELD

######################################################################
# For readline/libedit (We simply move the mimimum amount of stuff from
# the readline/libedit configure.in here)

dnl Checks for header files.
AC_CHECK_HEADERS(malloc.h sys/cdefs.h)

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_PROG_GCC_TRADITIONAL
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(re_comp regcomp strdup)

dnl Sun compilers have their own vis.h that is about something
dnl totally different. So, not to change the libedit source, we
dnl do some additional checks before we define HAVE_VIS_H.
AC_CHECK_HEADER(vis.h,
  [AC_CHECK_FUNC(strvis,
    [AC_DEFINE([HAVE_VIS_H], [1],[Found vis.h and the strvis() function])])])

AC_CHECK_FUNCS(strlcat strlcpy)
AC_CHECK_FUNCS(issetugid)
AC_CHECK_FUNCS(fgetln)
AC_CHECK_FUNCS(getline flockfile)

# from old readline settting:

MAKE_SHELL=/bin/sh
AC_SUBST(MAKE_SHELL)

# Already-done: stdlib.h string.h unistd.h termios.h
AC_CHECK_HEADERS(varargs.h stdarg.h dirent.h locale.h ndir.h sys/dir.h \
 sys/file.h sys/ndir.h sys/ptem.h sys/pte.h sys/select.h sys/stream.h \
 sys/mman.h curses.h termcap.h termio.h termbits.h asm/termbits.h grp.h \
paths.h semaphore.h)

# Already-done: strcasecmp
AC_CHECK_FUNCS(lstat putenv select setenv setlocale strcoll tcgetattr)

AC_HEADER_STAT
MYSQL_SIGNAL_CHECK
MYSQL_CHECK_GETPW_FUNCS
MYSQL_HAVE_TIOCGWINSZ
MYSQL_HAVE_FIONREAD
MYSQL_HAVE_TIOCSTAT
MYSQL_STRUCT_DIRENT_D_INO
MYSQL_STRUCT_DIRENT_D_NAMLEN
MYSQL_TYPE_SIGHANDLER
MYSQL_CHECK_MULTIBYTE
if test "$with_named_curses" = "no"
then
  MYSQL_CHECK_LIB_TERMCAP
else
  TERMCAP_LIB="$with_named_curses"
fi
AC_SUBST(TERMCAP_LIB)

# Check if the termcap function 'tgoto' is already declared in
# system header files or if it need to be declared locally
AC_CHECK_DECLS(tgoto,,,[
#ifdef HAVE_CURSES_H
# include <curses.h>
#elif HAVE_NCURSES_H
# include <ncurses.h>
#endif
#ifdef HAVE_TERM_H
# include <term.h>
#endif
])

LIBEDIT_LOBJECTS=""
AC_CHECK_FUNC(strunvis, ,[LIBEDIT_LOBJECTS="$LIBEDIT_LOBJECTS unvis.o"])
AC_CHECK_FUNC(strvis,   ,[LIBEDIT_LOBJECTS="$LIBEDIT_LOBJECTS vis.o"])
AC_CHECK_FUNC(strlcpy,  ,[LIBEDIT_LOBJECTS="$LIBEDIT_LOBJECTS strlcpy.o"])
AC_CHECK_FUNC(strlcat,  ,[LIBEDIT_LOBJECTS="$LIBEDIT_LOBJECTS strlcat.o"])
AC_CHECK_FUNC(fgetln,   ,[LIBEDIT_LOBJECTS="$LIBEDIT_LOBJECTS fgetln.o"])
AC_SUBST(LIBEDIT_LOBJECTS)
enable_readline="yes"

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
MYSQL_TYPE_QSORT
AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF

AC_CHECK_FUNCS(bcmp bfill bmove bsearch bzero \
  chsize cuserid fchmod fcntl \
  fconvert fdatasync finite fpresetsticky fpsetmask fsync ftruncate \
  getcwd getpass getpassphrase getpwnam \
  getpwuid getrlimit getrusage getwd index initgroups isnan \
  localtime_r gethrtime gmtime_r \
  locking longjmp lrand48 madvise mallinfo memcpy memmove \
  mkstemp mlockall perror poll pread pthread_attr_create mmap mmap64 getpagesize \
  pthread_attr_getstacksize pthread_attr_setprio pthread_attr_setschedparam \
  pthread_attr_setstacksize pthread_condattr_create pthread_getsequence_np \
  pthread_key_delete pthread_rwlock_rdlock pthread_setprio \
  pthread_setprio_np pthread_setschedparam pthread_sigmask readlink \
  realpath rename rint rwlock_init setupterm \
  shmget shmat shmdt shmctl sigaction sigemptyset sigaddset \
  sighold sigset sigthreadmask port_create sleep \
  snprintf socket stpcpy strcasecmp strerror strsignal strnlen strpbrk strstr \
  strtol strtoll strtoul strtoull tell tempnam vidattr \
  posix_fallocate backtrace backtrace_symbols backtrace_symbols_fd)

# Check that isinf() is available in math.h and can be used in both C and C++
# code
AC_MSG_CHECKING(for isinf in math.h)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <math.h>]], [[
    float f = 0.0; 
    int r = isinf(f); 
    return r;
  ]])],[
    AC_MSG_RESULT(yes)
    AC_MSG_CHECKING(whether isinf() can be used in C++ code)
    AC_LANG_PUSH([C++])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <math.h>]], [[
      float f = 0.0;
      int r = isinf(f);
      return r;
    ]])],[
      AC_MSG_RESULT(yes)
      AC_DEFINE(HAVE_ISINF, [1], [isinf() macro or function])
    ],[
      AC_MSG_RESULT(no)
    ])
    AC_LANG_POP([])
  ],[
  AC_MSG_RESULT(no)])


CFLAGS="$ORG_CFLAGS"

# Sanity check: We chould not have any fseeko symbol unless
# large_file_support=yes
AC_CHECK_FUNC(fseeko,
[if test "$large_file_support" = no -a "$TARGET_LINUX" = "true";
then
  AC_MSG_ERROR("Found fseeko symbol but large_file_support is not enabled!")
fi]
)

# Check definition of pthread_getspecific
AC_CACHE_CHECK("args to pthread_getspecific", mysql_cv_getspecific_args,
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#if !defined(_REENTRANT)
#define _REENTRANT
#endif
#define _POSIX_PTHREAD_SEMANTICS 
#include <pthread.h> ]], [[ void *pthread_getspecific(pthread_key_t key);
pthread_getspecific((pthread_key_t) NULL); ]])],[mysql_cv_getspecific_args=POSIX],[mysql_cv_getspecific_args=other]))
  if test "$mysql_cv_getspecific_args" = "other"
  then
    AC_DEFINE([HAVE_NONPOSIX_PTHREAD_GETSPECIFIC], [1],
              [For some non posix threads])
  fi

  # Check definition of pthread_mutex_init
  AC_CACHE_CHECK("args to pthread_mutex_init", mysql_cv_mutex_init_args,
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#define _REENTRANT
#define _POSIX_PTHREAD_SEMANTICS 
#include <pthread.h> ]], [[ 
  pthread_mutexattr_t attr;
  pthread_mutex_t mp;
  pthread_mutex_init(&mp,&attr); ]])],[mysql_cv_mutex_init_args=POSIX],[mysql_cv_mutex_init_args=other]))
  if test "$mysql_cv_mutex_init_args" = "other"
  then
    AC_DEFINE([HAVE_NONPOSIX_PTHREAD_MUTEX_INIT], [1],
              [For some non posix threads])
  fi
#---END:

#---START: Used in for client configure
# Check definition of readdir_r
AC_CACHE_CHECK("args to readdir_r", mysql_cv_readdir_r,
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#define _REENTRANT
#define _POSIX_PTHREAD_SEMANTICS 
#include <pthread.h>
#include <dirent.h>]], [[ int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
readdir_r((DIR *) NULL, (struct dirent *) NULL, (struct dirent **) NULL); ]])],[mysql_cv_readdir_r=POSIX],[mysql_cv_readdir_r=other]))
if test "$mysql_cv_readdir_r" = "POSIX"
then
  AC_DEFINE([HAVE_READDIR_R], [1], [POSIX readdir_r])
fi

# Check definition of posix sigwait()
AC_CACHE_CHECK("style of sigwait", mysql_cv_sigwait,
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#define _REENTRANT
#define _POSIX_PTHREAD_SEMANTICS 
#include <pthread.h>
#include <signal.h>]], [[#ifndef _AIX
sigset_t set;
int sig;
sigwait(&set,&sig);
#endif]])],[mysql_cv_sigwait=POSIX],[mysql_cv_sigwait=other]))
if test "$mysql_cv_sigwait" = "POSIX"
then
  AC_DEFINE([HAVE_SIGWAIT], [1], [POSIX sigwait])
fi

if test "$mysql_cv_sigwait" != "POSIX"
then
unset mysql_cv_sigwait
# Check definition of posix sigwait()
AC_CACHE_CHECK("style of sigwait", mysql_cv_sigwait,
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#define _REENTRANT
#define _POSIX_PTHREAD_SEMANTICS 
#include <pthread.h>
#include <signal.h>]], [[sigset_t set;
int sig;
sigwait(&set);]])],[mysql_cv_sigwait=NONPOSIX],[mysql_cv_sigwait=other]))
if test "$mysql_cv_sigwait" = "NONPOSIX"
then
  AC_DEFINE([HAVE_NONPOSIX_SIGWAIT], [1], [sigwait with one argument])
fi
fi
#---END:

# Check if pthread_attr_setscope() exists
AC_CACHE_CHECK("for pthread_attr_setscope", mysql_cv_pthread_attr_setscope,
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#define _REENTRANT
#define _POSIX_PTHREAD_SEMANTICS 
#include <pthread.h>]], [[pthread_attr_t thr_attr;
pthread_attr_setscope(&thr_attr,0);]])],[mysql_cv_pthread_attr_setscope=yes],[mysql_cv_pthread_attr_setscope=no]))
if test "$mysql_cv_pthread_attr_setscope" = "yes"
then
  AC_DEFINE([HAVE_PTHREAD_ATTR_SETSCOPE], [1], [pthread_attr_setscope])
fi

# Check for bad includes
AC_MSG_CHECKING("can netinet files be included")
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>]], [[ printf("1\n"); ]])],[netinet_inc=yes],[netinet_inc=no])
if test "$netinet_inc" = "no"
then
  AC_DEFINE([HAVE_BROKEN_NETINET_INCLUDES], [1], [Can netinet be included])
fi
AC_MSG_RESULT("$netinet_inc")

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

MYSQL_CHECK_BIG_TABLES
MYSQL_CHECK_MAX_INDEXES
MYSQL_CHECK_VIO

#--------------------------------------------------------------------
# Declare our plugin modules
# Has to be done late, as the plugin may need to check for existence of
# functions tested above
#--------------------------------------------------------------------

MYSQL_CONFIGURE_PLUGINS([none])

AC_SUBST(CLIENT_LIBS)
AC_SUBST(STATIC_NSS_FLAGS)

# IMPORTANT - do not modify LIBS past this line - this hack is the only way
# I know to add the static NSS magic if we have static NSS libraries with
# glibc - Sasha

LDFLAGS="$LDFLAGS $OTHER_LIBC_LIB"
LIBS="$LIBS $STATIC_NSS_FLAGS"

AC_SUBST(mysql_plugin_dirs)
AC_SUBST(mysql_plugin_libs)
AC_SUBST(mysql_plugin_defs)

AC_ARG_ENABLE([pedantic-warnings],
    [AS_HELP_STRING([--disable-pedantic-warnings],
       [Toggle pedanticness @<:@default=on@:>@])],
    [ac_warn_pedantic="$enableval"],
    [ac_warn_pedantic="yes"])

AC_ARG_ENABLE([fail],
    [AS_HELP_STRING([--enable-fail],
       [Turn warnings into failures @<:@default=off@:>@])],
    [ac_warn_fail="$enableval"],
    [ac_warn_fail="no"])

AC_ARG_ENABLE([unreachable],
    [AS_HELP_STRING([--enable-unreachable],
       [Enable warnings about unreachable code @<:@default=off@:>@])],
    [ac_warn_unreachable="$enableval"],
    [ac_warn_unreachable="no"])


if test "$GCC" = "yes"
then

  GCC_WARNINGS="-W -Wall"
  GXX_WARNINGS="${GCC_WARNINGS}"


  if test "$ac_warn_pedantic" = "yes"
  then
    GCC_WARNINGS="${GCC_WARNINGS}  -std=gnu99 -pedantic -Wundef -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls  "
    GXX_WARNINGS="${GXX_WARNINGS}  -std=gnu++98 -pedantic -Wundef -Wredundant-decls "
  fi

  if test "$ac_warn_unreachable" = "yes"
  then
    GCC_WARNINGS="${GCC_WARNINGS} -Wunreachable-code"
    GXX_WARNINGS="${GXX_WARNINGS} -Wunreachable-code"
  fi

  if test "$ac_warn_fail" = "yes"
  then
    GCC_WARNINGS="${GCC_WARNINGS} -Werror"
    GXX_WARNINGS="${GXX_WARNINGS} -Werror"
  fi

    CXXFLAGS="$CXXFLAGS ${GXX_WARNINGS}"
    CFLAGS="$CFLAGS ${GCC_WARNINGS} "
fi

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

AC_CONFIG_FILES(Makefile extra/Makefile mysys/Makefile mysys/tests/Makefile dnl
 strings/Makefile strings/tests/Makefile regex/Makefile storage/Makefile dnl
 vio/Makefile dnl
 libmysql/Makefile client/Makefile dnl
 sql/Makefile sql/share/Makefile dnl
 sql/sql_builtin.cc sql-common/Makefile dnl
 dbug/Makefile scripts/Makefile include/Makefile dnl
 support-files/Makefile dnl
 mysql-test/Makefile dnl
 include/mysql_version.h plugin/Makefile)

AC_CONFIG_COMMANDS([default], , test -z "$CONFIG_HEADERS" || echo timestamp > stamp-h)

# Ensure that table handlers gets all modifications to CFLAGS/CXXFLAGS
AC_CONFIG_COMMANDS_POST(ac_configure_args="$ac_configure_args CFLAGS='$CFLAGS' CXXFLAGS='$CXXFLAGS'")

AC_OUTPUT
