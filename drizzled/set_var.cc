/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
  @file

  @brief
  Handling of MySQL SQL variables

  @details
  To add a new variable, one has to do the following:

  - Use one of the 'sys_var... classes from set_var.h or write a specific
    one for the variable type.
  - Define it in the 'variable definition list' in this file.
  - If the variable is thread specific, add it to 'system_variables' struct.
    If not, add it to mysqld.cc and an declaration in 'mysql_priv.h'
  - If the variable should be changed from the command line, add a definition
    of it in the my_option structure list in mysqld.cc
  - Don't forget to initialize new fields in global_system_variables and
    max_system_variables!

  @todo
    Add full support for the variable character_set (for 4.1)

  @todo
    When updating myisam_delay_key_write, we should do a 'flush tables'
    of all MyISAM tables to ensure that they are reopen with the
    new attribute.

  @note
    Be careful with var->save_result: sys_var::check() only updates
    uint64_t_value; so other members of the union are garbage then; to use
    them you must first assign a value to them (in specific ::check() for
    example).
*/

#include <drizzled/server_includes.h>
#include <drizzled/replication/mi.h>
#include <mysys/my_getopt.h>
#include <mysys/thr_alarm.h>
#include <storage/myisam/myisam.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/tztime.h>
#include <drizzled/slave.h>
#include <drizzled/data_home.h>
#include <drizzled/set_var.h>
#include <drizzled/session.h>
#include <drizzled/sql_base.h>

extern const CHARSET_INFO *character_set_filesystem;
extern I_List<NAMED_LIST> key_caches;


static DYNAMIC_ARRAY fixed_show_vars;
static HASH system_variable_hash;

const char *bool_type_names[]= { "OFF", "ON", NULL };
TYPELIB bool_typelib=
{
  array_elements(bool_type_names)-1, "", bool_type_names, NULL
};

const char *delay_key_write_type_names[]= { "OFF", "ON", "ALL", NULL };
TYPELIB delay_key_write_typelib=
{
  array_elements(delay_key_write_type_names)-1, "",
  delay_key_write_type_names, NULL
};

const char *slave_exec_mode_names[]=
{ "STRICT", "IDEMPOTENT", NULL };
static const unsigned int slave_exec_mode_names_len[]=
{ sizeof("STRICT") - 1, sizeof("IDEMPOTENT") - 1, 0 };
TYPELIB slave_exec_mode_typelib=
{
  array_elements(slave_exec_mode_names)-1, "",
  slave_exec_mode_names, (unsigned int *) slave_exec_mode_names_len
};

static bool sys_update_init_connect(Session*, set_var*);
static void sys_default_init_connect(Session*, enum_var_type type);
static bool sys_update_init_slave(Session*, set_var*);
static void sys_default_init_slave(Session*, enum_var_type type);
static bool set_option_bit(Session *session, set_var *var);
static bool set_option_autocommit(Session *session, set_var *var);
static int  check_log_update(Session *session, set_var *var);
static int  check_pseudo_thread_id(Session *session, set_var *var);
static void fix_low_priority_updates(Session *session, enum_var_type type);
static int check_tx_isolation(Session *session, set_var *var);
static void fix_tx_isolation(Session *session, enum_var_type type);
static int check_completion_type(Session *session, set_var *var);
static void fix_completion_type(Session *session, enum_var_type type);
static void fix_net_read_timeout(Session *session, enum_var_type type);
static void fix_net_write_timeout(Session *session, enum_var_type type);
static void fix_net_retry_count(Session *session, enum_var_type type);
static void fix_max_join_size(Session *session, enum_var_type type);
static void fix_myisam_max_sort_file_size(Session *session, enum_var_type type);
static void fix_max_binlog_size(Session *session, enum_var_type type);
static void fix_max_relay_log_size(Session *session, enum_var_type type);
static void fix_max_connections(Session *session, enum_var_type type);
static void fix_session_mem_root(Session *session, enum_var_type type);
static void fix_trans_mem_root(Session *session, enum_var_type type);
static void fix_server_id(Session *session, enum_var_type type);
static uint64_t fix_unsigned(Session *, uint64_t, const struct my_option *);
static bool get_unsigned(Session *session, set_var *var);
bool throw_bounds_warning(Session *session, bool fixed, bool unsignd,
                          const char *name, int64_t val);
static KEY_CACHE *create_key_cache(const char *name, uint32_t length);
static unsigned char *get_error_count(Session *session);
static unsigned char *get_warning_count(Session *session);
static unsigned char *get_tmpdir(Session *session);

/*
  Variable definition list

  These are variables that can be set from the command line, in
  alphabetic order.

  The variables are linked into the list. A variable is added to
  it in the constructor (see sys_var class for details).
*/

static sys_var_chain vars = { NULL, NULL };

static sys_var_session_uint32_t
sys_auto_increment_increment(&vars, "auto_increment_increment",
                             &SV::auto_increment_increment, NULL, NULL,
                             sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_session_uint32_t
sys_auto_increment_offset(&vars, "auto_increment_offset",
                          &SV::auto_increment_offset, NULL, NULL,
                          sys_var::SESSION_VARIABLE_IN_BINLOG);

static sys_var_const_str       sys_basedir(&vars, "basedir", drizzle_home);
static sys_var_long_ptr	sys_binlog_cache_size(&vars, "binlog_cache_size",
					      &binlog_cache_size);
static sys_var_session_uint64_t	sys_bulk_insert_buff_size(&vars, "bulk_insert_buffer_size",
                                                          &SV::bulk_insert_buff_size);
static sys_var_session_uint32_t	sys_completion_type(&vars, "completion_type",
                                                    &SV::completion_type,
                                                    check_completion_type,
                                                    fix_completion_type);
static sys_var_collation_sv
sys_collation_connection(&vars, "collation_connection",
                         &SV::collation_connection, &default_charset_info,
                         sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_collation_sv
sys_collation_database(&vars, "collation_database", &SV::collation_database,
                       &default_charset_info,
                       sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_collation_sv
sys_collation_server(&vars, "collation_server", &SV::collation_server,
                     &default_charset_info,
                     sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_long_ptr	sys_connect_timeout(&vars, "connect_timeout",
					    &connect_timeout);
static sys_var_const_str       sys_datadir(&vars, "datadir", drizzle_real_data_home);
static sys_var_enum		sys_delay_key_write(&vars, "delay_key_write",
					    &delay_key_write_options,
					    &delay_key_write_typelib,
					    fix_delay_key_write);

static sys_var_long_ptr	sys_expire_logs_days(&vars, "expire_logs_days",
					     &expire_logs_days);
static sys_var_bool_ptr	sys_flush(&vars, "flush", &myisam_flush);
sys_var_str             sys_init_connect(&vars, "init_connect", 0,
                                         sys_update_init_connect,
                                         sys_default_init_connect,0);
sys_var_str             sys_init_slave(&vars, "init_slave", 0,
                                       sys_update_init_slave,
                                       sys_default_init_slave,0);
static sys_var_session_uint32_t	sys_interactive_timeout(&vars, "interactive_timeout",
                                                        &SV::net_interactive_timeout);
static sys_var_session_uint64_t	sys_join_buffer_size(&vars, "join_buffer_size",
                                                     &SV::join_buff_size);
static sys_var_key_buffer_size	sys_key_buffer_size(&vars, "key_buffer_size");
static sys_var_key_cache_long  sys_key_cache_block_size(&vars, "key_cache_block_size",
                                                        offsetof(KEY_CACHE,
                                                                 param_block_size));
static sys_var_key_cache_long	sys_key_cache_division_limit(&vars, "key_cache_division_limit",
                                                           offsetof(KEY_CACHE,
                                                                    param_division_limit));
static sys_var_key_cache_long  sys_key_cache_age_threshold(&vars, "key_cache_age_threshold",
                                                           offsetof(KEY_CACHE,
                                                                    param_age_threshold));
static sys_var_bool_ptr	sys_local_infile(&vars, "local_infile",
                                         &opt_local_infile);
static sys_var_session_bool	sys_low_priority_updates(&vars, "low_priority_updates",
                                                     &SV::low_priority_updates,
                                                     fix_low_priority_updates);
#ifndef TO_BE_DELETED	/* Alias for the low_priority_updates */
static sys_var_session_bool	sys_sql_low_priority_updates(&vars, "sql_low_priority_updates",
                                                         &SV::low_priority_updates,
                                                         fix_low_priority_updates);
#endif
static sys_var_session_uint32_t	sys_max_allowed_packet(&vars, "max_allowed_packet",
                                                       &SV::max_allowed_packet);
static sys_var_long_ptr	sys_max_binlog_cache_size(&vars, "max_binlog_cache_size",
                                                  &max_binlog_cache_size);
static sys_var_long_ptr	sys_max_binlog_size(&vars, "max_binlog_size",
                                            &max_binlog_size,
                                            fix_max_binlog_size);
static sys_var_long_ptr	sys_max_connections(&vars, "max_connections",
                                            &max_connections,
                                            fix_max_connections);
static sys_var_long_ptr	sys_max_connect_errors(&vars, "max_connect_errors",
                                               &max_connect_errors);
static sys_var_session_uint64_t	sys_max_error_count(&vars, "max_error_count",
                                                  &SV::max_error_count);
static sys_var_session_uint64_t	sys_max_heap_table_size(&vars, "max_heap_table_size",
                                                        &SV::max_heap_table_size);
static sys_var_session_uint64_t sys_pseudo_thread_id(&vars, "pseudo_thread_id",
                                              &SV::pseudo_thread_id,
                                              0, check_pseudo_thread_id,
                                              sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_session_ha_rows	sys_max_join_size(&vars, "max_join_size",
                                                  &SV::max_join_size,
                                                  fix_max_join_size);
static sys_var_session_uint64_t	sys_max_seeks_for_key(&vars, "max_seeks_for_key",
                                                      &SV::max_seeks_for_key);
static sys_var_session_uint64_t   sys_max_length_for_sort_data(&vars, "max_length_for_sort_data",
                                                               &SV::max_length_for_sort_data);
static sys_var_long_ptr	sys_max_relay_log_size(&vars, "max_relay_log_size",
                                               &max_relay_log_size,
                                               fix_max_relay_log_size);
static sys_var_session_uint64_t	sys_max_sort_length(&vars, "max_sort_length",
                                                    &SV::max_sort_length);
static sys_var_session_uint64_t	sys_max_tmp_tables(&vars, "max_tmp_tables",
                                                   &SV::max_tmp_tables);
static sys_var_long_ptr	sys_max_write_lock_count(&vars, "max_write_lock_count",
						 &max_write_lock_count);
static sys_var_session_uint64_t sys_min_examined_row_limit(&vars, "min_examined_row_limit",
                                                           &SV::min_examined_row_limit);
static sys_var_session_uint64_t	sys_myisam_max_sort_file_size(&vars, "myisam_max_sort_file_size", &SV::myisam_max_sort_file_size, fix_myisam_max_sort_file_size, 1);
static sys_var_session_uint32_t       sys_myisam_repair_threads(&vars, "myisam_repair_threads", &SV::myisam_repair_threads);
static sys_var_session_uint64_t	sys_myisam_sort_buffer_size(&vars, "myisam_sort_buffer_size", &SV::myisam_sort_buff_size);

static sys_var_session_enum         sys_myisam_stats_method(&vars, "myisam_stats_method",
                                                            &SV::myisam_stats_method,
                                                            &myisam_stats_method_typelib,
                                                            NULL);
static sys_var_session_uint32_t	sys_net_buffer_length(&vars, "net_buffer_length",
                                                      &SV::net_buffer_length);
static sys_var_session_uint32_t	sys_net_read_timeout(&vars, "net_read_timeout",
                                                     &SV::net_read_timeout,
                                                     0, fix_net_read_timeout);
static sys_var_session_uint32_t	sys_net_write_timeout(&vars, "net_write_timeout",
                                                      &SV::net_write_timeout,
                                                      0, fix_net_write_timeout);
static sys_var_session_uint32_t	sys_net_retry_count(&vars, "net_retry_count",
                                                    &SV::net_retry_count,
                                                    0, fix_net_retry_count);
static sys_var_session_bool	sys_new_mode(&vars, "new", &SV::new_mode);
static sys_var_bool_ptr_readonly sys_old_mode(&vars, "old",
                                              &global_system_variables.old_mode);
/* these two cannot be static */
sys_var_session_bool sys_old_alter_table(&vars, "old_alter_table",
                                         &SV::old_alter_table);
static sys_var_session_bool sys_optimizer_prune_level(&vars, "optimizer_prune_level",
                                                      &SV::optimizer_prune_level);
static sys_var_session_uint32_t sys_optimizer_search_depth(&vars, "optimizer_search_depth",
                                                           &SV::optimizer_search_depth);

const char *optimizer_use_mrr_names[] = {"auto", "force", "disable", NULL};
TYPELIB optimizer_use_mrr_typelib= {
  array_elements(optimizer_use_mrr_names) - 1, "",
  optimizer_use_mrr_names, NULL
};

static sys_var_session_enum sys_optimizer_use_mrr(&vars, "optimizer_use_mrr",
                                                  &SV::optimizer_use_mrr,
                                                  &optimizer_use_mrr_typelib,
                                                  NULL);

static sys_var_session_uint64_t sys_preload_buff_size(&vars, "preload_buffer_size",
                                                      &SV::preload_buff_size);
static sys_var_session_uint32_t sys_read_buff_size(&vars, "read_buffer_size",
                                                   &SV::read_buff_size);
static sys_var_opt_readonly	sys_readonly(&vars, "read_only", &opt_readonly);
static sys_var_session_uint32_t	sys_read_rnd_buff_size(&vars, "read_rnd_buffer_size",
                                                       &SV::read_rnd_buff_size);
static sys_var_session_uint32_t	sys_div_precincrement(&vars, "div_precision_increment",
                                                      &SV::div_precincrement);

static sys_var_session_uint64_t	sys_range_alloc_block_size(&vars, "range_alloc_block_size",
                                                           &SV::range_alloc_block_size);
static sys_var_session_uint32_t	sys_query_alloc_block_size(&vars, "query_alloc_block_size",
                                                           &SV::query_alloc_block_size,
                                                           false, fix_session_mem_root);
static sys_var_session_uint32_t	sys_query_prealloc_size(&vars, "query_prealloc_size",
                                                        &SV::query_prealloc_size,
                                                        false, fix_session_mem_root);
static sys_var_readonly sys_tmpdir(&vars, "tmpdir", OPT_GLOBAL, SHOW_CHAR, get_tmpdir);
static sys_var_session_uint32_t	sys_trans_alloc_block_size(&vars, "transaction_alloc_block_size",
                                                           &SV::trans_alloc_block_size,
                                                           false, fix_trans_mem_root);
static sys_var_session_uint32_t	sys_trans_prealloc_size(&vars, "transaction_prealloc_size",
                                                        &SV::trans_prealloc_size,
                                                        false, fix_trans_mem_root);

static sys_var_const_str_ptr sys_secure_file_priv(&vars, "secure_file_priv",
                                             &opt_secure_file_priv);
static sys_var_uint32_t_ptr  sys_server_id(&vars, "server_id", &server_id,
                                           fix_server_id);

static sys_var_bool_ptr	sys_slave_compressed_protocol(&vars, "slave_compressed_protocol",
						      &opt_slave_compressed_protocol);
static sys_var_bool_ptr         sys_slave_allow_batching(&vars, "slave_allow_batching",
                                                         &slave_allow_batching);
static sys_var_set_slave_mode slave_exec_mode(&vars,
                                              "slave_exec_mode",
                                              &slave_exec_mode_options,
                                              &slave_exec_mode_typelib,
                                              0);
static sys_var_long_ptr	sys_slow_launch_time(&vars, "slow_launch_time",
                                             &slow_launch_time);
static sys_var_session_uint64_t	sys_sort_buffer(&vars, "sort_buffer_size",
                                                &SV::sortbuff_size);
/*
  sql_mode should *not* have binlog_mode=SESSION_VARIABLE_IN_BINLOG:
  even though it is written to the binlog, the slave ignores the
  MODE_NO_DIR_IN_CREATE variable, so slave's value differs from
  master's (see log_event.cc: Query_log_event::do_apply_event()).
*/
static sys_var_session_optimizer_switch   sys_optimizer_switch(&vars, "optimizer_switch",
                                                               &SV::optimizer_switch);

static sys_var_session_storage_engine sys_storage_engine(&vars, "storage_engine",
				       &SV::table_plugin);
static sys_var_const_str	sys_system_time_zone(&vars, "system_time_zone",
                                             system_time_zone);
static sys_var_long_ptr	sys_table_def_size(&vars, "table_definition_cache",
                                           &table_def_size);
static sys_var_long_ptr	sys_table_cache_size(&vars, "table_open_cache",
					     &table_cache_size);
static sys_var_long_ptr	sys_table_lock_wait_timeout(&vars, "table_lock_wait_timeout",
                                                    &table_lock_wait_timeout);
static sys_var_long_ptr	sys_thread_cache_size(&vars, "thread_cache_size",
					      &thread_cache_size);
sys_var_long_ptr	sys_thread_pool_size(&vars, "thread_pool_size",
					      &thread_pool_size);
static sys_var_session_enum	sys_tx_isolation(&vars, "tx_isolation",
                                             &SV::tx_isolation,
                                             &tx_isolation_typelib,
                                             fix_tx_isolation,
                                             check_tx_isolation);
static sys_var_session_uint64_t	sys_tmp_table_size(&vars, "tmp_table_size",
					   &SV::tmp_table_size);
static sys_var_bool_ptr  sys_timed_mutexes(&vars, "timed_mutexes", &timed_mutexes);
static sys_var_const_str	sys_version(&vars, "version", server_version);
static sys_var_const_str	sys_version_comment(&vars, "version_comment",
                                            COMPILATION_COMMENT);
static sys_var_const_str	sys_version_compile_machine(&vars, "version_compile_machine",
                                                    MACHINE_TYPE);
static sys_var_const_str	sys_version_compile_os(&vars, "version_compile_os",
                                               SYSTEM_TYPE);
static sys_var_session_uint32_t	sys_net_wait_timeout(&vars, "wait_timeout",
                                                     &SV::net_wait_timeout);

/* Condition pushdown to storage engine */
static sys_var_session_bool
sys_engine_condition_pushdown(&vars, "engine_condition_pushdown",
			      &SV::engine_condition_pushdown);

/* Time/date/datetime formats */

static sys_var_session_date_time_format sys_time_format(&vars, "time_format",
					     &SV::time_format,
					     DRIZZLE_TIMESTAMP_TIME);
static sys_var_session_date_time_format sys_date_format(&vars, "date_format",
					     &SV::date_format,
					     DRIZZLE_TIMESTAMP_DATE);
static sys_var_session_date_time_format sys_datetime_format(&vars, "datetime_format",
						 &SV::datetime_format,
						 DRIZZLE_TIMESTAMP_DATETIME);

/* Variables that are bits in Session */

sys_var_session_bit sys_autocommit(&vars, "autocommit", 0,
                               set_option_autocommit,
                               OPTION_NOT_AUTOCOMMIT,
                               1);
static sys_var_session_bit	sys_big_selects(&vars, "sql_big_selects", 0,
					set_option_bit,
					OPTION_BIG_SELECTS);
static sys_var_session_bit	sys_log_binlog(&vars, "sql_log_bin",
                                       check_log_update,
				       set_option_bit,
				       OPTION_BIN_LOG);
static sys_var_session_bit	sys_sql_warnings(&vars, "sql_warnings", 0,
					 set_option_bit,
					 OPTION_WARNINGS);
static sys_var_session_bit	sys_sql_notes(&vars, "sql_notes", 0,
					 set_option_bit,
					 OPTION_SQL_NOTES);
static sys_var_session_bit	sys_safe_updates(&vars, "sql_safe_updates", 0,
					 set_option_bit,
					 OPTION_SAFE_UPDATES);
static sys_var_session_bit	sys_buffer_results(&vars, "sql_buffer_result", 0,
					   set_option_bit,
					   OPTION_BUFFER_RESULT);
static sys_var_session_bit	sys_quote_show_create(&vars, "sql_quote_show_create", 0,
					      set_option_bit,
					      OPTION_QUOTE_SHOW_CREATE);
static sys_var_session_bit	sys_foreign_key_checks(&vars, "foreign_key_checks", 0,
					       set_option_bit,
					       OPTION_NO_FOREIGN_KEY_CHECKS,
                                               1, sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_session_bit	sys_unique_checks(&vars, "unique_checks", 0,
					  set_option_bit,
					  OPTION_RELAXED_UNIQUE_CHECKS,
                                          1,
                                          sys_var::SESSION_VARIABLE_IN_BINLOG);
/* Local state variables */

static sys_var_session_ha_rows	sys_select_limit(&vars, "sql_select_limit",
						 &SV::select_limit);
static sys_var_timestamp sys_timestamp(&vars, "timestamp",
                                       sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_last_insert_id
sys_last_insert_id(&vars, "last_insert_id",
                   sys_var::SESSION_VARIABLE_IN_BINLOG);
/*
  identity is an alias for last_insert_id(), so that we are compatible
  with Sybase
*/
static sys_var_last_insert_id
sys_identity(&vars, "identity", sys_var::SESSION_VARIABLE_IN_BINLOG);

static sys_var_session_lc_time_names
sys_lc_time_names(&vars, "lc_time_names", sys_var::SESSION_VARIABLE_IN_BINLOG);

/*
  insert_id should *not* be marked as written to the binlog (i.e., it
  should *not* have binlog_status==SESSION_VARIABLE_IN_BINLOG),
  because we want any statement that refers to insert_id explicitly to
  be unsafe.  (By "explicitly", we mean using @@session.insert_id,
  whereas insert_id is used "implicitly" when NULL value is inserted
  into an auto_increment column).

  We want statements referring explicitly to @@session.insert_id to be
  unsafe, because insert_id is modified internally by the slave sql
  thread when NULL values are inserted in an AUTO_INCREMENT column.
  This modification interfers with the value of the
  @@session.insert_id variable if @@session.insert_id is referred
  explicitly by an insert statement (as is seen by executing "SET
  @@session.insert_id=0; CREATE TABLE t (a INT, b INT KEY
  AUTO_INCREMENT); INSERT INTO t(a) VALUES (@@session.insert_id);" in
  statement-based logging mode: t will be different on master and
  slave).
*/
static sys_var_insert_id sys_insert_id(&vars, "insert_id");
static sys_var_readonly		sys_error_count(&vars, "error_count",
						OPT_SESSION,
						SHOW_LONG,
						get_error_count);
static sys_var_readonly		sys_warning_count(&vars, "warning_count",
						  OPT_SESSION,
						  SHOW_LONG,
						  get_warning_count);

static sys_var_rand_seed1 sys_rand_seed1(&vars, "rand_seed1",
                                         sys_var::SESSION_VARIABLE_IN_BINLOG);
static sys_var_rand_seed2 sys_rand_seed2(&vars, "rand_seed2",
                                         sys_var::SESSION_VARIABLE_IN_BINLOG);

static sys_var_session_uint32_t sys_default_week_format(&vars, "default_week_format",
                                                        &SV::default_week_format);

sys_var_session_uint64_t sys_group_concat_max_len(&vars, "group_concat_max_len",
                                                  &SV::group_concat_max_len);

sys_var_session_time_zone sys_time_zone(&vars, "time_zone",
                                    sys_var::SESSION_VARIABLE_IN_BINLOG);

/* Global read-only variable containing hostname */
static sys_var_const_str        sys_hostname(&vars, "hostname", glob_hostname);

static sys_var_const_str_ptr    sys_repl_report_host(&vars, "report_host", &report_host);

sys_var_session_bool  sys_keep_files_on_create(&vars, "keep_files_on_create", 
                                           &SV::keep_files_on_create);
/* Read only variables */

static sys_var_have_variable sys_have_compress(&vars, "have_compress", &have_compress);
static sys_var_have_variable sys_have_symlink(&vars, "have_symlink", &have_symlink);
/*
  Additional variables (not derived from sys_var class, not accessible as
  @@varname in SELECT or SET). Sorted in alphabetical order to facilitate
  maintenance - SHOW VARIABLES will sort its output.
  TODO: remove this list completely
*/

#define FIXED_VARS_SIZE (sizeof(fixed_vars) / sizeof(SHOW_VAR))
static SHOW_VAR fixed_vars[]= {
  {"back_log",                (char*) &back_log,                    SHOW_LONG},
  {"init_file",               (char*) &opt_init_file,               SHOW_CHAR_PTR},
  {"language",                language,                             SHOW_CHAR},
#ifdef HAVE_MLOCKALL
  {"locked_in_memory",	      (char*) &locked_in_memory,	    SHOW_MY_BOOL},
#endif
  {"log_bin",                 (char*) &opt_bin_log,                 SHOW_BOOL},
  {"log_error",               (char*) log_error_file,               SHOW_CHAR},
  {"myisam_recover_options",  (char*) &myisam_recover_options_str,  SHOW_CHAR_PTR},
  {"open_files_limit",	      (char*) &open_files_limit,	    SHOW_LONG},
  {"pid_file",                (char*) pidfile_name,                 SHOW_CHAR},
  {"plugin_dir",              (char*) opt_plugin_dir,               SHOW_CHAR},
  {"port",                    (char*) &drizzled_port,               SHOW_INT},
  {"protocol_version",        (char*) &protocol_version,            SHOW_INT},
  {"thread_stack",            (char*) &my_thread_stack_size,        SHOW_LONG},
};


bool sys_var::check(Session *, set_var *var)
{
  var->save_result.uint64_t_value= var->value->val_int();
  return 0;
}

bool sys_var_str::check(Session *session, set_var *var)
{
  int res;
  if (!check_func)
    return 0;

  if ((res=(*check_func)(session, var)) < 0)
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0),
             name, var->value->str_value.ptr());
  return res;
}

/*
  Functions to check and update variables
*/


/*
  Update variables 'init_connect, init_slave'.

  In case of 'DEFAULT' value
  (for example: 'set GLOBAL init_connect=DEFAULT')
  'var' parameter is NULL pointer.
*/

bool update_sys_var_str(sys_var_str *var_str, rw_lock_t *var_mutex,
			set_var *var)
{
  char *res= 0, *old_value=(char *)(var ? var->value->str_value.ptr() : 0);
  uint32_t new_length= (var ? var->value->str_value.length() : 0);
  if (!old_value)
    old_value= (char*) "";
  if (!(res= my_strndup(old_value, new_length, MYF(0))))
    return 1;
  /*
    Replace the old value in such a way that the any thread using
    the value will work.
  */
  rw_wrlock(var_mutex);
  old_value= var_str->value;
  var_str->value= res;
  var_str->value_length= new_length;
  rw_unlock(var_mutex);
  free(old_value);
  return 0;
}


static bool sys_update_init_connect(Session *, set_var *var)
{
  return update_sys_var_str(&sys_init_connect, &LOCK_sys_init_connect, var);
}


static void sys_default_init_connect(Session *, enum_var_type)
{
  update_sys_var_str(&sys_init_connect, &LOCK_sys_init_connect, 0);
}


static bool sys_update_init_slave(Session *, set_var *var)
{
  return update_sys_var_str(&sys_init_slave, &LOCK_sys_init_slave, var);
}


static void sys_default_init_slave(Session *, enum_var_type)
{
  update_sys_var_str(&sys_init_slave, &LOCK_sys_init_slave, 0);
}


/**
  If one sets the LOW_PRIORIY UPDATES flag, we also must change the
  used lock type.
*/

static void fix_low_priority_updates(Session *session, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    thr_upgraded_concurrent_insert_lock= 
      (global_system_variables.low_priority_updates ?
       TL_WRITE_LOW_PRIORITY : TL_WRITE);
  else
    session->update_lock_default= (session->variables.low_priority_updates ?
			       TL_WRITE_LOW_PRIORITY : TL_WRITE);
}


static void
fix_myisam_max_sort_file_size(Session *, enum_var_type)
{
  myisam_max_temp_length=
    (my_off_t) global_system_variables.myisam_max_sort_file_size;
}

/**
  Set the OPTION_BIG_SELECTS flag if max_join_size == HA_POS_ERROR.
*/

static void fix_max_join_size(Session *session, enum_var_type type)
{
  if (type != OPT_GLOBAL)
  {
    if (session->variables.max_join_size == HA_POS_ERROR)
      session->options|= OPTION_BIG_SELECTS;
    else
      session->options&= ~OPTION_BIG_SELECTS;
  }
}


/**
  Can't change the 'next' tx_isolation while we are already in
  a transaction
*/
static int check_tx_isolation(Session *session, set_var *var)
{
  if (var->type == OPT_DEFAULT && (session->server_status & SERVER_STATUS_IN_TRANS))
  {
    my_error(ER_CANT_CHANGE_TX_ISOLATION, MYF(0));
    return 1;
  }
  return 0;
}

/*
  If one doesn't use the SESSION modifier, the isolation level
  is only active for the next command.
*/
static void fix_tx_isolation(Session *session, enum_var_type type)
{
  if (type == OPT_SESSION)
    session->session_tx_isolation= ((enum_tx_isolation)
                                    session->variables.tx_isolation);
}

static void fix_completion_type(Session *, enum_var_type) {}

static int check_completion_type(Session *, set_var *var)
{
  int64_t val= var->value->val_int();
  if (val < 0 || val > 2)
  {
    char buf[64];
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->var->name, llstr(val, buf));
    return 1;
  }
  return 0;
}


/*
  If we are changing the thread variable, we have to copy it to NET too
*/

static void fix_net_read_timeout(Session *session, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    my_net_set_read_timeout(&session->net, session->variables.net_read_timeout);
}


static void fix_net_write_timeout(Session *session, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    my_net_set_write_timeout(&session->net, session->variables.net_write_timeout);
}

static void fix_net_retry_count(Session *session, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    session->net.retry_count=session->variables.net_retry_count;
}


extern void fix_delay_key_write(Session *, enum_var_type)
{
  switch ((enum_delay_key_write) delay_key_write_options) {
  case DELAY_KEY_WRITE_NONE:
    myisam_delay_key_write=0;
    break;
  case DELAY_KEY_WRITE_ON:
    myisam_delay_key_write=1;
    break;
  case DELAY_KEY_WRITE_ALL:
    myisam_delay_key_write=1;
    ha_open_options|= HA_OPEN_DELAY_KEY_WRITE;
    break;
  }
}

bool sys_var_set::update(Session *, set_var *var)
{
  *value= var->save_result.uint32_t_value;
  return 0;
}

unsigned char *sys_var_set::value_ptr(Session *session,
                              enum_var_type,
                              LEX_STRING *)
{
  char buff[256];
  String tmp(buff, sizeof(buff), &my_charset_utf8_general_ci);
  ulong length;
  ulong val= *value;

  tmp.length(0);
  for (uint32_t i= 0; val; val>>= 1, i++)
  {
    if (val & 1)
    {
      tmp.append(enum_names->type_names[i],
                 enum_names->type_lengths[i]);
      tmp.append(',');
    }
  }

  if ((length= tmp.length()))
    length--;
  return (unsigned char*) session->strmake(tmp.ptr(), length);
}

void sys_var_set_slave_mode::set_default(Session *, enum_var_type)
{
  slave_exec_mode_options= 0;
  bit_do_set(slave_exec_mode_options, SLAVE_EXEC_MODE_STRICT);
}

bool sys_var_set_slave_mode::check(Session *session, set_var *var)
{
  bool rc=  sys_var_set::check(session, var);
  if (!rc &&
      bit_is_set(var->save_result.uint32_t_value, SLAVE_EXEC_MODE_STRICT) == 1 &&
      bit_is_set(var->save_result.uint32_t_value, SLAVE_EXEC_MODE_IDEMPOTENT) == 1)
  {
    rc= true;
    my_error(ER_SLAVE_AMBIGOUS_EXEC_MODE, MYF(0), "");
  }
  return rc;
}

bool sys_var_set_slave_mode::update(Session *session, set_var *var)
{
  bool rc;
  pthread_mutex_lock(&LOCK_global_system_variables);
  rc= sys_var_set::update(session, var);
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return rc;
}

void fix_slave_exec_mode(enum_var_type)
{
  if (bit_is_set(slave_exec_mode_options, SLAVE_EXEC_MODE_STRICT) == 1 &&
      bit_is_set(slave_exec_mode_options, SLAVE_EXEC_MODE_IDEMPOTENT) == 1)
  {
    sql_print_error(_("Ambiguous slave modes combination."
                    " STRICT will be used"));
    bit_do_clear(slave_exec_mode_options, SLAVE_EXEC_MODE_IDEMPOTENT);
  }
  if (bit_is_set(slave_exec_mode_options, SLAVE_EXEC_MODE_IDEMPOTENT) == 0)
    bit_do_set(slave_exec_mode_options, SLAVE_EXEC_MODE_STRICT);
}


static void fix_max_binlog_size(Session *, enum_var_type)
{
  drizzle_bin_log.set_max_size(max_binlog_size);
  if (!max_relay_log_size)
    active_mi->rli.relay_log.set_max_size(max_binlog_size);
  return;
}

static void fix_max_relay_log_size(Session *, enum_var_type)
{
  active_mi->rli.relay_log.set_max_size(max_relay_log_size ?
                                        max_relay_log_size: max_binlog_size);
  return;
}

static void fix_max_connections(Session *, enum_var_type)
{
  resize_thr_alarm(max_connections +  10);
}


static void fix_session_mem_root(Session *session, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    reset_root_defaults(session->mem_root,
                        session->variables.query_alloc_block_size,
                        session->variables.query_prealloc_size);
}


static void fix_trans_mem_root(Session *session, enum_var_type type)
{
  if (type != OPT_GLOBAL)
    reset_root_defaults(&session->transaction.mem_root,
                        session->variables.trans_alloc_block_size,
                        session->variables.trans_prealloc_size);
}


static void fix_server_id(Session *session, enum_var_type)
{
  server_id_supplied = 1;
  session->server_id= server_id;
}


bool throw_bounds_warning(Session *session, bool fixed, bool unsignd,
                          const char *name, int64_t val)
{
  if (fixed)
  {
    char buf[22];

    if (unsignd)
      ullstr((uint64_t) val, buf);
    else
      llstr(val, buf);

    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), name, buf);
  }
  return false;
}

static uint64_t fix_unsigned(Session *session, uint64_t num,
                              const struct my_option *option_limits)
{
  bool fixed= false;
  uint64_t out= getopt_ull_limit_value(num, option_limits, &fixed);

  throw_bounds_warning(session, fixed, true, option_limits->name, (int64_t) num);
  return out;
}

static bool get_unsigned(Session *, set_var *var)
{
  if (var->value->unsigned_flag)
    var->save_result.uint64_t_value= (uint64_t) var->value->val_int();
  else
  {
    int64_t v= var->value->val_int();
    var->save_result.uint64_t_value= (uint64_t) ((v < 0) ? 0 : v);
  }
  return 0;
}


sys_var_long_ptr::
sys_var_long_ptr(sys_var_chain *chain, const char *name_arg, ulong *value_ptr_arg,
                 sys_after_update_func after_update_arg)
  :sys_var_long_ptr_global(chain, name_arg, value_ptr_arg,
                           &LOCK_global_system_variables, after_update_arg)
{}


bool sys_var_long_ptr_global::check(Session *session, set_var *var)
{
  return get_unsigned(session, var);
}

bool sys_var_long_ptr_global::update(Session *session, set_var *var)
{
  uint64_t tmp= var->save_result.uint64_t_value;
  pthread_mutex_lock(guard);
  if (option_limits)
    *value= (ulong) fix_unsigned(session, tmp, option_limits);
  else
  {
    if (tmp > UINT32_MAX)
    {
      tmp= UINT32_MAX;
      throw_bounds_warning(session, true, true, name,
                           (int64_t) var->save_result.uint64_t_value);
    }
    *value= (ulong) tmp;
  }

  pthread_mutex_unlock(guard);
  return 0;
}


void sys_var_long_ptr_global::set_default(Session *, enum_var_type)
{
  bool not_used;
  pthread_mutex_lock(guard);
  *value= (ulong) getopt_ull_limit_value((ulong) option_limits->def_value,
                                         option_limits, &not_used);
  pthread_mutex_unlock(guard);
}

bool sys_var_uint32_t_ptr::update(Session *session, set_var *var)
{
  uint32_t tmp= var->save_result.uint32_t_value;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (option_limits)
    *value= (uint32_t) fix_unsigned(session, tmp, option_limits);
  else
    *value= (uint32_t) tmp;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_uint32_t_ptr::set_default(Session *, enum_var_type)
{
  bool not_used;
  pthread_mutex_lock(&LOCK_global_system_variables);
  *value= getopt_ull_limit_value((uint32_t) option_limits->def_value,
                                 option_limits, &not_used);
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


bool sys_var_uint64_t_ptr::update(Session *session, set_var *var)
{
  uint64_t tmp= var->save_result.uint64_t_value;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (option_limits)
    *value= (uint64_t) fix_unsigned(session, tmp, option_limits);
  else
    *value= (uint64_t) tmp;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_uint64_t_ptr::set_default(Session *, enum_var_type)
{
  bool not_used;
  pthread_mutex_lock(&LOCK_global_system_variables);
  *value= getopt_ull_limit_value((uint64_t) option_limits->def_value,
                                 option_limits, &not_used);
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


bool sys_var_bool_ptr::update(Session *, set_var *var)
{
  *value= (bool) var->save_result.uint32_t_value;
  return 0;
}


void sys_var_bool_ptr::set_default(Session *, enum_var_type)
{
  *value= (bool) option_limits->def_value;
}


bool sys_var_enum::update(Session *, set_var *var)
{
  *value= (uint) var->save_result.uint32_t_value;
  return 0;
}


unsigned char *sys_var_enum::value_ptr(Session *, enum_var_type, LEX_STRING *)
{
  return (unsigned char*) enum_names->type_names[*value];
}


unsigned char *sys_var_enum_const::value_ptr(Session *, enum_var_type,
                                             LEX_STRING *)
{
  return (unsigned char*) enum_names->type_names[global_system_variables.*offset];
}

/*
  32 bit types for session variables 
*/
bool sys_var_session_uint32_t::check(Session *session, set_var *var)
{
  return (get_unsigned(session, var) ||
          (check_func && (*check_func)(session, var)));
}

bool sys_var_session_uint32_t::update(Session *session, set_var *var)
{
  uint64_t tmp= var->save_result.uint64_t_value;
  
  /* Don't use bigger value than given with --maximum-variable-name=.. */
  if ((uint32_t) tmp > max_system_variables.*offset)
  {
    throw_bounds_warning(session, true, true, name, (int64_t) tmp);
    tmp= max_system_variables.*offset;
  }
  
  if (option_limits)
    tmp= (uint32_t) fix_unsigned(session, tmp, option_limits);
  else if (tmp > UINT32_MAX)
  {
    tmp= UINT32_MAX;
    throw_bounds_warning(session, true, true, name, (int64_t) var->save_result.uint64_t_value);
  }
  
  if (var->type == OPT_GLOBAL)
     global_system_variables.*offset= (uint32_t) tmp;
   else
     session->variables.*offset= (uint32_t) tmp;

   return 0;
 }


 void sys_var_session_uint32_t::set_default(Session *session, enum_var_type type)
 {
   if (type == OPT_GLOBAL)
   {
     bool not_used;
     /* We will not come here if option_limits is not set */
     global_system_variables.*offset=
       (uint32_t) getopt_ull_limit_value((uint32_t) option_limits->def_value,
                                      option_limits, &not_used);
   }
   else
     session->variables.*offset= global_system_variables.*offset;
 }


unsigned char *sys_var_session_uint32_t::value_ptr(Session *session,
                                                enum_var_type type,
                                                LEX_STRING *)
{
  if (type == OPT_GLOBAL)
    return (unsigned char*) &(global_system_variables.*offset);
  return (unsigned char*) &(session->variables.*offset);
}


bool sys_var_session_ha_rows::update(Session *session, set_var *var)
{
  uint64_t tmp= var->save_result.uint64_t_value;

  /* Don't use bigger value than given with --maximum-variable-name=.. */
  if ((ha_rows) tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

  if (option_limits)
    tmp= (ha_rows) fix_unsigned(session, tmp, option_limits);
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    pthread_mutex_lock(&LOCK_global_system_variables);    
    global_system_variables.*offset= (ha_rows) tmp;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    session->variables.*offset= (ha_rows) tmp;
  return 0;
}


void sys_var_session_ha_rows::set_default(Session *session, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    bool not_used;
    /* We will not come here if option_limits is not set */
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset=
      (ha_rows) getopt_ull_limit_value((ha_rows) option_limits->def_value,
                                       option_limits, &not_used);
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    session->variables.*offset= global_system_variables.*offset;
}


unsigned char *sys_var_session_ha_rows::value_ptr(Session *session,
                                                  enum_var_type type,
                                                  LEX_STRING *)
{
  if (type == OPT_GLOBAL)
    return (unsigned char*) &(global_system_variables.*offset);
  return (unsigned char*) &(session->variables.*offset);
}

bool sys_var_session_uint64_t::check(Session *session, set_var *var)
{
  return (get_unsigned(session, var) ||
	  (check_func && (*check_func)(session, var)));
}

bool sys_var_session_uint64_t::update(Session *session,  set_var *var)
{
  uint64_t tmp= var->save_result.uint64_t_value;

  if (tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

  if (option_limits)
    tmp= fix_unsigned(session, tmp, option_limits);
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= (uint64_t) tmp;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    session->variables.*offset= (uint64_t) tmp;
  return 0;
}


void sys_var_session_uint64_t::set_default(Session *session, enum_var_type type)
{
  if (type == OPT_GLOBAL)
  {
    bool not_used;
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset=
      getopt_ull_limit_value((uint64_t) option_limits->def_value,
                             option_limits, &not_used);
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    session->variables.*offset= global_system_variables.*offset;
}


unsigned char *sys_var_session_uint64_t::value_ptr(Session *session,
                                                   enum_var_type type,
                                                   LEX_STRING *)
{
  if (type == OPT_GLOBAL)
    return (unsigned char*) &(global_system_variables.*offset);
  return (unsigned char*) &(session->variables.*offset);
}


bool sys_var_session_bool::update(Session *session,  set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= (bool) var->save_result.uint32_t_value;
  else
    session->variables.*offset= (bool) var->save_result.uint32_t_value;
  return 0;
}


void sys_var_session_bool::set_default(Session *session,  enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= (bool) option_limits->def_value;
  else
    session->variables.*offset= global_system_variables.*offset;
}


unsigned char *sys_var_session_bool::value_ptr(Session *session,
                                               enum_var_type type,
                                               LEX_STRING *)
{
  if (type == OPT_GLOBAL)
    return (unsigned char*) &(global_system_variables.*offset);
  return (unsigned char*) &(session->variables.*offset);
}


bool sys_var::check_enum(Session *,
                         set_var *var, const TYPELIB *enum_names)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *value;
  String str(buff, sizeof(buff), system_charset_info), *res;

  if (var->value->result_type() == STRING_RESULT)
  {
    if (!(res=var->value->val_str(&str)))
    {
      value= res ? res->c_ptr() : "NULL";
      goto err;
    }
  }
  else
  {
    uint64_t tmp=var->value->val_int();
    if (tmp >= enum_names->count)
    {
      llstr(tmp,buff);
      value=buff;				// Wrong value is here
      goto err;
    }
    var->save_result.uint32_t_value= (uint32_t) tmp;	// Save for update
  }
  return 0;

err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, value);
  return 1;
}


bool sys_var::check_set(Session *, set_var *var, TYPELIB *enum_names)
{
  bool not_used;
  char buff[STRING_BUFFER_USUAL_SIZE], *error= 0;
  uint32_t error_len= 0;
  String str(buff, sizeof(buff), system_charset_info), *res;

  if (var->value->result_type() == STRING_RESULT)
  {
    if (!(res= var->value->val_str(&str)))
    {
      my_stpcpy(buff, "NULL");
      goto err;
    }

    if (!m_allow_empty_value &&
        res->length() == 0)
    {
      buff[0]= 0;
      goto err;
    }

    var->save_result.uint32_t_value= ((uint32_t)
				   find_set(enum_names, res->c_ptr(),
					    res->length(),
                                            NULL,
                                            &error, &error_len,
					    &not_used));
    if (error_len)
    {
      strmake(buff, error, cmin(sizeof(buff) - 1, (ulong)error_len));
      goto err;
    }
  }
  else
  {
    uint64_t tmp= var->value->val_int();

    if (!m_allow_empty_value &&
        tmp == 0)
    {
      buff[0]= '0';
      buff[1]= 0;
      goto err;
    }

    /*
      For when the enum is made to contain 64 elements, as 1ULL<<64 is
      undefined, we guard with a "count<64" test.
    */
    if (unlikely((tmp >= ((1UL) << enum_names->count)) &&
                 (enum_names->count < 64)))
    {
      llstr(tmp, buff);
      goto err;
    }
    var->save_result.uint32_t_value= (uint32_t) tmp;  // Save for update
  }
  return 0;

err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, buff);
  return 1;
}


/**
  Return an Item for a variable.

  Used with @@[global.]variable_name.

  If type is not given, return local value if exists, else global.
*/

Item *sys_var::item(Session *session, enum_var_type var_type, LEX_STRING *base)
{
  if (check_type(var_type))
  {
    if (var_type != OPT_DEFAULT)
    {
      my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0),
               name, var_type == OPT_GLOBAL ? "SESSION" : "GLOBAL");
      return 0;
    }
    /* As there was no local variable, return the global value */
    var_type= OPT_GLOBAL;
  }
  switch (show_type()) {
  case SHOW_INT:
  {
    uint32_t value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(uint*) value_ptr(session, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_uint((uint64_t) value);
  }
  case SHOW_LONG:
  {
    uint32_t value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(uint32_t*) value_ptr(session, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_uint((uint64_t) value);
  }
  case SHOW_LONGLONG:
  {
    int64_t value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(int64_t*) value_ptr(session, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int(value);
  }
  case SHOW_DOUBLE:
  {
    double value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(double*) value_ptr(session, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    /* 6, as this is for now only used with microseconds */
    return new Item_float(value, 6);
  }
  case SHOW_HA_ROWS:
  {
    ha_rows value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(ha_rows*) value_ptr(session, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int((uint64_t) value);
  }
  case SHOW_MY_BOOL:
  {
    int32_t value;
    pthread_mutex_lock(&LOCK_global_system_variables);
    value= *(bool*) value_ptr(session, var_type, base);
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return new Item_int(value,1);
  }
  case SHOW_CHAR_PTR:
  {
    Item *tmp;
    pthread_mutex_lock(&LOCK_global_system_variables);
    char *str= *(char**) value_ptr(session, var_type, base);
    if (str)
    {
      uint32_t length= strlen(str);
      tmp= new Item_string(session->strmake(str, length), length,
                           system_charset_info, DERIVATION_SYSCONST);
    }
    else
    {
      tmp= new Item_null();
      tmp->collation.set(system_charset_info, DERIVATION_SYSCONST);
    }
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return tmp;
  }
  case SHOW_CHAR:
  {
    Item *tmp;
    pthread_mutex_lock(&LOCK_global_system_variables);
    char *str= (char*) value_ptr(session, var_type, base);
    if (str)
      tmp= new Item_string(str, strlen(str),
                           system_charset_info, DERIVATION_SYSCONST);
    else
    {
      tmp= new Item_null();
      tmp->collation.set(system_charset_info, DERIVATION_SYSCONST);
    }
    pthread_mutex_unlock(&LOCK_global_system_variables);
    return tmp;
  }
  default:
    my_error(ER_VAR_CANT_BE_READ, MYF(0), name);
  }
  return 0;
}


bool sys_var_session_enum::update(Session *session, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= var->save_result.uint32_t_value;
  else
    session->variables.*offset= var->save_result.uint32_t_value;
  return 0;
}


void sys_var_session_enum::set_default(Session *session, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= (ulong) option_limits->def_value;
  else
    session->variables.*offset= global_system_variables.*offset;
}


unsigned char *sys_var_session_enum::value_ptr(Session *session,
                                               enum_var_type type,
                                               LEX_STRING *)
{
  ulong tmp= ((type == OPT_GLOBAL) ?
	      global_system_variables.*offset :
	      session->variables.*offset);
  return (unsigned char*) enum_names->type_names[tmp];
}

bool sys_var_session_bit::check(Session *session, set_var *var)
{
  return (check_enum(session, var, &bool_typelib) ||
          (check_func && (*check_func)(session, var)));
}

bool sys_var_session_bit::update(Session *session, set_var *var)
{
  int res= (*update_func)(session, var);
  return res;
}


unsigned char *sys_var_session_bit::value_ptr(Session *session, enum_var_type,
                                              LEX_STRING *)
{
  /*
    If reverse is 0 (default) return 1 if bit is set.
    If reverse is 1, return 0 if bit is set
  */
  session->sys_var_tmp.bool_value= ((session->options & bit_flag) ?
				   !reverse : reverse);
  return (unsigned char*) &session->sys_var_tmp.bool_value;
}


/** Update a date_time format variable based on given value. */

void sys_var_session_date_time_format::update2(Session *session, enum_var_type type,
					   DATE_TIME_FORMAT *new_value)
{
  DATE_TIME_FORMAT *old;

  if (type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    old= (global_system_variables.*offset);
    (global_system_variables.*offset)= new_value;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
  {
    old= (session->variables.*offset);
    (session->variables.*offset)= new_value;
  }
  free((char*) old);
  return;
}


bool sys_var_session_date_time_format::update(Session *session, set_var *var)
{
  DATE_TIME_FORMAT *new_value;
  /* We must make a copy of the last value to get it into normal memory */
  new_value= date_time_format_copy((Session*) 0,
				   var->save_result.date_time_format);
  if (!new_value)
    return 1;					// Out of memory
  update2(session, var->type, new_value);		// Can't fail
  return 0;
}


bool sys_var_session_date_time_format::check(Session *session, set_var *var)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  String str(buff,sizeof(buff), system_charset_info), *res;
  DATE_TIME_FORMAT *format;

  if (!(res=var->value->val_str(&str)))
    res= &my_empty_string;

  if (!(format= date_time_format_make(date_time_type,
				      res->ptr(), res->length())))
  {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, res->c_ptr());
    return 1;
  }
  
  /*
    We must copy result to thread space to not get a memory leak if
    update is aborted
  */
  var->save_result.date_time_format= date_time_format_copy(session, format);
  free((char*) format);
  return var->save_result.date_time_format == 0;
}


void sys_var_session_date_time_format::set_default(Session *session, enum_var_type type)
{
  DATE_TIME_FORMAT *res= 0;

  if (type == OPT_GLOBAL)
  {
    const char *format;
    if ((format= opt_date_time_formats[date_time_type]))
      res= date_time_format_make(date_time_type, format, strlen(format));
  }
  else
  {
    /* Make copy with malloc */
    res= date_time_format_copy((Session *) 0, global_system_variables.*offset);
  }

  if (res)					// Should always be true
    update2(session, type, res);
}


unsigned char *sys_var_session_date_time_format::value_ptr(Session *session,
                                                           enum_var_type type,
                                                           LEX_STRING *)
{
  if (type == OPT_GLOBAL)
  {
    char *res;
    /*
      We do a copy here just to be sure things will work even if someone
      is modifying the original string while the copy is accessed
      (Can't happen now in SQL SHOW, but this is a good safety for the future)
    */
    res= session->strmake((global_system_variables.*offset)->format.str,
		      (global_system_variables.*offset)->format.length);
    return (unsigned char*) res;
  }
  return (unsigned char*) (session->variables.*offset)->format.str;
}


typedef struct old_names_map_st
{
  const char *old_name;
  const char *new_name;
} my_old_conv;

bool sys_var_collation::check(Session *, set_var *var)
{
  const CHARSET_INFO *tmp;

  if (var->value->result_type() == STRING_RESULT)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff,sizeof(buff), system_charset_info), *res;
    if (!(res=var->value->val_str(&str)))
    {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, "NULL");
      return 1;
    }
    if (!(tmp=get_charset_by_name(res->c_ptr(),MYF(0))))
    {
      my_error(ER_UNKNOWN_COLLATION, MYF(0), res->c_ptr());
      return 1;
    }
  }
  else // INT_RESULT
  {
    if (!(tmp=get_charset((int) var->value->val_int(),MYF(0))))
    {
      char buf[20];
      int10_to_str((int) var->value->val_int(), buf, -10);
      my_error(ER_UNKNOWN_COLLATION, MYF(0), buf);
      return 1;
    }
  }
  var->save_result.charset= tmp;	// Save for update
  return 0;
}


bool sys_var_character_set::check(Session *, set_var *var)
{
  const CHARSET_INFO *tmp;

  if (var->value->result_type() == STRING_RESULT)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String str(buff,sizeof(buff), system_charset_info), *res;
    if (!(res=var->value->val_str(&str)))
    {
      if (!nullable)
      {
        my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, "NULL");
        return 1;
      }
      tmp= NULL;
    }
    else if (!(tmp= get_charset_by_csname(res->c_ptr(),MY_CS_PRIMARY,MYF(0))))
    {
      my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), res->c_ptr());
      return 1;
    }
  }
  else // INT_RESULT
  {
    if (!(tmp=get_charset((int) var->value->val_int(),MYF(0))))
    {
      char buf[20];
      int10_to_str((int) var->value->val_int(), buf, -10);
      my_error(ER_UNKNOWN_CHARACTER_SET, MYF(0), buf);
      return 1;
    }
  }
  var->save_result.charset= tmp;	// Save for update
  return 0;
}


bool sys_var_character_set::update(Session *session, set_var *var)
{
  ci_ptr(session,var->type)[0]= var->save_result.charset;
  session->update_charset();
  return 0;
}


unsigned char *sys_var_character_set::value_ptr(Session *session,
                                                enum_var_type type,
                                                LEX_STRING *)
{
  const CHARSET_INFO * const cs= ci_ptr(session,type)[0];
  return cs ? (unsigned char*) cs->csname : (unsigned char*) NULL;
}


bool sys_var_collation_sv::update(Session *session, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= var->save_result.charset;
  else
  {
    session->variables.*offset= var->save_result.charset;
    session->update_charset();
  }
  return 0;
}


void sys_var_collation_sv::set_default(Session *session, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= *global_default;
  else
  {
    session->variables.*offset= global_system_variables.*offset;
    session->update_charset();
  }
}


unsigned char *sys_var_collation_sv::value_ptr(Session *session,
                                               enum_var_type type,
                                               LEX_STRING *)
{
  const CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
                           global_system_variables.*offset :
                           session->variables.*offset);
  return cs ? (unsigned char*) cs->name : (unsigned char*) "NULL";
}


LEX_STRING default_key_cache_base= {(char *) "default", 7 };

static KEY_CACHE zero_key_cache;

KEY_CACHE *get_key_cache(LEX_STRING *cache_name)
{
  safe_mutex_assert_owner(&LOCK_global_system_variables);
  if (!cache_name || ! cache_name->length)
    cache_name= &default_key_cache_base;
  return ((KEY_CACHE*) find_named(&key_caches,
                                      cache_name->str, cache_name->length, 0));
}


unsigned char *sys_var_key_cache_param::value_ptr(Session *, enum_var_type,
                                                  LEX_STRING *base)
{
  KEY_CACHE *key_cache= get_key_cache(base);
  if (!key_cache)
    key_cache= &zero_key_cache;
  return (unsigned char*) key_cache + offset ;
}


bool sys_var_key_buffer_size::update(Session *session, set_var *var)
{
  uint64_t tmp= var->save_result.uint64_t_value;
  LEX_STRING *base_name= &var->base;
  KEY_CACHE *key_cache;
  bool error= 0;

  /* If no basename, assume it's for the key cache named 'default' */
  if (!base_name->length)
    base_name= &default_key_cache_base;

  pthread_mutex_lock(&LOCK_global_system_variables);
  key_cache= get_key_cache(base_name);
                            
  if (!key_cache)
  {
    /* Key cache didn't exists */
    if (!tmp)					// Tried to delete cache
      goto end;					// Ok, nothing to do
    if (!(key_cache= create_key_cache(base_name->str, base_name->length)))
    {
      error= 1;
      goto end;
    }
  }

  /*
    Abort if some other thread is changing the key cache
    TODO: This should be changed so that we wait until the previous
    assignment is done and then do the new assign
  */
  if (key_cache->in_init)
    goto end;

  if (!tmp)					// Zero size means delete
  {
    if (key_cache == dflt_key_cache)
    {
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_CANT_DROP_DEFAULT_KEYCACHE,
                          ER(ER_WARN_CANT_DROP_DEFAULT_KEYCACHE));
      goto end;					// Ignore default key cache
    }

    if (key_cache->key_cache_inited)		// If initied
    {
      /*
	Move tables using this key cache to the default key cache
	and clear the old key cache.
      */
      NAMED_LIST *list; 
      key_cache= (KEY_CACHE *) find_named(&key_caches, base_name->str,
					      base_name->length, &list);
      key_cache->in_init= 1;
      pthread_mutex_unlock(&LOCK_global_system_variables);
      error= reassign_keycache_tables(session, key_cache, dflt_key_cache);
      pthread_mutex_lock(&LOCK_global_system_variables);
      key_cache->in_init= 0;
    }
    /*
      We don't delete the key cache as some running threads my still be
      in the key cache code with a pointer to the deleted (empty) key cache
    */
    goto end;
  }

  key_cache->param_buff_size=
    (uint64_t) fix_unsigned(session, tmp, option_limits);

  /* If key cache didn't existed initialize it, else resize it */
  key_cache->in_init= 1;
  pthread_mutex_unlock(&LOCK_global_system_variables);

  if (!key_cache->key_cache_inited)
    error= (bool) (ha_init_key_cache("", key_cache));
  else
    error= (bool)(ha_resize_key_cache(key_cache));

  pthread_mutex_lock(&LOCK_global_system_variables);
  key_cache->in_init= 0;  

end:
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return error;
}


/**
  @todo
  Abort if some other thread is changing the key cache.
  This should be changed so that we wait until the previous
  assignment is done and then do the new assign
*/
bool sys_var_key_cache_long::update(Session *session, set_var *var)
{
  ulong tmp= (ulong) var->value->val_int();
  LEX_STRING *base_name= &var->base;
  bool error= 0;

  if (!base_name->length)
    base_name= &default_key_cache_base;

  pthread_mutex_lock(&LOCK_global_system_variables);
  KEY_CACHE *key_cache= get_key_cache(base_name);

  if (!key_cache && !(key_cache= create_key_cache(base_name->str,
				                  base_name->length)))
  {
    error= 1;
    goto end;
  }

  /*
    Abort if some other thread is changing the key cache
    TODO: This should be changed so that we wait until the previous
    assignment is done and then do the new assign
  */
  if (key_cache->in_init)
    goto end;

  *((ulong*) (((char*) key_cache) + offset))=
    (ulong) fix_unsigned(session, tmp, option_limits);

  /*
    Don't create a new key cache if it didn't exist
    (key_caches are created only when the user sets block_size)
  */
  key_cache->in_init= 1;

  pthread_mutex_unlock(&LOCK_global_system_variables);

  error= (bool) (ha_resize_key_cache(key_cache));

  pthread_mutex_lock(&LOCK_global_system_variables);
  key_cache->in_init= 0;

end:
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return error;
}


bool sys_var_log_state::update(Session *, set_var *var)
{
  bool res;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (!var->save_result.uint32_t_value)
    res= false;
  else
    res= true;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return res;
}

void sys_var_log_state::set_default(Session *, enum_var_type)
{
}


bool update_sys_var_str_path(Session *, sys_var_str *var_str,
                             set_var *var, const char *log_ext,
                             bool log_state, uint32_t log_type)
{
  char buff[FN_REFLEN];
  char *res= 0, *old_value=(char *)(var ? var->value->str_value.ptr() : 0);
  bool result= 0;
  uint32_t str_length= (var ? var->value->str_value.length() : 0);

  switch (log_type) {
  default:
    assert(0);                                  // Impossible
  }

  if (!old_value)
  {
    old_value= make_default_log_name(buff, log_ext);
    str_length= strlen(old_value);
  }
  if (!(res= my_strndup(old_value, str_length, MYF(MY_FAE+MY_WME))))
  {
    result= 1;
    goto err;
  }

  pthread_mutex_lock(&LOCK_global_system_variables);
  logger.lock_exclusive();

  old_value= var_str->value;
  var_str->value= res;
  var_str->value_length= str_length;
  free(old_value);
  if (log_state)
  {
    switch (log_type) {
    default:
      assert(0);
    }
  }

  logger.unlock();
  pthread_mutex_unlock(&LOCK_global_system_variables);

err:
  return result;
}


bool sys_var_log_output::update(Session *, set_var *var)
{
  pthread_mutex_lock(&LOCK_global_system_variables);
  logger.lock_exclusive();
  *value= var->save_result.uint32_t_value;
  logger.unlock();
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return 0;
}


void sys_var_log_output::set_default(Session *, enum_var_type)
{
  pthread_mutex_lock(&LOCK_global_system_variables);
  logger.lock_exclusive();
  *value= LOG_FILE;
  logger.unlock();
  pthread_mutex_unlock(&LOCK_global_system_variables);
}


unsigned char *sys_var_log_output::value_ptr(Session *session,
                                             enum_var_type, LEX_STRING *)
{
  char buff[256];
  String tmp(buff, sizeof(buff), &my_charset_utf8_general_ci);
  ulong length;
  ulong val= *value;

  tmp.length(0);
  for (uint32_t i= 0; val; val>>= 1, i++)
  {
    if (val & 1)
    {
      tmp.append(log_output_typelib.type_names[i],
                 log_output_typelib.type_lengths[i]);
      tmp.append(',');
    }
  }

  if ((length= tmp.length()))
    length--;
  return (unsigned char*) session->strmake(tmp.ptr(), length);
}


/*****************************************************************************
  Functions to handle SET NAMES and SET CHARACTER SET
*****************************************************************************/

int set_var_collation_client::check(Session *)
{
  /* Currently, UCS-2 cannot be used as a client character set */
  if (character_set_client->mbminlen > 1)
  {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "character_set_client",
             character_set_client->csname);
    return 1;
  }
  return 0;
}

int set_var_collation_client::update(Session *session)
{
  session->variables.character_set_client= character_set_client;
  session->variables.character_set_results= character_set_results;
  session->variables.collation_connection= collation_connection;
  session->update_charset();
  session->protocol_text.init(session);
  return 0;
}

/****************************************************************************/

bool sys_var_timestamp::update(Session *session,  set_var *var)
{
  session->set_time((time_t) var->save_result.uint64_t_value);
  return 0;
}


void sys_var_timestamp::set_default(Session *session, enum_var_type)
{
  session->user_time=0;
}


unsigned char *sys_var_timestamp::value_ptr(Session *session, enum_var_type,
                                            LEX_STRING *)
{
  session->sys_var_tmp.long_value= (long) session->start_time;
  return (unsigned char*) &session->sys_var_tmp.long_value;
}


bool sys_var_last_insert_id::update(Session *session, set_var *var)
{
  session->first_successful_insert_id_in_prev_stmt=
    var->save_result.uint64_t_value;
  return 0;
}


unsigned char *sys_var_last_insert_id::value_ptr(Session *session,
                                                 enum_var_type,
                                                 LEX_STRING *)
{
  /*
    this tmp var makes it robust againt change of type of
    read_first_successful_insert_id_in_prev_stmt().
  */
  session->sys_var_tmp.uint64_t_value=
    session->read_first_successful_insert_id_in_prev_stmt();
  return (unsigned char*) &session->sys_var_tmp.uint64_t_value;
}


bool sys_var_insert_id::update(Session *session, set_var *var)
{
  session->force_one_auto_inc_interval(var->save_result.uint64_t_value);
  return 0;
}


unsigned char *sys_var_insert_id::value_ptr(Session *session, enum_var_type,
                                            LEX_STRING *)
{
  session->sys_var_tmp.uint64_t_value=
    session->auto_inc_intervals_forced.minimum();
  return (unsigned char*) &session->sys_var_tmp.uint64_t_value;
}


bool sys_var_rand_seed1::update(Session *session, set_var *var)
{
  session->rand.seed1= (ulong) var->save_result.uint64_t_value;
  return 0;
}

bool sys_var_rand_seed2::update(Session *session, set_var *var)
{
  session->rand.seed2= (ulong) var->save_result.uint64_t_value;
  return 0;
}


bool sys_var_session_time_zone::check(Session *session, set_var *var)
{
  char buff[MAX_TIME_ZONE_NAME_LENGTH];
  String str(buff, sizeof(buff), &my_charset_utf8_general_ci);
  String *res= var->value->val_str(&str);

  if (!(var->save_result.time_zone= my_tz_find(session, res)))
  {
    my_error(ER_UNKNOWN_TIME_ZONE, MYF(0), res ? res->c_ptr() : "NULL");
    return 1;
  }
  return 0;
}


bool sys_var_session_time_zone::update(Session *session, set_var *var)
{
  /* We are using Time_zone object found during check() phase. */
  if (var->type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.time_zone= var->save_result.time_zone;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    session->variables.time_zone= var->save_result.time_zone;
  return 0;
}


unsigned char *sys_var_session_time_zone::value_ptr(Session *session,
                                                    enum_var_type type,
                                                    LEX_STRING *)
{
  /*
    We can use ptr() instead of c_ptr() here because String contaning
    time zone name is guaranteed to be zero ended.
  */
  if (type == OPT_GLOBAL)
    return (unsigned char *)(global_system_variables.time_zone->get_name()->ptr());
  else
  {
    /*
      This is an ugly fix for replication: we don't replicate properly queries
      invoking system variables' values to update tables; but
      CONVERT_TZ(,,@@session.time_zone) is so popular that we make it
      replicable (i.e. we tell the binlog code to store the session
      timezone). If it's the global value which was used we can't replicate
      (binlog code stores session value only).
    */
    return (unsigned char *)(session->variables.time_zone->get_name()->ptr());
  }
}


void sys_var_session_time_zone::set_default(Session *session, enum_var_type type)
{
 pthread_mutex_lock(&LOCK_global_system_variables);
 if (type == OPT_GLOBAL)
 {
   if (default_tz_name)
   {
     String str(default_tz_name, &my_charset_utf8_general_ci);
     /*
       We are guaranteed to find this time zone since its existence
       is checked during start-up.
     */
     global_system_variables.time_zone= my_tz_find(session, &str);
   }
   else
     global_system_variables.time_zone= my_tz_SYSTEM;
 }
 else
   session->variables.time_zone= global_system_variables.time_zone;
 pthread_mutex_unlock(&LOCK_global_system_variables);
}


bool sys_var_session_lc_time_names::check(Session *, set_var *var)
{
  MY_LOCALE *locale_match;

  if (var->value->result_type() == INT_RESULT)
  {
    if (!(locale_match= my_locale_by_number((uint) var->value->val_int())))
    {
      char buf[20];
      int10_to_str((int) var->value->val_int(), buf, -10);
      my_printf_error(ER_UNKNOWN_ERROR, "Unknown locale: '%s'", MYF(0), buf);
      return 1;
    }
  }
  else // STRING_RESULT
  {
    char buff[6]; 
    String str(buff, sizeof(buff), &my_charset_utf8_general_ci), *res;
    if (!(res=var->value->val_str(&str)))
    {
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name, "NULL");
      return 1;
    }
    const char *locale_str= res->c_ptr();
    if (!(locale_match= my_locale_by_name(locale_str)))
    {
      my_printf_error(ER_UNKNOWN_ERROR,
                      "Unknown locale: '%s'", MYF(0), locale_str);
      return 1;
    }
  }

  var->save_result.locale_value= locale_match;
  return 0;
}


bool sys_var_session_lc_time_names::update(Session *session, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.lc_time_names= var->save_result.locale_value;
  else
    session->variables.lc_time_names= var->save_result.locale_value;
  return 0;
}


unsigned char *sys_var_session_lc_time_names::value_ptr(Session *session,
                                                        enum_var_type type,
                                                        LEX_STRING *)
{
  return type == OPT_GLOBAL ?
                 (unsigned char *) global_system_variables.lc_time_names->name :
                 (unsigned char *) session->variables.lc_time_names->name;
}


void sys_var_session_lc_time_names::set_default(Session *session, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.lc_time_names= my_default_lc_time_names;
  else
    session->variables.lc_time_names= global_system_variables.lc_time_names;
}

/*
  Handling of microseoncds given as seconds.part_seconds

  NOTES
    The argument to long query time is in seconds in decimal
    which is converted to uint64_t integer holding microseconds for storage.
    This is used for handling long_query_time
*/

bool sys_var_microseconds::update(Session *session, set_var *var)
{
  double num= var->value->val_real();
  int64_t microseconds;
  if (num > (double) option_limits->max_value)
    num= (double) option_limits->max_value;
  if (num < (double) option_limits->min_value)
    num= (double) option_limits->min_value;
  microseconds= (int64_t) (num * 1000000.0 + 0.5);
  if (var->type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    (global_system_variables.*offset)= microseconds;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    session->variables.*offset= microseconds;
  return 0;
}


void sys_var_microseconds::set_default(Session *session, enum_var_type type)
{
  int64_t microseconds= (int64_t) (option_limits->def_value * 1000000.0);
  if (type == OPT_GLOBAL)
  {
    pthread_mutex_lock(&LOCK_global_system_variables);
    global_system_variables.*offset= microseconds;
    pthread_mutex_unlock(&LOCK_global_system_variables);
  }
  else
    session->variables.*offset= microseconds;
}


unsigned char *sys_var_microseconds::value_ptr(Session *session,
                                               enum_var_type type,
                                               LEX_STRING *)
{
  session->tmp_double_value= (double) ((type == OPT_GLOBAL) ?
                                   global_system_variables.*offset :
                                   session->variables.*offset) / 1000000.0;
  return (unsigned char*) &session->tmp_double_value;
}


/*
  Functions to update session->options bits
*/

static bool set_option_bit(Session *session, set_var *var)
{
  sys_var_session_bit *sys_var= ((sys_var_session_bit*) var->var);
  if ((var->save_result.uint32_t_value != 0) == sys_var->reverse)
    session->options&= ~sys_var->bit_flag;
  else
    session->options|= sys_var->bit_flag;
  return 0;
}


static bool set_option_autocommit(Session *session, set_var *var)
{
  /* The test is negative as the flag we use is NOT autocommit */

  uint64_t org_options= session->options;

  if (var->save_result.uint32_t_value != 0)
    session->options&= ~((sys_var_session_bit*) var->var)->bit_flag;
  else
    session->options|= ((sys_var_session_bit*) var->var)->bit_flag;

  if ((org_options ^ session->options) & OPTION_NOT_AUTOCOMMIT)
  {
    if ((org_options & OPTION_NOT_AUTOCOMMIT))
    {
      /* We changed to auto_commit mode */
      session->options&= ~(uint64_t) (OPTION_BEGIN | OPTION_KEEP_LOG);
      session->transaction.all.modified_non_trans_table= false;
      session->server_status|= SERVER_STATUS_AUTOCOMMIT;
      if (ha_commit(session))
	return 1;
    }
    else
    {
      session->transaction.all.modified_non_trans_table= false;
      session->server_status&= ~SERVER_STATUS_AUTOCOMMIT;
    }
  }
  return 0;
}

static int check_log_update(Session *, set_var *)
{
  return 0;
}


static int check_pseudo_thread_id(Session *, set_var *var)
{
  var->save_result.uint64_t_value= var->value->val_int();
  return 0;
}

static unsigned char *get_warning_count(Session *session)
{
  session->sys_var_tmp.long_value=
    (session->warn_count[(uint) DRIZZLE_ERROR::WARN_LEVEL_NOTE] +
     session->warn_count[(uint) DRIZZLE_ERROR::WARN_LEVEL_ERROR] +
     session->warn_count[(uint) DRIZZLE_ERROR::WARN_LEVEL_WARN]);
  return (unsigned char*) &session->sys_var_tmp.long_value;
}

static unsigned char *get_error_count(Session *session)
{
  session->sys_var_tmp.long_value= 
    session->warn_count[(uint) DRIZZLE_ERROR::WARN_LEVEL_ERROR];
  return (unsigned char*) &session->sys_var_tmp.long_value;
}


/**
  Get the tmpdir that was specified or chosen by default.

  This is necessary because if the user does not specify a temporary
  directory via the command line, one is chosen based on the environment
  or system defaults.  But we can't just always use drizzle_tmpdir, because
  that is actually a call to my_tmpdir() which cycles among possible
  temporary directories.

  @param session		thread handle

  @retval
    ptr		pointer to NUL-terminated string
*/
static unsigned char *get_tmpdir(Session *)
{
  if (opt_drizzle_tmpdir)
    return (unsigned char *)opt_drizzle_tmpdir;
  return (unsigned char*)drizzle_tmpdir;
}

/****************************************************************************
  Main handling of variables:
  - Initialisation
  - Searching during parsing
  - Update loop
****************************************************************************/

/**
  Find variable name in option my_getopt structure used for
  command line args.

  @param opt	option structure array to search in
  @param name	variable name

  @retval
    0		Error
  @retval
    ptr		pointer to option structure
*/

static struct my_option *find_option(struct my_option *opt, const char *name) 
{
  uint32_t length=strlen(name);
  for (; opt->name; opt++)
  {
    if (!getopt_compare_strings(opt->name, name, length) &&
	!opt->name[length])
    {
      /*
	Only accept the option if one can set values through it.
	If not, there is no default value or limits in the option.
      */
      return (opt->value) ? opt : 0;
    }
  }
  return 0;
}


/**
  Return variable name and length for hashing of variables.
*/

static unsigned char *get_sys_var_length(const sys_var *var, size_t *length,
                                         bool)
{
  *length= var->name_length;
  return (unsigned char*) var->name;
}


/*
  Add variables to the dynamic hash of system variables
  
  SYNOPSIS
    mysql_add_sys_var_chain()
    first       Pointer to first system variable to add
    long_opt    (optional)command line arguments may be tied for limit checks.
  
  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/


int mysql_add_sys_var_chain(sys_var *first, struct my_option *long_options)
{
  sys_var *var;
  
  /* A write lock should be held on LOCK_system_variables_hash */
  
  for (var= first; var; var= var->next)
  {
    var->name_length= strlen(var->name);
    /* this fails if there is a conflicting variable name. see HASH_UNIQUE */
    if (my_hash_insert(&system_variable_hash, (unsigned char*) var))
      goto error;
    if (long_options)
      var->option_limits= find_option(long_options, var->name);
  }
  return 0;

error:
  for (; first != var; first= first->next)
    hash_delete(&system_variable_hash, (unsigned char*) first);
  return 1;
}
 
 
/*
  Remove variables to the dynamic hash of system variables
   
  SYNOPSIS
    mysql_del_sys_var_chain()
    first       Pointer to first system variable to remove
   
  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/
 
int mysql_del_sys_var_chain(sys_var *first)
{
  int result= 0;
 
  /* A write lock should be held on LOCK_system_variables_hash */
   
  for (sys_var *var= first; var; var= var->next)
    result|= hash_delete(&system_variable_hash, (unsigned char*) var);

  return result;
}
 
 
static int show_cmp(SHOW_VAR *a, SHOW_VAR *b)
{
  return strcmp(a->name, b->name);
}
 
 
/*
  Constructs an array of system variables for display to the user.
  
  SYNOPSIS
    enumerate_sys_vars()
    session         current thread
    sorted      If TRUE, the system variables should be sorted
  
  RETURN VALUES
    pointer     Array of SHOW_VAR elements for display
    NULL        FAILURE
*/

SHOW_VAR* enumerate_sys_vars(Session *session, bool sorted)
{
  int count= system_variable_hash.records, i;
  int fixed_count= fixed_show_vars.elements;
  int size= sizeof(SHOW_VAR) * (count + fixed_count + 1);
  SHOW_VAR *result= (SHOW_VAR*) session->alloc(size);

  if (result)
  {
    SHOW_VAR *show= result + fixed_count;
    memcpy(result, fixed_show_vars.buffer, fixed_count * sizeof(SHOW_VAR));

    for (i= 0; i < count; i++)
    {
      sys_var *var= (sys_var*) hash_element(&system_variable_hash, i);
      show->name= var->name;
      show->value= (char*) var;
      show->type= SHOW_SYS;
      show++;
    }

    /* sort into order */
    if (sorted)
      my_qsort(result, count + fixed_count, sizeof(SHOW_VAR),
               (qsort_cmp) show_cmp);
    
    /* make last element empty */
    memset(show, 0, sizeof(SHOW_VAR));
  }
  return result;
}


/*
  Initialize the system variables
  
  SYNOPSIS
    set_var_init()
  
  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/

int set_var_init()
{
  uint32_t count= 0;
  
  for (sys_var *var=vars.first; var; var= var->next, count++) {};

  if (my_init_dynamic_array(&fixed_show_vars, sizeof(SHOW_VAR),
                            FIXED_VARS_SIZE + 64, 64))
    goto error;

  fixed_show_vars.elements= FIXED_VARS_SIZE;
  memcpy(fixed_show_vars.buffer, fixed_vars, sizeof(fixed_vars));

  if (hash_init(&system_variable_hash, system_charset_info, count, 0,
                0, (hash_get_key) get_sys_var_length, 0, HASH_UNIQUE))
    goto error;

  vars.last->next= NULL;
  if (mysql_add_sys_var_chain(vars.first, my_long_options))
    goto error;

  return(0);

error:
  fprintf(stderr, "failed to initialize system variables");
  return(1);
}


void set_var_free()
{
  hash_free(&system_variable_hash);
  delete_dynamic(&fixed_show_vars);
}


/*
  Add elements to the dynamic list of read-only system variables.
  
  SYNOPSIS
    mysql_append_static_vars()
    show_vars	Pointer to start of array
    count       Number of elements
  
  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/
int mysql_append_static_vars(const SHOW_VAR *show_vars, uint32_t count)
{
  for (; count > 0; count--, show_vars++)
    if (insert_dynamic(&fixed_show_vars, (unsigned char*) show_vars))
      return 1;
  return 0;
}


/**
  Find a user set-table variable.

  @param str	   Name of system variable to find
  @param length    Length of variable.  zero means that we should use strlen()
                   on the variable
  @param no_error  Refuse to emit an error, even if one occurred.

  @retval
    pointer	pointer to variable definitions
  @retval
    0		Unknown variable (error message is given)
*/

sys_var *intern_find_sys_var(const char *str, uint32_t length, bool no_error)
{
  sys_var *var;

  /*
    This function is only called from the sql_plugin.cc.
    A lock on LOCK_system_variable_hash should be held
  */
  var= (sys_var*) hash_search(&system_variable_hash,
			      (unsigned char*) str, length ? length : strlen(str));
  if (!(var || no_error))
    my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), (char*) str);

  return var;
}


/**
  Execute update of all variables.

  First run a check of all variables that all updates will go ok.
  If yes, then execute all updates, returning an error if any one failed.

  This should ensure that in all normal cases none all or variables are
  updated.

  @param Session		Thread id
  @param var_list       List of variables to update

  @retval
    0	ok
  @retval
    1	ERROR, message sent (normally no variables was updated)
  @retval
    -1  ERROR, message not sent
*/

int sql_set_variables(Session *session, List<set_var_base> *var_list)
{
  int error;
  List_iterator_fast<set_var_base> it(*var_list);

  set_var_base *var;
  while ((var=it++))
  {
    if ((error= var->check(session)))
      goto err;
  }
  if (!(error= test(session->is_error())))
  {
    it.rewind();
    while ((var= it++))
      error|= var->update(session);         // Returns 0, -1 or 1
  }

err:
  free_underlaid_joins(session, &session->lex->select_lex);
  return(error);
}


/**
  Say if all variables set by a SET support the ONE_SHOT keyword
  (currently, only character set and collation do; later timezones
  will).

  @param var_list	List of variables to update

  @note
    It has a "not_" because it makes faster tests (no need to "!")

  @retval
    0	all variables of the list support ONE_SHOT
  @retval
    1	at least one does not support ONE_SHOT
*/

bool not_all_support_one_shot(List<set_var_base> *var_list)
{
  List_iterator_fast<set_var_base> it(*var_list);
  set_var_base *var;
  while ((var= it++))
  {
    if (var->no_support_one_shot())
      return 1;
  }
  return 0;
}


/*****************************************************************************
  Functions to handle SET mysql_internal_variable=const_expr
*****************************************************************************/

int set_var::check(Session *session)
{
  if (var->is_readonly())
  {
    my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0), var->name, "read only");
    return -1;
  }
  if (var->check_type(type))
  {
    int err= type == OPT_GLOBAL ? ER_LOCAL_VARIABLE : ER_GLOBAL_VARIABLE;
    my_error(err, MYF(0), var->name);
    return -1;
  }
  /* value is a NULL pointer if we are using SET ... = DEFAULT */
  if (!value)
  {
    if (var->check_default(type))
    {
      my_error(ER_NO_DEFAULT, MYF(0), var->name);
      return -1;
    }
    return 0;
  }

  if ((!value->fixed &&
       value->fix_fields(session, &value)) || value->check_cols(1))
    return -1;
  if (var->check_update_type(value->result_type()))
  {
    my_error(ER_WRONG_TYPE_FOR_VAR, MYF(0), var->name);
    return -1;
  }
  return var->check(session, this) ? -1 : 0;
}

/**
  Update variable

  @param   session    thread handler
  @returns 0|1    ok or	ERROR

  @note ERROR can be only due to abnormal operations involving
  the server's execution evironment such as
  out of memory, hard disk failure or the computer blows up.
  Consider set_var::check() method if there is a need to return
  an error due to logics.
*/
int set_var::update(Session *session)
{
  if (!value)
    var->set_default(session, type);
  else if (var->update(session, this))
    return -1;				// should never happen
  if (var->after_update)
    (*var->after_update)(session, type);
  return 0;
}


/*****************************************************************************
  Functions to handle SET @user_variable=const_expr
*****************************************************************************/

int set_var_user::check(Session *session)
{
  /*
    Item_func_set_user_var can't substitute something else on its place =>
    0 can be passed as last argument (reference on item)
  */
  return (user_var_item->fix_fields(session, (Item**) 0) ||
	  user_var_item->check(0)) ? -1 : 0;
}


int set_var_user::update(Session *)
{
  if (user_var_item->update())
  {
    /* Give an error if it's not given already */
    my_message(ER_SET_CONSTANTS_ONLY, ER(ER_SET_CONSTANTS_ONLY), MYF(0));
    return -1;
  }
  return 0;
}

/****************************************************************************
 Functions to handle table_type
****************************************************************************/

/* Based upon sys_var::check_enum() */

bool sys_var_session_storage_engine::check(Session *session, set_var *var)
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  const char *value;
  String str(buff, sizeof(buff), &my_charset_utf8_general_ci), *res;

  var->save_result.plugin= NULL;
  if (var->value->result_type() == STRING_RESULT)
  {
    LEX_STRING engine_name;
    handlerton *hton;
    if (!(res=var->value->val_str(&str)) ||
        !(engine_name.str= (char *)res->ptr()) ||
        !(engine_name.length= res->length()) ||
	!(var->save_result.plugin= ha_resolve_by_name(session, &engine_name)) ||
        !(hton= plugin_data(var->save_result.plugin, handlerton *)))
    {
      value= res ? res->c_ptr() : "NULL";
      goto err;
    }
    return 0;
  }
  value= "unknown";

err:
  my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), value);
  return 1;
}


unsigned char *sys_var_session_storage_engine::value_ptr(Session *session,
                                                         enum_var_type type,
                                                         LEX_STRING *)
{
  unsigned char* result;
  handlerton *hton;
  LEX_STRING *engine_name;
  plugin_ref plugin= session->variables.*offset;
  if (type == OPT_GLOBAL)
    plugin= my_plugin_lock(session, &(global_system_variables.*offset));
  hton= plugin_data(plugin, handlerton*);
  engine_name= ha_storage_engine_name(hton);
  result= (unsigned char *) session->strmake(engine_name->str, engine_name->length);
  if (type == OPT_GLOBAL)
    plugin_unlock(session, plugin);
  return result;
}


void sys_var_session_storage_engine::set_default(Session *session, enum_var_type type)
{
  plugin_ref old_value, new_value, *value;
  if (type == OPT_GLOBAL)
  {
    value= &(global_system_variables.*offset);
    new_value= ha_lock_engine(NULL, myisam_hton);
  }
  else
  {
    value= &(session->variables.*offset);
    new_value= my_plugin_lock(NULL, &(global_system_variables.*offset));
  }
  assert(new_value);
  old_value= *value;
  *value= new_value;
  plugin_unlock(NULL, old_value);
}


bool sys_var_session_storage_engine::update(Session *session, set_var *var)
{
  plugin_ref *value= &(global_system_variables.*offset), old_value;
   if (var->type != OPT_GLOBAL)
     value= &(session->variables.*offset);
  old_value= *value;
  if (old_value != var->save_result.plugin)
  {
    *value= my_plugin_lock(NULL, &var->save_result.plugin);
    plugin_unlock(NULL, old_value);
  }
  return 0;
}

bool
sys_var_session_optimizer_switch::
symbolic_mode_representation(Session *session, uint32_t val, LEX_STRING *rep)
{
  char buff[STRING_BUFFER_USUAL_SIZE*8];
  String tmp(buff, sizeof(buff), &my_charset_utf8_general_ci);

  tmp.length(0);

  for (uint32_t i= 0; val; val>>= 1, i++)
  {
    if (val & 1)
    {
      tmp.append(optimizer_switch_typelib.type_names[i],
                 optimizer_switch_typelib.type_lengths[i]);
      tmp.append(',');
    }
  }

  if (tmp.length())
    tmp.length(tmp.length() - 1); /* trim the trailing comma */

  rep->str= session->strmake(tmp.ptr(), tmp.length());

  rep->length= rep->str ? tmp.length() : 0;

  return rep->length != tmp.length();
}


unsigned char *sys_var_session_optimizer_switch::value_ptr(Session *session,
                                                           enum_var_type type,
                                                           LEX_STRING *)
{
  LEX_STRING opts;
  uint64_t val= ((type == OPT_GLOBAL) ? global_system_variables.*offset :
                  session->variables.*offset);
  (void) symbolic_mode_representation(session, val, &opts);
  return (unsigned char *) opts.str;
}


void sys_var_session_optimizer_switch::set_default(Session *session, enum_var_type type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= 0;
  else
    session->variables.*offset= global_system_variables.*offset;
}


/****************************************************************************
  Named list handling
****************************************************************************/

unsigned char* find_named(I_List<NAMED_LIST> *list, const char *name, uint32_t length,
		NAMED_LIST **found)
{
  I_List_iterator<NAMED_LIST> it(*list);
  NAMED_LIST *element;
  while ((element= it++))
  {
    if (element->cmp(name, length))
    {
      if (found)
        *found= element;
      return element->data;
    }
  }
  return 0;
}


void delete_elements(I_List<NAMED_LIST> *list,
		     void (*free_element)(const char *name, unsigned char*))
{
  NAMED_LIST *element;
  while ((element= list->get()))
  {
    (*free_element)(element->name, element->data);
    delete element;
  }
  return;
}


/* Key cache functions */

static KEY_CACHE *create_key_cache(const char *name, uint32_t length)
{
  KEY_CACHE *key_cache;
  
  if ((key_cache= (KEY_CACHE*) my_malloc(sizeof(KEY_CACHE),
					     MYF(MY_ZEROFILL | MY_WME))))
  {
    if (!new NAMED_LIST(&key_caches, name, length, (unsigned char*) key_cache))
    {
      free((char*) key_cache);
      key_cache= 0;
    }
    else
    {
      /*
	Set default values for a key cache
	The values in dflt_key_cache_var is set by my_getopt() at startup

	We don't set 'buff_size' as this is used to enable the key cache
      */
      key_cache->param_block_size=     dflt_key_cache_var.param_block_size;
      key_cache->param_division_limit= dflt_key_cache_var.param_division_limit;
      key_cache->param_age_threshold=  dflt_key_cache_var.param_age_threshold;
    }
  }
  return(key_cache);
}


KEY_CACHE *get_or_create_key_cache(const char *name, uint32_t length)
{
  LEX_STRING key_cache_name;
  KEY_CACHE *key_cache;

  key_cache_name.str= (char *) name;
  key_cache_name.length= length;
  pthread_mutex_lock(&LOCK_global_system_variables);
  if (!(key_cache= get_key_cache(&key_cache_name)))
    key_cache= create_key_cache(name, length);
  pthread_mutex_unlock(&LOCK_global_system_variables);
  return key_cache;
}


void free_key_cache(const char *, KEY_CACHE *key_cache)
{
  ha_end_key_cache(key_cache);
  free((char*) key_cache);
}


bool process_key_caches(process_key_cache_t func)
{
  I_List_iterator<NAMED_LIST> it(key_caches);
  NAMED_LIST *element;

  while ((element= it++))
  {
    KEY_CACHE *key_cache= (KEY_CACHE *) element->data;
    func(element->name, key_cache);
  }
  return 0;
}


bool sys_var_opt_readonly::update(Session *session, set_var *var)
{
  bool result;

  /* Prevent self dead-lock */
  if (session->locked_tables || session->active_transaction())
  {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    return(true);
  }

  if (session->global_read_lock)
  {
    /*
      This connection already holds the global read lock.
      This can be the case with:
      - FLUSH TABLES WITH READ LOCK
      - SET GLOBAL READ_ONLY = 1
    */
    result= sys_var_bool_ptr::update(session, var);
    return(result);
  }

  /*
    Perform a 'FLUSH TABLES WITH READ LOCK'.
    This is a 3 step process:
    - [1] lock_global_read_lock()
    - [2] close_cached_tables()
    - [3] make_global_read_lock_block_commit()
    [1] prevents new connections from obtaining tables locked for write.
    [2] waits until all existing connections close their tables.
    [3] prevents transactions from being committed.
  */

  if (lock_global_read_lock(session))
    return(true);

  /*
    This call will be blocked by any connection holding a READ or WRITE lock.
    Ideally, we want to wait only for pending WRITE locks, but since:
    con 1> LOCK TABLE T FOR READ;
    con 2> LOCK TABLE T FOR WRITE; (blocked by con 1)
    con 3> SET GLOBAL READ ONLY=1; (blocked by con 2)
    can cause to wait on a read lock, it's required for the client application
    to unlock everything, and acceptable for the server to wait on all locks.
  */
  if ((result= close_cached_tables(session, NULL, false, true, true)) == true)
    goto end_with_read_lock;

  if ((result= make_global_read_lock_block_commit(session)) == true)
    goto end_with_read_lock;

  /* Change the opt_readonly system variable, safe because the lock is held */
  result= sys_var_bool_ptr::update(session, var);

end_with_read_lock:
  /* Release the lock */
  unlock_global_read_lock(session);
  return(result);
}

/****************************************************************************
  Used templates
****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class List<set_var_base>;
template class List_iterator_fast<set_var_base>;
template class I_List_iterator<NAMED_LIST>;
#endif
