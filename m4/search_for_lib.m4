dnl SEARCH_FOR_LIB(LIB, FUNCTIONS, FUNCTION,
dnl                [ACTION-IF-NOT-FOUND])

AC_DEFUN([SEARCH_FOR_LIB],
[
  AS_VAR_PUSHDEF([with_lib], [with_lib$1])
  AS_VAR_PUSHDEF([ac_header], [ac_cv_header_$3])
  AS_VAR_PUSHDEF([libs_var], AS_TR_CPP([$1_LIBS]))
  AS_VAR_PUSHDEF([cppflags_var], AS_TR_CPP([$1_CPPFLAGS]))
  AS_VAR_PUSHDEF([have_lib], [ac_cv_have_$1])
  AS_LITERAL_IF([$1],
                [AS_VAR_PUSHDEF([ac_lib], [ac_cv_lib_$1_$2])],
                [AS_VAR_PUSHDEF([ac_lib], [ac_cv_lib_$1''_$2])])

  AC_ARG_WITH([lib$1],
    [AS_HELP_STRING([--with-lib$1@<:@=DIR@:>@],
       [Use lib$1 in DIR])],
    [ AS_VAR_SET([with_lib], [$withval]) ],
    [ AS_VAR_SET([with_lib], [yes]) ])

  AS_IF([test AS_VAR_GET([with_lib]) = yes],[
    AC_CHECK_HEADERS([$3])
    AS_IF([test AS_VAR_GET([ac_header]) = no],
      AC_MSG_WARN([Couldn't find $3. Try installing lib$3 development packages])
      [$4])

    my_save_LIBS="$LIBS"
    LIBS=""
    AC_CHECK_LIB($1, $2)
    AS_VAR_SET([libs_var],[${LIBS}])
    LIBS="${my_save_LIBS}"
    AS_VAR_SET([cppflags_var],[""])
    AS_IF([test AS_VAR_GET([ac_header]) = "$3" -a AS_VAR_GET([ac_lib]) = yes],
      [AS_VAR_SET([have_lib],[yes])],
      [AS_VAR_SET([have_lib],[no])
       [$4]])
  ],[
    AC_MSG_CHECKING(for $1 in $withval)
    if test -f $withval/$3 -a -f $withval/lib$1.a
    then
      owd=`pwd`
      if cd $withval; then withval=`pwd`; cd $owd; fi
      AS_VAR_SET([cppflags_var],[-I$withval])
      AS_VAR_SET([libs_var],[-L$withval -l$1])
      AS_VAR_SET([have_lib],[yes])
    elif test -f $withval/include/$3 -a -f $withval/lib/lib$1.a; then
      owd=`pwd`
      if cd $withval; then withval=`pwd`; cd $owd; fi
      AS_VAR_SET([cppflags_var],[-I$withval/include])
      AS_VAR_SET([libs_var],[-L$withval/lib -l$1])
      AS_VAR_SET([have_lib],[yes])
    else
      AC_MSG_WARN([$3 or lib$1.a not found in $withval])
      AS_VAR_SET([have_lib],[no])
      [$4]
    fi
  ])
  AC_SUBST(LIBMEMCACHED_LIBS)
  AC_SUBST(LIBMEMCACHED_CPPFLAGS)
  AM_CONDITIONAL(HAVE_MEMCACHED,[test "$ac_cv_have_memcached" = "yes"])
  AS_VAR_POPDEF([with_lib])
  AS_VAR_POPDEF([ac_header])
  AS_VAR_POPDEF([libs_var])
  AS_VAR_POPDEF([cppflags_var])
  AS_VAR_POPDEF([have_lib])
  AS_VAR_POPDEF([ac_lib])
])    
