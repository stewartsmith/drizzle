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

MYSQLDATAdir =$(localstatedir)

MYSQLSHAREdir =$(pkgdatadir)

MYSQLBASEdir=$(prefix)

MYSQLLIBdir=            $(pkglibdir)

pkgplugindir =$(pkglibdir)/plugin

SUBDIRS = field . share

libexec_PROGRAMS =drizzled

EXTRA_PROGRAMS =gen_lex_hash

bin_PROGRAMS =

bin_SCRIPTS = 	drizzledumpslow \
		drizzled_safe 

AM_CPPFLAGS = ${GLOBAL_CPPFLAGS} ${LIBEVENT_CPPFLAGS}

DTRACE = @DTRACE@

DTRACEFLAGS = @DTRACEFLAGS@

DTRACEFILES = handler.o \
	      filesort.o \
	      sql_insert.o \
	      sql_delete.o \
	      sql_select.o \
	      sql_update.o

pkginclude_HEADERS =	error.h \
			plugin.h \
			plugin_audit.h

noinst_LTLIBRARIES = libhandler.la

LDADD = $(top_builddir)/vio/libviolt.la \
	$(top_builddir)/libdrizzle/libsqlcommon.la \
	$(top_builddir)/mysys/libmysyslt.la \
	$(top_builddir)/mystrings/libmystringslt.la \
	$(ZLIB_LIBS)

drizzled_LDADD = libhandler.la \
		 $(top_builddir)/libdrizzle/libdrizzle.la \
		 $(top_builddir)/drizzled/field/libfield.la \
		 $(libevent_libs) \
		 @mysql_plugin_libs@ \
		 $(LDADD) \
		 $(CXXLDFLAGS) \
		 $(LIBDL_LIBS) $(LIBEVENT_LIBS)

noinst_HEADERS = item.h \
		 item_func.h \
		 item_sum.h \
		 item_cmpfunc.h \
		 item_strfunc.h \
		 item_timefunc.h \
		 item_create.h \
		 item_subselect.h \
		 item_row.h \
		 mysql_priv.h \
		 sql_bitmap.h \
		 sql_class.h \
		 sql_lex.h \
		 sql_list.h \
		 sql_map.h \
		 sql_state.h \
		 sql_string.h \
		 unireg.h \
		 sql_error.h \
		 field.h \
		 handler.h \
		 mysqld_suffix.h \
		 rpl_constants.h \
		 opt_range.h \
		 protocol.h \
		 rpl_tblmap.h \
		 rpl_utility.h \
		 rpl_reporting.h \
		 log.h \
		 sql_show.h \
		 rpl_rli.h \
		 rpl_mi.h \
		 sql_select.h \
		 structs.h \
		 table.h \
		 sql_udf.h \
		 lex.h \
		 lex_symbol.h \
		 sql_crypt.h  \
		 sql_repl.h \
		 slave.h \
		 rpl_filter.h \
		 log_event.h \
		 rpl_record.h \
		 stacktrace.h \
		 sql_sort.h \
		 set_var.h \
		 tzfile.h \
		 client_settings.h \
		 tztime.h \
		 my_decimal.h\
		 sql_array.h \
		 scheduler.h \
		 sql_plugin.h \
		 global.h \
		 base.h \
		 probes.h

drizzled_SOURCES = sql_lex.cc \
		   sql_handler.cc \
		   item.cc \
		   item_sum.cc \
		   item_buff.cc \
		   item_func.cc \
		   item_cmpfunc.cc \
		   item_strfunc.cc \
		   item_timefunc.cc \
		   thr_malloc.cc \
		   item_create.cc \
		   item_subselect.cc \
		   item_row.cc \
		   field.cc \
		   strfunc.cc \
		   key.cc \
		   sql_class.cc \
		   sql_list.cc \
		   protocol.cc \
		   sql_state.c \
		   lock.cc \
		   sql_string.cc \
		   sql_map.cc \
		   drizzled.cc \
		   sql_connect.cc \
		   scheduler.cc \
		   sql_parse.cc \
		   set_var.cc \
		   sql_yacc.yy \
		   sql_base.cc \
		   table.cc \
		   sql_select.cc \
		   sql_insert.cc \
		   sql_error.cc \
		   sql_locale.cc \
		   sql_update.cc \
		   sql_delete.cc \
		   uniques.cc \
		   log.cc \
		   init.cc \
		   derror.cc \
		   unireg.cc \
		   log_event.cc \
		   rpl_record.cc \
		   discover.cc \
		   time.cc \
		   opt_range.cc \
		   opt_sum.cc \
		   records.cc \
		   filesort.cc \
		   sql_db.cc \
		   sql_table.cc \
		   sql_rename.cc \
		   sql_crypt.cc \
		   sql_load.cc \
		   mf_iocache.cc \
		   field_conv.cc \
		   sql_show.cc \
		   sql_udf.cc \
		   slave.cc \
		   sql_repl.cc \
		   rpl_filter.cc \
		   rpl_tblmap.cc \
		   rpl_utility.cc \
		   rpl_rli.cc \
		   rpl_mi.cc \
		   rpl_reporting.cc \
		   sql_union.cc \
		   sql_derived.cc \
		   sql_client.cc \
		   stacktrace.c \
		   repl_failsafe.h \
		   repl_failsafe.cc \
		   sql_olap.cc \
		   tztime.cc \
		   my_decimal.cc\
		   sql_plugin.cc \
		   sql_binlog.cc \
		   sql_builtin.cc 

libhandler_la_SOURCES = handler.cc

if HAVE_DTRACE
drizzled_SOURCES += probes.d
endif

gen_lex_hash_SOURCES =gen_lex_hash.cc

gen_lex_hash_LDFLAGS =  

DEFS =-DMYSQL_SERVER \
      -DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
      -DDATADIR="\"$(MYSQLDATAdir)\"" \
      -DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
      -DPLUGINDIR="\"$(pkgplugindir)\"" \
      -DLOCALEDIR=\"$(localedir)\" \
      @DEFS@

BUILT_MAINT_SRC = sql_yacc.cc \
		  sql_yacc.h

BUILT_SOURCES = $(BUILT_MAINT_SRC) \
		lex_hash.h \
		version.h

EXTRA_DIST = $(BUILT_MAINT_SRC) \
	     message.mc \
	     probes.d

CLEANFILES = lex_hash.h \
	     sql_yacc.output \
	     $(nodist_drizzled_SOURCES)

DISTCLEANFILES = $(EXTRA_PROGRAMS) \
		 $(BUILT_MAINT_SRC)

MAINTAINERCLEANFILES = $(BUILT_MAINT_SRC)

AM_YFLAGS = -d --verbose


# These files should not be included in distributions since they are
# generated by configure from the .h.in files
dist-hook:
	$(RM) -f $(distdir)/drizzle_version.h 

# This generates lex_hash.h
# NOTE Built sources should depend on their sources not the tool
# this avoid the rebuild of the built files in a source dist
lex_hash.h: gen_lex_hash$(EXEEXT) \
	$(srcdir)/lex.h
	$(top_builddir)/drizzled/gen_lex_hash$(EXEEXT) > $@

probes.h: probes.d
	$(DTRACE) $(DTRACEFLAGS) -h -s probes.d
	mv probes.h probes.h.bak
	sed "s/#include <unistd.h>//g" probes.h.bak > probes.h
	rm probes.h.bak

SUFFIXES = .d

.d.o : $(DTRACEFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $< $(DTRACEFILES)
