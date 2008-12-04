dnl Check to find libmemcached.

AC_DEFUN([_SEARCH_FOR_LIBMEMCACHED],[
  SEARCH_FOR_LIB(memcached,memcached_create,[libmemcached/memcached.h])
  AM_CONDITIONAL([BUILD_MEMCACHED],[test "$ac_cv_have_memcached" = "yes"])
])

AC_DEFUN([WITH_LIBMEMCACHED],[
  AC_REQUIRE([_SEARCH_FOR_LIBMEMCACHED])
])
