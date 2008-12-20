dnl
dnl Copyright (C) 2008 Sun Microsystems
dnl 
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; version 2 of the License.
dnl 
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl 
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
dnl
dnl Check to find libmemcached.

AC_DEFUN([_SEARCH_FOR_LIBMEMCACHED],[
  SEARCH_FOR_LIB(memcached,memcached_create,[libmemcached/memcached.h])
  AM_CONDITIONAL([BUILD_MEMCACHED],[test "$ac_cv_have_memcached" = "yes"])
])

dnl Split this into a _hidden function and a public with a require. This way
dnl any number of plugins can call the code and the real guts only get 
dnl called once.
AC_DEFUN([WITH_LIBMEMCACHED],[
  AC_REQUIRE([_SEARCH_FOR_LIBMEMCACHED])
])
