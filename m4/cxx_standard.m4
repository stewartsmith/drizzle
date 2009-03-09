AC_DEFUN([AC_CXX_CHECK_STANDARD],[
  AC_CACHE_CHECK([what C++ standard the compiler supports],
    [ac_cv_cxx_standard],[
    AC_LANG_PUSH(C++)
    save_CXXFLAGS="${CXXFLAGS}"
    AC_COMPILE_IFELSE(
      [AC_LANG_PROGRAM(
        [[
#include <string>

using namespace std;
        ]],[[
string foo("test string");
        ]])],
        [ac_cv_cxx_standard="gnu++0x"],
        [ac_cv_cxx_standard="gnu++98"])
    CXXFLAGS="${save_CXXFLAGS}"
    AC_LANG_POP()
  ])
  CXXFLAGS="-std=${ac_cv_cxx_standard} ${CXXFLAGS}"
])
