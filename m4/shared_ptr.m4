# We check two things: where is the memory include file, and in what
# namespace does shared_ptr reside.
# We include AC_COMPILE_IFELSE for all the combinations we've seen in the
# wild:
# 
#  GCC 4.3: namespace: std::  #include <memory>
#  GCC 4.2: namespace: tr1::  #include <tr1/memory>
#  GCC 4.2: namespace: boost::  #include <boost/shared_ptr.hpp>
#
# We define one of HAVE_HAVE_TR1_SHARED_PTR or HAVE_BOOST_SHARED_PTR
# depending on location, and SHARED_PTR_NAMESPACE to be the namespace in
# which shared_ptr is defined.
#

AC_DEFUN([AC_CXX_SHARED_PTR],[
  AC_REQUIRE([AC_CXX_CHECK_STANDARD])
  AC_LANG_PUSH(C++)
  AC_CHECK_HEADERS(memory tr1/memory boost/shared_ptr.hpp)
  AC_CACHE_CHECK([the location of shared_ptr header file],
    [ac_cv_shared_ptr_h],[
      for namespace in std tr1 std::tr1 boost
      do
        AC_COMPILE_IFELSE(
          [AC_LANG_PROGRAM([[
#if defined(HAVE_MEMORY) || defined(HAVE_TR1_MEMORY)
# if defined(HAVE_MEMORY)
#  include <memory>
# endif
# if defined(HAVE_TR1_MEMORY)
#  include <tr1/memory>
# endif
#else
# if defined(HAVE_BOOST_SHARED_PTR_HPP)
#  include <boost/shared_ptr.hpp>
# endif
#endif
#include <string>

using $namespace::shared_ptr;
using namespace std;
            ]],[[
shared_ptr<string> test_ptr(new string("test string"));
            ]])],
            [
              ac_cv_shared_ptr_namespace="${namespace}"
              break
            ],[ac_cv_shared_ptr_namespace=no])
       done
  ])
  if test "$ac_cv_shared_ptr_namespace" = no
  then
    AC_MSG_ERROR([a usable shared_ptr implementation is required. If you are on Solaris, please install boost, either via pkg install boost, or pkg-get -i boost. If you are elsewhere, please file a bug])
  fi
  AC_DEFINE_UNQUOTED([SHARED_PTR_NAMESPACE],
                     ${ac_cv_shared_ptr_namespace},
                     [The namespace in which SHARED_PTR can be found])
  AC_LANG_POP()
])
