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

pkglib_LTLIBRARIES = libmysqlclient.la
libmysqlclient_la_SOURCES = libmysql.c get_password.c \
                            client.c net_serv.c
libmysqlclient_la_LIBADD = ${sql_cmn_objects} ${sqlobjects} \
			   ${top_builddir}/mysys/libmysyslt.la \
			   ${top_builddir}/strings/libmystringslt.la \
			   ${top_builddir}/dbug/libdbuglt.la \
			   ${top_builddir}/sql-common/libsqlcommon.la \
			   ${top_builddir}/vio/libviolt.la 
libmysqlclient_la_LDFLAGS = -version-info @SHARED_LIB_VERSION@ @LD_VERSION_SCRIPT@ 

DEFS =			-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DDEFAULT_HOME_ENV=MYSQL_HOME \
			-DDEFAULT_GROUP_SUFFIX_ENV=MYSQL_GROUP_SUFFIX \
			-DDEFAULT_SYSCONFDIR="\"$(sysconfdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DUNDEF_THREADS_HACK

LIBS =		@CLIENT_LIBS@ 
AM_CPPFLAGS =	-I$(top_builddir)/include -I$(top_srcdir)/include -I$(top_srcdir)/vio -I$(top_srcdir)/mysys -I$(top_builddir)/sql-common

EXTRA_DIST = libmysql.def

CLEANFILES =		$(target_libadd) $(SHLIBOBJS) \
			$(target) $(BUILT_SOURCES)


# This part requires GNUmake
#
# This makes a distribution file with only the files needed to compile
# a minimal MySQL client library
#
# A list of needed headers collected from the deps information 000213
nh =		my_global.h config-win32.h dbug.h errmsg.h \
		m_ctype.h m_string.h \
		my_alarm.h my_config.h my_dir.h my_list.h my_net.h my_sys.h \
		mysql.h mysql_com.h mysql_version.h mysqld_error.h \
		mysys_err.h my_pthread.h thr_alarm.h violite.h hash.h \
                sql_common.h 

