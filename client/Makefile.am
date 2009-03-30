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

LDADD=			$(top_builddir)/mysys/libmysys.la \
			$(top_builddir)/mystrings/libmystrings.la \
			$(LIBDRIZZLE) $(LIBINTL) $(LIBZ)


noinst_HEADERS =	my_readline.h \
			client_priv.h \
			errname.h

CLEANFILES =		$(BUILT_SOURCES)

bin_PROGRAMS =			drizzle \
				drizzleadmin \
				drizzlecheck \
				drizzledump \
				drizzleimport \
				drizzletest \
				drizzleslap

drizzle_SOURCES=		drizzle.cc readline.cc get_password.c
drizzle_LDADD=			${LDADD} ${READLINE_LIBS}
drizzle_CXXFLAGS =		${AM_CXXFLAGS} ${W_EXCEPTIONS} 

drizzleadmin_SOURCES=		drizzleadmin.cc get_password.c

drizzlecheck_SOURCES =		drizzlecheck.cc get_password.c

drizzledump_SOURCES=            drizzledump.cc  get_password.c

drizzleimport_SOURCES=		drizzleimport.cc get_password.c

drizzleslap_SOURCES=            drizzleslap.cc get_password.c

drizzletest_SOURCES=		drizzletest.cc  get_password.c
drizzletest_LDADD=		${LDADD} ${LIBPCRE}

