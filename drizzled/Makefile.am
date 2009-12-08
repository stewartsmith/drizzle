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


sbin_PROGRAMS= drizzled

DTRACEFILES = cursor.o \
	      filesort.o \
	      drizzled.o \
	      session.o \
	      sql_delete.o \
	      sql_insert.o \
	      sql_select.o \
	      sql_update.o \
	      sql_parse.o \
	      statement/delete.o \
	      statement/insert.o \
	      statement/insert_select.o

drizzledincludedir = ${includedir}/drizzled
nobase_dist_drizzledinclude_HEADERS = \
		alter_column.h \
		alter_drop.h \
		alter_info.h \
		atomic/gcc_traits.h \
		atomic/pthread_traits.h \
		atomic/sun_studio.h \
		atomics.h \
		base.h \
		cached_item.h \
		calendar.h \
		check_stack_overrun.h \
		common.h \
		comp_creator.h \
		cost_vect.h \
		create_field.h \
		current_session.h \
		data_home.h \
		db.h \
		definitions.h \
		diagnostics_area.h \
		discrete_interval.h \
		dtcollation.h \
		enum.h \
		enum_nested_loop_state.h \
		errmsg_print.h \
		error.h \
		field.h \
		field/blob.h \
		field/date.h \
		field/datetime.h \
		field/decimal.h \
		field/double.h \
		field/enum.h \
		field/int64_t.h \
		field/long.h \
		field/null.h \
		field/num.h \
		field/real.h \
		field/str.h \
		field/timestamp.h \
		field/varstring.h \
		field_iterator.h \
		file_exchange.h \
		filesort_info.h \
		foreign_key.h \
		function/additive_op.h \
		function/coercibility.h \
		function/field.h \
		function/find_in_set.h \
		function/found_rows.h \
		function/func.h \
		function/get_system_var.h \
		function/get_user_var.h \
		function/last_insert.h \
		function/locate.h \
		function/math/abs.h \
		function/math/acos.h \
		function/math/asin.h \
		function/math/atan.h \
		function/math/ceiling.h \
		function/math/cos.h \
		function/math/dec.h \
		function/math/decimal_typecast.h \
		function/math/divide.h \
		function/math/exp.h \
		function/math/floor.h \
		function/math/int.h \
		function/math/int_divide.h \
		function/math/int_val.h \
		function/math/integer.h \
		function/math/ln.h \
		function/math/log.h \
		function/math/minus.h \
		function/math/mod.h \
		function/math/multiply.h \
		function/math/neg.h \
		function/math/ord.h \
		function/math/plus.h \
		function/math/pow.h \
		function/math/rand.h \
		function/math/real.h \
		function/math/round.h \
		function/math/sin.h \
		function/math/sqrt.h \
		function/math/tan.h \
		function/min_max.h \
		function/num1.h \
		function/num_op.h \
		function/numhybrid.h \
		function/rollup_const.h \
		function/row_count.h \
		function/set_user_var.h \
		function/sign.h \
		function/str/alloc_buffer.h \
		function/str/binary.h \
		function/str/char.h \
		function/str/collation.h \
		function/str/concat.h \
		function/str/conv.h \
		function/str/conv_charset.h \
		function/str/database.h \
		function/str/elt.h \
		function/str/export_set.h \
		function/str/format.h \
		function/str/hex.h \
		function/str/insert.h \
		function/str/left.h \
		function/str/load_file.h \
		function/str/make_set.h \
		function/str/pad.h \
		function/str/quote.h \
		function/str/repeat.h \
		function/str/replace.h \
		function/str/reverse.h \
		function/str/right.h \
		function/str/set_collation.h \
		function/str/str_conv.h \
		function/str/strfunc.h \
		function/str/substr.h \
		function/str/trim.h \
		function/str/user.h \
		function/str/uuid.h \
		function/str/weight_string.h \
		function/time/curdate.h \
		function/time/date.h \
		function/time/date_add_interval.h \
		function/time/date_format.h \
		function/time/dayname.h \
		function/time/dayofmonth.h \
		function/time/dayofyear.h \
		function/time/extract.h \
		function/time/from_days.h \
		function/time/from_unixtime.h \
		function/time/hour.h \
		function/time/last_day.h \
		function/time/makedate.h \
		function/time/microsecond.h \
		function/time/minute.h \
		function/time/month.h \
		function/time/now.h \
		function/time/period_add.h \
		function/time/period_diff.h \
		function/time/quarter.h \
		function/time/second.h \
		function/time/sysdate_local.h \
		function/time/timestamp_diff.h \
		function/time/to_days.h \
		function/time/typecast.h \
		function/time/unix_timestamp.h \
		function/time/weekday.h \
		function/time/year.h  \
		function/units.h \
		function/user_var_as_out_param.h \
		function_hash.h \
		ha_statistics.h \
		ha_trx_info.h \
		cursor.h \
		handler_structs.h \
		hybrid_type.h \
		hybrid_type_traits.h \
		hybrid_type_traits_decimal.h \
		hybrid_type_traits_integer.h \
		index_hint.h \
		internal_error_handler.h \
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
		item/default_value.h \
		item/direct_ref.h \
		item/empty_string.h \
		item/field.h \
		item/float.h \
		item/func.h \
		item/hex_string.h \
		item/ident.h \
		item/insert_value.h \
		item/int.h \
		item/int_with_ref.h \
		item/null.h \
		item/num.h \
		item/outer_ref.h \
		item/ref.h \
		item/ref_null_helper.h \
		item/return_date_time.h \
		item/return_int.h \
		item/row.h \
		item/string.h \
		item/subselect.h \
		item/sum.h \
		item/type_holder.h \
		item/uint.h \
		join.h \
		join_cache.h \
		join_table.h \
		key.h \
		key_map.h \
		key_part_spec.h \
		korr.h \
		lex_column.h \
		lex_input_stream.h \
		lex_string.h \
		lex_symbol.h \
		lock.h \
		lookup_symbol.h \
		memory/multi_malloc.h \
		my_decimal.h\
		my_var.h \
		name_resolution_context.h \
		name_resolution_context_state.h \
		natural_join_column.h \
		nested_join.h \
		open_tables_state.h \
		optimizer/explain_plan.h \
		optimizer/key_field.h \
		optimizer/key_use.h \
		optimizer/position.h \
		optimizer/range.h \
		optimizer/sargable_param.h \
		optimizer/sum.h \
		order.h \
		plugin.h \
		plugin/authentication.h \
		plugin/client.h \
		plugin/error_message.h \
		plugin/function.h \
		plugin/info_schema_table.h \
		plugin/library.h \
		plugin/listen.h \
		plugin/listen_tcp.h \
		plugin/logging.h \
		plugin/manifest.h \
		plugin/module.h \
		plugin/null_client.h \
		plugin/plugin.h \
		plugin/query_cache.h \
		plugin/registry.h \
		plugin/scheduler.h \
		plugin/storage_engine.h \
		plugin/transaction_applier.h \
		plugin/transaction_reader.h \
		plugin/transaction_replicator.h \
		probes.h \
		query_id.h \
		records.h \
		replication_services.h \
		security_context.h \
		select_create.h \
		select_dump.h \
		select_dumpvar.h \
		select_exists_subselect.h \
		select_export.h \
		select_insert.h \
		select_max_min_finder_subselect.h \
		select_result.h \
		select_result_interceptor.h \
		select_send.h \
		select_singlerow_subselect.h \
		select_subselect.h \
		select_to_file.h \
		select_union.h \
		session.h \
		set_var.h \
		show.h \
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
		sql_select.h \
		sql_sort.h \
		sql_state.h \
		sql_string.h \
		sql_table.h \
		sql_union.h \
		stacktrace.h \
		statement.h \
		statement/alter_schema.h \
		statement/alter_table.h \
		statement/analyze.h \
		statement/change_schema.h \
		statement/check.h \
		statement/checksum.h \
		statement/commit.h \
		statement/create_index.h \
		statement/create_schema.h \
		statement/create_table.h \
		statement/delete.h \
		statement/drop_index.h \
		statement/drop_schema.h \
		statement/drop_table.h \
		statement/empty_query.h \
		statement/flush.h \
		statement/insert.h \
		statement/insert_select.h \
		statement/kill.h \
		statement/load.h \
		statement/release_savepoint.h \
		statement/rename_table.h \
		statement/replace.h \
		statement/replace_select.h \
		statement/rollback.h \
		statement/rollback_to_savepoint.h \
		statement/savepoint.h \
		statement/select.h \
		statement/set_option.h \
		statement/show_create.h \
		statement/show_create_schema.h \
		statement/show_engine_status.h \
		statement/show_errors.h \
		statement/show_processlist.h \
		statement/show_status.h \
		statement/show_warnings.h \
		statement/start_transaction.h \
		statement/truncate.h \
		statement/unlock_tables.h \
		statement/update.h \
		stored_key.h \
		structs.h \
		symbol_hash.h \
		table.h \
		table_ident.h \
		table_identifier.h \
		table_list.h \
		table_proto.h \
		table_reference.h \
		table_share.h \
		temporal.h \
		temporal_format.h \
		temporal_interval.h \
		time_functions.h \
		tmp_table_param.h \
		tzfile.h \
		tztime.h \
		unique.h \
		unireg.h \
		user_var_entry.h \
		utf8.h \
		utf8/checked.h \
		utf8/core.h \
		utf8/unchecked.h \
		xid.h

noinst_LTLIBRARIES = \
		libhandler.la \
		libserialutil.la

LDADD = $(top_builddir)/mysys/libmysys.la \
	$(top_builddir)/mystrings/libmystrings.la \
	$(LTLIBDRIZZLE) $(LIBZ) $(LIBINTL) ${LIBC_P}

drizzled_LDADD = ${noinst_LTLIBRARIES} \
		 $(top_builddir)/drizzled/hash/libhash.la \
		 $(top_builddir)/drizzled/util/libutil.la \
		 $(top_builddir)/drizzled/message/libdrizzledmessage.la \
		 $(pandora_plugin_libs) \
		 $(LDADD) $(LIBUUID) ${top_builddir}/gnulib/libgnu.la \
		 $(LIBDL_LIBS) $(LIBPROTOBUF) $(LIBPCRE) $(LIBTBB) \
		 $(PANDORA_PLUGIN_DEP_LIBS)
drizzled_DEPENDENCIES= ${noinst_LTLIBRARIES} $(pandora_plugin_libs)

drizzled_LDFLAGS = -export-dynamic


noinst_HEADERS = \
		hash.h \
		gettext.h \
		global.h \
		server_includes.h

drizzled_SOURCES = \
		alter_info.cc \
		cached_item.cc \
		calendar.cc \
		check_stack_overrun.cc \
		comp_creator.cc \
		create_field.cc \
		current_session.cc \
		diagnostics_area.cc \
		drizzled.cc \
		dtcollation.cc \
		errmsg_print.cc \
		error.cc \
		field.cc \
		field/blob.cc \
		field/date.cc \
		field/datetime.cc \
		field/decimal.cc \
		field/double.cc \
		field/enum.cc \
		field/int64_t.cc \
		field/long.cc \
		field/null.cc \
		field/num.cc \
		field/real.cc \
		field/str.cc \
		field/timestamp.cc \
		field/varstring.cc \
		field_conv.cc \
		field_iterator.cc \
		file_exchange.cc \
		filesort.cc \
		foreign_key.cc \
		function/additive_op.cc \
		function/check_reserved_words.cc \
		function/coercibility.cc \
		function/field.cc \
		function/find_in_set.cc \
		function/found_rows.cc \
		function/func.cc \
		function/get_system_var.cc \
		function/get_user_var.cc \
		function/last_insert.cc \
		function/locate.cc \
		function/math/abs.cc \
		function/math/acos.cc \
		function/math/asin.cc \
		function/math/atan.cc \
		function/math/ceiling.cc \
		function/math/cos.cc \
		function/math/decimal_typecast.cc \
		function/math/divide.cc \
		function/math/exp.cc \
		function/math/floor.cc \
		function/math/int.cc \
		function/math/int_divide.cc \
		function/math/int_val.cc \
		function/math/integer.cc \
		function/math/ln.cc \
		function/math/log.cc \
		function/math/minus.cc \
		function/math/mod.cc \
		function/math/multiply.cc \
		function/math/neg.cc \
		function/math/ord.cc \
		function/math/plus.cc \
		function/math/pow.cc \
		function/math/rand.cc \
		function/math/real.cc \
		function/math/round.cc \
		function/math/sin.cc \
		function/math/sqrt.cc \
		function/math/tan.cc \
		function/min_max.cc \
		function/num1.cc \
		function/num_op.cc \
		function/numhybrid.cc \
		function/row_count.cc \
		function/set_user_var.cc \
		function/sign.cc \
		function/str/alloc_buffer.cc \
		function/str/binary.cc \
		function/str/char.cc \
		function/str/collation.cc \
		function/str/concat.cc \
		function/str/conv.cc \
		function/str/conv_charset.cc \
		function/str/database.cc \
		function/str/elt.cc \
		function/str/export_set.cc \
		function/str/format.cc \
		function/str/hex.cc \
		function/str/insert.cc \
		function/str/left.cc \
		function/str/load_file.cc \
		function/str/make_set.cc \
		function/str/pad.cc \
		function/str/quote.cc \
		function/str/repeat.cc \
		function/str/replace.cc \
		function/str/reverse.cc \
		function/str/right.cc \
		function/str/set_collation.cc \
		function/str/str_conv.cc \
		function/str/strfunc.cc \
		function/str/substr.cc \
		function/str/trim.cc \
		function/str/user.cc \
		function/str/uuid.cc \
		function/str/weight_string.cc \
		function/time/curdate.cc \
		function/time/date.cc \
		function/time/date_add_interval.cc \
		function/time/date_format.cc \
		function/time/dayname.cc \
		function/time/dayofmonth.cc \
		function/time/dayofyear.cc \
		function/time/extract.cc \
		function/time/from_days.cc \
		function/time/from_unixtime.cc \
		function/time/hour.cc \
		function/time/last_day.cc \
		function/time/makedate.cc \
		function/time/microsecond.cc \
		function/time/minute.cc \
		function/time/month.cc \
		function/time/now.cc \
		function/time/period_add.cc \
		function/time/period_diff.cc \
		function/time/quarter.cc \
		function/time/second.cc \
		function/time/sysdate_local.cc \
		function/time/timestamp_diff.cc \
		function/time/to_days.cc \
		function/time/typecast.cc \
		function/time/unix_timestamp.cc \
		function/time/weekday.cc \
		function/time/year.cc \
		function/units.cc \
		function/user_var_as_out_param.cc \
		ha_commands.cc \
		ha_trx_info.cc \
		hybrid_type_traits.cc \
		hybrid_type_traits_decimal.cc \
		hybrid_type_traits_integer.cc \
		index_hint.cc \
		item.cc \
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
		item/default_value.cc \
		item/direct_ref.cc \
		item/empty_string.cc \
		item/field.cc \
		item/float.cc \
		item/hex_string.cc \
		item/ident.cc \
		item/insert_value.cc \
		item/int.cc \
		item/int_with_ref.cc \
		item/null.cc \
		item/num.cc \
		item/outer_ref.cc \
		item/ref.cc \
		item/ref_null_helper.cc \
		item/row.cc \
		item/string.cc \
		item/subselect.cc \
		item/sum.cc \
		item/type_holder.cc \
		item/uint.cc \
		join.cc \
		join_cache.cc \
		join_table.cc \
		key.cc \
		key_map.cc \
		lock.cc \
		lookup_symbol.cc \
		memory/multi_malloc.cc \
		my_decimal.cc \
		name_resolution_context_state.cc \
		natural_join_column.cc \
		optimizer/explain_plan.cc \
		optimizer/key_field.cc \
		optimizer/range.cc \
		optimizer/sum.cc \
		plugin/authentication.cc \
		plugin/client.cc \
		plugin/error_message.cc \
		plugin/function.cc \
		plugin/info_schema_table.cc \
		plugin/library.cc \
		plugin/listen.cc \
		plugin/listen_tcp.cc \
		plugin/loader.cc \
		plugin/logging.cc \
		plugin/plugin.cc \
		plugin/query_cache.cc \
		plugin/registry.cc \
		plugin/scheduler.cc \
		plugin/transaction_applier.cc \
		plugin/transaction_replicator.cc \
		query_id.cc \
		records.cc \
		replication_services.cc \
		session.cc \
		set_var.cc \
		show.cc \
		sql_alloc.cc \
		sql_base.cc \
		sql_delete.cc \
		sql_derived.cc \
		sql_error.cc \
		sql_insert.cc \
		sql_lex.cc \
		sql_list.cc \
		sql_load.cc \
		sql_locale.cc \
		sql_parse.cc \
		sql_select.cc \
		sql_state.cc \
		sql_string.cc \
		sql_table.cc \
		sql_union.cc \
		sql_update.cc \
		sql_yacc.yy \
		stacktrace.cc \
		statement/alter_schema.cc \
		statement/alter_table.cc \
		statement/analyze.cc \
		statement/change_schema.cc \
		statement/check.cc \
		statement/checksum.cc \
		statement/commit.cc \
		statement/create_index.cc \
		statement/create_schema.cc \
		statement/create_table.cc \
		statement/delete.cc \
		statement/drop_index.cc \
		statement/drop_schema.cc \
		statement/drop_table.cc \
		statement/empty_query.cc \
		statement/flush.cc \
		statement/insert.cc \
		statement/insert_select.cc \
		statement/kill.cc \
		statement/load.cc \
		statement/release_savepoint.cc \
		statement/rename_table.cc \
		statement/replace.cc \
		statement/replace_select.cc \
		statement/rollback.cc \
		statement/rollback_to_savepoint.cc \
		statement/savepoint.cc \
		statement/select.cc \
		statement/set_option.cc \
		statement/show_create.cc \
		statement/show_create_schema.cc \
		statement/show_engine_status.cc \
		statement/show_errors.cc \
		statement/show_processlist.cc \
		statement/show_status.cc \
		statement/show_warnings.cc \
		statement/start_transaction.cc \
		statement/truncate.cc \
		statement/unlock_tables.cc \
		statement/update.cc \
		strfunc.cc \
		table.cc \
		table_identifier.cc \
		table_list.cc \
		table_share.cc \
		temporal.cc \
		temporal_format.cc \
		temporal_interval.cc \
		time_functions.cc \
		tztime.cc \
		uniques.cc \
		user_var_entry.cc \
		xid.cc

libserialutil_la_SOURCES = db.cc table_proto_write.cc
libserialutil_la_CXXFLAGS= ${AM_CXXFLAGS} ${PROTOSKIP_WARNINGS}

libhandler_la_SOURCES = cursor.cc plugin/storage_engine.cc

AM_YFLAGS = -p DRIZZLE -d --verbose


if BUILD_GCC_PCH
PCHHEADERS = \
		definitions.h.gch \
		plugin/storage_engine.h.gch  \
		sql_base.h.gch \
		sql_parse.h.gch
else
PCHHEADERS = 
endif

BUILT_MAINT_SRC = sql_yacc.cc \
		  sql_yacc.h

BUILT_SOURCES = $(BUILT_MAINT_SRC) \
		symbol_hash.h \
		function_hash.h \
		$(PCHHEADERS) 

EXTRA_DIST = \
		$(BUILT_MAINT_SRC) \
		symbol_hash.gperf \
		function_hash.gperf \
		message.mc \
		probes.d

CLEANFILES = \
		configmake.h \
		function_hash.h \
		function_hash.stamp-h \
		sql_yacc.output \
		symbol_hash.h \
		symbol_hash.stamp-h \
		$(PCHHEADERS) \
		$(nodist_drizzled_SOURCES)

if DTRACE_NEEDS_OBJECTS
drizzled_SOURCES += probes.d
endif

if HAVE_DTRACE
BUILT_SOURCES += generated_probes.h
CLEANFILES += generated_probes.h
endif


DISTCLEANFILES = $(EXTRA_PROGRAMS) \
		 $(BUILT_MAINT_SRC)

MAINTAINERCLEANFILES = $(BUILT_MAINT_SRC)

generated_probes.h : probes.d
	$(DTRACE) $(DTRACEFLAGS) -h -s probes.d -o generated_probes.h
	mv generated_probes.h generated_probes.h.bak
	sed "s/#include <unistd.h>//g" generated_probes.h.bak > generated_probes.h
	rm generated_probes.h.bak
	sed -e 's,void \*,const void \*,g' generated_probes.h | \
		sed -e 's,char \*,const char \*,g' | tr '\t' ' ' > dtrace_probes.tmp
	mv dtrace_probes.tmp generated_probes.h


SUFFIXES = .d .gch .gperf .stamp-h

if BUILD_GCC_PCH
.h.gch:
	$(CXXCOMPILE) -include config.h ${NO_UNUSED_MACROS} -c $<
endif

.d.o : $(DTRACEFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $< $(DTRACEFILES)

function_hash.h: function_hash.stamp-h
symbol_hash.h: symbol_hash.stamp-h

.gperf.stamp-h:
	$(GPERF) -D --initializer-suffix=,0 --struct-type --enum \
		--omit-struct-type --readonly-tables --language=C++ \
		--class-name=$* $<  > $@
	@if ! test $$? ; then rm $@ ; fi
	@if test -f $@ ; then \
		if ! diff $@ \
			   ${top_builddir}/drizzled/$*.h >/dev/null 2>&1 ; \
		then \
			cp $@ ${top_builddir}/drizzled/$*.h ; \
		fi \
	fi

