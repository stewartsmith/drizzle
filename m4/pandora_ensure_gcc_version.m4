dnl  Copyright (C) 2009 Sun Microsystems
dnl This file is free software; Sun Microsystems
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([_PANDORA_TRY_GCC],[
  AS_IF([test -f /usr/bin/gcc$1 -a -f /usr/bin/g++$1],
  [
    CPP="/usr/bin/gcc$1 -E"
    CC=/usr/bin/gcc$1
    CXX=/usr/bin/g++$1
  ])
])

dnl If the user is on a Mac and didn't ask for a specific compiler
dnl You're gonna get 4.2.
AC_DEFUN([PANDORA_MAC_GCC42],
  [AS_IF([test "$GCC" = "yes"],[
    AS_IF([test "$host_vendor" = "apple" -a "x${ac_cv_env_CC_set}" = "x"],[
      host_os_version=`echo ${host_os} | perl -ple 's/^\D+//g;s,\..*,,'`
      AS_IF([test "$host_os_version" -lt 10],[
        _PANDORA_TRY_GCC([-4.2])
      ])
    ])
  ])
])

dnl If the user is on CentOS or RHEL and didn't ask for a specific compiler
dnl You're gonna get 4.4 (forward compatible with 4.5)
AC_DEFUN([PANDORA_RH_GCC44],
  [AS_IF([test "$GCC" = "yes"],[
    AS_IF([test "x${ac_cv_env_CC_set}" = "x"],[
      _PANDORA_TRY_GCC([44])
      _PANDORA_TRY_GCC([45])
    ])
  ])
])

dnl 
AC_DEFUN([PANDORA_ENSURE_GCC_VERSION],[
  AC_REQUIRE([PANDORA_MAC_GCC42])
  AC_REQUIRE([PANDORA_RH_GCC44])
  AC_CACHE_CHECK([if GCC is recent enough], [ac_cv_gcc_recent],
    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if !defined(__GNUC__) || (__GNUC__ < 4) || ((__GNUC__ >= 4) && (__GNUC_MINOR__ < 2))
# error GCC is Too Old!
#endif
      ]])],
      [ac_cv_gcc_recent=yes],
      [ac_cv_gcc_recent=no])])
  AS_IF([test "$ac_cv_gcc_recent" = "no" -a "$host_vendor" = "apple"],
    AC_MSG_ERROR([Your version of GCC is too old. At least version 4.2 is required on OSX. You may need to install a version of XCode >= 3.1.2]))
  AS_IF([test "$ac_cv_gcc_recent" = "no"],
    AC_MSG_ERROR([Your version of GCC is too old. At least version 4.2 is required. On RHEL/CentOS systems, this is found in the gcc44 and gcc44-c++ packages.]))
])
