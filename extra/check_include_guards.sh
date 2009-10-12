#!/bin/sh
#  Copyright (C) 2009 Sun Microsystems, Inc
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

ACK=`which ack-grep`
if test "x$ACK" = "x" ; then
  ACK=`which ack`
  if test "x$ACK" = "x" ; then
    echo "WARNING: Neither ack-grep nor ack found on your system."
    echo "WARNING: Skipping header checks."
    exit 0
  fi
fi
    
command="python extra/cpplint.py  --filter=-whitespace,-runtime,-readability,-legal,-build,+build/header_guard"
if test "x$1" = "x" ; then
  ack-grep -g '.h$' | grep -v innobase | grep -v gnulib | grep -v '\.pb\.'| grep -v bak-header | grep -v '^intl' | grep -v '^config' | grep -v '\.am$' | grep -v '\.ac$' | grep -v m4 | grep -v sql_yacc.yy | grep -v '.gperf$' | grep -v 'drizzled/probes.h' | grep -v 'drizzled/function_hash.h' | grep -v 'drizzled/symbol_hash.h' | grep -v 'util/dummy.cc' | grep -v 'drizzled/sql_yacc.h' | grep -v 'drizzled/configmake.h' | xargs $command
else
  $command $1
fi
if test $? -ne 0 ; then
  echo "ERROR: Include guards are incorrect!"
  exit $?
fi

ack-grep 'global\.h' | grep h: | grep -v _priv.h: | grep -v server_includes.h
if ! test $? ; then
  echo "ERROR: Include of global.h in non-private header."
  exit $?
else
  echo "Checked that global.h is not erroneously included."
fi

ack-grep 'server_includes\.h' | grep h:
if ! test $? ; then
  echo "ERROR: Include of server_includes.h from a header file."
  exit $?
else
  echo "Checked that server_includes.h is not erroneously included."
fi

