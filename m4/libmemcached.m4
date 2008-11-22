dnl Check to find libmemcached.

AC_DEFUN([_SEARCH_FOR_LIBMEMCACHED],[
  SEARCH_FOR_LIB(memcached,memcached_create,[libmemcached/memcached.h])
])

AC_DEFUN([WITH_LIBMEMCACHED],[
  AC_REQUIRE([_SEARCH_FOR_LIBMEMCACHED])
])
