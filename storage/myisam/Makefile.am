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

if BUILD_DBUG
  d_dbug = $(top_builddir)/dbug/libdbuglt.la
endif

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir =          $(prefix)
MYSQLLIBdir =           $(pkglibdir)
AM_CPPFLAGS =           -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/server \
                        -I$(srcdir)
WRAPLIBS=

LDADD =

DEFS =                  @DEFS@

EXTRA_DIST =		mi_test_all.sh mi_test_all.res plug.in
pkgdata_DATA =		mi_test_all mi_test_all.res

noinst_LTLIBRARIES =	libmyisam.la libmyisamutil.la \
			libmyisamha.la libmyisamnone.la
bin_PROGRAMS =		myisamchk myisamlog myisampack
myisamchk_LDADD=	libmyisamutil.la \
			libmyisamnone.la \
			$(top_builddir)/mysys/libmysyslt.la \
			$(d_dbug) \
			$(top_builddir)/strings/libmystringslt.la -lz
myisamlog_LDADD=	libmyisamutil.la \
			libmyisamnone.la \
			$(top_builddir)/mysys/libmysyslt.la \
			$(d_dbug) \
			$(top_builddir)/strings/libmystringslt.la -lz
myisampack_LDADD=	libmyisamutil.la \
			libmyisamnone.la \
			$(top_builddir)/mysys/libmysyslt.la \
			$(d_dbug) \
			$(top_builddir)/strings/libmystringslt.la -lz
noinst_PROGRAMS =	mi_test1 mi_test2 mi_test3
noinst_HEADERS =	myisamdef.h \
			ha_myisam.h
mi_test1_LDADD=		libmyisamutil.la \
			libmyisamnone.la \
			$(top_builddir)/mysys/libmysyslt.la \
			$(d_dbug) \
			$(top_builddir)/strings/libmystringslt.la -lz
mi_test2_LDADD=		libmyisamutil.la \
			libmyisamnone.la \
			$(top_builddir)/mysys/libmysyslt.la \
			$(d_dbug) \
			$(top_builddir)/strings/libmystringslt.la -lz
mi_test3_LDADD=		libmyisamutil.la \
			libmyisamnone.la \
			$(top_builddir)/mysys/libmysyslt.la \
			$(d_dbug) \
			$(top_builddir)/strings/libmystringslt.la -lz
libmyisamutil_la_SOURCES = mi_open.c mi_extra.c mi_info.c mi_rkey.c \
			mi_rnext.c mi_rnext_same.c \
			mi_search.c mi_page.c mi_key.c mi_locking.c \
			mi_rrnd.c mi_scan.c mi_cache.c \
			mi_statrec.c mi_packrec.c mi_dynrec.c \
			mi_update.c mi_write.c mi_unique.c \
			mi_delete.c \
			my_pread.c my_lock.c \
			mf_keycache.c mf_keycaches.c \
			mi_rprev.c mi_rfirst.c mi_rlast.c mi_rsame.c \
			mi_rsamepos.c mi_panic.c mi_close.c mi_create.c\
			mi_range.c mi_dbug.c mi_checksum.c mi_log.c \
			mi_changed.c mi_static.c mi_delete_all.c \
			mi_delete_table.c mi_rename.c  mi_check.c \
			mi_keycache.c mi_preload.c \
			sort.c 
libmyisamha_la_SOURCES = ha_myisam.cc
libmyisamnone_la_SOURCES = mi_reportnone.c
libmyisam_la_SOURCES = 
libmyisam_la_LIBADD = libmyisamha.la libmyisamutil.la

CLEANFILES =		test?.MY? FT?.MY? isam.log mi_test_all

# Move to automake rules ?
prolint:;	plparse -b -u -hF1 "-width(0,0)" "-format=%f:%l:\s%t:%n\s%m" \
		"-elib(????)" "+elib(?3??)" my.lnt $(nisam_SOURCES)

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''bindir''@!$(bindir)!g' \
	  -e 's!@''scriptdir''@!$(bindir)!g' \
	  -e 's!@''prefix''@!$(prefix)!g' \
	  -e 's!@''datadir''@!$(datadir)!g' \
	  -e 's!@''localstatedir''@!$(localstatedir)!g' \
	  -e 's!@''libexecdir''@!$(libexecdir)!g' \
	  -e 's!@''CC''@!@CC@!'\
	  -e 's!@''CXX''@!@CXX@!'\
	  -e 's!@''GXX''@!@GXX@!'\
	  -e 's!@''PERL''@!@PERL@!' \
	  -e 's!@''CFLAGS''@!@SAVE_CFLAGS@!'\
	  -e 's!@''CXXFLAGS''@!@SAVE_CXXFLAGS@!'\
	  -e 's!@''LDFLAGS''@!@SAVE_LDFLAGS@!'\
	  -e 's!@''VERSION''@!@VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
	  -e 's!@''COMPILATION_COMMENT''@!@COMPILATION_COMMENT@!' \
	  -e 's!@''MACHINE_TYPE''@!@MACHINE_TYPE@!' \
	  -e 's!@''HOSTNAME''@!@HOSTNAME@!' \
	  -e 's!@''SYSTEM_TYPE''@!@SYSTEM_TYPE@!' \
	  -e 's!@''CHECK_PID''@!@CHECK_PID@!' \
	  -e 's!@''FIND_PROC''@!@FIND_PROC@!' \
	  -e 's!@''MYSQLD_DEFAULT_SWITCHES''@!@MYSQLD_DEFAULT_SWITCHES@!' \
	  -e 's!@''MYSQL_UNIX_ADDR''@!@MYSQL_UNIX_ADDR@!' \
	  -e 's!@''TARGET_LINUX''@!@TARGET_LINUX@!' \
	  -e "s!@""CONF_COMMAND""@!@CONF_COMMAND@!" \
	  -e 's!@''MYSQLD_USER''@!@MYSQLD_USER@!' \
	  -e 's!@''sysconfdir''@!@sysconfdir@!' \
	  -e 's!@''SHORT_MYSQL_INTRO''@!@SHORT_MYSQL_INTRO@!' \
	  -e 's!@''SHARED_LIB_VERSION''@!@SHARED_LIB_VERSION@!' \
	  -e 's!@''MYSQL_BASE_VERSION''@!@MYSQL_BASE_VERSION@!' \
	  -e 's!@''MYSQL_NO_DASH_VERSION''@!@MYSQL_NO_DASH_VERSION@!' \
	  -e 's!@''MYSQL_TCP_PORT''@!@MYSQL_TCP_PORT@!' \
	  -e 's!@''PERL_DBI_VERSION''@!@PERL_DBI_VERSION@!' \
	  -e 's!@''PERL_DBD_VERSION''@!@PERL_DBD_VERSION@!' \
	  -e 's!@''PERL_DATA_DUMPER''@!@PERL_DATA_DUMPER@!' \
	$< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

