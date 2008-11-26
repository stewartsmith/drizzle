#  Copyright (C) 2008 Sun Microsystems
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

noinst_LTLIBRARIES = 	libfield.la

libfield_la_SOURCES = 	blob.cc \
			date.cc \
			datetime.cc \
			enum.cc \
			fdecimal.cc \
			double.cc \
			int64_t.cc \
			long.cc \
			longstr.cc \
			null.cc \
			num.cc \
			fstring.cc \
			real.cc \
			str.cc \
			timestamp.cc \
			timetype.cc \
			varstring.cc


fieldincludedir = ${includedir}/drizzled/field
nobase_dist_fieldinclude_HEADERS = \
		blob.h \
		date.h \
		datetime.h \
		enum.h \
		fdecimal.h \
		double.h \
		int64_t.h \
		long.h \
		longstr.h \
		null.h \
		num.h \
		fstring.h \
		real.h \
		str.h \
		timestamp.h \
		timetype.h \
		varstring.h


DEFS= -DDRIZZLE_SERVER @DEFS@
