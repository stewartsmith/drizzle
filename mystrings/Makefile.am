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


libmystrings_la_SOURCES = 	strxmov.cc bmove_upp.cc \
				strfill.cc is_prefix.cc \
				longlong2str.cc \
				my_strtoll10.cc \
				bchange.cc int2str.cc str2int.cc \
                                llstr.cc \
				stpncpy.cc \
                                ctype.cc ctype-simple.cc ctype-mb.cc \
                                ctype-utf8.cc \
                                ctype-uca.cc ctype-bin.cc \
                                decimal.cc ctype-extra.cc str_alloc.cc \
                                dtoa.cc 

