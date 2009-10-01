AC_DEFUN([DRIZZLE_CHECK_READLINE_DECLARES_HIST_ENTRY], [
  AC_CACHE_CHECK([HIST_ENTRY is declared in readline/readline.h],
                 mysql_cv_hist_entry_declared,
    AC_TRY_COMPILE(
      [
#include <stdio.h>
#include <readline/readline.h>
      ],
      [ 
HIST_ENTRY entry;
      ],
      [
        mysql_cv_hist_entry_declared=yes
        AC_DEFINE_UNQUOTED(HAVE_HIST_ENTRY, [1],
                           [HIST_ENTRY is defined in the outer libeditreadline])
      ],
      [mysql_cv_libedit_interface=no]
    )
  )
])

AC_DEFUN([DRIZZLE_CHECK_RL_COMPENTRY], [
  AC_CACHE_CHECK([defined rl_compentry_func_t], [drizzle_cv_rl_compentry],[
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#include "stdio.h"
#include "readline/readline.h"
      ]],[[
rl_compentry_func_t *func2= (rl_compentry_func_t*)0;
      ]])
    ],[
      drizzle_cv_rl_compentry=yes
    ],[
      drizzle_cv_rl_compentry=no
    ])
  ])
  AS_IF([test "$drizzle_cv_rl_compentry" = "yes"],[
    AC_DEFINE([HAVE_RL_COMPENTRY], [1],
              [Does system provide rl_compentry_func_t])
  ])

  AC_LANG_PUSH(C++)
  save_CXXFLAGS="${CXXFLAGS}"
  CXXFLAGS="${AM_CXXFLAGS} ${CXXFLAGS}"
  AC_CACHE_CHECK([rl_compentry_func_t works], [drizzle_cv_rl_compentry_works],[
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#include "stdio.h"
#include "readline/readline.h"
      ]],[[
rl_completion_entry_function= (rl_compentry_func_t*)NULL;
      ]])
    ],[
      drizzle_cv_rl_compentry_works=yes
    ],[
      drizzle_cv_rl_compentry_works=no
    ])
  ])
  AS_IF([test "$drizzle_cv_rl_compentry_works" = "yes"],[
    AC_DEFINE([HAVE_WORKING_RL_COMPENTRY], [1],
              [Does system provide an rl_compentry_func_t that is usable])
  ])
  CXXFLAGS="${save_CXXFLAGS}"
  AC_LANG_POP()
])


AC_DEFUN([DRIZZLE_CHECK_RL_COMPLETION_FUNC], [
  AC_CACHE_CHECK([defined rl_completion_func_t], [drizzle_cv_rl_completion],[
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM([[
#include "stdio.h"
#include "readline/readline.h"
      ]],[[
rl_completion_func_t *func1= (rl_completion_func_t*)0;
      ]])
    ],[
      drizzle_cv_rl_completion=yes
    ],[
      drizzle_cv_rl_completion=no
    ])
  ])
  AS_IF([test "$drizzle_cv_rl_completion" = "yes"],[
    AC_DEFINE([HAVE_RL_COMPLETION], [1],
              [Does system provide rl_completion_func_t])
  ])
])

AC_DEFUN([DRIZZLE_CHECK_NEW_RL_INTERFACE],[
  DRIZZLE_CHECK_RL_COMPENTRY  
  DRIZZLE_CHECK_RL_COMPLETION_FUNC
])
