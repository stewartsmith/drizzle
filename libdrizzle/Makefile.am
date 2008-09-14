# Copyright (C) 2000-2004 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 GNU General Public License as
# published by the Free Software Foundation.
#
# There are special exceptions to the terms and conditions of the GPL as it
# is applied to this software. View the full text of the exception in file
# EXCEPTIONS-CLIENT in the directory of this software distribution.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA
#
# This file is public domain and comes with NO WARRANTY of any kind

MYSQLDATAdir =			$(localstatedir)
MYSQLSHAREdir =			$(pkgdatadir)
MYSQLBASEdir=			$(prefix)

libdrizzleincludedir = ${includedir}/libdrizzle
dist_libdrizzleinclude_HEADERS =	libdrizzle.h \
					drizzle_com.h \
					errmsg.h \
					sql_common.h

lib_LTLIBRARIES = libdrizzle.la
libdrizzle_la_SOURCES = libdrizzle.c client.c local_infile.c net_serv.c get_password.c
libdrizzle_la_LIBADD =	libsqlcommon.la \
			${top_builddir}/vio/libviolt.la 
libdrizzle_la_LDFLAGS = -version-info @SHARED_LIB_VERSION@ @LD_VERSION_SCRIPT@ 

noinst_LTLIBRARIES = libsqlcommon.la
libsqlcommon_la_SOURCES = pack.c errmsg.c password.c

DEFS =			-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DDEFAULT_HOME_ENV=DRIZZLE_HOME \
			-DDEFAULT_GROUP_SUFFIX_ENV=DRIZZLE_GROUP_SUFFIX \
			-DDEFAULT_SYSCONFDIR="\"$(sysconfdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DUNDEF_THREADS_HACK

noinst_HEADERS = 	client_settings.h \
			gettext.h

EXTRA_DIST =		libdrizzle.ver
CLEANFILES =		$(target_libadd) $(SHLIBOBJS) \
			$(target) $(BUILT_SOURCES)

