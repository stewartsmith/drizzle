#!/bin/sh

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

for file in `ack-grep -f | grep -v innobase | grep -v gnulib | grep -v '\.pb\.'| grep -v bak-header | grep -v '^intl' | grep -v '^config' | grep -v '\.am$' | grep -v '\.ac$' | grep -v m4 | grep -v sql_yacc.yy | grep -v '.gperf$' | grep -v 'drizzled/probes.h' | grep -v 'util/dummy.cc'` ; do
	grep Copyright $file >/dev/null 2>&1 || echo "No copyright header in $file"
	uncrustify -c config/uncrustify.cfg --replace $file
done

