dnl Copyright (C) 2010 Monty Taylor
dnl This file is free software; Monty Taylor
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

#--------------------------------------------------------------------
# Check for clock_gettime
#--------------------------------------------------------------------

AC_DEFUN([PANDORA_CLOCK_GETTIME],[
  AC_CACHE_CHECK([for working clock_gettime],[ac_cv_have_clock_gettime],[
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
      ]],[[
  struct timespec tp;
  int ret= clock_gettime(CLOCK_REALTIME, &tp);
    ]])],[
      ac_cv_have_clock_gettime=yes
    ],[
      ac_cv_have_clock_gettime=no
    ])
  ])
  AS_IF([test "x${ac_cv_have_clock_gettime}" = xyes],[
    AC_DEFINE([HAVE_CLOCK_GETTIME],[1],[Have a working clock_gettime function])
  ])
])
