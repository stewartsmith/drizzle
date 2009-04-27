#  Copyright (C) 2009 Sun Microsystems
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

ACLOCAL_AMFLAGS = -I m4 --force

# Process this file with automake to create Makefile.in
if BUILD_GETTEXT
  po=po
endif

SUBDIRS = ${po} \
	  gnulib \
	  drizzled/message \
	  . \
	  tests \
	  support-files

AM_YFLAGS = -p DRIZZLE -d --verbose

BUILT_MAINT_SRC = drizzled/sql_yacc.cc \
		  drizzled/sql_yacc.h
BUILT_SOURCES = $(BUILT_MAINT_SRC) \
		drizzled/configmake.h \
		drizzled/symbol_hash.h \
		drizzled/function_hash.h

EXTRA_DIST=	config/config.rpath m4/gnulib-cache.m4 \
		config/plugin.ac config/plugin.am \
		$(BUILT_MAINT_SRC) \
		drizzled/symbol_hash.gperf \
		drizzled/function_hash.gperf \
		drizzled/message.mc \
		drizzled/probes.d

CLEANFILES=	${BUILT_SOURCES} \
		drizzled/configmake.h \
		drizzled/function_hash.h \
		drizzled/function_hash.stamp-h \
		drizzled/sql_yacc.output \
		drizzled/symbol_hash.h \
		drizzled/symbol_hash.stamp-h

DISTCLEANFILES=	ac_available_languages_fragment \
		$(EXTRA_PROGRAMS) \
		$(BUILT_MAINT_SRC)
MAINTAINERCLEANFILES=	autom4te.cache $(BUILT_MAINT_SRC)

CLIENT_LDADD=		mysys/libmysys.la \
			mystrings/libmystrings.la \
			$(LIBDRIZZLE) $(LIBINTL) $(LIBZ) \
			client/libgetpassword.la


sbin_PROGRAMS= drizzled/drizzled


noinst_LTLIBRARIES= \
		client/libgetpassword.la \
		drizzled/libhandler.la \
		drizzled/libserialutil.la \
		mysys/libmysys.la \
		mystrings/libmystrings.la

noinst_PROGRAMS=
bin_PROGRAMS= \
		client/drizzle \
		client/drizzlecheck \
		client/drizzledump \
		client/drizzleimport \
		client/drizzletest \
		client/drizzleslap \
		extra/my_print_defaults \
		extra/mysql_waitpid \
		extra/innochecksum


client_libgetpassword_la_SOURCES= client/get_password.cc

noinst_HEADERS=	\
		client/client_priv.h \
		client/errname.h \
		client/get_password.h \
		client/my_readline.h \
		drizzled/configmake.h \
		drizzled/gettext.h \
		drizzled/global.h \
		drizzled/server_includes.h \
		drizzled/util/convert.h \
		drizzled/util/test.h \
		mystrings/decimal.h \
		mystrings/m_ctype.h \
		mystrings/m_string.h \
		mystrings/my_uctype.h \
		mystrings/t_ctype.h \
		mystrings/utf8.h \
		mysys/aio_result.h \
		mysys/base64.h \
		mysys/definitions.h \
		mysys/drizzle_time.h \
		mysys/hash.h \
		mysys/iocache.h \
		mysys/my_alloc.h \
		mysys/my_bit.h \
		mysys/my_dir.h \
		mysys/my_getopt.h \
		mysys/my_pthread.h \
		mysys/my_static.h \
		mysys/my_sys.h \
		mysys/my_time.h \
		mysys/my_tree.h \
		mysys/mysys_err.h \
		mysys/mysys_priv.h \
		mysys/sha1.h \
		mysys/thr_lock.h \
		mysys/typelib.h

mystrings_libmystrings_la_SOURCES= \
		mystrings/bmove_upp.cc \
		mystrings/ctype-bin.cc \
		mystrings/ctype-extra.cc \
		mystrings/ctype-mb.cc \
		mystrings/ctype-simple.cc \
		mystrings/ctype-uca.cc \
		mystrings/ctype-utf8.cc \
		mystrings/ctype.cc \
		mystrings/decimal.cc \
		mystrings/dtoa.cc \
		mystrings/int2str.cc \
		mystrings/is_prefix.cc \
		mystrings/llstr.cc \
		mystrings/longlong2str.cc \
		mystrings/my_strtoll10.cc \
		mystrings/str2int.cc

mysys_libmysys_la_SOURCES= \
		mysys/array.cc \
		mysys/base64.cc \
		mysys/charset-def.cc \
		mysys/charset.cc \
		mysys/checksum.cc \
		mysys/default.cc \
		mysys/default_modify.cc \
		mysys/errors.cc \
		mysys/hash.cc \
		mysys/mf_arr_appstr.cc \
		mysys/mf_cache.cc \
		mysys/mf_dirname.cc \
		mysys/mf_fn_ext.cc \
		mysys/mf_format.cc \
		mysys/mf_getdate.cc \
		mysys/mf_iocache.cc \
		mysys/mf_iocache2.cc \
		mysys/mf_loadpath.cc \
		mysys/mf_pack.cc \
		mysys/mf_qsort.cc \
		mysys/mf_qsort2.cc \
		mysys/mf_radix.cc \
		mysys/mf_same.cc \
		mysys/mf_sort.cc \
		mysys/mf_tempfile.cc \
		mysys/mf_wcomp.cc \
		mysys/mulalloc.cc \
		mysys/my_access.cc \
		mysys/my_alloc.cc \
		mysys/my_bit.cc \
		mysys/my_copy.cc \
		mysys/my_create.cc \
		mysys/my_delete.cc \
		mysys/my_dup.cc \
		mysys/my_error.cc \
		mysys/my_file.cc \
		mysys/my_filename.cc \
		mysys/my_getopt.cc \
		mysys/my_getsystime.cc \
		mysys/my_init.cc \
		mysys/my_lib.cc \
		mysys/my_open.cc \
		mysys/my_read.cc \
		mysys/my_redel.cc \
		mysys/my_rename.cc \
		mysys/my_static.cc \
		mysys/my_symlink.cc \
		mysys/my_symlink2.cc \
		mysys/my_sync.cc \
		mysys/my_thr_init.cc \
		mysys/my_time.cc \
		mysys/my_write.cc \
		mysys/ptr_cmp.cc \
		mysys/sha1.cc \
		mysys/thr_lock.cc \
		mysys/tree.cc \
		mysys/typelib.cc

client_drizzle_SOURCES=		client/drizzle.cc client/readline.cc
client_drizzle_LDADD=		${CLIENT_LDADD} ${READLINE_LIBS}

client_drizzlecheck_SOURCES =	client/drizzlecheck.cc
client_drizzlecheck_LDADD=	${CLIENT_LDADD}

client_drizzledump_SOURCES=	client/drizzledump.cc
client_drizzledump_LDADD=	${CLIENT_LDADD}

client_drizzleimport_SOURCES=	client/drizzleimport.cc
client_drizzleimport_LDADD=	${CLIENT_LDADD}

client_drizzleslap_SOURCES=	client/drizzleslap.cc
client_drizzleslap_LDADD=	${CLIENT_LDADD}

client_drizzletest_SOURCES=	client/drizzletest.cc
client_drizzletest_LDADD=	${CLIENT_LDADD} ${LIBPCRE}

extra_my_print_defaults_SOURCES= extra/my_print_defaults.cc
extra_my_print_defaults_LDADD= \
		$(top_builddir)/mysys/libmysys.la \
		$(top_builddir)/mystrings/libmystrings.la \
		$(LIBINTL)
extra_mysql_waitpid_SOURCES= extra/mysql_waitpid.cc
extra_mysql_waitpid_LDADD= \
		$(top_builddir)/mysys/libmysys.la \
		$(top_builddir)/mystrings/libmystrings.la \
		$(LIBINTL)

EXTRA_LTLIBRARIES=
pkgplugin_LTLIBRARIES=



DTRACEFILES = drizzled/handler.o \
	      drizzled/filesort.o \
	      drizzled/sql_delete.o \
	      drizzled/sql_insert.o \
	      drizzled/sql_select.o \
	      drizzled/sql_update.o

drizzledincludedir = ${includedir}/drizzled
nobase_dist_drizzledinclude_HEADERS = \
		drizzled/alter_column.h \
		drizzled/alter_drop.h \
		drizzled/atomic/gcc_traits.h \
		drizzled/atomic/pthread_traits.h \
		drizzled/atomic/sun_studio.h \
		drizzled/atomics.h \
		drizzled/authentication.h \
		drizzled/base.h \
		drizzled/cached_item.h \
		drizzled/calendar.h \
		drizzled/check_stack_overrun.h \
		drizzled/common.h \
		drizzled/comp_creator.h \
		drizzled/connect.h \
		drizzled/cost_vect.h \
		drizzled/current_session.h \
		drizzled/data_home.h \
		drizzled/db.h \
		drizzled/definitions.h \
		drizzled/diagnostics_area.h \
		drizzled/discrete_interval.h \
		drizzled/dtcollation.h \
		drizzled/errmsg.h \
		drizzled/errmsg_print.h \
		drizzled/error.h \
		drizzled/error_injection.h \
		drizzled/field.h \
		drizzled/field/blob.h \
		drizzled/field/date.h \
		drizzled/field/datetime.h \
		drizzled/field/decimal.h \
		drizzled/field/double.h \
		drizzled/field/enum.h \
		drizzled/field/int64_t.h \
		drizzled/field/long.h \
		drizzled/field/longstr.h \
		drizzled/field/null.h \
		drizzled/field/num.h \
		drizzled/field/real.h \
		drizzled/field/str.h \
		drizzled/field/timestamp.h \
		drizzled/field/varstring.h \
		drizzled/field_iterator.h \
		drizzled/file_exchange.h \
		drizzled/filesort_info.h \
		drizzled/foreign_key.h \
		drizzled/function/additive_op.h \
		drizzled/function/ascii.h \
		drizzled/function/benchmark.h \
		drizzled/function/bit.h \
		drizzled/function/bit_count.h \
		drizzled/function/bit_length.h \
		drizzled/function/char_length.h \
		drizzled/function/coercibility.h \
		drizzled/function/connection_id.h \
		drizzled/function/create.h \
		drizzled/function/field.h \
		drizzled/function/find_in_set.h \
		drizzled/function/found_rows.h \
		drizzled/function/func.h \
		drizzled/function/get_system_var.h \
		drizzled/function/get_user_var.h \
		drizzled/function/last_insert.h \
		drizzled/function/length.h \
		drizzled/function/locate.h \
		drizzled/function/math/abs.h \
		drizzled/function/math/acos.h \
		drizzled/function/math/asin.h \
		drizzled/function/math/atan.h \
		drizzled/function/math/ceiling.h \
		drizzled/function/math/cos.h \
		drizzled/function/math/dec.h \
		drizzled/function/math/decimal_typecast.h \
		drizzled/function/math/divide.h \
		drizzled/function/math/exp.h \
		drizzled/function/math/floor.h \
		drizzled/function/math/int.h \
		drizzled/function/math/int_divide.h \
		drizzled/function/math/int_val.h \
		drizzled/function/math/integer.h \
		drizzled/function/math/ln.h \
		drizzled/function/math/log.h \
		drizzled/function/math/minus.h \
		drizzled/function/math/mod.h \
		drizzled/function/math/multiply.h \
		drizzled/function/math/neg.h \
		drizzled/function/math/ord.h \
		drizzled/function/math/plus.h \
		drizzled/function/math/pow.h \
		drizzled/function/math/rand.h \
		drizzled/function/math/real.h \
		drizzled/function/math/round.h \
		drizzled/function/math/sin.h \
		drizzled/function/math/sqrt.h \
		drizzled/function/math/tan.h \
		drizzled/function/min_max.h \
		drizzled/function/num1.h \
		drizzled/function/num_op.h \
		drizzled/function/numhybrid.h \
		drizzled/function/rollup_const.h \
		drizzled/function/row_count.h \
		drizzled/function/set_user_var.h \
		drizzled/function/sign.h \
		drizzled/function/signed.h \
		drizzled/function/str/alloc_buffer.h \
		drizzled/function/str/binary.h \
		drizzled/function/str/char.h \
		drizzled/function/str/charset.h \
		drizzled/function/str/collation.h \
		drizzled/function/str/concat.h \
		drizzled/function/str/conv.h \
		drizzled/function/str/conv_charset.h \
		drizzled/function/str/database.h \
		drizzled/function/str/elt.h \
		drizzled/function/str/export_set.h \
		drizzled/function/str/format.h \
		drizzled/function/str/hex.h \
		drizzled/function/str/insert.h \
		drizzled/function/str/left.h \
		drizzled/function/str/load_file.h \
		drizzled/function/str/make_set.h \
		drizzled/function/str/pad.h \
		drizzled/function/str/quote.h \
		drizzled/function/str/repeat.h \
		drizzled/function/str/replace.h \
		drizzled/function/str/reverse.h \
		drizzled/function/str/right.h \
		drizzled/function/str/set_collation.h \
		drizzled/function/str/str_conv.h \
		drizzled/function/str/strfunc.h \
		drizzled/function/str/substr.h \
		drizzled/function/str/sysconst.h \
		drizzled/function/str/trim.h \
		drizzled/function/str/user.h \
		drizzled/function/str/uuid.h \
		drizzled/function/str/weight_string.h \
		drizzled/function/time/curdate.h \
		drizzled/function/time/date.h \
		drizzled/function/time/date_add_interval.h \
		drizzled/function/time/date_format.h \
		drizzled/function/time/dayname.h \
		drizzled/function/time/dayofmonth.h \
		drizzled/function/time/dayofyear.h \
		drizzled/function/time/extract.h \
		drizzled/function/time/from_days.h \
		drizzled/function/time/from_unixtime.h \
		drizzled/function/time/get_interval_value.h \
		drizzled/function/time/hour.h \
		drizzled/function/time/last_day.h \
		drizzled/function/time/makedate.h \
		drizzled/function/time/microsecond.h \
		drizzled/function/time/minute.h \
		drizzled/function/time/month.h \
		drizzled/function/time/now.h \
		drizzled/function/time/period_add.h \
		drizzled/function/time/period_diff.h \
		drizzled/function/time/quarter.h \
		drizzled/function/time/second.h \
		drizzled/function/time/sysdate_local.h \
		drizzled/function/time/timestamp_diff.h \
		drizzled/function/time/to_days.h \
		drizzled/function/time/typecast.h \
		drizzled/function/time/unix_timestamp.h \
		drizzled/function/time/weekday.h \
		drizzled/function/time/year.h  \
		drizzled/function/units.h \
		drizzled/function/unsigned.h \
		drizzled/function/update_hash.h \
		drizzled/function/user_var_as_out_param.h \
		drizzled/function_hash.h \
		drizzled/ha_statistics.h \
		drizzled/ha_trx_info.h \
		drizzled/handler.h \
		drizzled/handler_structs.h \
		drizzled/hybrid_type.h \
		drizzled/hybrid_type_traits.h \
		drizzled/hybrid_type_traits_decimal.h \
		drizzled/hybrid_type_traits_integer.h \
		drizzled/index_hint.h \
		drizzled/internal_error_handler.h \
		drizzled/item.h \
		drizzled/item/basic_constant.h \
		drizzled/item/bin_string.h \
		drizzled/item/blob.h \
		drizzled/item/cache.h \
		drizzled/item/cache_decimal.h \
		drizzled/item/cache_int.h \
		drizzled/item/cache_real.h \
		drizzled/item/cache_row.h \
		drizzled/item/cache_str.h \
		drizzled/item/cmpfunc.h \
		drizzled/item/copy_string.h \
		drizzled/item/create.h \
		drizzled/item/decimal.h \
		drizzled/item/direct_ref.h \
		drizzled/item/empty_string.h \
		drizzled/item/field.h \
		drizzled/item/float.h \
		drizzled/item/func.h \
		drizzled/item/hex_string.h \
		drizzled/item/ident.h \
		drizzled/item/insert_value.h \
		drizzled/item/int.h \
		drizzled/item/int_with_ref.h \
		drizzled/item/null.h \
		drizzled/item/num.h \
		drizzled/item/outer_ref.h \
		drizzled/item/param.h \
		drizzled/item/ref.h \
		drizzled/item/ref_null_helper.h \
		drizzled/item/return_date_time.h \
		drizzled/item/return_int.h \
		drizzled/item/row.h \
		drizzled/item/string.h \
		drizzled/item/subselect.h \
		drizzled/item/sum.h \
		drizzled/item/type_holder.h \
		drizzled/item/uint.h \
		drizzled/key.h \
		drizzled/key_map.h \
		drizzled/key_part_spec.h \
		drizzled/korr.h \
		drizzled/lex_column.h \
		drizzled/lex_string.h \
		drizzled/lex_symbol.h \
		drizzled/lock.h \
		drizzled/logging.h \
		drizzled/lookup_symbol.h \
		drizzled/multi_delete.h \
		drizzled/multi_update.h \
		drizzled/my_decimal.h\
		drizzled/my_var.h \
		drizzled/name_resolution_context.h \
		drizzled/name_resolution_context_state.h \
		drizzled/natural_join_column.h \
		drizzled/nested_join.h \
		drizzled/open_tables_state.h \
		drizzled/opt_range.h \
		drizzled/order.h \
		drizzled/plugin.h \
		drizzled/plugin_registry.h \
		drizzled/plugin/applier.h \
		drizzled/plugin/authentication.h \
		drizzled/plugin/error_message_handler.h \
		drizzled/plugin/logging_handler.h \
		drizzled/plugin/protocol.h \
		drizzled/plugin/qcache.h \
		drizzled/plugin/replicator.h \
		drizzled/plugin/scheduler.h \
		drizzled/plugin/storage_engine.h \
		drizzled/probes.h \
		drizzled/protocol.h \
		drizzled/qcache.h \
		drizzled/query_arena.h \
		drizzled/query_id.h \
		drizzled/registry.h \
		drizzled/rename.h \
		drizzled/transaction_services.h \
		drizzled/scheduling.h \
		drizzled/security_context.h \
		drizzled/select_create.h \
		drizzled/select_dump.h \
		drizzled/select_dumpvar.h \
		drizzled/select_exists_subselect.h \
		drizzled/select_export.h \
		drizzled/select_insert.h \
		drizzled/select_max_min_finder_subselect.h \
		drizzled/select_result.h \
		drizzled/select_result_interceptor.h \
		drizzled/select_send.h \
		drizzled/select_singlerow_subselect.h \
		drizzled/select_subselect.h \
		drizzled/select_to_file.h \
		drizzled/select_union.h \
		drizzled/session.h \
		drizzled/set_var.h \
		drizzled/show.h \
		drizzled/sj_tmp_table.h \
		drizzled/sql_alloc.h \
		drizzled/sql_array.h \
		drizzled/sql_base.h \
		drizzled/sql_bitmap.h \
		drizzled/sql_error.h \
		drizzled/sql_lex.h \
		drizzled/sql_list.h \
		drizzled/sql_load.h \
		drizzled/sql_locale.h \
		drizzled/sql_parse.h \
		drizzled/sql_plugin.h \
		drizzled/sql_select.h \
		drizzled/sql_sort.h \
		drizzled/sql_state.h \
		drizzled/sql_string.h \
		drizzled/sql_table.h \
		drizzled/sql_udf.h \
		drizzled/stacktrace.h \
		drizzled/structs.h \
		drizzled/symbol_hash.h \
		drizzled/table.h \
		drizzled/table_ident.h \
		drizzled/table_list.h \
		drizzled/table_map_iterator.h \
		drizzled/table_share.h \
		drizzled/temporal.h \
		drizzled/temporal_format.h \
		drizzled/tmp_table_param.h \
		drizzled/tzfile.h \
		drizzled/tztime.h \
		drizzled/unique.h \
		drizzled/unireg.h \
		drizzled/utf8.h \
		drizzled/utf8/checked.h \
		drizzled/utf8/core.h \
		drizzled/utf8/unchecked.h \
		drizzled/user_var_entry.h \
		drizzled/virtual_column_info.h \
		drizzled/xid.h



drizzled_drizzled_LDADD= \
		${noinst_LTLIBRARIES} \
		$(top_builddir)/drizzled/message/libdrizzledmessage.la \
		$(top_builddir)/mysys/libmysys.la \
		$(top_builddir)/mystrings/libmystrings.la \
		$(LTLIBDRIZZLE) $(LIBZ) $(LIBINTL) ${LIBC_P} \
		$(LIBUUID) ${top_builddir}/gnulib/libgnu.la \
		$(LIBDL_LIBS) $(LIBPROTOBUF) $(LIBPCRE) $(LIBTBB) \
		$(DRIZZLED_PLUGIN_DEP_LIBS)
drizzled_drizzled_DEPENDENCIES= ${noinst_LTLIBRARIES} $(DRIZZLED_PLUGIN_DEP_LIBS)


drizzled_drizzled_SOURCES = \
		drizzled/authentication.cc \
		drizzled/cached_item.cc \
		drizzled/calendar.cc \
		drizzled/check_stack_overrun.cc \
		drizzled/comp_creator.cc \
		drizzled/current_session.cc \
		drizzled/diagnostics_area.cc \
		drizzled/drizzled.cc \
		drizzled/dtcollation.cc \
		drizzled/errmsg.cc \
		drizzled/error.cc \
		drizzled/errmsg_print.cc \
		drizzled/field/blob.cc \
		drizzled/field/date.cc \
		drizzled/field/datetime.cc \
		drizzled/field/enum.cc \
		drizzled/field/decimal.cc \
		drizzled/field/double.cc \
		drizzled/field/int64_t.cc \
		drizzled/field/long.cc \
		drizzled/field/longstr.cc \
		drizzled/field/null.cc \
		drizzled/field/num.cc \
		drizzled/field/real.cc \
		drizzled/field/str.cc \
		drizzled/field/timestamp.cc \
		drizzled/field/varstring.cc \
		drizzled/field.cc \
		drizzled/field_conv.cc \
		drizzled/field_iterator.cc \
		drizzled/filesort.cc \
		drizzled/foreign_key.cc \
		drizzled/function/ascii.cc \
		drizzled/function/additive_op.cc \
		drizzled/function/benchmark.cc \
		drizzled/function/bit.cc \
		drizzled/function/bit_count.cc \
		drizzled/function/char_length.cc \
		drizzled/function/check_reserved_words.cc \
		drizzled/function/connection_id.cc \
		drizzled/function/coercibility.cc \
		drizzled/function/field.cc \
		drizzled/function/find_in_set.cc \
		drizzled/function/found_rows.cc \
		drizzled/function/func.cc \
		drizzled/function/get_system_var.cc \
		drizzled/function/get_user_var.cc \
		drizzled/function/last_insert.cc \
		drizzled/function/length.cc \
		drizzled/function/locate.cc \
		drizzled/function/min_max.cc \
		drizzled/function/num1.cc \
		drizzled/function/numhybrid.cc \
		drizzled/function/num_op.cc \
		drizzled/function/row_count.cc \
		drizzled/function/set_user_var.cc \
		drizzled/function/shift.cc \
		drizzled/function/sign.cc \
		drizzled/function/signed.cc \
		drizzled/function/update_hash.cc \
		drizzled/function/units.cc \
		drizzled/function/unsigned.cc \
		drizzled/function/user_var_as_out_param.cc \
		drizzled/function/user_var_entry.cc \
		drizzled/function/math/abs.cc \
		drizzled/function/math/acos.cc \
		drizzled/function/math/asin.cc \
		drizzled/function/math/atan.cc \
		drizzled/function/math/ceiling.cc \
		drizzled/function/math/cos.cc \
		drizzled/function/math/decimal_typecast.cc \
		drizzled/function/math/divide.cc \
		drizzled/function/math/exp.cc \
		drizzled/function/math/floor.cc \
		drizzled/function/math/int_divide.cc \
		drizzled/function/math/integer.cc \
		drizzled/function/math/int.cc \
		drizzled/function/math/int_val.cc \
		drizzled/function/math/ln.cc \
		drizzled/function/math/log.cc \
		drizzled/function/math/minus.cc \
		drizzled/function/math/mod.cc \
		drizzled/function/math/multiply.cc \
		drizzled/function/math/neg.cc \
		drizzled/function/math/ord.cc \
		drizzled/function/math/plus.cc \
		drizzled/function/math/pow.cc \
		drizzled/function/math/rand.cc \
		drizzled/function/math/real.cc \
		drizzled/function/math/round.cc \
		drizzled/function/math/sin.cc \
		drizzled/function/math/sqrt.cc \
		drizzled/function/math/tan.cc \
		drizzled/function/str/alloc_buffer.cc \
		drizzled/function/str/binary.cc \
		drizzled/function/str/char.cc \
		drizzled/function/str/charset.cc \
		drizzled/function/str/collation.cc \
		drizzled/function/str/concat.cc \
		drizzled/function/str/conv.cc \
		drizzled/function/str/conv_charset.cc \
		drizzled/function/str/database.cc \
		drizzled/function/str/elt.cc \
		drizzled/function/str/export_set.cc \
		drizzled/function/str/format.cc \
		drizzled/function/str/hex.cc \
		drizzled/function/str/insert.cc \
		drizzled/function/str/left.cc \
		drizzled/function/str/load_file.cc \
		drizzled/function/str/make_set.cc \
		drizzled/function/str/pad.cc \
		drizzled/function/str/quote.cc \
		drizzled/function/str/repeat.cc \
		drizzled/function/str/replace.cc \
		drizzled/function/str/reverse.cc \
		drizzled/function/str/right.cc \
		drizzled/function/str/set_collation.cc \
		drizzled/function/str/str_conv.cc \
		drizzled/function/str/strfunc.cc \
		drizzled/function/str/substr.cc \
		drizzled/function/str/sysconst.cc \
		drizzled/function/str/trim.cc \
		drizzled/function/str/user.cc \
		drizzled/function/str/uuid.cc \
		drizzled/function/str/weight_string.cc \
		drizzled/function/time/curdate.cc \
		drizzled/function/time/date.cc \
		drizzled/function/time/date_add_interval.cc \
		drizzled/function/time/date_format.cc \
		drizzled/function/time/dayname.cc \
		drizzled/function/time/dayofmonth.cc \
		drizzled/function/time/extract.cc \
		drizzled/function/time/dayofyear.cc \
		drizzled/function/time/from_days.cc \
		drizzled/function/time/from_unixtime.cc \
		drizzled/function/time/get_interval_value.cc \
		drizzled/function/time/hour.cc \
		drizzled/function/time/last_day.cc \
		drizzled/function/time/makedate.cc \
		drizzled/function/time/microsecond.cc \
		drizzled/function/time/minute.cc \
		drizzled/function/time/month.cc \
		drizzled/function/time/now.cc \
		drizzled/function/time/quarter.cc \
		drizzled/function/time/period_add.cc \
		drizzled/function/time/period_diff.cc \
		drizzled/function/time/second.cc \
		drizzled/function/time/sysdate_local.cc \
		drizzled/function/time/timestamp_diff.cc \
		drizzled/function/time/to_days.cc \
		drizzled/function/time/typecast.cc \
		drizzled/function/time/unix_timestamp.cc \
		drizzled/function/time/weekday.cc \
		drizzled/function/time/year.cc \
		drizzled/ha_trx_info.cc \
		drizzled/hybrid_type_traits.cc \
		drizzled/hybrid_type_traits_decimal.cc \
		drizzled/hybrid_type_traits_integer.cc \
		drizzled/index_hint.cc \
		drizzled/item/bin_string.cc \
		drizzled/item/cache.cc \
		drizzled/item/cache_decimal.cc \
		drizzled/item/cache_int.cc \
		drizzled/item/cache_real.cc \
		drizzled/item/cache_row.cc \
		drizzled/item/cache_str.cc \
		drizzled/item/cmpfunc.cc \
		drizzled/item/copy_string.cc \
		drizzled/item/create.cc \
		drizzled/item/decimal.cc \
		drizzled/item/direct_ref.cc \
		drizzled/item/empty_string.cc \
		drizzled/item/field.cc \
		drizzled/item/float.cc \
		drizzled/item/hex_string.cc \
		drizzled/item/insert_value.cc \
		drizzled/item/int_with_ref.cc \
		drizzled/item/ident.cc \
		drizzled/item/int.cc \
		drizzled/item/null.cc \
		drizzled/item/num.cc \
		drizzled/item/outer_ref.cc \
		drizzled/item/param.cc \
		drizzled/item/ref.cc \
		drizzled/item/ref_null_helper.cc \
		drizzled/item/row.cc \
		drizzled/item/string.cc \
		drizzled/item/subselect.cc \
		drizzled/item/sum.cc \
		drizzled/item/type_holder.cc \
		drizzled/item/uint.cc \
		drizzled/item.cc \
		drizzled/key.cc \
		drizzled/key_map.cc \
		drizzled/lock.cc \
		drizzled/logging.cc \
		drizzled/lookup_symbol.cc \
		drizzled/my_decimal.cc \
		drizzled/name_resolution_context_state.cc \
		drizzled/natural_join_column.cc \
		drizzled/opt_range.cc \
		drizzled/opt_sum.cc \
		drizzled/plugin_registry.cc \
		drizzled/protocol.cc \
		drizzled/qcache.cc \
		drizzled/query_id.cc \
		drizzled/records.cc \
		drizzled/rename.cc \
		drizzled/transaction_services.cc \
		drizzled/scheduling.cc \
		drizzled/session.cc \
		drizzled/set_var.cc \
		drizzled/show.cc \
		drizzled/sj_tmp_table.cc \
		drizzled/sql_base.cc \
		drizzled/sql_bitmap.cc \
		drizzled/sql_connect.cc \
		drizzled/sql_delete.cc \
		drizzled/sql_derived.cc \
		drizzled/sql_error.cc \
		drizzled/sql_insert.cc \
		drizzled/sql_lex.cc \
		drizzled/sql_list.cc \
		drizzled/sql_load.cc \
		drizzled/sql_locale.cc \
		drizzled/sql_olap.cc \
		drizzled/sql_parse.cc \
		drizzled/sql_plugin.cc \
		drizzled/sql_select.cc \
		drizzled/sql_state.cc \
		drizzled/sql_string.cc \
		drizzled/sql_table.cc \
		drizzled/sql_udf.cc \
		drizzled/sql_union.cc \
		drizzled/sql_update.cc \
		drizzled/sql_yacc.yy \
		drizzled/stacktrace.cc \
		drizzled/strfunc.cc \
		drizzled/table.cc \
                drizzled/table_map_iterator.cc \
		drizzled/temporal.cc \
		drizzled/temporal_format.cc \
		drizzled/thr_malloc.cc \
		drizzled/time.cc \
		drizzled/tztime.cc \
		drizzled/uniques.cc \
		drizzled/util/convert.cc \
		drizzled/virtual_column_info.cc \
		drizzled/xid.cc

if BUILD_DTRACE
drizzled_drizzled_SOURCES += drizzled/probes.d
endif

drizzled_libserialutil_la_SOURCES = drizzled/db.cc drizzled/table_proto_write.cc
drizzled_libserialutil_la_CXXFLAGS= ${AM_CXXFLAGS} ${PROTOSKIP_WARNINGS}

drizzled_libhandler_la_SOURCES = drizzled/handler.cc drizzled/plugin/storage_engine.cc

include config/plugin.am

# Create empty datadir 
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(localstatedir) 
	@if test `id -u` = 0 ; then \
		if test -n "$(GROUPADD)" && test -n "$(USERADD)"; then \
			$(GROUPADD) $(DRIZZLED_USER) ;\
			$(USERADD) -g $(DRIZZLED_USER) $(DRIZZLED_USER) ;\
			chown $(DRIZZLED_USER) $(DESTDIR)$(localstatedir) ;\
		fi \
	fi


.PHONY: test \
    test-force \
    test-full \
    test-force-full \
    test-force-mem \
    test-pl \
    test-force-pl \
    test-full-pl \
    test-force-full-pl \
    test-force-pl-mem \
    test-ps test-nr \
    test-pr test-ns \
    test-binlog-statement \
    test-ext-funcs \
    test-ext-rpl \
    test-ext-jp \
    test-ext-stress \
    test-ext \
    test-fast \
    test-fast-cursor \
    test-fast-view \
    test-full-qa \
    indent

# Target 'test' will run the regression test suite using the built server.
#
# If you are running in a shared environment, users can avoid clashing
# port numbers by setting individual small numbers 1-100 to the
# environment variable MTR_BUILD_THREAD. The script "test-run"
# will then calculate the various port numbers it needs from this,
# making sure each user use different ports.

test-nr:
	cd tests ; \
	  ./test-run $(force) --mysqld=--binlog-format=row

test-ns:
	cd tests ; \
	  ./test-run $(force) $(mem) --mysqld=--binlog-format=mixed

test-binlog-statement:
	cd tests ; \
	  ./test-run $(force) --mysqld=--binlog-format=statement

test: test-drizzle

test-drizzle:
	cd tests ; \
	$(MAKE) $(AM_MAKEFLAGS) test-drizzle

test-big:
	cd tests ; \
	$(MAKE) $(AM_MAKEFLAGS) test-big

doxygen:
	doxygen Doxyfile

indent:
	for f in `find ${top_srcdir} -type f | grep -v innobase |\
                  ${EGREP} '\.(cc|c|h)$$' ` ; do \
            uncrustify -f $$f -c ${top_srcdir}/config/uncrustify.cfg \
               -o indentoutput.tmp && mv indentoutput.tmp "$$f" ;\
        done

drizzled/configmake.h: ${top_srcdir}/Makefile.in
	@echo "Making $@"
	@rm -f $@-t $@
	@{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
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
	  echo '#define PKGPLUGINDIR "$(pkgplugindir)"'; \
	} | sed '/""/d' > $@-t
	@if diff $@-t $@ >/dev/null 2>&1 ; then \
	  rm @-t ; \
	else \
	  mv $@-t $@ ; \
	fi

if HAVE_LCOV

lcov: lcov-clean test lcov/index.html

lcov/drizzle.output: drizzled/drizzled
	mkdir -p lcov
	${LCOV} --directory ${top_srcdir}/lcov --capture --output-file lcov/drizzle.output

lcov/index.html: lcov/drizzle.output
	${GENHTML} -o lcov lcov/drizzle.output

lcov-clean:
	ln -fs pars/pars0lex.l ${top_srcdir}/storage/innobase/pars0lex.l
	ln -fs pars/lexyy.c ${top_srcdir}/storage/innobase/lexyy.c
	ln -fs pars/pars0grm.c ${top_srcdir}/storage/innobase/pars0grm.c
	ln -fs pars/pars0grm.y ${top_srcdir}/storage/innobase/pars0grm.y

	${LCOV} --directory ${top_srcdir} --zerocounters

endif

drizzled/probes.h: drizzled/probes.d
	$(DTRACE) $(DTRACEFLAGS) -h -s drizzled/probes.d
	mv drizzled/probes.h drizzled/probes.h.bak
	sed "s/#include <unistd.h>//g" drizzled/probes.h.bak > drizzled/probes.h
	rm drizzled/probes.h.bak


SUFFIXES = .d .gperf .stamp-h


.d.o : $(DTRACEFILES)
	$(DTRACE) $(DTRACEFLAGS) -G -s $< $(DTRACEFILES)

drizzled/function_hash.h: drizzled/function_hash.stamp-h
drizzled/symbol_hash.h: drizzled/symbol_hash.stamp-h

.gperf.stamp-h:
	$(GPERF) --initializer-suffix=,0 --struct-type --enum \
		--omit-struct-type --readonly-tables --language=C++ \
		--class-name=$(*F) $<  > $@
	@if ! test $$? ; then rm $@ ; fi
	@if test -f $@ ; then \
		if ! diff $@ \
			${top_builddir}/$*.h >/dev/null 2>&1 ; \
		then \
			cp $@ ${top_builddir}/$*.h ; \
		fi \
	fi

