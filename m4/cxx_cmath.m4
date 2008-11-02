# We check two things: where the include file is for cmath. We
# include AC_TRY_COMPILE for all the combinations we've seen in the
# wild.  We define one of HAVE_CMATH or HAVE_TR1_CMATH or 
# HAVE_BOOST_CMATH depending
# on location.

AC_DEFUN([AC_CXX_CMATH],
  [AC_MSG_CHECKING(the location of cmath)
  AC_LANG_SAVE
   AC_LANG_CPLUSPLUS
   ac_cv_cxx_cmath=""
   ac_cv_cxx_cmath_namespace=""
   for location in tr1/cmath boost/cmath cmath; do
     for namespace in __gnu_cxx "" std stdext std::tr1; do
       if test -z "$ac_cv_cxx_cmath"; then
         AC_TRY_COMPILE([#include <$location>],
                        [$namespace::isfinite(1)],
                        [ac_cv_cxx_cmath="<$location>";
                         ac_cv_cxx_cmath_namespace="$namespace";])
       fi
    done
   done
   if test -n "$ac_cv_cxx_cmath"; then
      AC_MSG_RESULT([$ac_cv_cxx_cmath])
   else
      ac_cv_cxx_cmath="<math.h>"
      ac_cv_cxx_cmath_namespace=""
      AC_MSG_RESULT()
      AC_MSG_WARN([Could not find a cmath header.])
   fi
   AC_DEFINE_UNQUOTED(CMATH_H,$ac_cv_cxx_cmath,
                      [the location of <cmath>])
   if test "x$ac_cv_cxx_cmath_namespace" != "x"
   then
     AC_DEFINE_UNQUOTED(CMATH_NAMESPACE,$ac_cv_cxx_cmath_namespace,
                        [the namespace of C99 math extensions])
   fi
])
