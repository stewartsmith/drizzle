dnl AC_PANDORA_WARNINGS([less-warnings|warnings-always-on])
dnl   less-warnings turn on a limited set of warnings
dnl   warnings-always-on always set warnings=error regardless of tarball/vc
 
AC_DEFUN([PANDORA_WARNINGS],[
  PW_FULL_WARNINGS=yes
  PW_VC_WARN_OFF=yes
  for arg in $@ ; do
    case "$arg" in
      less-warnings)
        PW_FULL_WARNINGS=no
        ;;
      warnings-always-on)
        PW_VC_WARN_OFF=no
        ;;
    esac
  done

  AC_REQUIRE([PANDORA_BUILDING_FROM_VC])
  AS_IF([test "${PW_VC_WARN_OFF}" = "no" -o "$ac_cv_building_from_vc" = "yes"],
    [ac_cv_warnings_as_errors=yes],
    [ac_cv_warnings_as_errors=no])

  AC_ARG_ENABLE([profiling],
      [AS_HELP_STRING([--enable-profiling],
         [Toggle profiling @<:@default=off@:>@])],
      [ac_profiling="$enableval"],
      [ac_profiling="no"])

  AC_ARG_ENABLE([coverage],
      [AS_HELP_STRING([--enable-coverage],
         [Toggle coverage @<:@default=off@:>@])],
      [ac_coverage="$enableval"],
      [ac_coverage="no"])

  AS_IF([test "$GCC" = "yes"],[

    AS_IF([test "$ac_profiling" = "yes"],[
      CC_PROFILING="-pg"
      save_LIBS="${LIBS}"
      LIBS=""
      AC_CHECK_LIB(c_p, read)
      LIBC_P="${LIBS}"
      LIBS="${save_LIBS}"
      AC_SUBST(LIBC_P)
    ],[
      CC_PROFILING=" "
    ])

    AS_IF([test "$ac_coverage" = "yes"],
          [CC_COVERAGE="-fprofile-arcs -ftest-coverage"])
	 
    AS_IF([test "$ac_cv_warnings_as_errors" = "yes"],
          [W_FAIL="-Werror"])

    AC_CACHE_CHECK([whether it is safe to use -fdiagnostics-show-option],
      [ac_cv_safe_to_use_fdiagnostics_show_option_],
      [save_CFLAGS="$CFLAGS"
       CFLAGS="-fdiagnostics-show-option ${AM_CFLAGS}"
       AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([],[])],
         [ac_cv_safe_to_use_fdiagnostics_show_option_=yes],
         [ac_cv_safe_to_use_fdiagnostics_show_option_=no])
       CFLAGS="$save_CFLAGS"])

    AS_IF([test "$ac_cv_safe_to_use_fdiagnostics_show_option_" = "yes"],
          [
            F_DIAGNOSTICS_SHOW_OPTION="-fdiagnostics-show-option"
          ])

    AC_CACHE_CHECK([whether it is safe to use -Wconversion],
      [ac_cv_safe_to_use_wconversion_],
      [save_CFLAGS="$CFLAGS"
       CFLAGS="-Wconversion ${W_FAIL} -pedantic ${AM_CFLAGS}"
       AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([[
#include <stdbool.h>
void foo(bool a)
{
  (void)a;
}
         ]],[[
foo(0);
         ]])],
         [ac_cv_safe_to_use_wconversion_=yes],
         [ac_cv_safe_to_use_wconversion_=no])
       CFLAGS="$save_CFLAGS"])

    AS_IF([test "$ac_cv_safe_to_use_wconversion_" = "yes"],
      [W_CONVERSION="-Wconversion"
      AC_CACHE_CHECK([whether it is safe to use -Wconversion with htons],
        [ac_cv_safe_to_use_Wconversion_],
        [save_CFLAGS="$CFLAGS"
         CFLAGS="-Wconversion ${W_FAIL} -pedantic ${AM_CFLAGS}"
         AC_COMPILE_IFELSE(
           [AC_LANG_PROGRAM(
             [[
#include <netinet/in.h>
             ]],[[
uint16_t x= htons(80);
             ]])],
           [ac_cv_safe_to_use_Wconversion_=yes],
           [ac_cv_safe_to_use_Wconversion_=no])
         CFLAGS="$save_CFLAGS"])

      AS_IF([test "$ac_cv_safe_to_use_Wconversion_" = "no"],
            [NO_CONVERSION="-Wno-conversion"])
    ])

    NO_STRICT_ALIASING="-fno-strict-aliasing -Wno-strict-aliasing"
    NO_SHADOW="-Wno-shadow"

    AS_IF([test "${PW_FULL_WARNINGS}" = "yes"],[
      BASE_WARNINGS_FULL="-Wformat=2 ${W_CONVERSION} -Wstrict-aliasing"
      CC_WARNINGS_FULL="-Wswitch-default -Wswitch-enum"
      CXX_WARNINGS_FULL="-Weffc++ -Wold-style-cast"
    ],[
      BASE_WARNINGS_FULL="-Wformat ${NO_STRICT_ALIASING}"
    ])

    BASE_WARNINGS="-pedantic -Wall -Wextra ${W_FAIL} -Wundef -Wshadow -Wmissing-declarations -Wstrict-aliasing ${F_DIAGNOSTICS_SHOW_OPTION} ${CFLAG_VISIBILITY} ${BASE_WARNINGS_FULL}"
    CC_WARNINGS="${BASE_WARNINGS} -Wstrict-prototypes -Wmissing-prototypes -Wredundant-decls ${CC_WARNINGS_FULL} -Wcast-align"
    CXX_WARNINGS="${BASE_WARNINGS} -Woverloaded-virtual -Wnon-virtual-dtor -Wctor-dtor-privacy -Wno-long-long ${CXX_WARNINGS_FULL}"

    AC_CACHE_CHECK([whether it is safe to use -Wlogical-op],
      [ac_cv_safe_to_use_Wlogical_op_],
      [save_CFLAGS="$CFLAGS"
       CFLAGS="-Wlogical-op -Werror -pedantic ${AM_CFLAGS}"
       AC_COMPILE_IFELSE([
         AC_LANG_PROGRAM(
         [[
#include <stdio.h>
         ]], [[]])
      ],
      [ac_cv_safe_to_use_Wlogical_op_=yes],
      [ac_cv_safe_to_use_Wlogical_op_=no])
    CFLAGS="$save_CFLAGS"])
    AS_IF([test "$ac_cv_safe_to_use_Wlogical_op_" = "yes"],
          [CC_WARNINGS="${CC_WARNINGS} -Wlogical-op"])

    AC_CACHE_CHECK([whether it is safe to use -Wredundant-decls from C++],
      [ac_cv_safe_to_use_Wredundant_decls_],
      [AC_LANG_PUSH(C++)
       save_CXXFLAGS="${CXXFLAGS}"
       CXXFLAGS="-Wredundant-decls ${W_FAIL} -pedantic -Wredundant-decls"
       AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([
template <typename E> struct C { void foo(); };
template <typename E> void C<E>::foo() { }
template <> void C<int>::foo();
          AC_INCLUDES_DEFAULT])],
          [ac_cv_safe_to_use_Wredundant_decls_=yes],
          [ac_cv_safe_to_use_Wredundant_decls_=no])
        CXXFLAGS="${save_CXXFLAGS}"
        AC_LANG_POP()])
    AS_IF([test "$ac_cv_safe_to_use_Wredundant_decls_" = "yes"],
          [CXX_WARNINGS="${CXX_WARNINGS} -Wredundant-decls"],
          [CXX_WARNINGS="${CXX_WARNINGS} -Wno-redundant-decls"])

    NO_REDUNDANT_DECLS="-Wno-redundant-decls"
    PROTOSKIP_WARNINGS="-Wno-effc++ -Wno-shadow"
    
  ])

  AS_IF([test "$SUNCC" = "yes"],[

    AS_IF([test "$ac_profiling" = "yes"],
          [CC_PROFILING="-xinstrument=datarace"])

    AS_IF([test "$ac_cv_warnings_as_errors" = "yes"],
          [W_FAIL="-errwarn=%all"])

    AC_CACHE_CHECK([whether E_PASTE_RESULT_NOT_TOKEN is usable],
      [ac_cv_paste_result],
      [
        save_CFLAGS="${CFLAGS}"
        CFLAGS="-errwarn=%all -erroff=E_PASTE_RESULT_NOT_TOKEN ${CFLAGS}"
        AC_COMPILE_IFELSE(
          [AC_LANG_PROGRAM([
            AC_INCLUDES_DEFAULT
          ],[
            int x= 0;])],
          [ac_cv_paste_result=yes],
          [ac_cv_paste_result=no])
        CFLAGS="${save_CFLAGS}"
      ])
    AS_IF([test $ac_cv_paste_result = yes],
      [W_PASTE_RESULT=",E_PASTE_RESULT_NOT_TOKEN"])


    AS_IF([test "${PW_FULL_WARNINGS}" = "yes"],[
      CC_WARNINGS_FULL="-erroff=E_INTEGER_OVERFLOW_DETECTED${W_PASTE_RESULT}"
    ],[
      CC_WARNINGS_FULL="-erroff=E_ATTRIBUTE_NOT_VAR"
      CXX_WARNINGS_FULL="-erroff=attrskipunsup,doubunder,reftotemp,inllargeuse,truncwarn1,signextwarn,inllargeint"
    ])

    CC_WARNINGS="-v -errtags=yes ${W_FAIL} ${CC_WARNINGS_FULL}"
    CXX_WARNINGS="+w +w2 -xwe -xport64 -errtags=yes ${CXX_WARNINGS_FULL} ${W_FAIL}"
    PROTOSKIP_WARNINGS="-erroff=attrskipunsup,doubunder,reftotemp,wbadinitl,identexpected,inllargeuse,truncwarn1,signextwarn"
    NO_UNREACHED="-erroff=E_STATEMENT_NOT_REACHED"

  ])

  AC_SUBST(NO_CONVERSION)
  AC_SUBST(NO_REDUNDANT_DECLS)
  AC_SUBST(NO_UNREACHED)
  AC_SUBST(NO_SHADOW)
  AC_SUBST(NO_STRICT_ALIASING)
  AC_SUBST(PROTOSKIP_WARNINGS)


])
