AC_DEFUN([DRIZZLE_CHECK_READLINE_DECLARES_HIST_ENTRY], [
    AC_CACHE_CHECK([HIST_ENTRY is declared in readline/readline.h], mysql_cv_hist_entry_declared,
	AC_TRY_COMPILE(
	    [
		#include "stdio.h"
		#include "readline/readline.h"
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

AC_DEFUN([DRIZZLE_CHECK_NEW_RL_INTERFACE], [
    save_CFLAGS="${CFLAGS}"
    CFLAGS="${save_CFLAGS} ${READLINE_CFLAGS}"
    AC_CACHE_CHECK([defined rl_compentry_func_t and rl_completion_func_t], mysql_cv_new_rl_interface,
	AC_TRY_COMPILE(
	    [
		#include "stdio.h"
		#include "readline/readline.h"
	    ],
	    [ 
		rl_completion_func_t *func1= (rl_completion_func_t*)0;
		rl_compentry_func_t *func2= (rl_compentry_func_t*)0;
	    ],
	    [
		mysql_cv_new_rl_interface=yes
                AC_DEFINE_UNQUOTED([USE_NEW_READLINE_INTERFACE], [1],
                                   [used new readline interface (are rl_completion_func_t and rl_compentry_func_t defined)])
	    ],
	    [mysql_cv_new_rl_interface=no]
        )
    )
  CFLAGS="${save_CFLAGS}"
])

