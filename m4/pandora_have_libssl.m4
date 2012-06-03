dnl  Copyright (C) 2009 Sun Microsystems, Inc.
dnl This file is free software; Sun Microsystems, Inc.
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

#--------------------------------------------------------------------
# Check for openssl
#--------------------------------------------------------------------


AC_DEFUN([_PANDORA_SEARCH_LIBSSL],[
  AC_REQUIRE([AC_LIB_PREFIX])

  AC_LIB_HAVE_LINKFLAGS(ssl,,
  [
    #include <openssl/ssl.h>
  ],[
    SSL_CTX *ctx;
    ctx= SSL_CTX_new(TLSv1_client_method());
  ])

  AM_CONDITIONAL(HAVE_LIBSSL, [test "x${ac_cv_libssl}" = "xyes"])
])

AC_DEFUN([_PANDORA_HAVE_LIBSSL],[

  AC_ARG_ENABLE([libssl],
    [AS_HELP_STRING([--disable-libssl],
      [Build with libssl support @<:@default=on@:>@])],
    [ac_enable_libssl="$enableval"],
    [ac_enable_libssl="yes"])

  _PANDORA_SEARCH_LIBSSL
])


AC_DEFUN([PANDORA_HAVE_LIBSSL],[
  AC_REQUIRE([_PANDORA_HAVE_LIBSSL])
])

AC_DEFUN([_PANDORA_REQUIRE_LIBSSL],[
  ac_enable_libssl="yes"
  _PANDORA_SEARCH_LIBSSL

  AS_IF([test x$ac_cv_libssl = xno],[
    PANDORA_MSG_ERROR([libssl is required for ${PACKAGE}. On Debian this can be found in libssl-dev. On RedHat this can be found in openssl-devel.])
  ])
])

AC_DEFUN([PANDORA_REQUIRE_LIBSSL],[
  AC_REQUIRE([_PANDORA_REQUIRE_LIBSSL])
])
