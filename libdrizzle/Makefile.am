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

if BUILD_DBUG
  d_dbug = $(top_builddir)/dbug/libdbuglt.la
endif

MYSQLDATAdir =			$(localstatedir)
MYSQLSHAREdir =			$(pkgdatadir)
MYSQLBASEdir=			$(prefix)

pkglib_LTLIBRARIES = libdrizzle.la
libdrizzle_la_SOURCES = libdrizzle.c get_password.c \
                            client.c net_serv.c
libdrizzle_la_LIBADD = ${sql_cmn_objects} ${sqlobjects} \
			   ${top_builddir}/mysys/libmysyslt.la \
			   ${top_builddir}/strings/libmystringslt.la \
			   $(d_dbug) \
			   ${top_builddir}/sql-common/libsqlcommon.la \
			   ${top_builddir}/vio/libviolt.la 
libdrizzle_la_LDFLAGS = -version-info @SHARED_LIB_VERSION@ @LD_VERSION_SCRIPT@ 

DEFS =			-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DDEFAULT_HOME_ENV=MYSQL_HOME \
			-DDEFAULT_GROUP_SUFFIX_ENV=MYSQL_GROUP_SUFFIX \
			-DDEFAULT_SYSCONFDIR="\"$(sysconfdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DUNDEF_THREADS_HACK

LIBS =		@CLIENT_LIBS@ 
AM_CPPFLAGS =	-I$(top_builddir)/include -I$(top_srcdir)/vio -I$(top_srcdir)/mysys -I$(top_builddir)/sql-common

noinst_HEADERS = client_settings.h

CLEANFILES =		$(target_libadd) $(SHLIBOBJS) \
			$(target) $(BUILT_SOURCES)

