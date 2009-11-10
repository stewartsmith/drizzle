dnl -*- mode: m4; c-basic-offset: 2; indent-tabs-mode: nil; -*-
dnl vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
dnl   
dnl pandora-build: A pedantic build system
dnl Copyright (C) 2009 Sun Microsystems, Inc.
dnl This file is free software; Sun Microsystems
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl
dnl From Monty Taylor

AC_DEFUN([PANDORA_WITH_GETTEXT],[

  mkdir -p po
  echo "# This file is auto-generated from configure. Do not edit directly" > po/POTFILES.in.in
  # The grep -v '${PACKAGE}-' is to exclude any distcheck leftovers
  for f in `cd ${srcdir} ; find . | grep -v "${PACKAGE}-" | ${EGREP} '\.(cc|c|h|yy)$' | cut -c3- | sort`
    do
      if grep gettext.h "${srcdir}/$f" | grep include >/dev/null 2>&1
      then
        echo "$f" >> po/POTFILES.in.in
      fi
    done
    if diff po/POTFILES.in.in po/POTFILES.in >/dev/null 2>&1
    then
      rm po/POTFILES.in.in
    else
      mv po/POTFILES.in.in po/POTFILES.in
    fi

  AM_GNU_GETTEXT(external, need-formatstring-macros)
  AM_GNU_GETTEXT_VERSION([0.17])
  AS_IF([test "x$MSGMERGE" = "x" -o "x$MSGMERGE" = "x:"],[
    AM_PATH_PROG_WITH_TEST(GMSGMERGE, gmsgmerge,
      [$ac_dir/$ac_word --update -q /dev/null /dev/null >&]AS_MESSAGE_LOG_FD[ 2>&1], :)
    MSGMERGE="${GMSGMERGE}"
  ])
  AM_CONDITIONAL([BUILD_GETTEXT],[test "x$MSGMERGE" != "x" -a "x$MSGMERGE" != "x:"])

])
