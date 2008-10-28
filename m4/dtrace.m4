dnl ---------------------------------------------------------------------------
dnl Macro: DTRACE_TEST
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE([dtrace],
    [AS_HELP_STRING([--enable-dtrace],
            [Build with support for the DTRACE. @<:@default=off@:>@])],
    [ 
      AC_DEFINE([HAVE_DTRACE], [1], [Enables DTRACE Support])
      AC_DEFINE([_DTRACE_VERSION], [1], [DTRACE Version to Use])
      AC_CHECK_PROGS(DTRACE, dtrace)
      ENABLE_DTRACE="yes" 
      AC_SUBST(DTRACEFLAGS)
      AC_SUBST(HAVE_DTRACE)
    ],
    [
      ENABLE_DTRACE="no" 
    ]
    )
AM_CONDITIONAL([HAVE_DTRACE], [ test "$ENABLE_DTRACE" = "yes" ])
dnl ---------------------------------------------------------------------------
dnl End Macro: DTRACE_TEST
dnl ---------------------------------------------------------------------------
