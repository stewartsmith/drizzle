# Local macros for automake & autoconf

#---START: Used in for client configure
AC_DEFUN([DRIZZLE_TYPE_ACCEPT],
[ac_save_CXXFLAGS="$CXXFLAGS"
AC_CACHE_CHECK([base type of last arg to accept], mysql_cv_btype_last_arg_accept,
AC_LANG_PUSH(C++)
if test "$ac_cv_prog_gxx" = "yes"
then
  # Add -Werror, remove -fbranch-probabilities (Bug #268)
  CXXFLAGS=`echo "$CXXFLAGS -Werror" | sed -e 's/-fbranch-probabilities//; s/-Wall//; s/-Wcheck//'`
fi
mysql_cv_btype_last_arg_accept=none
[AC_TRY_COMPILE([#if defined(inline)
#undef inline
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
],
[int a = accept(1, (struct sockaddr *) 0, (socklen_t *) 0); return (a != 0);],
mysql_cv_btype_last_arg_accept=socklen_t)]
if test "$mysql_cv_btype_last_arg_accept" = "none"; then
[AC_TRY_COMPILE([#if defined(inline)
#undef inline
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
],
[int a = accept(1, (struct sockaddr *) 0, (size_t *) 0); return (a != 0);],
mysql_cv_btype_last_arg_accept=size_t)]
fi
if test "$mysql_cv_btype_last_arg_accept" = "none"; then
mysql_cv_btype_last_arg_accept=int
fi)
AC_LANG_POP(C++)
AC_DEFINE_UNQUOTED([SOCKET_SIZE_TYPE], [$mysql_cv_btype_last_arg_accept],
                   [The base type of the last arg to accept])
CXXFLAGS="$ac_save_CXXFLAGS"
])
#---END:

dnl Find type of qsort
AC_DEFUN([DRIZZLE_TYPE_QSORT],
[AC_CACHE_CHECK([return type of qsort], mysql_cv_type_qsort,
[AC_TRY_COMPILE([#include <stdlib.h>
#ifdef __cplusplus
extern "C"
#endif
void qsort(void *base, size_t nel, size_t width,
 int (*compar) (const void *, const void *));
],
[int i;], mysql_cv_type_qsort=void, mysql_cv_type_qsort=int)])
AC_DEFINE_UNQUOTED([RETQSORTTYPE], [$mysql_cv_type_qsort],
                   [The return type of qsort (int or void).])
if test "$mysql_cv_type_qsort" = "void"
then
 AC_DEFINE_UNQUOTED([QSORT_TYPE_IS_VOID], [1], [qsort returns void])
fi
])

#---START: Figure out whether to use 'struct rlimit' or 'struct rlimit64'
AC_DEFUN([DRIZZLE_TYPE_STRUCT_RLIMIT],
[ac_save_CXXFLAGS="$CXXFLAGS"
AC_CACHE_CHECK([struct type to use with setrlimit], mysql_cv_btype_struct_rlimit,
AC_LANG_PUSH(C++)
if test "$ac_cv_prog_gxx" = "yes"
then
  # Add -Werror, remove -fbranch-probabilities (Bug #268)
  CXXFLAGS=`echo "$CXXFLAGS -Werror" | sed -e 's/-fbranch-probabilities//; s/-Wall//; s/-Wcheck//'`
fi
mysql_cv_btype_struct_rlimit=none
[AC_TRY_COMPILE([#if defined(inline)
#undef inline
#endif
#include <stdlib.h>
#include <sys/resource.h>
],
[struct rlimit64 rl; setrlimit(RLIMIT_CORE, &rl);],
mysql_cv_btype_struct_rlimit="struct rlimit64")]
if test "$mysql_cv_btype_struct_rlimit" = "none"; then
mysql_cv_btype_struct_rlimit="struct rlimit"
fi)
AC_LANG_POP(C++)
AC_DEFINE_UNQUOTED([STRUCT_RLIMIT], [$mysql_cv_btype_struct_rlimit],
                   [The struct rlimit type to use with setrlimit])
CXXFLAGS="$ac_save_CXXFLAGS"
])
#---END:

AC_DEFUN([DRIZZLE_TIMESPEC_TS],
[AC_CACHE_CHECK([if struct timespec has a ts_sec member], mysql_cv_timespec_ts,
[AC_TRY_COMPILE([#include <pthread.h>
#ifdef __cplusplus
extern "C"
#endif
],
[struct timespec abstime;

abstime.ts_sec = time(NULL)+1;
abstime.ts_nsec = 0;
], mysql_cv_timespec_ts=yes, mysql_cv_timespec_ts=no)])
if test "$mysql_cv_timespec_ts" = "yes"
then
  AC_DEFINE([HAVE_TIMESPEC_TS_SEC], [1],
            [Timespec has a ts_sec instead of tv_sev])
fi
])

AC_DEFUN([DRIZZLE_TZNAME],
[AC_CACHE_CHECK([if we have tzname variable], mysql_cv_tzname,
[AC_TRY_COMPILE([#include <time.h>
#ifdef __cplusplus
extern "C"
#endif
],
[ tzset();
  return tzname[0] != 0;
], mysql_cv_tzname=yes, mysql_cv_tzname=no)])
if test "$mysql_cv_tzname" = "yes"
then
  AC_DEFINE([HAVE_TZNAME], [1], [Have the tzname variable])
fi
])


AC_DEFUN([DRIZZLE_PTHREAD_YIELD],[
# Some OSes like Mac OS X have that as a replacement for pthread_yield()
AC_CHECK_FUNCS(pthread_yield_np, AC_DEFINE([HAVE_PTHREAD_YIELD_NP],[],[Define if you have pthread_yield_np]))
AC_CACHE_CHECK([if pthread_yield takes zero arguments], ac_cv_pthread_yield_zero_arg,
[AC_TRY_LINK([#define _GNU_SOURCE
#include <pthread.h>
#ifdef __cplusplus
extern "C"
#endif
],
[
  pthread_yield();
], ac_cv_pthread_yield_zero_arg=yes, ac_cv_pthread_yield_zero_arg=yeso)])
if test "$ac_cv_pthread_yield_zero_arg" = "yes"
then
  AC_DEFINE([HAVE_PTHREAD_YIELD_ZERO_ARG], [1],
            [pthread_yield that doesn't take any arguments])
fi
AC_CACHE_CHECK([if pthread_yield takes 1 argument], ac_cv_pthread_yield_one_arg,
[AC_TRY_LINK([#define _GNU_SOURCE
#include <pthread.h>
#ifdef __cplusplus
extern "C"
#endif
],
[
  pthread_yield(0);
], ac_cv_pthread_yield_one_arg=yes, ac_cv_pthread_yield_one_arg=no)])
if test "$ac_cv_pthread_yield_one_arg" = "yes"
then
  AC_DEFINE([HAVE_PTHREAD_YIELD_ONE_ARG], [1],
            [pthread_yield function with one argument])
fi
]
)

#---END:

# From fileutils-3.14/aclocal.m4

# @defmac AC_PROG_CC_STDC
# @maindex PROG_CC_STDC
# @ovindex CC
# If the C compiler in not in ANSI C mode by default, try to add an option
# to output variable @code{CC} to make it so.  This macro tries various
# options that select ANSI C on some system or another.  It considers the
# compiler to be in ANSI C mode if it defines @code{__STDC__} to 1 and
# handles function prototypes correctly.
#
# Patched by monty to only check if __STDC__ is defined. With the original 
# check it's impossible to get things to work with the Sunpro compiler from
# Workshop 4.2
#
# If you use this macro, you should check after calling it whether the C
# compiler has been set to accept ANSI C; if not, the shell variable
# @code{am_cv_prog_cc_stdc} is set to @samp{no}.  If you wrote your source
# code in ANSI C, you can make an un-ANSIfied copy of it by using the
# program @code{ansi2knr}, which comes with Ghostscript.
# @end defmac

AC_DEFUN([AM_PROG_CC_STDC],
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(am_cv_prog_cc_stdc,
[am_cv_prog_cc_stdc=no
ac_save_CC="$CC"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc -D__EXTENSIONS__
# removed "-Xc -D__EXTENSIONS__" beacause sun c++ does not like it.
for ac_arg in "" -qlanglvl=ansi -std1 "-Aa -D_HPUX_SOURCE" 
do
  CC="$ac_save_CC $ac_arg"
  AC_TRY_COMPILE(
[#if !defined(__STDC__)
choke me
#endif
/* DYNIX/ptx V4.1.3 can't compile sys/stat.h with -Xc -D__EXTENSIONS__. */
#ifdef _SEQUENT_
# include <sys/types.h>
# include <sys/stat.h>
#endif
], [
int test (int i, double x);
struct s1 {int (*f) (int a);};
struct s2 {int (*f) (double a);};],
[am_cv_prog_cc_stdc="$ac_arg"; break])
done
CC="$ac_save_CC"
])
AC_MSG_RESULT($am_cv_prog_cc_stdc)
case "x$am_cv_prog_cc_stdc" in
  x|xno) ;;
  *) CC="$CC $am_cv_prog_cc_stdc" ;;
esac
])


dnl Check type of signal routines (posix, 4.2bsd, 4.1bsd or v7)
AC_DEFUN([DRIZZLE_SIGNAL_CHECK],
[AC_REQUIRE([AC_TYPE_SIGNAL])
AC_MSG_CHECKING(for type of signal functions)
AC_CACHE_VAL(mysql_cv_signal_vintage,
[
  AC_TRY_LINK([#include <signal.h>],[
    sigset_t ss;
    struct sigaction sa;
    sigemptyset(&ss); sigsuspend(&ss);
    sigaction(SIGINT, &sa, (struct sigaction *) 0);
    sigprocmask(SIG_BLOCK, &ss, (sigset_t *) 0);
  ], mysql_cv_signal_vintage=posix,
  [
    AC_TRY_LINK([#include <signal.h>], [
	int mask = sigmask(SIGINT);
	sigsetmask(mask); sigblock(mask); sigpause(mask);
    ], mysql_cv_signal_vintage=4.2bsd,
    [
      AC_TRY_LINK([
	#include <signal.h>
	RETSIGTYPE foo() { }], [
		int mask = sigmask(SIGINT);
		sigset(SIGINT, foo); sigrelse(SIGINT);
		sighold(SIGINT); sigpause(SIGINT);
        ], mysql_cv_signal_vintage=svr3, mysql_cv_signal_vintage=v7
    )]
  )]
)
])
AC_MSG_RESULT($mysql_cv_signal_vintage)
if test "$mysql_cv_signal_vintage" = posix; then
AC_DEFINE(HAVE_POSIX_SIGNALS, [1],
          [Signal handling is POSIX (sigset/sighold, etc)])
elif test "$mysql_cv_signal_vintage" = "4.2bsd"; then
AC_DEFINE([HAVE_BSD_SIGNALS], [1], [BSD style signals])
elif test "$mysql_cv_signal_vintage" = svr3; then
AC_DEFINE(HAVE_USG_SIGHOLD, [1], [sighold() is present and usable])
fi
])

AC_DEFUN([DRIZZLE_CHECK_GETPW_FUNCS],
[AC_MSG_CHECKING(whether programs are able to redeclare getpw functions)
AC_CACHE_VAL(mysql_cv_can_redecl_getpw,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <pwd.h>
extern struct passwd *getpwent();], [struct passwd *z; z = getpwent();],
  mysql_cv_can_redecl_getpw=yes,mysql_cv_can_redecl_getpw=no)])
AC_MSG_RESULT($mysql_cv_can_redecl_getpw)
if test "$mysql_cv_can_redecl_getpw" = "no"; then
AC_DEFINE(HAVE_GETPW_DECLS, [1], [getpwent() declaration present])
fi
])

AC_DEFUN([DRIZZLE_HAVE_TIOCGWINSZ],
[AC_MSG_CHECKING(for TIOCGWINSZ in sys/ioctl.h)
AC_CACHE_VAL(mysql_cv_tiocgwinsz_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = TIOCGWINSZ;],
  mysql_cv_tiocgwinsz_in_ioctl=yes,mysql_cv_tiocgwinsz_in_ioctl=no)])
AC_MSG_RESULT($mysql_cv_tiocgwinsz_in_ioctl)
if test "$mysql_cv_tiocgwinsz_in_ioctl" = "yes"; then   
AC_DEFINE([GWINSZ_IN_SYS_IOCTL], [1],
          [READLINE: your system defines TIOCGWINSZ in sys/ioctl.h.])
fi
])

AC_DEFUN([DRIZZLE_HAVE_TIOCSTAT],
[AC_MSG_CHECKING(for TIOCSTAT in sys/ioctl.h)
AC_CACHE_VAL(mysql_cv_tiocstat_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = TIOCSTAT;],
  mysql_cv_tiocstat_in_ioctl=yes,mysql_cv_tiocstat_in_ioctl=no)])
AC_MSG_RESULT($mysql_cv_tiocstat_in_ioctl)
if test "$mysql_cv_tiocstat_in_ioctl" = "yes"; then   
AC_DEFINE(TIOCSTAT_IN_SYS_IOCTL, [1],
          [declaration of TIOCSTAT in sys/ioctl.h])
fi
])

AC_DEFUN([DRIZZLE_TYPE_SIGHANDLER],
[AC_MSG_CHECKING([whether signal handlers are of type void])
AC_CACHE_VAL(mysql_cv_void_sighandler,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <signal.h>
#ifdef signal
#undef signal
#endif
#ifdef __cplusplus
extern "C"
#endif
void (*signal ()) ();],
[int i;], mysql_cv_void_sighandler=yes, mysql_cv_void_sighandler=no)])dnl
AC_MSG_RESULT($mysql_cv_void_sighandler)
if test "$mysql_cv_void_sighandler" = "yes"; then
AC_DEFINE(VOID_SIGHANDLER, [1], [sighandler type is void (*signal ()) ();])
fi
])

AC_DEFUN([DRIZZLE_STACK_DIRECTION],
 [AC_CACHE_CHECK(stack direction for C alloca, ac_cv_c_stack_direction,
 [AC_TRY_RUN([#include <stdlib.h>
 int find_stack_direction ()
 {
   static char *addr = 0;
   auto char dummy;
   if (addr == 0)
     {
       addr = &dummy;
       return find_stack_direction ();
     }
   else
     return (&dummy > addr) ? 1 : -1;
 }
 int main ()
 {
   exit (find_stack_direction() < 0);
 }], ac_cv_c_stack_direction=1, ac_cv_c_stack_direction=-1,
   ac_cv_c_stack_direction=)])
 AC_DEFINE_UNQUOTED(STACK_DIRECTION, $ac_cv_c_stack_direction)
])dnl

AC_DEFUN([DRIZZLE_CHECK_LONGLONG_TO_FLOAT],
[
AC_MSG_CHECKING(if conversion of int64_t to float works)
AC_CACHE_VAL(ac_cv_conv_longlong_to_float,
[AC_TRY_RUN([#include <stdio.h>
#include <stdint.h>
int main()
{
  int64_t ll=1;
  float f;
  FILE *file=fopen("conftestval", "w");
  f = (float) ll;
  fprintf(file,"%g\n",f);
  fclose(file);
  return (0);
}], ac_cv_conv_longlong_to_float=`cat conftestval`,
    ac_cv_conv_longlong_to_float=0,
    ac_cv_conv_longlong_to_float="yes")])dnl  # Cross compiling, assume can convert
if test "$ac_cv_conv_longlong_to_float" = "1" -o "$ac_cv_conv_longlong_to_float" = "yes"
then
  ac_cv_conv_longlong_to_float=yes
else
  ac_cv_conv_longlong_to_float=no
fi
AC_MSG_RESULT($ac_cv_conv_longlong_to_float)
])

AC_DEFUN([DRIZZLE_CHECK_VIO], [
dnl
dnl we always use vio: no need for special defines 
dnl
  AC_DEFINE([HAVE_VIO_READ_BUFF], [1],
            [Define to enable buffered read. This works only if syscalls
            read/recv return as soon as there is some data in the kernel
            buffer, no matter how big the given buffer is.])
])

# Local version of _AC_PROG_CXX_EXIT_DECLARATION that does not
# include #stdlib.h as default as this breaks things on Solaris
# (Conflicts with pthreads and big file handling)

m4_define([_AC_PROG_CXX_EXIT_DECLARATION],
[for ac_declaration in \
   ''\
   'extern "C" void std::exit (int) throw (); using std::exit;' \
   'extern "C" void std::exit (int); using std::exit;' \
   'extern "C" void exit (int) throw ();' \
   'extern "C" void exit (int);' \
   'void exit (int);' \
   '#include <stdlib.h>'
do
  _AC_COMPILE_IFELSE([AC_LANG_PROGRAM([$ac_declaration
@%:@include <stdlib.h>],
                                      [exit (42);])],
                     [],
                     [continue])
  _AC_COMPILE_IFELSE([AC_LANG_PROGRAM([$ac_declaration],
                                      [exit (42);])],
                     [break])
done
rm -f conftest*
if test -n "$ac_declaration"; then
  echo '#ifdef __cplusplus' >>confdefs.h
  echo $ac_declaration      >>confdefs.h
  echo '#endif'             >>confdefs.h
fi
])# _AC_PROG_CXX_EXIT_DECLARATION

dnl ---------------------------------------------------------------------------


dnl ---------------------------------------------------------------------------
dnl Macro: DRIZZLE_CHECK_MAX_INDEXES
dnl Sets MAX_INDEXES
dnl ---------------------------------------------------------------------------
AC_DEFUN([DRIZZLE_CHECK_MAX_INDEXES], [
  AC_ARG_WITH([max-indexes],
              AS_HELP_STRING([--with-max-indexes=N],
                             [Sets the maximum number of indexes per table, default 64]),
              [max_indexes="$withval"],
              [max_indexes=64])
  AC_MSG_CHECKING([max indexes per table])
  AC_DEFINE_UNQUOTED([MAX_INDEXES], [$max_indexes],
                     [Maximum number of indexes per table])
  AC_MSG_RESULT([$max_indexes])
])
dnl ---------------------------------------------------------------------------
dnl END OF DRIZZLE_CHECK_MAX_INDEXES SECTION
dnl ---------------------------------------------------------------------------

AC_DEFUN([DRIZZLE_CHECK_C_VERSION],[

  dnl Print version of C compiler
  AC_MSG_CHECKING("C Compiler version")
  if test "$GCC" = "yes"
  then
    CC_VERSION=`$CC --version | sed 1q`
  elif test "$SUNCC" = "yes"
  then
    CC_VERSION=`$CC -V 2>&1 | sed 1q`
  else
    CC_VERSION=""
  fi
  AC_MSG_RESULT("$CC_VERSION")
  AC_SUBST(CC_VERSION)
])


AC_DEFUN([DRIZZLE_CHECK_CXX_VERSION], [
  dnl Print version of CXX compiler
  AC_MSG_CHECKING("C++ Compiler version")
  if test "$GCC" = "yes"
  then
    CXX_VERSION=`$CXX --version | sed 1q`
  elif test "$SUNCC" = "yes"
  then
    CXX_VERSION=`$CXX -V 2>&1 | sed 1q`
  else
    CXX_VERSION=""
  fi
  AC_MSG_RESULT("$CXX_VERSION")
  AC_SUBST(CXX_VERSION)
])

AC_DEFUN([DRIZZLE_PROG_AR], [
case $CXX_VERSION in
  MIPSpro*)
    AR=$CXX
    ARFLAGS="-ar -o"
  ;;
  *Forte*)
    AR=$CXX
    ARFLAGS="-xar -o"
  ;;
  *)
    AC_CHECK_PROG([AR], [ar], [ar])
    if test -z "$AR" || test "$AR" = "false"
    then
      AC_MSG_ERROR([You need ar to build the library])
    fi
    if test -z "$ARFLAGS"
    then
      ARFLAGS="cru"
    fi
esac
AC_SUBST(AR)
AC_SUBST(ARFLAGS)
])

dnl
dnl  Macro to check time_t range: according to C standard
dnl  array index must be greater than 0 => if time_t is signed,
dnl  the code in the macros below won't compile.
dnl

AC_DEFUN([DRIZZLE_CHECK_TIME_T],[
    AC_MSG_CHECKING(if time_t is unsigned)
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
        [[
#include <time.h>
        ]],
        [[
        int array[(((time_t)-1) > 0) ? 1 : -1];
        ]] )
    ], [
    AC_DEFINE([TIME_T_UNSIGNED], 1, [Define to 1 if time_t is unsigned])
    AC_MSG_RESULT(yes)
    ],
    [AC_MSG_RESULT(no)]
    )
])

