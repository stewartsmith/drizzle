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

AM_CPPFLAGS =			-I$(top_builddir)/include -I$(top_srcdir)/include \
									-I$(top_srcdir)/server

LDADD =						$(top_builddir)/mysys/libmysyslt.la \
            			$(top_builddir)/strings/libmystringslt.la \
									-lz

BUILT_SOURCES=		$(top_builddir)/include/mysqld_error.h \
                  $(top_builddir)/include/sql_state.h \
                  $(top_builddir)/include/mysqld_ername.h

pkginclude_HEADERS=	$(BUILT_SOURCES)

DISTCLEANFILES =	$(BUILT_SOURCES) comp_err

SUBDIRS =		

DIST_SUBDIRS =		

# This will build mysqld_error.h, mysqld_ername.h and sql_state.h
# NOTE Built files should depend on their sources to avoid
# the built files being rebuilt in source dist
$(top_builddir)/include/mysqld_error.h: comp_err.c \
			$(top_srcdir)/server/share/errmsg.txt
	$(MAKE) $(AM_MAKEFLAGS) comp_err$(EXEEXT)
	$(top_builddir)/extra/comp_err$(EXEEXT) \
	--charset=$(top_srcdir)/server/share/charsets \
	--out-dir=$(top_builddir)/server/share/ \
	--header_file=$(top_builddir)/include/mysqld_error.h \
	--name_file=$(top_builddir)/include/mysqld_ername.h \
	--state_file=$(top_builddir)/include/sql_state.h \
	--in_file=$(top_srcdir)/server/share/errmsg.txt

$(top_builddir)/include/mysqld_ername.h: $(top_builddir)/include/mysqld_error.h
$(top_builddir)/include/sql_state.h: $(top_builddir)/include/mysqld_error.h

bin_PROGRAMS =			my_print_defaults \
										mysql_waitpid \
										innochecksum

noinst_PROGRAMS =	

EXTRA_PROGRAMS =	comp_err
