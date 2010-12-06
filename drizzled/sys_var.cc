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
  @file Handling of MySQL SQL variables

  @details
  To add a new variable, one has to do the following:

  - Use one of the 'sys_var... classes from set_var.h or write a specific
    one for the variable type.
  - Define it in the 'variable definition list' in this file.
  - If the variable is thread specific, add it to 'system_variables' struct.
    If not, add it to mysqld.cc and an declaration in 'mysql_priv.h'
  - If the variable should be changed from the command line, add a definition
    of it in the option structure list in mysqld.cc
  - Don't forget to initialize new fields in global_system_variables and
    max_system_variables!

  @todo
    Add full support for the variable character_set (for 4.1)

  @note
    Be careful with var->save_result: sys_var::check() only updates
    uint64_t_value; so other members of the union are garbage then; to use
    them you must first assign a value to them (in specific ::check() for
    example).
*/

#include "config.h"
#include "drizzled/option.h"
#include "drizzled/error.h"
#include "drizzled/gettext.h"
#include "drizzled/tztime.h"
#include "drizzled/data_home.h"
#include "drizzled/set_var.h"
#include "drizzled/session.h"
#include "drizzled/sql_base.h"
#include "drizzled/lock.h"
#include "drizzled/item/uint.h"
#include "drizzled/item/null.h"
#include "drizzled/item/float.h"
#include "drizzled/item/string.h"
#include "drizzled/plugin.h"
#include "drizzled/version.h"
#include "drizzled/strfunc.h"
#include "drizzled/internal/m_string.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/charset.h"
#include "drizzled/transaction_services.h"
#include "drizzled/constrained_value.h"

#include <cstdio>
#include <map>
#include <vector>
#include <algorithm>

using namespace std;

namespace drizzled
{

namespace internal
{
extern bool timed_mutexes;
}

extern plugin::StorageEngine *myisam_engine;
extern bool timed_mutexes;

extern struct option my_long_options[];
extern const CHARSET_INFO *character_set_filesystem;
extern size_t my_thread_stack_size;

class sys_var_pluginvar;
typedef map<string, sys_var *> SystemVariableMap;
static SystemVariableMap system_variable_map;
extern char *opt_drizzle_tmpdir;

extern TYPELIB tx_isolation_typelib;

const char *bool_type_names[]= { "OFF", "ON", NULL };
TYPELIB bool_typelib=
{
  array_elements(bool_type_names)-1, "", bool_type_names, NULL
};

static bool set_option_bit(Session *session, set_var *var);
static bool set_option_autocommit(Session *session, set_var *var);
static int  check_pseudo_thread_id(Session *session, set_var *var);
static int check_tx_isolation(Session *session, set_var *var);
static void fix_tx_isolation(Session *session, sql_var_t type);
static int check_completion_type(Session *session, set_var *var);
static void fix_completion_type(Session *session, sql_var_t type);
static void fix_max_join_size(Session *session, sql_var_t type);
static void fix_session_mem_root(Session *session, sql_var_t type);
static void fix_server_id(Session *session, sql_var_t type);
static bool get_unsigned32(Session *session, set_var *var);
static bool get_unsigned64(Session *session, set_var *var);
bool throw_bounds_warning(Session *session, bool fixed, bool unsignd,
                          const std::string &name, int64_t val);
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
static sys_var_session_uint64_t
sys_auto_increment_increment("auto_increment_increment",
                             &drizzle_system_variables::auto_increment_increment);
static sys_var_session_uint64_t
sys_auto_increment_offset("auto_increment_offset",
                          &drizzle_system_variables::auto_increment_offset);

static sys_var_fs_path sys_basedir("basedir", basedir);
static sys_var_fs_path sys_pid_file("pid_file", pid_file);
static sys_var_fs_path sys_plugin_dir("plugin_dir", plugin_dir);

static sys_var_size_t_ptr sys_thread_stack_size("thread_stack",
                                                      &my_thread_stack_size);
static sys_var_constrained_value_readonly<uint32_t> sys_back_log("back_log", back_log);

static sys_var_session_uint64_t	sys_bulk_insert_buff_size("bulk_insert_buffer_size",
                                                          &drizzle_system_variables::bulk_insert_buff_size);
static sys_var_session_uint32_t	sys_completion_type("completion_type",
                                                    &drizzle_system_variables::completion_type,
                                                    check_completion_type,
                                                    fix_completion_type);
static sys_var_collation_sv
sys_collation_server("collation_server", &drizzle_system_variables::collation_server, &default_charset_info);
static sys_var_fs_path       sys_datadir("datadir", getDataHome());

static sys_var_session_uint64_t	sys_join_buffer_size("join_buffer_size",
                                                     &drizzle_system_variables::join_buff_size);
static sys_var_session_uint32_t	sys_max_allowed_packet("max_allowed_packet",
                                                       &drizzle_system_variables::max_allowed_packet);
static sys_var_uint64_t_ptr	sys_max_connect_errors("max_connect_errors",
                                               &max_connect_errors);
static sys_var_session_uint64_t	sys_max_error_count("max_error_count",
                                                  &drizzle_system_variables::max_error_count);
static sys_var_session_uint64_t	sys_max_heap_table_size("max_heap_table_size",
                                                        &drizzle_system_variables::max_heap_table_size);
static sys_var_session_uint64_t sys_pseudo_thread_id("pseudo_thread_id",
                                              &drizzle_system_variables::pseudo_thread_id,
                                              0, check_pseudo_thread_id);
static sys_var_session_ha_rows	sys_max_join_size("max_join_size",
                                                  &drizzle_system_variables::max_join_size,
                                                  fix_max_join_size);
static sys_var_session_uint64_t	sys_max_seeks_for_key("max_seeks_for_key",
                                                      &drizzle_system_variables::max_seeks_for_key);
static sys_var_session_uint64_t   sys_max_length_for_sort_data("max_length_for_sort_data",
                                                               &drizzle_system_variables::max_length_for_sort_data);
static sys_var_session_size_t	sys_max_sort_length("max_sort_length",
                                                    &drizzle_system_variables::max_sort_length);
static sys_var_uint64_t_ptr	sys_max_write_lock_count("max_write_lock_count",
                                                 &max_write_lock_count);
static sys_var_session_uint64_t sys_min_examined_row_limit("min_examined_row_limit",
                                                           &drizzle_system_variables::min_examined_row_limit);

/* these two cannot be static */
static sys_var_session_bool sys_optimizer_prune_level("optimizer_prune_level",
                                                      &drizzle_system_variables::optimizer_prune_level);
static sys_var_session_uint32_t sys_optimizer_search_depth("optimizer_search_depth",
                                                           &drizzle_system_variables::optimizer_search_depth);

static sys_var_session_uint64_t sys_preload_buff_size("preload_buffer_size",
                                                      &drizzle_system_variables::preload_buff_size);
static sys_var_session_uint32_t sys_read_buff_size("read_buffer_size",
                                                   &drizzle_system_variables::read_buff_size);
static sys_var_session_uint32_t	sys_read_rnd_buff_size("read_rnd_buffer_size",
                                                       &drizzle_system_variables::read_rnd_buff_size);
static sys_var_session_uint32_t	sys_div_precincrement("div_precision_increment",
                                                      &drizzle_system_variables::div_precincrement);

static sys_var_session_size_t	sys_range_alloc_block_size("range_alloc_block_size",
                                                           &drizzle_system_variables::range_alloc_block_size);

static sys_var_session_bool sys_replicate_query("replicate_query",
                                                &drizzle_system_variables::replicate_query);

static sys_var_session_uint32_t	sys_query_alloc_block_size("query_alloc_block_size",
                                                           &drizzle_system_variables::query_alloc_block_size,
                                                           NULL, fix_session_mem_root);
static sys_var_session_uint32_t	sys_query_prealloc_size("query_prealloc_size",
                                                        &drizzle_system_variables::query_prealloc_size,
                                                        NULL, fix_session_mem_root);
static sys_var_readonly sys_tmpdir("tmpdir", OPT_GLOBAL, SHOW_CHAR, get_tmpdir);

static sys_var_fs_path sys_secure_file_priv("secure_file_priv",
                                            secure_file_priv);

static sys_var_const_str_ptr sys_scheduler("scheduler",
                                           (char**)&opt_scheduler);

static sys_var_uint32_t_ptr  sys_server_id("server_id", &server_id,
                                           fix_server_id);

static sys_var_session_size_t	sys_sort_buffer("sort_buffer_size",
                                                &drizzle_system_variables::sortbuff_size);

static sys_var_session_size_t sys_transaction_message_threshold("transaction_message_threshold",
                                                                &drizzle_system_variables::transaction_message_threshold);

static sys_var_session_storage_engine sys_storage_engine("storage_engine",
				       &drizzle_system_variables::storage_engine);
static sys_var_const_str	sys_system_time_zone("system_time_zone",
                                             system_time_zone);
static sys_var_size_t_ptr	sys_table_def_size("table_definition_cache",
                                             &table_def_size);
static sys_var_uint64_t_ptr	sys_table_cache_size("table_open_cache",
					     &table_cache_size);
static sys_var_uint64_t_ptr	sys_table_lock_wait_timeout("table_lock_wait_timeout",
                                                    &table_lock_wait_timeout);
static sys_var_session_enum	sys_tx_isolation("tx_isolation",
                                             &drizzle_system_variables::tx_isolation,
                                             &tx_isolation_typelib,
                                             fix_tx_isolation,
                                             check_tx_isolation);
static sys_var_session_uint64_t	sys_tmp_table_size("tmp_table_size",
					   &drizzle_system_variables::tmp_table_size);
static sys_var_bool_ptr  sys_timed_mutexes("timed_mutexes", &internal::timed_mutexes);
static sys_var_const_str  sys_version("version", version().c_str());

static sys_var_const_str	sys_version_comment("version_comment",
                                            COMPILATION_COMMENT);
static sys_var_const_str	sys_version_compile_machine("version_compile_machine",
                                                      HOST_CPU);
static sys_var_const_str	sys_version_compile_os("version_compile_os",
                                                 HOST_OS);
static sys_var_const_str	sys_version_compile_vendor("version_compile_vendor",
                                                 HOST_VENDOR);

/* Variables that are bits in Session */

sys_var_session_bit sys_autocommit("autocommit", 0,
                               set_option_autocommit,
                               OPTION_NOT_AUTOCOMMIT,
                               1);
static sys_var_session_bit	sys_big_selects("sql_big_selects", 0,
					set_option_bit,
					OPTION_BIG_SELECTS);
static sys_var_session_bit	sys_sql_warnings("sql_warnings", 0,
					 set_option_bit,
					 OPTION_WARNINGS);
static sys_var_session_bit	sys_sql_notes("sql_notes", 0,
					 set_option_bit,
					 OPTION_SQL_NOTES);
static sys_var_session_bit	sys_buffer_results("sql_buffer_result", 0,
					   set_option_bit,
					   OPTION_BUFFER_RESULT);
static sys_var_session_bit	sys_foreign_key_checks("foreign_key_checks", 0,
					       set_option_bit,
					       OPTION_NO_FOREIGN_KEY_CHECKS, 1);
static sys_var_session_bit	sys_unique_checks("unique_checks", 0,
					  set_option_bit,
					  OPTION_RELAXED_UNIQUE_CHECKS, 1);
/* Local state variables */

static sys_var_session_ha_rows	sys_select_limit("sql_select_limit",
						 &drizzle_system_variables::select_limit);
static sys_var_timestamp sys_timestamp("timestamp");
static sys_var_last_insert_id
sys_last_insert_id("last_insert_id");
/*
  identity is an alias for last_insert_id(), so that we are compatible
  with Sybase
*/
static sys_var_last_insert_id sys_identity("identity");

static sys_var_session_lc_time_names sys_lc_time_names("lc_time_names");

/*
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
static sys_var_readonly sys_error_count("error_count",
                                        OPT_SESSION,
                                        SHOW_INT,
                                        get_error_count);
static sys_var_readonly sys_warning_count("warning_count",
                                          OPT_SESSION,
                                          SHOW_INT,
                                          get_warning_count);

sys_var_session_uint64_t sys_group_concat_max_len("group_concat_max_len",
                                                  &drizzle_system_variables::group_concat_max_len);

sys_var_session_time_zone sys_time_zone("time_zone");

/* Global read-only variable containing hostname */
static sys_var_const_str        sys_hostname("hostname", glob_hostname);

bool sys_var::check(Session *session, set_var *var)
{
  if (check_func)
  {
    int res;
    if ((res=(*check_func)(session, var)) < 0)
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), getName().c_str(), var->value->str_value.ptr());
    return res;
  }
  var->save_result.uint64_t_value= var->value->val_int();
  return 0;
}

bool sys_var_str::check(Session *session, set_var *var)
{
  if (!check_func)
    return 0;

  int res;
  if ((res=(*check_func)(session, var)) < 0)
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), getName().c_str(), var->value->str_value.ptr());
  return res;
}

bool sys_var_std_string::check(Session *session, set_var *var)
{
  if (check_func == NULL)
  {
    return false;
  }

  int res= (*check_func)(session, var);
  if (res != 0)
  {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), getName().c_str(), var->value->str_value.ptr());
    return true;
  }
  return false;
}

/*
  Functions to check and update variables
*/


/**
  Set the OPTION_BIG_SELECTS flag if max_join_size == HA_POS_ERROR.
*/

static void fix_max_join_size(Session *session, sql_var_t type)
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
static void fix_tx_isolation(Session *session, sql_var_t type)
{
  if (type == OPT_SESSION)
    session->session_tx_isolation= ((enum_tx_isolation)
                                    session->variables.tx_isolation);
}

static void fix_completion_type(Session *, sql_var_t) {}

static int check_completion_type(Session *, set_var *var)
{
  int64_t val= var->value->val_int();
  if (val < 0 || val > 2)
  {
    char buf[64];
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), var->var->getName().c_str(), internal::llstr(val, buf));
    return 1;
  }
  return 0;
}


static void fix_session_mem_root(Session *session, sql_var_t type)
{
  if (type != OPT_GLOBAL)
    session->mem_root->reset_root_defaults(session->variables.query_alloc_block_size,
                                           session->variables.query_prealloc_size);
}


static void fix_server_id(Session *, sql_var_t)
{
}


bool throw_bounds_warning(Session *session, bool fixed, bool unsignd,
                          const std::string &name, int64_t val)
{
  if (fixed)
  {
    char buf[22];

    if (unsignd)
      internal::ullstr((uint64_t) val, buf);
    else
      internal::llstr(val, buf);

    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), name.c_str(), buf);
  }
  return false;
}

uint64_t fix_unsigned(Session *session, uint64_t num,
                              const struct option *option_limits)
{
  bool fixed= false;
  uint64_t out= getopt_ull_limit_value(num, option_limits, &fixed);

  throw_bounds_warning(session, fixed, true, option_limits->name, (int64_t) num);
  return out;
}


static size_t fix_size_t(Session *session, size_t num,
                           const struct option *option_limits)
{
  bool fixed= false;
  size_t out= (size_t)getopt_ull_limit_value(num, option_limits, &fixed);

  throw_bounds_warning(session, fixed, true, option_limits->name, (int64_t) num);
  return out;
}

static bool get_unsigned32(Session *session, set_var *var)
{
  if (var->value->unsigned_flag)
    var->save_result.uint32_t_value= 
      static_cast<uint32_t>(var->value->val_int());
  else
  {
    int64_t v= var->value->val_int();
    if (v > UINT32_MAX)
      throw_bounds_warning(session, true, true,var->var->getName().c_str(), v);
    
    var->save_result.uint32_t_value= 
      static_cast<uint32_t>((v > UINT32_MAX) ? UINT32_MAX : (v < 0) ? 0 : v);
  }
  return false;
}

static bool get_unsigned64(Session *, set_var *var)
{
  if (var->value->unsigned_flag)
      var->save_result.uint64_t_value=(uint64_t) var->value->val_int();
  else
  {
    int64_t v= var->value->val_int();
      var->save_result.uint64_t_value= (uint64_t) ((v < 0) ? 0 : v);
  }
  return 0;
}

static bool get_size_t(Session *, set_var *var)
{
  if (var->value->unsigned_flag)
    var->save_result.size_t_value= (size_t) var->value->val_int();
  else
  {
    ssize_t v= (ssize_t)var->value->val_int();
    var->save_result.size_t_value= (size_t) ((v < 0) ? 0 : v);
  }
  return 0;
}

bool sys_var_uint32_t_ptr::check(Session *, set_var *var)
{
  var->save_result.uint32_t_value= (uint32_t)var->value->val_int();
  return 0;
}

bool sys_var_uint32_t_ptr::update(Session *session, set_var *var)
{
  uint32_t tmp= var->save_result.uint32_t_value;
  LOCK_global_system_variables.lock();
  if (option_limits)
  {
    uint32_t newvalue= (uint32_t) fix_unsigned(session, tmp, option_limits);
    if(newvalue==tmp)
      *value= newvalue;
  }
  else
    *value= (uint32_t) tmp;
  LOCK_global_system_variables.unlock();
  return 0;
}


void sys_var_uint32_t_ptr::set_default(Session *, sql_var_t)
{
  bool not_used;
  LOCK_global_system_variables.lock();
  *value= (uint32_t)getopt_ull_limit_value((uint32_t) option_limits->def_value,
                                           option_limits, &not_used);
  LOCK_global_system_variables.unlock();
}


bool sys_var_uint64_t_ptr::update(Session *session, set_var *var)
{
  uint64_t tmp= var->save_result.uint64_t_value;
  LOCK_global_system_variables.lock();
  if (option_limits)
  {
    uint64_t newvalue= (uint64_t) fix_unsigned(session, tmp, option_limits);
    if(newvalue==tmp)
      *value= newvalue;
  }
  else
    *value= (uint64_t) tmp;
  LOCK_global_system_variables.unlock();
  return 0;
}


void sys_var_uint64_t_ptr::set_default(Session *, sql_var_t)
{
  if (have_default_value)
  {
    *value= default_value;
  }
  else
  {
    bool not_used;
    LOCK_global_system_variables.lock();
    *value= getopt_ull_limit_value((uint64_t) option_limits->def_value,
                                   option_limits, &not_used);
    LOCK_global_system_variables.unlock();
  }
}


bool sys_var_size_t_ptr::update(Session *session, set_var *var)
{
  size_t tmp= var->save_result.size_t_value;
  LOCK_global_system_variables.lock();
  if (option_limits)
    *value= fix_size_t(session, tmp, option_limits);
  else
    *value= tmp;
  LOCK_global_system_variables.unlock();
  return 0;
}


void sys_var_size_t_ptr::set_default(Session *, sql_var_t)
{
  bool not_used;
  LOCK_global_system_variables.lock();
  *value= (size_t)getopt_ull_limit_value((size_t) option_limits->def_value,
                                         option_limits, &not_used);
  LOCK_global_system_variables.unlock();
}

bool sys_var_bool_ptr::update(Session *, set_var *var)
{
  *value= (bool) var->save_result.uint32_t_value;
  return 0;
}


void sys_var_bool_ptr::set_default(Session *, sql_var_t)
{
  *value= default_value;
}


/*
  32 bit types for session variables
*/
bool sys_var_session_uint32_t::check(Session *session, set_var *var)
{
  return (get_unsigned32(session, var) ||
          (check_func && (*check_func)(session, var)));
}

bool sys_var_session_uint32_t::update(Session *session, set_var *var)
{
  uint64_t tmp= (uint64_t) var->save_result.uint32_t_value;

  /* Don't use bigger value than given with --maximum-variable-name=.. */
  if ((uint32_t) tmp > max_system_variables.*offset)
  {
    throw_bounds_warning(session, true, true, getName(), (int64_t) tmp);
    tmp= max_system_variables.*offset;
  }

  if (option_limits)
    tmp= (uint32_t) fix_unsigned(session, tmp, option_limits);
  else if (tmp > UINT32_MAX)
  {
    tmp= UINT32_MAX;
    throw_bounds_warning(session, true, true, getName(), (int64_t) var->save_result.uint64_t_value);
  }

  if (var->type == OPT_GLOBAL)
     global_system_variables.*offset= (uint32_t) tmp;
   else
     session->variables.*offset= (uint32_t) tmp;

   return 0;
 }


 void sys_var_session_uint32_t::set_default(Session *session, sql_var_t type)
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
                                                sql_var_t type,
                                                const LEX_STRING *)
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
    LOCK_global_system_variables.lock();
    global_system_variables.*offset= (ha_rows) tmp;
    LOCK_global_system_variables.unlock();
  }
  else
    session->variables.*offset= (ha_rows) tmp;
  return 0;
}


void sys_var_session_ha_rows::set_default(Session *session, sql_var_t type)
{
  if (type == OPT_GLOBAL)
  {
    bool not_used;
    /* We will not come here if option_limits is not set */
    LOCK_global_system_variables.lock();
    global_system_variables.*offset=
      (ha_rows) getopt_ull_limit_value((ha_rows) option_limits->def_value,
                                       option_limits, &not_used);
    LOCK_global_system_variables.unlock();
  }
  else
    session->variables.*offset= global_system_variables.*offset;
}


unsigned char *sys_var_session_ha_rows::value_ptr(Session *session,
                                                  sql_var_t type,
                                                  const LEX_STRING *)
{
  if (type == OPT_GLOBAL)
    return (unsigned char*) &(global_system_variables.*offset);
  return (unsigned char*) &(session->variables.*offset);
}

bool sys_var_session_uint64_t::check(Session *session, set_var *var)
{
  return (get_unsigned64(session, var) ||
	  (check_func && (*check_func)(session, var)));
}

bool sys_var_session_uint64_t::update(Session *session,  set_var *var)
{
  uint64_t tmp= var->save_result.uint64_t_value;

  if (tmp > max_system_variables.*offset)
  {
    throw_bounds_warning(session, true, true, getName(), (int64_t) tmp);
    tmp= max_system_variables.*offset;
  }

  if (option_limits)
    tmp= fix_unsigned(session, tmp, option_limits);
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    LOCK_global_system_variables.lock();
    global_system_variables.*offset= (uint64_t) tmp;
    LOCK_global_system_variables.unlock();
  }
  else
    session->variables.*offset= (uint64_t) tmp;
  return 0;
}


void sys_var_session_uint64_t::set_default(Session *session, sql_var_t type)
{
  if (type == OPT_GLOBAL)
  {
    bool not_used;
    LOCK_global_system_variables.lock();
    global_system_variables.*offset=
      getopt_ull_limit_value((uint64_t) option_limits->def_value,
                             option_limits, &not_used);
    LOCK_global_system_variables.unlock();
  }
  else
    session->variables.*offset= global_system_variables.*offset;
}


unsigned char *sys_var_session_uint64_t::value_ptr(Session *session,
                                                   sql_var_t type,
                                                   const LEX_STRING *)
{
  if (type == OPT_GLOBAL)
    return (unsigned char*) &(global_system_variables.*offset);
  return (unsigned char*) &(session->variables.*offset);
}

bool sys_var_session_size_t::check(Session *session, set_var *var)
{
  return (get_size_t(session, var) ||
	  (check_func && (*check_func)(session, var)));
}

bool sys_var_session_size_t::update(Session *session,  set_var *var)
{
  size_t tmp= var->save_result.size_t_value;

  if (tmp > max_system_variables.*offset)
    tmp= max_system_variables.*offset;

  if (option_limits)
    tmp= fix_size_t(session, tmp, option_limits);
  if (var->type == OPT_GLOBAL)
  {
    /* Lock is needed to make things safe on 32 bit systems */
    LOCK_global_system_variables.lock();
    global_system_variables.*offset= tmp;
    LOCK_global_system_variables.unlock();
  }
  else
    session->variables.*offset= tmp;
  return 0;
}


void sys_var_session_size_t::set_default(Session *session, sql_var_t type)
{
  if (type == OPT_GLOBAL)
  {
    bool not_used;
    LOCK_global_system_variables.lock();
    global_system_variables.*offset=
      (size_t)getopt_ull_limit_value((size_t) option_limits->def_value,
                                     option_limits, &not_used);
    LOCK_global_system_variables.unlock();
  }
  else
    session->variables.*offset= global_system_variables.*offset;
}


unsigned char *sys_var_session_size_t::value_ptr(Session *session,
                                                 sql_var_t type,
                                                 const LEX_STRING *)
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


void sys_var_session_bool::set_default(Session *session,  sql_var_t type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= (bool) option_limits->def_value;
  else
    session->variables.*offset= global_system_variables.*offset;
}


unsigned char *sys_var_session_bool::value_ptr(Session *session,
                                               sql_var_t type,
                                               const LEX_STRING *)
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
    if (!(res=var->value->val_str(&str)) ||
        (var->save_result.uint32_t_value= find_type(enum_names, res->ptr(),
                                                    res->length(),1)) == 0)
    {
      value= res ? res->c_ptr() : "NULL";
      goto err;
    }

    var->save_result.uint32_t_value--;
  }
  else
  {
    uint64_t tmp=var->value->val_int();
    if (tmp >= enum_names->count)
    {
      internal::llstr(tmp,buff);
      value=buff;				// Wrong value is here
      goto err;
    }
    var->save_result.uint32_t_value= (uint32_t) tmp;	// Save for update
  }
  return 0;

err:
  my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.c_str(), value);
  return 1;
}


/**
  Return an Item for a variable.

  Used with @@[global.]variable_name.

  If type is not given, return local value if exists, else global.
*/

Item *sys_var::item(Session *session, sql_var_t var_type, const LEX_STRING *base)
{
  if (check_type(var_type))
  {
    if (var_type != OPT_DEFAULT)
    {
      my_error(ER_INCORRECT_GLOBAL_LOCAL_VAR, MYF(0),
               name.c_str(), var_type == OPT_GLOBAL ? "SESSION" : "GLOBAL");
      return 0;
    }
    /* As there was no local variable, return the global value */
    var_type= OPT_GLOBAL;
  }
  switch (show_type()) {
  case SHOW_LONG:
  case SHOW_INT:
  {
    uint32_t value;
    LOCK_global_system_variables.lock();
    value= *(uint*) value_ptr(session, var_type, base);
    LOCK_global_system_variables.unlock();
    return new Item_uint((uint64_t) value);
  }
  case SHOW_LONGLONG:
  {
    int64_t value;
    LOCK_global_system_variables.lock();
    value= *(int64_t*) value_ptr(session, var_type, base);
    LOCK_global_system_variables.unlock();
    return new Item_int(value);
  }
  case SHOW_DOUBLE:
  {
    double value;
    LOCK_global_system_variables.lock();
    value= *(double*) value_ptr(session, var_type, base);
    LOCK_global_system_variables.unlock();
    /* 6, as this is for now only used with microseconds */
    return new Item_float(value, 6);
  }
  case SHOW_HA_ROWS:
  {
    ha_rows value;
    LOCK_global_system_variables.lock();
    value= *(ha_rows*) value_ptr(session, var_type, base);
    LOCK_global_system_variables.unlock();
    return new Item_int((uint64_t) value);
  }
  case SHOW_SIZE:
  {
    size_t value;
    LOCK_global_system_variables.lock();
    value= *(size_t*) value_ptr(session, var_type, base);
    LOCK_global_system_variables.unlock();
    return new Item_int((uint64_t) value);
  }
  case SHOW_MY_BOOL:
  {
    int32_t value;
    LOCK_global_system_variables.lock();
    value= *(bool*) value_ptr(session, var_type, base);
    LOCK_global_system_variables.unlock();
    return new Item_int(value,1);
  }
  case SHOW_CHAR_PTR:
  {
    Item *tmp;
    LOCK_global_system_variables.lock();
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
    LOCK_global_system_variables.unlock();
    return tmp;
  }
  case SHOW_CHAR:
  {
    Item *tmp;
    LOCK_global_system_variables.lock();
    char *str= (char*) value_ptr(session, var_type, base);
    if (str)
      tmp= new Item_string(str, strlen(str),
                           system_charset_info, DERIVATION_SYSCONST);
    else
    {
      tmp= new Item_null();
      tmp->collation.set(system_charset_info, DERIVATION_SYSCONST);
    }
    LOCK_global_system_variables.unlock();
    return tmp;
  }
  default:
    my_error(ER_VAR_CANT_BE_READ, MYF(0), name.c_str());
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


void sys_var_session_enum::set_default(Session *session, sql_var_t type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= (uint32_t) option_limits->def_value;
  else
    session->variables.*offset= global_system_variables.*offset;
}


unsigned char *sys_var_session_enum::value_ptr(Session *session,
                                               sql_var_t type,
                                               const LEX_STRING *)
{
  uint32_t tmp= ((type == OPT_GLOBAL) ?
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


unsigned char *sys_var_session_bit::value_ptr(Session *session, sql_var_t,
                                              const LEX_STRING *)
{
  /*
    If reverse is 0 (default) return 1 if bit is set.
    If reverse is 1, return 0 if bit is set
  */
  session->sys_var_tmp.bool_value= ((session->options & bit_flag) ?
				   !reverse : reverse);
  return (unsigned char*) &session->sys_var_tmp.bool_value;
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
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.c_str(), "NULL");
      return 1;
    }
    if (!(tmp=get_charset_by_name(res->c_ptr())))
    {
      my_error(ER_UNKNOWN_COLLATION, MYF(0), res->c_ptr());
      return 1;
    }
  }
  else // INT_RESULT
  {
    if (!(tmp=get_charset((int) var->value->val_int())))
    {
      char buf[20];
      internal::int10_to_str((int) var->value->val_int(), buf, -10);
      my_error(ER_UNKNOWN_COLLATION, MYF(0), buf);
      return 1;
    }
  }
  var->save_result.charset= tmp;	// Save for update
  return 0;
}


bool sys_var_collation_sv::update(Session *session, set_var *var)
{
  if (var->type == OPT_GLOBAL)
    global_system_variables.*offset= var->save_result.charset;
  else
  {
    session->variables.*offset= var->save_result.charset;
  }
  return 0;
}


void sys_var_collation_sv::set_default(Session *session, sql_var_t type)
{
  if (type == OPT_GLOBAL)
    global_system_variables.*offset= *global_default;
  else
  {
    session->variables.*offset= global_system_variables.*offset;
  }
}


unsigned char *sys_var_collation_sv::value_ptr(Session *session,
                                               sql_var_t type,
                                               const LEX_STRING *)
{
  const CHARSET_INFO *cs= ((type == OPT_GLOBAL) ?
                           global_system_variables.*offset :
                           session->variables.*offset);
  return cs ? (unsigned char*) cs->name : (unsigned char*) "NULL";
}

/****************************************************************************/

bool sys_var_timestamp::update(Session *session,  set_var *var)
{
  session->set_time((time_t) var->save_result.uint64_t_value);
  return 0;
}


void sys_var_timestamp::set_default(Session *session, sql_var_t)
{
  session->user_time=0;
}


unsigned char *sys_var_timestamp::value_ptr(Session *session, sql_var_t,
                                            const LEX_STRING *)
{
  session->sys_var_tmp.int32_t_value= (int32_t) session->start_time;
  return (unsigned char*) &session->sys_var_tmp.int32_t_value;
}


bool sys_var_last_insert_id::update(Session *session, set_var *var)
{
  session->first_successful_insert_id_in_prev_stmt=
    var->save_result.uint64_t_value;
  return 0;
}


unsigned char *sys_var_last_insert_id::value_ptr(Session *session,
                                                 sql_var_t,
                                                 const LEX_STRING *)
{
  /*
    this tmp var makes it robust againt change of type of
    read_first_successful_insert_id_in_prev_stmt().
  */
  session->sys_var_tmp.uint64_t_value=
    session->read_first_successful_insert_id_in_prev_stmt();
  return (unsigned char*) &session->sys_var_tmp.uint64_t_value;
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
    LOCK_global_system_variables.lock();
    global_system_variables.time_zone= var->save_result.time_zone;
    LOCK_global_system_variables.unlock();
  }
  else
    session->variables.time_zone= var->save_result.time_zone;
  return 0;
}


unsigned char *sys_var_session_time_zone::value_ptr(Session *session,
                                                    sql_var_t type,
                                                    const LEX_STRING *)
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


void sys_var_session_time_zone::set_default(Session *session, sql_var_t type)
{
 LOCK_global_system_variables.lock();
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
 LOCK_global_system_variables.unlock();
}


bool sys_var_session_lc_time_names::check(Session *, set_var *var)
{
  MY_LOCALE *locale_match;

  if (var->value->result_type() == INT_RESULT)
  {
    if (!(locale_match= my_locale_by_number((uint32_t) var->value->val_int())))
    {
      char buf[20];
      internal::int10_to_str((int) var->value->val_int(), buf, -10);
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
      my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), name.c_str(), "NULL");
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
                                                        sql_var_t type,
                                                        const LEX_STRING *)
{
  return type == OPT_GLOBAL ?
                 (unsigned char *) global_system_variables.lc_time_names->name :
                 (unsigned char *) session->variables.lc_time_names->name;
}


void sys_var_session_lc_time_names::set_default(Session *session, sql_var_t type)
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
    LOCK_global_system_variables.lock();
    (global_system_variables.*offset)= microseconds;
    LOCK_global_system_variables.unlock();
  }
  else
    session->variables.*offset= microseconds;
  return 0;
}


void sys_var_microseconds::set_default(Session *session, sql_var_t type)
{
  int64_t microseconds= (int64_t) (option_limits->def_value * 1000000.0);
  if (type == OPT_GLOBAL)
  {
    LOCK_global_system_variables.lock();
    global_system_variables.*offset= microseconds;
    LOCK_global_system_variables.unlock();
  }
  else
    session->variables.*offset= microseconds;
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
      session->options&= ~(uint64_t) (OPTION_BEGIN);
      session->server_status|= SERVER_STATUS_AUTOCOMMIT;
      TransactionServices &transaction_services= TransactionServices::singleton();
      if (transaction_services.commitTransaction(session, true))
        return 1;
    }
    else
    {
      session->server_status&= ~SERVER_STATUS_AUTOCOMMIT;
    }
  }
  return 0;
}

static int check_pseudo_thread_id(Session *, set_var *var)
{
  var->save_result.uint64_t_value= var->value->val_int();
  return 0;
}

static unsigned char *get_warning_count(Session *session)
{
  session->sys_var_tmp.uint32_t_value=
    (session->warn_count[(uint32_t) DRIZZLE_ERROR::WARN_LEVEL_NOTE] +
     session->warn_count[(uint32_t) DRIZZLE_ERROR::WARN_LEVEL_ERROR] +
     session->warn_count[(uint32_t) DRIZZLE_ERROR::WARN_LEVEL_WARN]);
  return (unsigned char*) &session->sys_var_tmp.uint32_t_value;
}

static unsigned char *get_error_count(Session *session)
{
  session->sys_var_tmp.uint32_t_value=
    session->warn_count[(uint32_t) DRIZZLE_ERROR::WARN_LEVEL_ERROR];
  return (unsigned char*) &session->sys_var_tmp.uint32_t_value;
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
  assert(drizzle_tmpdir.size());
  return (unsigned char*)drizzle_tmpdir.c_str();
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

static struct option *find_option(struct option *opt, const char *name)
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





/*
  Constructs an array of system variables for display to the user.

  SYNOPSIS
    enumerate_sys_vars()
    session         current thread

  RETURN VALUES
    pointer     Array of drizzle_show_var elements for display
    NULL        FAILURE
*/

drizzle_show_var* enumerate_sys_vars(Session *session)
{
  int size= sizeof(drizzle_show_var) * (system_variable_map.size() + 1);
  drizzle_show_var *result= (drizzle_show_var*) session->alloc(size);

  if (result)
  {
    drizzle_show_var *show= result;

    SystemVariableMap::const_iterator iter= system_variable_map.begin();
    while (iter != system_variable_map.end())
    {
      sys_var *var= (*iter).second;
      show->name= var->getName().c_str();
      show->value= (char*) var;
      show->type= SHOW_SYS;
      ++show;
      ++iter;
    }

    /* make last element empty */
    memset(show, 0, sizeof(drizzle_show_var));
  }
  return result;
}



void add_sys_var_to_list(sys_var *var)
{
  string lower_name(var->getName());
  transform(lower_name.begin(), lower_name.end(),
            lower_name.begin(), ::tolower);

  /* this fails if there is a conflicting variable name. */
  if (system_variable_map.find(lower_name) != system_variable_map.end())
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Variable named %s already exists!\n"),
                  var->getName().c_str());
    throw exception();
  } 

  pair<SystemVariableMap::iterator, bool> ret= 
    system_variable_map.insert(make_pair(lower_name, var));
  if (ret.second == false)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Could not add Variable: %s\n"),
                  var->getName().c_str());
    throw exception();
  }
}

void add_sys_var_to_list(sys_var *var, struct option *long_options)
{
  add_sys_var_to_list(var);
  var->setOptionLimits(find_option(long_options, var->getName().c_str()));
}

/*
  Initialize the system variables

  SYNOPSIS
    sys_var_init()

  RETURN VALUES
    0           SUCCESS
    otherwise   FAILURE
*/

int sys_var_init()
{
  try
  {
    add_sys_var_to_list(&sys_auto_increment_increment, my_long_options);
    add_sys_var_to_list(&sys_auto_increment_offset, my_long_options);
    add_sys_var_to_list(&sys_autocommit, my_long_options);
    add_sys_var_to_list(&sys_back_log, my_long_options);
    add_sys_var_to_list(&sys_basedir, my_long_options);
    add_sys_var_to_list(&sys_big_selects, my_long_options);
    add_sys_var_to_list(&sys_buffer_results, my_long_options);
    add_sys_var_to_list(&sys_bulk_insert_buff_size, my_long_options);
    add_sys_var_to_list(&sys_collation_server, my_long_options);
    add_sys_var_to_list(&sys_completion_type, my_long_options);
    add_sys_var_to_list(&sys_datadir, my_long_options);
    add_sys_var_to_list(&sys_div_precincrement, my_long_options);
    add_sys_var_to_list(&sys_error_count, my_long_options);
    add_sys_var_to_list(&sys_foreign_key_checks, my_long_options);
    add_sys_var_to_list(&sys_group_concat_max_len, my_long_options);
    add_sys_var_to_list(&sys_hostname, my_long_options);
    add_sys_var_to_list(&sys_identity, my_long_options);
    add_sys_var_to_list(&sys_join_buffer_size, my_long_options);
    add_sys_var_to_list(&sys_last_insert_id, my_long_options);
    add_sys_var_to_list(&sys_lc_time_names, my_long_options);
    add_sys_var_to_list(&sys_max_allowed_packet, my_long_options);
    add_sys_var_to_list(&sys_max_connect_errors, my_long_options);
    add_sys_var_to_list(&sys_max_error_count, my_long_options);
    add_sys_var_to_list(&sys_max_heap_table_size, my_long_options);
    add_sys_var_to_list(&sys_max_join_size, my_long_options);
    add_sys_var_to_list(&sys_max_length_for_sort_data, my_long_options);
    add_sys_var_to_list(&sys_max_seeks_for_key, my_long_options);
    add_sys_var_to_list(&sys_max_sort_length, my_long_options);
    add_sys_var_to_list(&sys_max_write_lock_count, my_long_options);
    add_sys_var_to_list(&sys_min_examined_row_limit, my_long_options);
    add_sys_var_to_list(&sys_optimizer_prune_level, my_long_options);
    add_sys_var_to_list(&sys_optimizer_search_depth, my_long_options);
    add_sys_var_to_list(&sys_pid_file, my_long_options);
    add_sys_var_to_list(&sys_plugin_dir, my_long_options);
    add_sys_var_to_list(&sys_preload_buff_size, my_long_options);
    add_sys_var_to_list(&sys_pseudo_thread_id, my_long_options);
    add_sys_var_to_list(&sys_query_alloc_block_size, my_long_options);
    add_sys_var_to_list(&sys_query_prealloc_size, my_long_options);
    add_sys_var_to_list(&sys_range_alloc_block_size, my_long_options);
    add_sys_var_to_list(&sys_read_buff_size, my_long_options);
    add_sys_var_to_list(&sys_read_rnd_buff_size, my_long_options);
    add_sys_var_to_list(&sys_replicate_query, my_long_options);
    add_sys_var_to_list(&sys_scheduler, my_long_options);
    add_sys_var_to_list(&sys_secure_file_priv, my_long_options);
    add_sys_var_to_list(&sys_select_limit, my_long_options);
    add_sys_var_to_list(&sys_server_id, my_long_options);
    add_sys_var_to_list(&sys_sort_buffer, my_long_options);
    add_sys_var_to_list(&sys_sql_notes, my_long_options);
    add_sys_var_to_list(&sys_sql_warnings, my_long_options);
    add_sys_var_to_list(&sys_storage_engine, my_long_options);
    add_sys_var_to_list(&sys_system_time_zone, my_long_options);
    add_sys_var_to_list(&sys_table_cache_size, my_long_options);
    add_sys_var_to_list(&sys_table_def_size, my_long_options);
    add_sys_var_to_list(&sys_table_lock_wait_timeout, my_long_options);
    add_sys_var_to_list(&sys_thread_stack_size, my_long_options);
    add_sys_var_to_list(&sys_time_zone, my_long_options);
    add_sys_var_to_list(&sys_timed_mutexes, my_long_options);
    add_sys_var_to_list(&sys_timestamp, my_long_options);
    add_sys_var_to_list(&sys_tmp_table_size, my_long_options);
    add_sys_var_to_list(&sys_tmpdir, my_long_options);
    add_sys_var_to_list(&sys_transaction_message_threshold, my_long_options);
    add_sys_var_to_list(&sys_tx_isolation, my_long_options);
    add_sys_var_to_list(&sys_unique_checks, my_long_options);
    add_sys_var_to_list(&sys_version, my_long_options);
    add_sys_var_to_list(&sys_version_comment, my_long_options);
    add_sys_var_to_list(&sys_version_compile_machine, my_long_options);
    add_sys_var_to_list(&sys_version_compile_os, my_long_options);
    add_sys_var_to_list(&sys_version_compile_vendor, my_long_options);
    add_sys_var_to_list(&sys_warning_count, my_long_options);
  }
  catch (std::exception&)
  {
    errmsg_printf(ERRMSG_LVL_ERROR, _("Failed to initialize system variables"));
    return(1);
  }
  return(0);
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

sys_var *intern_find_sys_var(const char *str, uint32_t, bool no_error)
{
  string lower_name(str);
  transform(lower_name.begin(), lower_name.end(),
            lower_name.begin(), ::tolower);

  sys_var *result= NULL;

  SystemVariableMap::iterator iter= system_variable_map.find(lower_name);
  if (iter != system_variable_map.end())
  {
    result= (*iter).second;
  } 

  /*
    This function is only called from the sql_plugin.cc.
    A lock on LOCK_system_variable_hash should be held
  */
  if (result == NULL)
  {
    if (no_error)
    {
      return NULL;
    }
    else
    {
      my_error(ER_UNKNOWN_SYSTEM_VARIABLE, MYF(0), (char*) str);
      return NULL;
    }
  }

  return result;
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

  var->save_result.storage_engine= NULL;
  if (var->value->result_type() == STRING_RESULT)
  {
    res= var->value->val_str(&str);
    if (res == NULL || res->ptr() == NULL)
    {
      value= "NULL";
      goto err;
    }
    else
    {
      const std::string engine_name(res->ptr());
      plugin::StorageEngine *engine;
      var->save_result.storage_engine= plugin::StorageEngine::findByName(*session, engine_name);
      if (var->save_result.storage_engine == NULL)
      {
        value= res->c_ptr();
        goto err;
      }
      engine= var->save_result.storage_engine;
    }
    return 0;
  }
  value= "unknown";

err:
  my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), value);
  return 1;
}


unsigned char *sys_var_session_storage_engine::value_ptr(Session *session,
                                                         sql_var_t type,
                                                         const LEX_STRING *)
{
  unsigned char* result;
  string engine_name;
  plugin::StorageEngine *engine= session->variables.*offset;
  if (type == OPT_GLOBAL)
    engine= global_system_variables.*offset;
  engine_name= engine->getName();
  result= (unsigned char *) session->strmake(engine_name.c_str(),
                                             engine_name.size());
  return result;
}


void sys_var_session_storage_engine::set_default(Session *session, sql_var_t type)
{
  plugin::StorageEngine *old_value, *new_value, **value;
  if (type == OPT_GLOBAL)
  {
    value= &(global_system_variables.*offset);
    new_value= myisam_engine;
  }
  else
  {
    value= &(session->variables.*offset);
    new_value= global_system_variables.*offset;
  }
  assert(new_value);
  old_value= *value;
  *value= new_value;
}


bool sys_var_session_storage_engine::update(Session *session, set_var *var)
{
  plugin::StorageEngine **value= &(global_system_variables.*offset), *old_value;
   if (var->type != OPT_GLOBAL)
     value= &(session->variables.*offset);
  old_value= *value;
  if (old_value != var->save_result.storage_engine)
  {
    *value= var->save_result.storage_engine;
  }
  return 0;
}

} /* namespace drizzled */
