# Copyright (C) 2000-2006 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

noinst_LTLIBRARIES =	libmystrings.la


mystringsincludedir = ${includedir}/mystrings
dist_mystringsinclude_HEADERS = 	decimal.h \
					m_ctype.h \
					m_string.h

noinst_HEADERS =	my_uctype.h \
			t_ctype.h


# IFF the don't exist on
# the target system. This helps prevent empty object file problems.
libmystrings_la_LIBADD = $(LTLIBOBJS)
libmystrings_la_SOURCES = 	strxmov.c bmove_upp.c \
				strfill.c is_prefix.c \
				strmake.c longlong2str.c \
				my_strtoll10.c \
				bchange.c strxnmov.c int2str.c str2int.c \
                                llstr.c \
				stpcpy.c \
				stpncpy.c \
                                ctype.c ctype-simple.c ctype-mb.c \
                                ctype-utf8.c \
                                ctype-uca.c ctype-bin.c \
                                decimal.c ctype-extra.c str_alloc.c \
                                dtoa.c 

