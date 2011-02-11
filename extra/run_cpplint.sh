#!/usr/bin/env bash
#  Copyright (C) 2009 Sun Microsystems, Inc.
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

# If srcdir is set, that means that we're in a distcheck build, and need to
# operate in that context.
if test "x${srcdir}" != "x" ; then
  cd ${srcdir}
fi

command="python extra/cpplint.py  --filter=-whitespace,-runtime,-readability,+legal,-build,+build/header_guard,+build/include_config,+build/namespaces"
if test "x$1" = "x" ; then
  echo "ERROR: supply a filename to line."
  exit 1
else
  $command $1
fi
retval=$?
if test ${retval} -ne 0 ; then
  echo "ERROR: cpplint found errors in the tree... please see output above"
  exit ${retval}
fi

