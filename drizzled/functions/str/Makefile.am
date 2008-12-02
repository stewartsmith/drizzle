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

noinst_LTLIBRARIES = 	libstrfunc.la

AM_CPPFLAGS = ${GLOBAL_CPPFLAGS} ${PROTOBUF_CFLAGS}

libstrfunc_la_SOURCES = alloc_buffer.cc \
		 binary.cc \
		 char.cc \
		 charset.cc \
		 collation.cc \
		 concat.cc \
		 conv.cc \
		 conv_charset.cc \
		 database.cc \
		 elt.cc \
		 export_set.cc \
		 format.cc \
		 hex.cc \
		 insert.cc \
		 left.cc \
		 load_file.cc \
		 make_set.cc \
		 pad.cc \
		 quote.cc \
		 repeat.cc \
		 replace.cc \
		 reverse.cc \
		 right.cc \
		 set_collation.cc \
		 str_conv.cc \
		 strfunc.cc \
		 substr.cc \
		 sysconst.cc \
		 trim.cc \
		 user.cc \
		 uuid.cc \
		 weight_string.cc

functionsincludedir = ${includedir}/drizzled/functions/str
nobase_dist_functionsinclude_HEADERS = \
		alloc_buffer.h \
		binary.h \
		char.h \
		charset.h \
		collation.h \
		concat.h \
		conv.h \
		conv_charset.h \
		database.h \
		elt.h \
		export_set.h \
		format.h \
		hex.h \
		insert.h \
		left.h \
		load_file.h \
		make_set.h \
		pad.h \
		quote.h \
		repeat.h \
		replace.h \
		reverse.h \
		right.h \
		set_collation.h \
		str_conv.h \
		strfunc.h \
		substr.h \
		sysconst.h \
		trim.h \
		user.h \
		uuid.h \
		weight_string.h

DEFS= -DDRIZZLE_SERVER @DEFS@
