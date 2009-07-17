# Local macros for automake & autoconf


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
