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
dnl SEARCH_FOR_LIB(LIB, FUNCTIONS, FUNCTION,
dnl                [ACTION-IF-NOT-FOUND],
dnl                [LIBS_TO_ADD])

AC_DEFUN([SEARCH_FOR_LIB],
[
  AS_VAR_PUSHDEF([with_lib], [with_$1])
  AS_VAR_PUSHDEF([ac_header], [ac_cv_header_$3])
  AS_VAR_PUSHDEF([have_lib], [ac_cv_have_$1])
  AS_VAR_PUSHDEF([libs_var], AS_TR_CPP([$1_LIBS]))
  AS_VAR_PUSHDEF([cflags_var], AS_TR_CPP([$1_CFLAGS]))
  AS_VAR_PUSHDEF([path_var], AS_TR_CPP([$1_PATH]))
  AS_VAR_PUSHDEF([header_var], AS_TR_CPP([HAVE_$3]))
  AS_LITERAL_IF([$1],
                [AS_VAR_PUSHDEF([ac_lib], [ac_cv_lib_$1_$2])],
                [AS_VAR_PUSHDEF([ac_lib], [ac_cv_lib_$1''_$2])])

  AS_IF([test "x$prefix" = "xNONE"],
    [AS_VAR_SET([path_var],["$ac_default_prefix"])],
    [AS_VAR_SET([path_var],["$prefix"])])


  AC_ARG_WITH([$1],
    [AS_HELP_STRING([--with-$1@<:@=DIR@:>@],
       [Use lib$1 in DIR])],
    [ AS_VAR_SET([with_lib], [$withval]) ],
    [ AS_VAR_SET([with_lib], [yes]) ])

  AS_IF([test AS_VAR_GET([with_lib]) = yes],[
    AC_CHECK_HEADER([$3])

    my_save_LIBS="$LIBS"
    LIBS="$5"
    AC_CHECK_LIB($1, $2)
    AS_VAR_SET([libs_var],[${LIBS}])
    LIBS="${my_save_LIBS}"
    AS_VAR_SET([cflags_var],[""])
    AS_IF([test AS_VAR_GET([ac_header]) = yes -a AS_VAR_GET([ac_lib]) = yes],
      [AS_VAR_SET([have_lib],[yes])
       AS_VAR_SET([path_var],[$PATH])
      ],
      [AS_VAR_SET([have_lib],[no])
       AS_VAR_SET([with_lib],["AS_VAR_GET([path_var]) /usr/local /opt/csw /opt/local"])
      ])
  ])
  AS_IF([test "AS_VAR_GET([with_lib])" != yes],[
   for libloc in AS_VAR_GET([with_lib])
   do
    AC_MSG_CHECKING(for $1 in $libloc)
    if test -f $libloc/$3 -a -f $libloc/lib$1.a
    then
      owd=`pwd`
      if cd $libloc; then libloc=`pwd`; cd $owd; fi
      AS_VAR_SET([cflags_var],[-I$libloc])
      AS_VAR_SET([libs_var],["-L$libloc -l$1"])
      AS_VAR_SET([path_var],["$libloc:$PATH"])
      AS_VAR_SET([have_lib],[yes])
      AS_VAR_SET([ac_header],[yes])
      AC_MSG_RESULT([yes])
      break
    elif test -f $libloc/include/$3 -a -f $libloc/lib/lib$1.a; then
      owd=`pwd`
      if cd $libloc; then libloc=`pwd`; cd $owd; fi
      AS_VAR_SET([cflags_var],[-I$libloc/include])
      AS_VAR_SET([libs_var],["-L$libloc/lib -l$1"])
      AS_VAR_SET([path_var],["$libloc/bin:$PATH"])
      AS_VAR_SET([have_lib],[yes])
      AS_VAR_SET([ac_header],[yes])
      AC_MSG_RESULT([yes])
      break
    else
      AC_MSG_RESULT([no])
      AS_VAR_SET([have_lib],[no])
    fi
   done
  ])
  AS_IF([test AS_VAR_GET([have_lib]) = no],[
    AC_MSG_WARN([$3 or lib$1.a not found. Try installing $1 developement packages])
    $4
  ])
  AS_IF([test AS_VAR_GET([ac_header]) = "yes"],
    AC_DEFINE(header_var,[1],
              [Define to 1 if you have the <$3> header file.]))

  AC_SUBST(libs_var)
  AC_SUBST(cflags_var)
  AC_SUBST(path_var)
  AS_VAR_POPDEF([with_lib])
  AS_VAR_POPDEF([ac_header])
  AS_VAR_POPDEF([libs_var])
  AS_VAR_POPDEF([cflags_var])
  AS_VAR_POPDEF([path_var])
  AS_VAR_POPDEF([have_lib])
  AS_VAR_POPDEF([ac_lib])
  AS_VAR_POPDEF([header_var])
])    
