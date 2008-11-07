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

AM_CPPFLAGS = ${GLOBAL_CPPFLAGS} ${PROTOBUF_CPPFLAGS}

libstrfunc_la_SOURCES = concat.cc \
		  conv.cc \
		  database.cc \
		  elt.cc \
		  format.cc \
		  insert.cc \
		  left.cc \
		  make_set.cc \
		  replace.cc \
		  reverse.cc \
		  right.cc \
		  strfunc.cc \
		  substr.cc \
		  sysconst.cc \
		  trim.cc \
		  user.cc

noinst_HEADERS = concat.h \
		 conv.h \
		 database.h \
		 elt.h \
		 format.h \
		 insert.h \
		 left.h \
		 make_set.h \
		 replace.h \
		 reverse.h \
		 right.h \
		 strfunc.h \
		 substr.h \
		 sysconst.h \
		 trim.h \
		 user.h

DEFS= -DDRIZZLE_SERVER @DEFS@
