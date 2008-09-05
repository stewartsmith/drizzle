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

LDADD=			$(top_builddir)/libdrizzle/libdrizzle.la \
			$(LIBINTL) -lz


noinst_HEADERS =	completion_hash.h \
			my_readline.h \
			client_priv.h \
			errname.h

CLEANFILES =		$(BUILT_SOURCES)

bin_PROGRAMS =	drizzle \
				drizzleadmin \
				drizzlebinlog \
				drizzlecheck \
				drizzledump \
				drizzleimport \
				drizzletest \
				drizzleslap

drizzle_SOURCES =		drizzle.cc readline.cc completion_hash.cc
drizzle_LDADD =			$(READLINE_LIBS) $(TERMCAP_LIBS) \
				$(LDADD) $(CXXLDFLAGS)
drizzle_CXXFLAGS =		${AM_CXXFLAGS} -fexceptions
drizzleadmin_SOURCES =		drizzleadmin.cc

drizzlebinlog_SOURCES =     drizzlebinlog.cc \
                $(top_srcdir)/mysys/mf_tempdir.c \
                $(top_srcdir)/mysys/my_bit.c \
                $(top_srcdir)/mysys/my_bitmap.c \
                $(top_srcdir)/mysys/base64.c

drizzlecheck_SOURCES =		drizzlecheck.cc
drizzledump_SOURCES=            drizzledump.cc 

drizzleimport_SOURCES=		drizzleimport.cc

drizzleslap_SOURCES=            drizzleslap.cc

drizzletest_SOURCES=		drizzletest.cc 
drizzletest_LDADD =		${LDADD} $(PCRE_LIBS)
drizzletest_CXXFLAGS = 		${AM_CXXFLAGS} $(PCRE_CFLAGS)

DEFS =	-DDEFAULT_DRIZZLE_HOME="\"$(prefix)\"" \
	-DDATADIR="\"$(localstatedir)\"" \
	-DLOCALEDIR=\"$(localedir)\" @DEFS@
