#  Copyright (C) 2009 Sun Microsystems, Inc.
#  This file is free software; Sun Microsystems, Inc.
#  gives unlimited permission to copy and/or distribute it,
#  with or without modifications, as long as this notice is preserved.

#
# Test whether madvise() is declared in C++ code -- it is not on some
# systems, such as Solaris
AC_DEFUN([LOCAL_MADVISE],
    [AC_PREREQ([2.63])dnl
    AC_LANG_PUSH([C++])
    AC_CHECK_DECLS([madvise],[],[],[AC_INCLUDES_DEFAULT[
#if HAVE_SYS_MMAN_H
# include <sys/types.h>
# include <sys/mman.h>
#endif
      ]])
    AC_LANG_POP([C++])
    ])

# Which version of the canonical setup we're using
AC_DEFUN([PANDORA_CANONICAL_VERSION],[0.175])

AC_DEFUN([PANDORA_MSG_ERROR],[
  AS_IF([test "x${pandora_cv_skip_requires}" != "xno"],[
    AC_MSG_ERROR($1)
  ],[
    AC_MSG_WARN($1)
  ])
])

AC_DEFUN([PANDORA_BLOCK_BAD_OPTIONS],[
  AS_IF([test "x${prefix}" = "x"],[
    PANDORA_MSG_ERROR([--prefix requires an argument])
  ])
])

# The standard setup for how we build Pandora projects
AC_DEFUN([PANDORA_CANONICAL_TARGET],[
  ifdef([m4_define],,[define([m4_define],   defn([define]))])
  ifdef([m4_undefine],,[define([m4_undefine],   defn([undefine]))])
  m4_define([PCT_ALL_ARGS],[$*])
  m4_define([PCT_REQUIRE_CXX],[no])
  m4_define([PCT_FORCE_GCC42],[no])
  m4_define([PCT_DONT_SUPPRESS_INCLUDE],[no])
  m4_define([PCT_NO_VC_CHANGELOG],[no])
  m4_define([PCT_VERSION_FROM_VC],[no])
  m4_define([PCT_USE_VISIBILITY],[yes])
  m4_foreach([pct_arg],[$*],[
    m4_case(pct_arg,
      [require-cxx], [
        m4_undefine([PCT_REQUIRE_CXX])
        m4_define([PCT_REQUIRE_CXX],[yes])
      ],
      [force-gcc42], [
        m4_undefine([PCT_FORCE_GCC42])
        m4_define([PCT_FORCE_GCC42],[yes])
      ],
      [skip-visibility], [
        m4_undefine([PCT_USE_VISIBILITY])
        m4_define([PCT_USE_VISIBILITY],[no])
      ],
      [dont-suppress-include], [
        m4_undefine([PCT_DONT_SUPPRESS_INCLUDE])
        m4_define([PCT_DONT_SUPPRESS_INCLUDE],[yes])
      ],
      [no-vc-changelog], [
        m4_undefine([PCT_NO_VC_CHANGELOG])
        m4_define([PCT_NO_VC_CHANGELOG],[yes])
      ],
      [version-from-vc], [
        m4_undefine([PCT_VERSION_FROM_VC])
        m4_define([PCT_VERSION_FROM_VC],[yes])
    ])
  ])

  PANDORA_BLOCK_BAD_OPTIONS

  # We need to prevent canonical target
  # from injecting -O2 into CFLAGS - but we won't modify anything if we have
  # set CFLAGS on the command line, since that should take ultimate precedence
  AS_IF([test "x${ac_cv_env_CFLAGS_set}" = "x"],
        [CFLAGS=""])
  AS_IF([test "x${ac_cv_env_CXXFLAGS_set}" = "x"],
        [CXXFLAGS=""])
  
  m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

  m4_if(m4_substr(m4_esyscmd(test -d gnulib && echo 0),0,1),0,[
    gl_EARLY
  ],[
    PANDORA_EXTENSIONS 
  ])
  
  AC_REQUIRE([AC_PROG_CC])
  m4_if(PCT_FORCE_GCC42, [yes], [
    AC_REQUIRE([PANDORA_ENSURE_GCC_VERSION])
  ])
  AC_REQUIRE([PANDORA_64BIT])

  m4_if(PCT_NO_VC_CHANGELOG,yes,[
    vc_changelog=no
  ],[
    vc_changelog=yes
  ])
  m4_if(PCT_VERSION_FROM_VC,yes,[
    PANDORA_VC_INFO_HEADER
  ],[
    PANDORA_TEST_VC_DIR

    changequote(<<, >>)dnl
    PANDORA_RELEASE_ID=`echo $VERSION | sed 's/[^0-9]//g'`
    changequote([, ])dnl

    PANDORA_RELEASE_COMMENT=""
    AC_DEFINE_UNQUOTED([PANDORA_RELEASE_VERSION],["$VERSION"],
                       [Version of the software])

    AC_SUBST(PANDORA_RELEASE_COMMENT)
    AC_SUBST(PANDORA_RELEASE_VERSION)
    AC_SUBST(PANDORA_RELEASE_ID)
  ])
  PANDORA_VERSION

# Once we can use a modern autoconf, we can use this
# AC_PROG_CC_C99
  AC_REQUIRE([AC_PROG_CXX])
  PANDORA_EXTENSIONS
  AM_PROG_CC_C_O

  PANDORA_PLATFORM

# autoconf doesn't automatically provide a fail-if-no-C++ macro
# so we check c++98 features and fail if we don't have them, mainly
# for that reason
  PANDORA_CHECK_CXX_STANDARD
  m4_if(PCT_REQUIRE_CXX, [yes], [
    AS_IF([test "$ac_cv_cxx_stdcxx_98" = "no"],[
      PANDORA_MSG_ERROR([No working C++ Compiler has been found. ${PACKAGE} requires a C++ compiler that can handle C++98])
    ])
  ])
  AX_CXX_CINTTYPES
  
  PANDORA_CHECK_C_VERSION
  PANDORA_CHECK_CXX_VERSION

  AC_CACHE_CHECK([if system defines RUSAGE_THREAD], [ac_cv_rusage_thread],[
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
      [[
#include <sys/time.h>
#include <sys/resource.h>
      ]],[[
      int x= RUSAGE_THREAD;
      ]])
    ],[
      ac_cv_rusage_thread=yes
    ],[
      ac_cv_rusage_thread=no
    ])
  ])
  AS_IF([test "$ac_cv_rusage_thread" = "no"],[
    AC_DEFINE([RUSAGE_THREAD], [RUSAGE_SELF],
      [Define if system doesn't define])
  ])

  LT_LIB_M

  PANDORA_OPTIMIZE

  LOCAL_MADVISE

  PANDORA_HAVE_GCC_ATOMICS

  PANDORA_HEADER_ASSERT

  PANDORA_WARNINGS(PCT_ALL_ARGS)

  PANDORA_ENABLE_DTRACE

  AC_LIB_PREFIX

  AX_PROG_SPHINX_BUILD

  m4_if(m4_substr(m4_esyscmd(test -d po && echo 0),0,1),0, [
    AM_PO_SUBDIRS
    IT_PROG_INTLTOOL([0.35],[no-xml])
    
    GETTEXT_PACKAGE=$PACKAGE
    AC_CHECK_LIB(intl, libintl_gettext)
    AC_SUBST([GETTEXT_PACKAGE])
    AS_IF([test "x${USE_NLS}" = "xyes" -a "x${pandora_have_intltool}" = "xyes"],
          [AC_DEFINE([ENABLE_NLS],[1],[Turn on language support])
           AC_CONFIG_FILES([po/Makefile.in])
      ])
  ])
  AM_CONDITIONAL(BUILD_PO,[test "x${USE_NLS}" = "xyes" -a "x${pandora_have_intltool}" = "xyes"])

  AS_IF([test "x${gl_LIBOBJS}" != "x"],[
    AS_IF([test "$GCC" = "yes"],[
      AM_CPPFLAGS="-isystem \${top_srcdir}/gnulib -isystem \${top_builddir}/gnulib ${AM_CPPFLAGS}"
    ],[
    AM_CPPFLAGS="-I\${top_srcdir}/gnulib -I\${top_builddir}/gnulib ${AM_CPPFLAGS}"
    ])
  ])
  m4_if(m4_substr(m4_esyscmd(test -d src && echo 0),0,1),0,[
    AM_CPPFLAGS="-I\$(top_srcdir)/src -I\$(top_builddir)/src ${AM_CPPFLAGS}"
  ],[
    AM_CPPFLAGS="-I\$(top_srcdir) -I\$(top_builddir) ${AM_CPPFLAGS}"
  ])

  PANDORA_USE_PIPE

  AH_TOP([
#ifndef __CONFIG_H__
#define __CONFIG_H__

/* _SYS_FEATURE_TESTS_H is Solaris, _FEATURES_H is GCC */
#if defined( _SYS_FEATURE_TESTS_H) || defined(_FEATURES_H)
#error "You should include config.h as your first include file"
#endif

#include <config/top.h>
])
  mkdir -p config
  cat > config/top.h.stamp <<EOF_CONFIG_TOP

#if defined(i386) && !defined(__i386__)
#define __i386__
#endif

#if defined(_FILE_OFFSET_BITS)
# undef _FILE_OFFSET_BITS
#endif
EOF_CONFIG_TOP

  diff config/top.h.stamp config/top.h >/dev/null 2>&1 || mv config/top.h.stamp config/top.h
  rm -f config/top.h.stamp

  AH_BOTTOM([
#if defined(__cplusplus)
# include CSTDINT_H
# include CINTTYPES_H
#else
# include <stdint.h>
# include <inttypes.h>
#endif

#if !defined(HAVE_ULONG) && !defined(__USE_MISC)
# define HAVE_ULONG 1
typedef unsigned long int ulong;
#endif

/* To hide the platform differences between MS Windows and Unix, I am
 * going to use the Microsoft way and #define the Microsoft-specific
 * functions to the unix way. Microsoft use a separate subsystem for sockets,
 * but Unix normally just use a filedescriptor on the same functions. It is
 * a lot easier to map back to the unix way with macros than going the other
 * way without side effect ;-)
 */
#ifdef TARGET_OS_WINDOWS
#define random() rand()
#define srandom(a) srand(a)
#define get_socket_errno() WSAGetLastError()
#else
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(a) close(a)
#define get_socket_errno() errno
#endif

#if defined(__cplusplus)
# if defined(DEBUG)
#  include <cassert>
#  include <cstddef>
# endif
template<typename To, typename From>
inline To implicit_cast(From const &f) {
  return f;
}
template<typename To, typename From>     // use like this: down_cast<T*>(foo);
inline To down_cast(From* f) {                   // so we only accept pointers
  // Ensures that To is a sub-type of From *.  This test is here only
  // for compile-time type checking, and has no overhead in an
  // optimized build at run-time, as it will be optimized away
  // completely.
  if (false) {
    implicit_cast<From*, To>(0);
  }

#if defined(DEBUG)
  assert(f == NULL || dynamic_cast<To>(f) != NULL);  // RTTI: debug mode only!
#endif
  return static_cast<To>(f);
}
#endif /* defined(__cplusplus) */

#endif /* __CONFIG_H__ */
  ])

  AM_CFLAGS="${AM_CFLAGS} ${CC_WARNINGS} ${CC_PROFILING} ${CC_COVERAGE}"
  AM_CXXFLAGS="${AM_CXXFLAGS} ${CXX_WARNINGS} ${CC_PROFILING} ${CC_COVERAGE}"

  AC_SUBST([AM_CFLAGS])
  AC_SUBST([AM_CXXFLAGS])
  AC_SUBST([AM_CPPFLAGS])
  AC_SUBST([AM_LDFLAGS])

])
