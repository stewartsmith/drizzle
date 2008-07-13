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

# This file is public domain and comes with NO WARRANTY of any kind

AM_CPPFLAGS =			-I$(top_builddir)/include \
				-I$(top_srcdir)/include \
				-I$(top_srcdir)/regex \
				-I$(top_srcdir)/server \
				-I$(top_srcdir)/strings

LDADD=				$(top_builddir)/libdrizzle/libdrizzle.la -lz


noinst_HEADERS =		completion_hash.h my_readline.h \
				client_priv.h

CLEANFILES =			$(BUILT_SOURCES)

bin_PROGRAMS =			mysql \
				mysqladmin \
				mysqlbinlog \
				mysqlcheck \
				mysqldump \
				mysqlimport \
				mysqltest \
				mysqlslap 

mysql_SOURCES =			mysql.c readline.c completion_hash.c
mysql_LDADD =			-lreadline @TERMCAP_LIB@ $(GLIB_LIBS) \
				$(LDADD) $(CXXLDFLAGS)
mysql_CPPFLAGS = 		$(AM_CPPFLAGS) $(GLIB_CFLAGS)
mysqladmin_SOURCES =		mysqladmin.cc

mysqlbinlog_SOURCES =		mysqlbinlog.cc \
				$(top_srcdir)/mysys/mf_tempdir.c \
				$(top_srcdir)/mysys/my_new.cc \
				$(top_srcdir)/mysys/my_bit.c \
				$(top_srcdir)/mysys/my_bitmap.c \
				$(top_srcdir)/mysys/base64.c 
mysqlbinlog_LDADD =		$(LDADD) $(CXXLDFLAGS) 

mysqldump_SOURCES=              mysqldump.c 

mysqlimport_SOURCES=		mysqlimport.c

mysqlslap_SOURCES=		mysqlslap.c
mysqlslap_CFLAGS=		-DTHREAD -UUNDEF_THREADS_HACK

mysqltest_SOURCES=		mysqltest.cc 
mysqltest_LDADD =		${LDADD} @PCRE_LIBS@
mysqltest_CFLAGS = 		${AM_CPPFLAGS} @PCRE_CFLAGS@

# Fix for mit-threads
DEFS =			-DUNDEF_THREADS_HACK \
			-DDEFAULT_MYSQL_HOME="\"$(prefix)\"" \
			-DDATADIR="\"$(localstatedir)\""
