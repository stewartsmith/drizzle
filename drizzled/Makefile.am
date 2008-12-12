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

sbin_PROGRAMS= drizzled

EXTRA_PROGRAMS= gen_lex_hash

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
		configmake.h \
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
		field/blob.h \
		field/date.h \
		field/datetime.h \
		field/enum.h \
		field/decimal.h \
		field/double.h \
		field/int64_t.h \
		field/long.h \
		field/longstr.h \
		field/null.h \
		field/num.h \
		field/real.h \
		field/str.h \
		field/timestamp.h \
		field/timetype.h \
		field/varstring.h \
		filesort_info.h \
		foreign_key.h \
		functions/abs.h \
		functions/acos.h \
		functions/ascii.h \
		functions/asin.h \
		functions/atan.h \
		functions/additive_op.h \
		functions/benchmark.h \
		functions/bit.h \
		functions/bit_count.h \
		functions/bit_length.h \
		functions/ceiling.h \
		functions/char_length.h \
		functions/coercibility.h \
		functions/connection_id.h \
		functions/cos.h \
		functions/dec.h \
		functions/decimal_typecast.h \
		functions/divide.h \
		functions/exp.h \
		functions/field.h \
		functions/find_in_set.h \
		functions/floor.h \
		functions/found_rows.h \
		functions/func.h \
		functions/get_system_var.h \
		functions/get_user_var.h \
		functions/int.h \
		functions/integer.h \
		functions/int_divide.h \
		functions/int_val.h \
		functions/last_insert.h \
		functions/length.h \
		functions/ln.h \
		functions/locate.h \
		functions/log.h \
		functions/master_pos_wait.h \
		functions/min_max.h \
		functions/minus.h \
		functions/mod.h \
		functions/multiply.h \
		functions/neg.h \
		functions/num1.h \
		functions/numhybrid.h \
		functions/num_op.h \
		functions/ord.h \
		functions/plus.h \
		functions/pow.h \
		functions/real.h \
		functions/rollup_const.h \
		functions/round.h \
		functions/row_count.h \
		functions/set_user_var.h \
		functions/sign.h \
		functions/signed.h \
		functions/sin.h \
		functions/sqrt.h \
		functions/tan.h \
		functions/unsigned.h \
		functions/user_var_as_out_param.h \
		functions/str/alloc_buffer.h \
		functions/str/binary.h \
		functions/str/char.h \
		functions/str/charset.h \
		functions/str/collation.h \
		functions/str/concat.h \
		functions/str/conv.h \
		functions/str/conv_charset.h \
		functions/str/database.h \
		functions/str/elt.h \
		functions/str/export_set.h \
		functions/str/format.h \
		functions/str/hex.h \
		functions/str/insert.h \
		functions/str/left.h \
		functions/str/load_file.h \
		functions/str/make_set.h \
		functions/str/pad.h \
		functions/str/quote.h \
		functions/str/repeat.h \
		functions/str/replace.h \
		functions/str/reverse.h \
		functions/str/right.h \
		functions/str/set_collation.h \
		functions/str/str_conv.h \
		functions/str/strfunc.h \
		functions/str/substr.h \
		functions/str/sysconst.h \
		functions/str/trim.h \
		functions/str/user.h \
		functions/str/uuid.h \
		functions/str/weight_string.h
		functions/time/add_time.h \
		functions/time/curdate.h \
		functions/time/curtime.h \
		functions/time/date.h \
		functions/time/date_add_interval.h \
		functions/time/date_format.h \
		functions/time/dayname.h \
		functions/time/dayofmonth.h \
		functions/time/dayofyear.h \
		functions/time/extract.h \
		functions/time/from_days.h \
		functions/time/from_unixtime.h \
		functions/time/get_format.h \
		functions/time/get_interval_value.h \
		functions/time/hour.h \
		functions/time/last_day.h \
		functions/time/makedate.h \
		functions/time/make_datetime_with_warn.h \
		functions/time/make_datetime.h \
		functions/time/maketime.h \
		functions/time/make_time_with_warn.h \
		functions/time/microsecond.h \
		functions/time/minute.h \
		functions/time/month.h \
		functions/time/now.h \
		functions/time/quarter.h \
		functions/time/period_add.h \
		functions/time/period_diff.h \
		functions/time/sec_to_time.h \
		functions/time/second.h \
		functions/time/str_to_date.h \
		functions/time/sysdate_local.h \
		functions/time/str_timefunc.h \
		functions/time/timediff.h \
		functions/time/timestamp_diff.h \
		functions/time/time_to_sec.h \
		functions/time/to_days.h \
		functions/time/typecast.h \
		functions/time/unix_timestamp.h \
		functions/time/week.h \
		functions/time/week_mode.h \
		functions/time/weekday.h \
		functions/time/year.h  \
		functions/time/yearweek.h \
		ha_trx_info.h \
		ha_statistics.h \
		handler.h \
		handler_structs.h \
		hybrid_type.h \
		hybrid_type_traits.h \
		hybrid_type_traits_decimal.h \
		hybrid_type_traits_integer.h \
	       	item.h \
		item/basic_constant.h \
		item/bin_string.h \
		item/blob.h \
		item/cache.h \
		item/cache_decimal.h \
		item/cache_int.h \
		item/cache_real.h \
		item/cache_row.h \
		item/cache_str.h \
		item/cmpfunc.h \
		item/copy_string.h \
		item/create.h \
		item/decimal.h \
		item/direct_ref.h \
		item/empty_string.h \
		item/field.h \
		item/float.h \
		item/hex_string.h \
		item/insert_value.h \
		item/int_with_ref.h \
		item/func.h \
		item/ident.h \
		item/int.h \
		item/null.h \
		item/num.h \
		item/outer_ref.h \
		item/param.h \
		item/ref.h \
		item/ref_null_helper.h \
		item/return_date_time.h \
		item/return_int.h \
		item/row.h \
		item/string.h \
		item/subselect.h \
		item/sum.h \
		item/timefunc.h \
		item/type_holder.h \
		item/uint.h \
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

noinst_LTLIBRARIES = \
		libhandler.la \
		libyacc.la \
		libitem.la \
		libutil.la \
		libfield.la \
		libfunctions.la

LDADD = $(top_builddir)/libdrizzle/libdrizzle.la \
	$(top_builddir)/mysys/libmysys.la \
	$(top_builddir)/mystrings/libmystrings.la \
	$(Z_LIBS) $(LIBINTL) 

drizzled_LDADD = ${noinst_LTLIBRARIES} \
		 $(top_builddir)/libdrizzle/libdrizzle.la \
		 $(top_builddir)/drizzled/serialize/libserialize.la \
		 $(libevent_libs) \
		 @mysql_plugin_libs@ \
		 -lprotobuf \
		 $(LDADD) \
		 $(CXXLDFLAGS) \
		 $(LIBDL_LIBS) $(EVENT_LIBS) $(PROTOBUF_LIBS)
drizzled_DEPENDENCIES = @mysql_plugin_libs@ ${noinst_LTLIBRARIES}

drizzled_LDFLAGS =


noinst_HEADERS = \
		configvar.h \
		gettext.h \
		global.h \
		server_includes.h \
		util/convert.h \
		util/math.h \
		util/test.h

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

if BUILD_DTRACE
drizzled_SOURCES += probes.d
endif

gen_lex_hash_SOURCES= gen_lex_hash.cc

gen_lex_hash_LDFLAGS= 


libhandler_la_SOURCES = handler.cc handlerton.cc

# Code output from YACC contains GOBS of unused macros, and I can't
# figure out how to turn it off.
libyacc_la_CXXFLAGS = ${AM_CXXFLAGS} ${NO_UNUSED_MACROS}
libyacc_la_SOURCES = sql_yacc.yy
AM_YFLAGS = -p DRIZZLE -d --verbose

# This may seem silly, but it's required to get the object files prefixed
# with libitem_ so that item/field.cc and field.cc don't conflict.
libitem_la_CXXFLAGS = ${AM_CXXFLAGS}
libitem_la_SOURCES= \
		item/bin_string.cc \
		item/cache.cc \
		item/cache_decimal.cc \
		item/cache_int.cc \
		item/cache_real.cc \
		item/cache_row.cc \
		item/cache_str.cc \
		item/cmpfunc.cc \
		item/copy_string.cc \
		item/create.cc \
		item/decimal.cc \
		item/direct_ref.cc \
		item/empty_string.cc \
		item/field.cc \
		item/float.cc \
		item/hex_string.cc \
		item/insert_value.cc \
		item/int_with_ref.cc \
		item/ident.cc \
		item/int.cc \
		item/null.cc \
		item/num.cc \
		item/outer_ref.cc \
		item/param.cc \
		item/ref.cc \
		item/ref_null_helper.cc \
		item/row.cc \
		item/string.cc \
		item/subselect.cc \
		item/sum.cc \
		item/type_holder.cc \
		item/uint.cc


libutil_la_CXXFLAGS = ${AM_CXXFLAGS}
libutil_la_SOURCES = \
		util/math.cc


libfield_la_CXXFLAGS = ${AM_CXXFLAGS}
libfield_la_SOURCES = 	
		field/blob.cc \
		field/date.cc \
		field/datetime.cc \
		field/enum.cc \
		field/decimal.cc \
		field/double.cc \
		field/int64_t.cc \
		field/long.cc \
		field/longstr.cc \
		field/null.cc \
		field/num.cc \
		field/real.cc \
		field/str.cc \
		field/timestamp.cc \
		field/timetype.cc \
		field/varstring.cc


libfunctions_la_CPPFLAGS = ${GLOBAL_CPPFLAGS} ${PROTOBUF_CFLAGS}
libfunctions_la_SOURCES = \
		functions/abs.cc \
		functions/acos.cc \
		functions/ascii.cc \
		functions/asin.cc \
		functions/atan.cc \
		functions/additive_op.cc \
		functions/benchmark.cc \
		functions/bit.cc \
		functions/bit_count.cc \
		functions/char_length.cc \
		functions/check_reserved_words.cc \
		functions/ceiling.cc \
		functions/connection_id.cc \
		functions/coercibility.cc \
		functions/cos.cc \
		functions/decimal_typecast.cc \
		functions/divide.cc \
		functions/exp.cc \
		functions/get_user_var.cc \
		functions/get_variable.cc \
		functions/field.cc \
		functions/find_in_set.cc \
		functions/floor.cc \
		functions/found_rows.cc \
		functions/func.cc \
		functions/get_system_var.cc \
		functions/get_variable.cc \
		functions/int.cc \
		functions/integer.cc \
		functions/int_divide.cc \
		functions/int_val.cc \
		functions/last_insert.cc \
		functions/length.cc \
		functions/ln.cc \
		functions/locate.cc \
		functions/log.cc \
		functions/master_pos_wait.cc \
		functions/min_max.cc \
		functions/minus.cc \
		functions/mod.cc \
		functions/multiply.cc \
		functions/neg.cc \
		functions/num1.cc \
		functions/numhybrid.cc \
		functions/num_op.cc \
		functions/ord.cc \
		functions/plus.cc \
		functions/pow.cc \
		functions/rand.cc \
		functions/real.cc \
		functions/round.cc \
		functions/row_count.cc \
		functions/set_user_var.cc \
		functions/shift.cc \
		functions/sign.cc \
		functions/signed.cc \
		functions/sin.cc \
		functions/sqrt.cc \
		functions/tan.cc \
		functions/update_hash.cc \
		functions/units.cc \
		functions/unsigned.cc \
		functions/user_var_as_out_param.cc \
		functions/user_var_entry.cc \
		functions/str/alloc_buffer.cc \
		functions/str/binary.cc \
		functions/str/char.cc \
		functions/str/charset.cc \
		functions/str/collation.cc \
		functions/str/concat.cc \
		functions/str/conv.cc \
		functions/str/conv_charset.cc \
		functions/str/database.cc \
		functions/str/elt.cc \
		functions/str/export_set.cc \
		functions/str/format.cc \
		functions/str/hex.cc \
		functions/str/insert.cc \
		functions/str/left.cc \
		functions/str/load_file.cc \
		functions/str/make_set.cc \
		functions/str/pad.cc \
		functions/str/quote.cc \
		functions/str/repeat.cc \
		functions/str/replace.cc \
		functions/str/reverse.cc \
		functions/str/right.cc \
		functions/str/set_collation.cc \
		functions/str/str_conv.cc \
		functions/str/strfunc.cc \
		functions/str/substr.cc \
		functions/str/sysconst.cc \
		functions/str/trim.cc \
		functions/str/user.cc \
		functions/str/uuid.cc \
		functions/str/weight_string.cc \
		functions/time/add_time.cc \
		functions/time/curdate.cc \
		functions/time/curtime.cc \
		functions/time/date.cc \
		functions/time/date_add_interval.cc \
		functions/time/date_format.cc \
		functions/time/dayname.cc \
		functions/time/dayofmonth.cc \
		functions/time/extract.cc \
		functions/time/dayofyear.cc \
		functions/time/from_days.cc \
		functions/time/from_unixtime.cc \
		functions/time/get_format.cc \
		functions/time/get_interval_value.cc \
		functions/time/hour.cc \
		functions/time/last_day.cc \
		functions/time/makedate.cc \
		functions/time/make_datetime_with_warn.cc \
		functions/time/make_datetime.cc \
		functions/time/maketime.cc \
		functions/time/make_time_with_warn.cc \
		functions/time/microsecond.cc \
		functions/time/minute.cc \
		functions/time/month.cc \
		functions/time/now.cc \
		functions/time/quarter.cc \
		functions/time/period_add.cc \
		functions/time/period_diff.cc \
		functions/time/sec_to_time.cc \
		functions/time/second.cc \
		functions/time/str_to_date.cc \
		functions/time/sysdate_local.cc \
		functions/time/timediff.cc \
		functions/time/timestamp_diff.cc \
		functions/time/time_to_sec.cc \
		functions/time/to_days.cc \
		functions/time/typecast.cc \
		functions/time/unix_timestamp.cc \
		functions/time/week.cc \
		functions/time/week_mode.cc \
		functions/time/weekday.cc \
		functions/time/year.cc \
		functions/time/yearweek.cc



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
		sql_plugin.h.gch \
		util/convert.h.gch \
		util/test.h.gch
else
PCHHEADERS = 
endif

BUILT_MAINT_SRC = sql_yacc.cc \
		  sql_yacc.h

BUILT_SOURCES = $(BUILT_MAINT_SRC) \
		lex_hash.h \
		configvar.h \
		$(PCHHEADERS)

EXTRA_DIST = \
		$(BUILT_MAINT_SRC) \
		drizzledumpslow \
		message.mc \
		probes.d

CLEANFILES = \
		configmake.h \
		lex_hash.h \
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

configmake.h: Makefile
	rm -f $@-t $@
	{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  echo '#define PREFIX "$(prefix)"'; \
	  echo '#define EXEC_PREFIX "$(exec_prefix)"'; \
	  echo '#define BINDIR "$(bindir)"'; \
	  echo '#define SBINDIR "$(sbindir)"'; \
	  echo '#define LIBEXECDIR "$(libexecdir)"'; \
	  echo '#define DATAROOTDIR "$(datarootdir)"'; \
	  echo '#define DATADIR "$(datadir)"'; \
	  echo '#define SYSCONFDIR "$(sysconfdir)"'; \
	  echo '#define SHAREDSTATEDIR "$(sharedstatedir)"'; \
	  echo '#define LOCALSTATEDIR "$(localstatedir)"'; \
	  echo '#define INCLUDEDIR "$(includedir)"'; \
	  echo '#define OLDINCLUDEDIR "$(oldincludedir)"'; \
	  echo '#define DOCDIR "$(docdir)"'; \
	  echo '#define INFODIR "$(infodir)"'; \
	  echo '#define HTMLDIR "$(htmldir)"'; \
	  echo '#define DVIDIR "$(dvidir)"'; \
	  echo '#define PDFDIR "$(pdfdir)"'; \
	  echo '#define PSDIR "$(psdir)"'; \
	  echo '#define LIBDIR "$(libdir)"'; \
	  echo '#define LISPDIR "$(lispdir)"'; \
	  echo '#define LOCALEDIR "$(localedir)"'; \
	  echo '#define MANDIR "$(mandir)"'; \
	  echo '#define MANEXT "$(manext)"'; \
	  echo '#define PKGDATADIR "$(pkgdatadir)"'; \
	  echo '#define PKGINCLUDEDIR "$(pkgincludedir)"'; \
	  echo '#define PKGLIBDIR "$(pkglibdir)"'; \
	  echo '#define PKGLIBEXECDIR "$(pkglibexecdir)"'; \
	} | sed '/""/d' > $@-t
	mv $@-t $@


if BUILD_GCC_PCH
%.h.gch: %.h
	$(CXXCOMPILE) -include config.h ${NO_UNUSED_MACROS} -c $<
endif

SUFFIXES = .d

.d.o : $(DTRACEFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $< $(DTRACEFILES)
