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

SUBDIRS = util field item functions functions/str functions/time .

sbin_PROGRAMS =drizzled

EXTRA_PROGRAMS =gen_lex_hash

bin_PROGRAMS =

bin_SCRIPTS = 	drizzledumpslow \
		drizzled_safe 

AM_CPPFLAGS = ${GLOBAL_CPPFLAGS} ${EVENT_CFLAGS} ${PROTOBUF_CFLAGS}

DTRACE = @DTRACE@

DTRACEFLAGS = @DTRACEFLAGS@

DTRACEFILES = handler.o \
	      filesort.o \
	      sql_delete.o \
	      sql_insert.o \
	      sql_select.o \
	      sql_update.o

drizzledincludedir = ${includedir}/drizzled
nobase_dist_drizzledinclude_HEADERS = \
		alter_column.h \
		alter_drop.h \
		authentication.h \
		base.h \
		cached_item.h \
		check_stack_overrun.h \
		client_settings.h \
		current_session.h \
		common.h \
		comp_creator.h \
		configvar.h \
		connect.h \
		cost_vect.h \
		data_home.h \
		db.h \
		definitions.h \
		dtcollation.h \
		errmsg.h \
		error.h \
		error_injection.h \
		field.h \
		field_iterator.h \
		filesort_info.h \
		foreign_key.h \
		ha_trx_info.h \
		ha_statistics.h \
		handler.h \
		handler_structs.h \
		hybrid_type.h \
		hybrid_type_traits.h \
		hybrid_type_traits_decimal.h \
		hybrid_type_traits_integer.h \
	       	item.h \
		key.h \
                key_map.h \
		key_part_spec.h \
		korr.h \
		lex.h \
		lex_string.h \
		lex_symbol.h \
		lock.h \
		log_event.h \
		logging.h \
		log.h \
		my_decimal.h\
		name_resolution_context.h \
		name_resolution_context_state.h \
		opt_range.h \
		order.h \
		parser.h \
		plugin.h \
		plugin_authentication.h \
		plugin_configvar.h \
		plugin_replicator.h \
		plugin_errmsg.h \
		plugin_logging.h \
		plugin_qcache.h \
		plugin_parser.h \
	 	plugin_scheduling.h \
		probes.h \
		protocol.h \
		qcache.h \
		query_id.h \
		rename.h \
		replication/binlog.h \
		replication/constants.h \
		replication/mi.h \
		replication/record.h \
		replication/replication.h \
		replication/reporting.h \
		replication/rli.h \
		replication/tblmap.h \
		replication/utility.h \
		replicator.h \
		scheduler.h \
		scheduling.h \
		session.h \
		set_var.h \
		show.h \
		sj_tmp_table.h \
		slave.h \
		sql_alloc.h \
		sql_array.h \
		sql_base.h \
		sql_bitmap.h \
		sql_error.h \
		sql_lex.h \
		sql_list.h \
		sql_load.h \
		sql_locale.h \
		sql_parse.h \
		sql_plugin.h \
		sql_select.h \
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
		tableop_hooks.h \
		tmp_table.h \
		tzfile.h \
		tztime.h \
		virtual_column_info.h \
		xid.h

noinst_LTLIBRARIES = libhandler.la libyacc.la

LDADD = $(top_builddir)/libdrizzle/libdrizzle.la \
	$(top_builddir)/mysys/libmysys.la \
	$(top_builddir)/mystrings/libmystrings.la \
	$(Z_LIBS) $(LIBINTL) 

drizzled_LDADD = libhandler.la libyacc.la \
		 $(top_builddir)/libdrizzle/libdrizzle.la \
		 $(top_builddir)/drizzled/item/libitem.la \
		 $(top_builddir)/drizzled/field/libfield.la \
		 $(top_builddir)/drizzled/serialize/libserialize.la \
		 $(top_builddir)/drizzled/functions/libfunctions.la \
		 $(top_builddir)/drizzled/functions/str/libstrfunc.la \
		 $(top_builddir)/drizzled/functions/time/libtimefunc.la \
		 $(top_builddir)/drizzled/util/libutil.la \
		 $(libevent_libs) \
		 @mysql_plugin_libs@ \
		 -lprotobuf \
		 $(LDADD) \
		 $(CXXLDFLAGS) \
		 $(LIBDL_LIBS) $(EVENT_LIBS) $(PROTOBUF_LIBS)
drizzled_DEPENDENCIES = @mysql_plugin_libs@ libyacc.la libhandler.la

drizzled_LDFLAGS =

noinst_HEADERS = \
		gettext.h \
		global.h \
		server_includes.h

drizzled_SOURCES = \
		authentication.cc \
		cached_item.cc \
		check_stack_overrun.cc \
		comp_creator.cc \
		configvar.cc \
		current_session.cc \
		db.cc \
		discover.cc \
		drizzled.cc \
		dtcollation.cc \
		errmsg.cc \
		error.cc \
		field.cc \
		field_conv.cc \
		field_iterator.cc \
		filesort.cc \
		ha_trx_info.cc \
		hybrid_type_traits.cc \
		hybrid_type_traits_decimal.cc \
		hybrid_type_traits_integer.cc \
		item.cc \
		key.cc \
                key_map.cc \
		lock.cc \
		log.cc \
		log_event.cc \
		logging.cc \
		mf_iocache.cc \
		my_decimal.cc\
		name_resolution_context_state.cc \
		natural_join_column.cc \
		opt_range.cc \
		opt_sum.cc \
		parser.cc \
		protocol.cc \
		qcache.cc \
		query_id.cc \
		records.cc \
		rename.cc \
		replication/binlog.cc \
		replication/mi.cc \
		replication/record.cc \
		replication/replication.cc \
		replication/reporting.cc \
		replication/rli.cc \
		replication/tblmap.cc \
		replication/utility.cc \
		replicator.cc \
		scheduler.cc \
                scheduling.cc \
		session.cc \
		set_var.cc \
		show.cc \
		sj_tmp_table.cc \
		slave.cc \
		sql_base.cc \
		sql_builtin.cc \
		sql_client.cc \
		sql_connect.cc \
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
		sql_select.cc \
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
		tableop_hooks.cc \
		thr_malloc.cc \
		time.cc \
		tmp_table.cc \
		tztime.cc \
		uniques.cc \
		unireg.cc \
		virtual_column_info.cc \
		xid.cc

libhandler_la_SOURCES = handler.cc handlerton.cc
libyacc_la_SOURCES = sql_yacc.yy
AM_YFLAGS = -p DRIZZLE -d --verbose
# Code output from YACC contains GOBS of unused macros, and I can't
# figure out how to turn it off.
libyacc_la_CXXFLAGS = ${AM_CXXFLAGS} -Wno-unused-macros


if BUILD_DTRACE
drizzled_SOURCES += probes.d
endif

gen_lex_hash_SOURCES= gen_lex_hash.cc

gen_lex_hash_LDFLAGS= 

DEFS =-DDRIZZLE_SERVER \
      -DDEFAULT_DRIZZLE_HOME="\"$(MYSQLBASEdir)\"" \
      -DDATADIR="\"$(MYSQLDATAdir)\"" \
      -DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
      -DPLUGINDIR="\"$(pkgplugindir)\"" \
      -DLOCALEDIR=\"$(localedir)\" \
      @DEFS@

if BUILD_GCC_PCH
PCHHEADERS = \
		definitions.h.gch \
		handlerton.h.gch  \
		sql_base.h.gch \
		sql_parse.h.gch \
		sql_plugin.h.gch
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
	     $(PCHHEADERS) \
	     $(nodist_drizzled_SOURCES)

DISTCLEANFILES = $(EXTRA_PROGRAMS) \
		 $(BUILT_MAINT_SRC)

MAINTAINERCLEANFILES = $(BUILT_MAINT_SRC)

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

if BUILD_GCC_PCH
%.h.gch: %.h
	$(CXXCOMPILE) -include config.h -Wno-unused-macros -c $<
endif

SUFFIXES = .d

.d.o : $(DTRACEFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $< $(DTRACEFILES)
