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

#called from the top level Makefile

AM_CPPFLAGS =	-I$(top_srcdir)/strings \
		-I$(top_srcdir)/drizzled \
		-I$(top_srcdir)/include \
		-I$(top_srcdir)

noinst_LTLIBRARIES = 	libfield.la

libfield_la_SOURCES = 	blob.cc \
			date.cc \
			datetime.cc \
			fdecimal.cc \
			double.cc \
			int64_t.cc \
			long.cc \
			null.cc \
			set.cc \
			short.cc \
			fstring.cc \
			timestamp.cc \
			timetype.cc \
			varstring.cc \
			year.cc

noinst_HEADERS = blob.h \
		 date.h \
		 datetime.h \
		 fdecimal.h \
		 double.h \
		 int64_t.h \
		 long.h \
		 null.h \
		 set.h \
		 short.h \
		 fstring.h \
		 timestamp.h \
		 timetype.h \
		 varstring.h \
		 year.h
	

DEFS= -DMYSQL_SERVER @DEFS@
