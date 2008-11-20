dnl Check to find libmemcached.

AC_DEFUN([_SEARCH_FOR_LIBMEMCACHED],[
  AC_ARG_WITH(libmemcached,
    [AS_HELP_STRING([--with-libmemcached@<:@=DIR@:>@],
       [Use libmemcached in DIR])],
    [ with_libmemcached=$withval ],
    [ with_libmemcached=yes ])

  if test "$with_libmemcached" = "yes"
  then
    AC_CHECK_HEADERS(libmemcached/memcached.h)
    if test "$ac_cv_header_libmemcached_memcached_h" = "no"
    then
      AC_MSG_WARN([Couldn't find memcached.h. Try installing libmemcached development packages])
    fi
    my_save_LIBS="$LIBS"
    LIBS=""
    AC_CHECK_LIB(memcached, memcached_create)
    LIBMEMCACHED_LIBS="${LIBS}"
    LIBS="${my_save_LIBS}"
    LIBMEMCACHED_CPPFLAGS=""
    if test "$ac_cv_header_libmemcached_memcached_h" = "yes" -a "$ac_cv_lib_memcached_memcached_create" = "yes"
    then
      ac_cv_have_memcached=yes
    else
      ac_cv_have_memcached=no
    fi
  else
    AC_MSG_CHECKING(for libmemcached in $withval)
    if test -f $withval/libmemcached/memcached.h -a -f $withval/libmemcached.a
    then
      owd=`pwd`
      if cd $withval; then withval=`pwd`; cd $owd; fi
      LIBMEMCACHED_CPPFLAGS="-I$withval"
      LIBMEMCACHED_LIBS="-L$withval -lmemcached"
      ac_cv_have_memcached=yes
    elif test -f $withval/include/libmemcached/memcached.h -a -f $withval/lib/libmemcached.a; then
      owd=`pwd`
      if cd $withval; then withval=`pwd`; cd $owd; fi
      LIBMEMCACHED_CPPFLAGS="-I$withval/include"
      LIBMEMCACHED_LIBS="-L$withval/lib -lmemcached"
      ac_cv_have_memcached=yes
    else
      AC_MSG_WARN([memcached.h or libmemcached.a not found in $withval])
      ac_cv_have_memcached=no
    fi
  fi
  AC_SUBST(LIBMEMCACHED_LIBS)
  AC_SUBST(LIBMEMCACHED_CPPFLAGS)
  AM_CONDITIONAL(HAVE_MEMCACHED,[test "$ac_cv_have_memcached" = "yes"])
])    

AC_DEFUN([WITH_LIBMEMCACHED],[
  AC_REQUIRE([_SEARCH_FOR_LIBMEMCACHED])
])
