#  Copyright (C) 2008 Sun Microsystems
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

MYSQLDATAdir =$(localstatedir)

MYSQLSHAREdir =$(pkgdatadir)

MYSQLBASEdir=$(prefix)

MYSQLLIBdir=            $(pkglibdir)

pkgplugindir =$(pkglibdir)/plugin

SUBDIRS = util field serialize functions .

sbin_PROGRAMS =drizzled

EXTRA_PROGRAMS =gen_lex_hash

bin_PROGRAMS =

bin_SCRIPTS = 	drizzledumpslow \
		drizzled_safe 

AM_CPPFLAGS = ${GLOBAL_CPPFLAGS} ${LIBEVENT_CPPFLAGS} 

DTRACE = @DTRACE@

DTRACEFLAGS = @DTRACEFLAGS@

DTRACEFILES = handler.o \
	      filesort.o \
	      sql_delete.o \
	      sql_insert.o \
	      sql_select.o \
	      sql_update.o

drizzledincludedir = ${includedir}/drizzled
dist_drizzledinclude_HEADERS =	common.h \
				error.h \
				korr.h \
				plugin.h \
				plugin_authentication.h \
				plugin_configvar.h \
				plugin_errmsg.h \
				plugin_logging.h \
				plugin_qcache.h \
				plugin_parser.h \
				plugin_scheduling.h

noinst_LTLIBRARIES = libhandler.la libyacc.la

LDADD = $(top_builddir)/libdrizzle/libdrizzle.la \
	$(top_builddir)/mysys/libmysys.la \
	$(top_builddir)/mystrings/libmystrings.la \
	$(ZLIB_LIBS) $(LIBINTL)

drizzled_LDADD = libhandler.la libyacc.la \
		 $(top_builddir)/libdrizzle/libdrizzle.la \
		 $(top_builddir)/drizzled/field/libfield.la \
		 $(top_builddir)/drizzled/serialize/libserialize.la \
		 $(top_builddir)/drizzled/functions/libfunctions.la \
		 $(libevent_libs) \
		 @mysql_plugin_libs@ \
		 -lprotobuf \
		 $(LDADD) \
		 $(CXXLDFLAGS) \
		 $(LIBDL_LIBS) $(LIBEVENT_LIBS)
drizzled_DEPENDENCIES = @mysql_plugin_libs@

drizzled_LDFLAGS =

noinst_HEADERS = \
		authentication.h \
		base.h \
		client_settings.h \
		common_includes.h \
		configvar.h \
		connect.h \
		data_home.h \
		definitions.h \
		errmsg.h \
		error_injection.h \
		field.h \
		field_iterator.h \
		filesort_info.h \
		gettext.h \
		global.h \
		handler.h \
		item_cmpfunc.h \
		item_create.h \
		item_func.h \
	       	item.h \
		item_row.h \
		item_strfunc.h \
		item_subselect.h \
		item_sum.h \
		item_timefunc.h \
                key_map.h \
		lex.h \
		lex_symbol.h \
		log_event.h \
		logging.h \
		log.h \
		my_decimal.h\
		opt_range.h \
		order.h \
		parser.h \
		probes.h \
		protocol.h \
		qcache.h \
		query_id.h \
		rpl_constants.h \
		rpl_filter.h \
		rpl_mi.h \
		rpl_record.h \
		rpl_reporting.h \
		rpl_rli.h \
		rpl_tblmap.h \
		rpl_utility.h \
		scheduler.h \
		scheduling.h \
		server_includes.h \
		set_var.h \
		show.h \
		sj_tmp_table.h \
		slave.h \
		sql_alloc.h \
		sql_array.h \
		sql_bitmap.h \
		sql_class.h \
		sql_error.h \
		sql_lex.h \
		sql_list.h \
		sql_locale.h \
		sql_parse.h \
		sql_plugin.h \
		sql_repl.h \
		sql_select.h \
		sql_show.h \
		sql_sort.h \
		sql_state.h \
		sql_string.h \
		sql_table.h \
		sql_udf.h \
		stacktrace.h \
		structs.h \
		table.h \
		table_list.h \
		table_map_iterator.h \
		tmp_table.h \
		tzfile.h \
		tztime.h

drizzled_SOURCES = \
		authentication.cc \
		configvar.cc \
		discover.cc \
		drizzled.cc \
		errmsg.cc \
		error.cc \
		field.cc \
		field_conv.cc \
		field_iterator.cc \
		filesort.cc \
		init.cc \
		item_buff.cc \
		item.cc \
		item_cmpfunc.cc \
		item_create.cc \
		item_func.cc \
		item_row.cc \
		item_strfunc.cc \
		item_subselect.cc \
		item_sum.cc \
		item_timefunc.cc \
		key.cc \
                key_map.cc \
		lock.cc \
		log.cc \
		log_event.cc \
		logging.cc \
		mf_iocache.cc \
		my_decimal.cc\
		natural_join_column.cc \
		opt_range.cc \
		opt_sum.cc \
		parser.cc \
		protocol.cc \
		qcache.cc \
		query_id.cc \
		records.cc \
		rpl_filter.cc \
		rpl_mi.cc \
		rpl_record.cc \
		rpl_reporting.cc \
		rpl_rli.cc \
		rpl_tblmap.cc \
		rpl_utility.cc \
		scheduler.cc \
                scheduling.cc \
		set_var.cc \
		sj_tmp_table.cc \
		slave.cc \
		sql_base.cc \
		sql_binlog.cc \
		sql_builtin.cc \
		sql_class.cc \
		sql_client.cc \
		sql_connect.cc \
		sql_db.cc \
		sql_delete.cc \
		sql_derived.cc \
		sql_error.cc \
		sql_handler.cc \
		sql_insert.cc \
		sql_lex.cc \
		sql_list.cc \
		sql_load.cc \
		sql_locale.cc \
		sql_olap.cc \
		sql_parse.cc \
		sql_plugin.cc \
		sql_rename.cc \
		sql_repl.cc \
		sql_select.cc \
		sql_show.cc \
		sql_state.cc \
		sql_string.cc \
		sql_table.cc \
		sql_udf.cc \
		sql_union.cc \
		sql_update.cc \
		stacktrace.cc \
		strfunc.cc \
		table.cc \
                table_map_iterator.cc \
		thr_malloc.cc \
		time.cc \
		tmp_table.cc \
		tztime.cc \
		uniques.cc \
		unireg.cc

libhandler_la_SOURCES = handler.cc
libyacc_la_SOURCES = sql_yacc.yy
# Code output from YACC contains GOBS of unused macros, and I can't
# figure out how to turn it off.
libyacc_la_CXXFLAGS = ${AM_CXXFLAGS} -Wno-unused-macros


if HAVE_DTRACE
drizzled_SOURCES += probes.d
endif

gen_lex_hash_SOURCES =gen_lex_hash.cc

gen_lex_hash_LDFLAGS =  

DEFS =-DDRIZZLE_SERVER \
      -DDEFAULT_DRIZZLE_HOME="\"$(MYSQLBASEdir)\"" \
      -DDATADIR="\"$(MYSQLDATAdir)\"" \
      -DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
      -DPLUGINDIR="\"$(pkgplugindir)\"" \
      -DLOCALEDIR=\"$(localedir)\" \
      @DEFS@

if GCC_PCH
PCHHEADERS =
else
PCHHEADERS =
endif

BUILT_MAINT_SRC = sql_yacc.cc \
		  sql_yacc.h

BUILT_SOURCES = $(BUILT_MAINT_SRC) \
		lex_hash.h \
		$(PCHHEADERS)

EXTRA_DIST = $(BUILT_MAINT_SRC) \
	     drizzledumpslow \
	     message.mc \
	     probes.d

CLEANFILES = lex_hash.h \
	     sql_yacc.output \
	     $(nodist_drizzled_SOURCES)

DISTCLEANFILES = $(EXTRA_PROGRAMS) \
		 $(BUILT_MAINT_SRC)

MAINTAINERCLEANFILES = $(BUILT_MAINT_SRC)

AM_YFLAGS = -d --verbose


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

if GCC_PCH
%.h.gch: %.h
	$(CXXCOMPILE) -include config.h -c $<
endif

SUFFIXES = .d

.d.o : $(DTRACEFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $< $(DTRACEFILES)
