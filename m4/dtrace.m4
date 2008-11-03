dnl ---------------------------------------------------------------------------
dnl Macro: DTRACE_TEST
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE([dtrace],
    [AS_HELP_STRING([--enable-dtrace],
            [Build with support for the DTRACE. @<:@default=off@:>@])],
    [ENABLE_DTRACE="yes"],
    [ENABLE_DTRACE="no"])

if test "$ENABLE_DTRACE" = "yes"
then
  AC_DEFINE([HAVE_DTRACE], [1], [Enables DTRACE Support])
  AC_CHECK_PROGS(DTRACE, dtrace)
  AC_SUBST(DTRACEFLAGS)
  AC_SUBST(HAVE_DTRACE)
fi
AM_CONDITIONAL([HAVE_DTRACE], [ test "$ENABLE_DTRACE" = "yes" ])
dnl ---------------------------------------------------------------------------
dnl End Macro: DTRACE_TEST
dnl ---------------------------------------------------------------------------
