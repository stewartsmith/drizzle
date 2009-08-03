dnl -*- mode: m4; c-basic-offset: 2; indent-tabs-mode: nil; -*-
dnl vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
dnl   
dnl pandora-build: A pedantic build system
dnl Copyright (C) 2009 Sun Microsystems, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl From Monty Taylor

dnl --------------------------------------------------------------------
dnl  Check for Google Proto Buffers
dnl --------------------------------------------------------------------

AC_DEFUN([_PANDORA_SEARCH_LIBPROTOBUF],[
  AC_REQUIRE([PANDORA_HAVE_PTHREAD])

  AC_LANG_PUSH([C++])
  save_CXXFLAGS="${CXXFLAGS}"
  CXXFLAGS="${PTHREAD_CFLAGS} ${CXXFLAGS}"
  AC_LIB_HAVE_LINKFLAGS(protobuf,,
    [#include <google/protobuf/descriptor.h>],
    [google::protobuf::FileDescriptor* file;],
    [system])
  CXXFLAGS="${save_CXXFLAGS}"
  AC_LANG_POP()
])

AC_DEFUN([PANDORA_HAVE_LIBPROTOBUF],[
  AC_REQUIRE([_PANDORA_SEARCH_LIBPROTOBUF])
])

AC_DEFUN([PANDORA_REQUIRE_LIBPROTOBUF],[
  AC_REQUIRE([PANDORA_HAVE_LIBPROTOBUF])
  AS_IF([test x$ac_cv_libprotobuf = xno],
      AC_MSG_ERROR([libprotobuf is required for ${PACKAGE}. On Debian this can be found in libprotobuf-dev. On RedHat this can be found in protobuf-devel.]))
])

AC_DEFUN([PANDORA_PROTOBUF_RECENT],[
  AC_REQUIRE([_PANDORA_SEARCH_LIBPROTOBUF])
  AC_LANG_PUSH([C++])
  AC_CACHE_CHECK([if protobuf is recent enough], [drizzle_cv_protobuf_recent],
    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <google/protobuf/descriptor.h>
#if GOOGLE_PROTOBUF_VERSION < 2000002
# error Your version of Protobuf is too old
#endif
      ]])],
      [drizzle_cv_protobuf_recent=yes],
      [drizzle_cv_protobuf_recent=no])])
  AS_IF([test "$drizzle_cv_protobuf_recent" = "no"],[
    AC_MSG_ERROR([Your version of Google Protocol Buffers is too old. ${PACKAGE} requires at least version 2.0.2])
  ])
  AC_LANG_POP()
])

AC_DEFUN([_PANDORA_SEARCH_PROTOC],[
  AC_REQUIRE([_PANDORA_SEARCH_LIBPROTOBUF])
  AC_PATH_PROG([PROTOC],[protoc],[no],[$LIBPROTOBUF_PREFIX/bin:$PATH])
])

AC_DEFUN([PANDORA_HAVE_PROTOC],[
  AC_REQUIRE([_PANDORA_SEARCH_PROTOC])
])

AC_DEFUN([PANDORA_REQUIRE_PROTOC],[
  AC_REQUIRE([PANDORA_HAVE_PROTOC])
  AS_IF([test "x$PROTOC" = "xno"],[
    AC_MSG_ERROR([Couldn't find the protoc compiler. On Debian this can be found in protobuf-compiler. On RedHat this can be found in protobuf-compiler.])
  ])
])


