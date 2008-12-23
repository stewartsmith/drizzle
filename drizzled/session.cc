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


/*****************************************************************************
**
** This file implements classes defined in session.h
** Especially the classes to handle a result from a select
**
*****************************************************************************/
#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <drizzled/replication/rli.h>
#include <drizzled/replication/record.h>
#include <drizzled/log_event.h>
#include <sys/stat.h>
#include <mysys/thr_alarm.h>
#include <mysys/mysys_err.h>
#include <drizzled/error.h>
#include <drizzled/query_id.h>
#include <drizzled/data_home.h>
#include <drizzled/sql_base.h>
#include <drizzled/lock.h>
#include <drizzled/item/cache.h>
#include <drizzled/item/float.h>
#include <drizzled/item/return_int.h>
#include <drizzled/item/empty_string.h>

extern scheduler_functions thread_scheduler;
/*
  The following is used to initialise Table_ident with a internal
  table name
*/
char internal_table_name[2]= "*";
char empty_c_string[1]= {0};    /* used for not defined db */

const char * const Session::DEFAULT_WHERE= "field list";
extern pthread_key_t THR_Session;
extern pthread_key_t THR_Mem_root;


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
/* Used templates */
template class List<Key>;
template class List_iterator<Key>;
template class List<Key_part_spec>;
template class List_iterator<Key_part_spec>;
template class List<Alter_drop>;
template class List_iterator<Alter_drop>;
template class List<Alter_column>;
template class List_iterator<Alter_column>;
#endif


/****************************************************************************
** User variables
****************************************************************************/

extern "C" unsigned char *get_var_key(user_var_entry *entry, size_t *length,
                              bool not_used __attribute__((unused)))
{
  *length= entry->name.length;
  return (unsigned char*) entry->name.str;
}

extern "C" void free_user_var(user_var_entry *entry)
{
  char *pos= (char*) entry+ALIGN_SIZE(sizeof(*entry));
  if (entry->value && entry->value != pos)
    free(entry->value);
  free((char*) entry);
}

bool Key_part_spec::operator==(const Key_part_spec& other) const
{
  return length == other.length &&
         field_name.length == other.field_name.length &&
         !strcmp(field_name.str, other.field_name.str);
}

/**
  Construct an (almost) deep copy of this key. Only those
  elements that are known to never change are not copied.
  If out of memory, a partial copy is returned and an error is set
  in Session.
*/

Key::Key(const Key &rhs, MEM_ROOT *mem_root)
  :type(rhs.type),
  key_create_info(rhs.key_create_info),
  columns(rhs.columns, mem_root),
  name(rhs.name),
  generated(rhs.generated)
{
  list_copy_and_replace_each_value(columns, mem_root);
}

/**
  Construct an (almost) deep copy of this foreign key. Only those
  elements that are known to never change are not copied.
  If out of memory, a partial copy is returned and an error is set
  in Session.
*/

Foreign_key::Foreign_key(const Foreign_key &rhs, MEM_ROOT *mem_root)
  :Key(rhs),
  ref_table(rhs.ref_table),
  ref_columns(rhs.ref_columns),
  delete_opt(rhs.delete_opt),
  update_opt(rhs.update_opt),
  match_opt(rhs.match_opt)
{
  list_copy_and_replace_each_value(ref_columns, mem_root);
}

/*
  Test if a foreign key (= generated key) is a prefix of the given key
  (ignoring key name, key type and order of columns)

  NOTES:
    This is only used to test if an index for a FOREIGN KEY exists

  IMPLEMENTATION
    We only compare field names

  RETURN
    0	Generated key is a prefix of other key
    1	Not equal
*/

bool foreign_key_prefix(Key *a, Key *b)
{
  /* Ensure that 'a' is the generated key */
  if (a->generated)
  {
    if (b->generated && a->columns.elements > b->columns.elements)
      std::swap(a, b);                       // Put shorter key in 'a'
  }
  else
  {
    if (!b->generated)
      return true;                              // No foreign key
    std::swap(a, b);                       // Put generated key in 'a'
  }

  /* Test if 'a' is a prefix of 'b' */
  if (a->columns.elements > b->columns.elements)
    return true;                                // Can't be prefix

  List_iterator<Key_part_spec> col_it1(a->columns);
  List_iterator<Key_part_spec> col_it2(b->columns);
  const Key_part_spec *col1, *col2;

#ifdef ENABLE_WHEN_INNODB_CAN_HANDLE_SWAPED_FOREIGN_KEY_COLUMNS
  while ((col1= col_it1++))
  {
    bool found= 0;
    col_it2.rewind();
    while ((col2= col_it2++))
    {
      if (*col1 == *col2)
      {
        found= true;
	break;
      }
    }
    if (!found)
      return true;                              // Error
  }
  return false;                                 // Is prefix
#else
  while ((col1= col_it1++))
  {
    col2= col_it2++;
    if (!(*col1 == *col2))
      return true;
  }
  return false;                                 // Is prefix
#endif
}


/*
  Check if the foreign key options are compatible with columns
  on which the FK is created.

  RETURN
    0   Key valid
    1   Key invalid
*/
bool Foreign_key::validate(List<Create_field> &table_fields)
{
  Create_field  *sql_field;
  Key_part_spec *column;
  List_iterator<Key_part_spec> cols(columns);
  List_iterator<Create_field> it(table_fields);
  while ((column= cols++))
  {
    it.rewind();
    while ((sql_field= it++) &&
           my_strcasecmp(system_charset_info,
                         column->field_name.str,
                         sql_field->field_name)) {}
    if (!sql_field)
    {
      my_error(ER_KEY_COLUMN_DOES_NOT_EXITS, MYF(0), column->field_name.str);
      return true;
    }
    if (type == Key::FOREIGN_KEY && sql_field->vcol_info)
    {
      if (delete_opt == FK_OPTION_SET_NULL)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_VIRTUAL_COLUMN, MYF(0),
                 "ON DELETE SET NULL");
        return true;
      }
      if (update_opt == FK_OPTION_SET_NULL)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_VIRTUAL_COLUMN, MYF(0),
                 "ON UPDATE SET NULL");
        return true;
      }
      if (update_opt == FK_OPTION_CASCADE)
      {
        my_error(ER_WRONG_FK_OPTION_FOR_VIRTUAL_COLUMN, MYF(0),
                 "ON UPDATE CASCADE");
        return true;
      }
    }
  }
  return false;
}


/****************************************************************************
** Thread specific functions
****************************************************************************/

Open_tables_state::Open_tables_state(ulong version_arg)
  :version(version_arg), state_flags(0U)
{
  reset_open_tables_state();
}

/*
  The following functions form part of the C plugin API
*/

extern "C" int mysql_tmpfile(const char *prefix)
{
  char filename[FN_REFLEN];
  File fd = create_temp_file(filename, drizzle_tmpdir, prefix,
                             O_CREAT | O_EXCL | O_RDWR,
                             MYF(MY_WME));
  if (fd >= 0) {
    unlink(filename);
  }

  return fd;
}


extern "C"
int session_in_lock_tables(const Session *session)
{
  return test(session->in_lock_tables);
}


extern "C"
int session_tablespace_op(const Session *session)
{
  return test(session->tablespace_op);
}


/**
   Set the process info field of the Session structure.

   This function is used by plug-ins. Internally, the
   Session::set_proc_info() function should be used.

   @see Session::set_proc_info
 */
extern "C" void
set_session_proc_info(Session *session, const char *info)
{
  session->set_proc_info(info);
}

extern "C"
const char *get_session_proc_info(Session *session)
{
  return session->get_proc_info();
}

extern "C"
void **session_ha_data(const Session *session, const struct handlerton *hton)
{
  return (void **) &session->ha_data[hton->slot].ha_ptr;
}

extern "C"
int64_t session_test_options(const Session *session, int64_t test_options)
{
  return session->options & test_options;
}

extern "C"
int session_sql_command(const Session *session)
{
  return (int) session->lex->sql_command;
}

extern "C"
int session_tx_isolation(const Session *session)
{
  return (int) session->variables.tx_isolation;
}

extern "C"
void session_inc_row_count(Session *session)
{
  session->row_count++;
}

/**
  Clear this diagnostics area.

  Normally called at the end of a statement.
*/

void
Diagnostics_area::reset_diagnostics_area()
{
  can_overwrite_status= false;
  /** Don't take chances in production */
  m_message[0]= '\0';
  m_sql_errno= 0;
  m_server_status= 0;
  m_affected_rows= 0;
  m_last_insert_id= 0;
  m_total_warn_count= 0;
  is_sent= false;
  /** Tiny reset in debug mode to see garbage right away */
  m_status= DA_EMPTY;
}


/**
  Set OK status -- ends commands that do not return a
  result set, e.g. INSERT/UPDATE/DELETE.
*/

void
Diagnostics_area::set_ok_status(Session *session, ha_rows affected_rows_arg,
                                uint64_t last_insert_id_arg,
                                const char *message_arg)
{
  assert(! is_set());
  /*
    In production, refuse to overwrite an error or a custom response
    with an OK packet.
  */
  if (is_error() || is_disabled())
    return;
  /** Only allowed to report success if has not yet reported an error */

  m_server_status= session->server_status;
  m_total_warn_count= session->total_warn_count;
  m_affected_rows= affected_rows_arg;
  m_last_insert_id= last_insert_id_arg;
  if (message_arg)
    strncpy(m_message, message_arg, sizeof(m_message) - 1);
  else
    m_message[0]= '\0';
  m_status= DA_OK;
}


/**
  Set EOF status.
*/

void
Diagnostics_area::set_eof_status(Session *session)
{
  /** Only allowed to report eof if has not yet reported an error */

  assert(! is_set());
  /*
    In production, refuse to overwrite an error or a custom response
    with an EOF packet.
  */
  if (is_error() || is_disabled())
    return;

  m_server_status= session->server_status;
  /*
    If inside a stored procedure, do not return the total
    number of warnings, since they are not available to the client
    anyway.
  */
  m_total_warn_count= session->total_warn_count;

  m_status= DA_EOF;
}

/**
  Set ERROR status.
*/

void
Diagnostics_area::set_error_status(Session *session __attribute__((unused)),
                                   uint32_t sql_errno_arg,
                                   const char *message_arg)
{
  /*
    Only allowed to report error if has not yet reported a success
    The only exception is when we flush the message to the client,
    an error can happen during the flush.
  */
  assert(! is_set() || can_overwrite_status);
  /*
    In production, refuse to overwrite a custom response with an
    ERROR packet.
  */
  if (is_disabled())
    return;

  m_sql_errno= sql_errno_arg;
  strncpy(m_message, message_arg, sizeof(m_message) - 1);

  m_status= DA_ERROR;
}


/**
  Mark the diagnostics area as 'DISABLED'.

  This is used in rare cases when the COM_ command at hand sends a response
  in a custom format. One example is the query cache, another is
  COM_STMT_PREPARE.
*/

void
Diagnostics_area::disable_status()
{
  assert(! is_set());
  m_status= DA_DISABLED;
}


Session::Session()
   :Statement(&main_lex, &main_mem_root,
              /* statement id */ 0),
   Open_tables_state(refresh_version), rli_fake(0),
   lock_id(&main_lock_id),
   user_time(0),
   arg_of_last_insert_id_function(false),
   first_successful_insert_id_in_prev_stmt(0),
   first_successful_insert_id_in_cur_stmt(0),
   global_read_lock(0),
   is_fatal_error(0),
   transaction_rollback_request(0),
   is_fatal_sub_stmt_error(0),
   in_lock_tables(0),
   derived_tables_processing(false),
   m_lip(NULL)
{
  ulong tmp;

  /*
    Pass nominal parameters to init_alloc_root only to ensure that
    the destructor works OK in case of an error. The main_mem_root
    will be re-initialized in init_for_queries().
  */
  init_sql_alloc(&main_mem_root, ALLOC_ROOT_MIN_BLOCK_SIZE, 0);
  thread_stack= 0;
  catalog= (char*)"std"; // the only catalog we have for now
  some_tables_deleted=no_errors=password= 0;
  count_cuted_fields= CHECK_FIELD_IGNORE;
  killed= NOT_KILLED;
  col_access=0;
  is_slave_error= thread_specific_used= false;
  hash_clear(&handler_tables_hash);
  tmp_table=0;
  used_tables=0;
  cuted_fields= sent_row_count= row_count= 0L;
  limit_found_rows= 0;
  row_count_func= -1;
  statement_id_counter= 0UL;
  // Must be reset to handle error with Session's created for init of mysqld
  lex->current_select= 0;
  start_time=(time_t) 0;
  start_utime= 0L;
  utime_after_lock= 0L;
  current_linfo =  0;
  slave_thread = 0;
  memset(&variables, 0, sizeof(variables));
  thread_id= 0;
  file_id = 0;
  query_id= 0;
  warn_id= 0;
  db_charset= global_system_variables.collation_database;
  memset(ha_data, 0, sizeof(ha_data));
  replication_data= 0;
  mysys_var=0;
  binlog_evt_union.do_union= false;
  dbug_sentry=Session_SENTRY_MAGIC;
  net.vio= 0;
  client_capabilities= 0;                       // minimalistic client
  system_thread= NON_SYSTEM_THREAD;
  cleanup_done= abort_on_warning= no_warnings_for_error= 0;
  peer_port= 0;					// For SHOW PROCESSLIST
  transaction.m_pending_rows_event= 0;
  transaction.on= 1;
  pthread_mutex_init(&LOCK_delete, MY_MUTEX_INIT_FAST);

  /* Variables with default values */
  proc_info="login";
  where= Session::DEFAULT_WHERE;
  server_id = ::server_id;
  slave_net= NULL;
  command=COM_CONNECT;
  *scramble= '\0';

  init();
  /* Initialize sub structures */
  init_sql_alloc(&warn_root, WARN_ALLOC_BLOCK_SIZE, WARN_ALLOC_PREALLOC_SIZE);
  user_connect=(USER_CONN *)0;
  hash_init(&user_vars, system_charset_info, USER_VARS_HASH_SIZE, 0, 0,
	    (hash_get_key) get_var_key,
	    (hash_free_key) free_user_var, 0);

  /* For user vars replication*/
  if (opt_bin_log)
    my_init_dynamic_array(&user_var_events,
			  sizeof(BINLOG_USER_VAR_EVENT *), 16, 16);
  else
    memset(&user_var_events, 0, sizeof(user_var_events));

  /* Protocol */
  protocol= &protocol_text;			// Default protocol
  protocol_text.init(this);

  const Query_id& query_id= Query_id::get_query_id();
  tablespace_op= false;
  tmp= sql_rnd();
  randominit(&rand, tmp + (ulong) &rand, tmp + query_id.value());
  substitute_null_with_insert_id = false;
  thr_lock_info_init(&lock_info); /* safety: will be reset after start */
  thr_lock_owner_init(&main_lock_id, &lock_info);

  m_internal_handler= NULL;
}


void Session::push_internal_handler(Internal_error_handler *handler)
{
  /*
    TODO: The current implementation is limited to 1 handler at a time only.
    Session and sp_rcontext need to be modified to use a common handler stack.
  */
  assert(m_internal_handler == NULL);
  m_internal_handler= handler;
}


bool Session::handle_error(uint32_t sql_errno, const char *message,
                       DRIZZLE_ERROR::enum_warning_level level)
{
  if (m_internal_handler)
  {
    return m_internal_handler->handle_error(sql_errno, message, level, this);
  }

  return false;                                 // 'false', as per coding style
}


void Session::pop_internal_handler()
{
  assert(m_internal_handler != NULL);
  m_internal_handler= NULL;
}

#if defined(__cplusplus)
extern "C" {
#endif

void *session_alloc(Session *session, unsigned int size)
{
  return session->alloc(size);
}

void *session_calloc(Session *session, unsigned int size)
{
  return session->calloc(size);
}

char *session_strdup(Session *session, const char *str)
{
  return session->strdup(str);
}

char *session_strmake(Session *session, const char *str, unsigned int size)
{
  return session->strmake(str, size);
}

void *session_memdup(Session *session, const void* str, unsigned int size)
{
  return session->memdup(str, size);
}

void session_get_xid(const Session *session, DRIZZLE_XID *xid)
{
  *xid = *(DRIZZLE_XID *) &session->transaction.xid_state.xid;
}

#if defined(__cplusplus)
}
#endif

/*
  Init common variables that has to be reset on start and on change_user
*/

void Session::init(void)
{
  pthread_mutex_lock(&LOCK_global_system_variables);
  plugin_sessionvar_init(this);
  variables.time_format= date_time_format_copy((Session*) 0,
					       variables.time_format);
  variables.date_format= date_time_format_copy((Session*) 0,
					       variables.date_format);
  variables.datetime_format= date_time_format_copy((Session*) 0,
						   variables.datetime_format);
  /*
    variables= global_system_variables above has reset
    variables.pseudo_thread_id to 0. We need to correct it here to
    avoid temporary tables replication failure.
  */
  variables.pseudo_thread_id= thread_id;
  pthread_mutex_unlock(&LOCK_global_system_variables);
  server_status= SERVER_STATUS_AUTOCOMMIT;
  options= session_startup_options;

  if (variables.max_join_size == HA_POS_ERROR)
    options |= OPTION_BIG_SELECTS;
  else
    options &= ~OPTION_BIG_SELECTS;

  transaction.all.modified_non_trans_table= transaction.stmt.modified_non_trans_table= false;
  open_options=ha_open_options;
  update_lock_default= TL_WRITE;
  session_tx_isolation= (enum_tx_isolation) variables.tx_isolation;
  warn_list.empty();
  memset(warn_count, 0, sizeof(warn_count));
  total_warn_count= 0;
  update_charset();
  memset(&status_var, 0, sizeof(status_var));
}


/*
  Init Session for query processing.
  This has to be called once before we call mysql_parse.
  See also comments in session.h.
*/

void Session::init_for_queries()
{
  set_time();
  ha_enable_transaction(this,true);

  reset_root_defaults(mem_root, variables.query_alloc_block_size,
                      variables.query_prealloc_size);
  reset_root_defaults(&transaction.mem_root,
                      variables.trans_alloc_block_size,
                      variables.trans_prealloc_size);
  transaction.xid_state.xid.null();
  transaction.xid_state.in_session=1;
}


/* Do operations that may take a long time */

void Session::cleanup(void)
{
  assert(cleanup_done == 0);

  killed= KILL_CONNECTION;
#ifdef ENABLE_WHEN_BINLOG_WILL_BE_ABLE_TO_PREPARE
  if (transaction.xid_state.xa_state == XA_PREPARED)
  {
#error xid_state in the cache should be replaced by the allocated value
  }
#endif
  {
    ha_rollback(this);
    xid_cache_delete(&transaction.xid_state);
  }
  if (locked_tables)
  {
    lock=locked_tables; locked_tables=0;
    close_thread_tables(this);
  }
  mysql_ha_cleanup(this);
  delete_dynamic(&user_var_events);
  hash_free(&user_vars);
  close_temporary_tables(this);
  free((char*) variables.time_format);
  free((char*) variables.date_format);
  free((char*) variables.datetime_format);

  if (global_read_lock)
    unlock_global_read_lock(this);

  cleanup_done=1;
  return;
}

Session::~Session()
{
  Session_CHECK_SENTRY(this);
  /* Ensure that no one is using Session */
  pthread_mutex_lock(&LOCK_delete);
  pthread_mutex_unlock(&LOCK_delete);
  add_to_status(&global_status_var, &status_var);

  /* Close connection */
  if (net.vio)
  {
    net_close(&net);
    net_end(&net);
  }
  if (!cleanup_done)
    cleanup();

  ha_close_connection(this);
  plugin_sessionvar_cleanup(this);

  if (db)
  {
    free(db);
    db= NULL;
  }
  free_root(&warn_root,MYF(0));
  free_root(&transaction.mem_root,MYF(0));
  mysys_var=0;					// Safety (shouldn't be needed)
  pthread_mutex_destroy(&LOCK_delete);
  dbug_sentry= Session_SENTRY_GONE;
  if (rli_fake)
  {
    delete rli_fake;
    rli_fake= NULL;
  }

  free_root(&main_mem_root, MYF(0));
  pthread_setspecific(THR_Session,  0);
  return;
}


/*
  Add all status variables to another status variable array

  SYNOPSIS
   add_to_status()
   to_var       add to this array
   from_var     from this array

  NOTES
    This function assumes that all variables are long/ulong.
    If this assumption will change, then we have to explictely add
    the other variables after the while loop
*/

void add_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var)
{
  ulong *end= (ulong*) ((unsigned char*) to_var +
                        offsetof(STATUS_VAR, last_system_status_var) +
			sizeof(ulong));
  ulong *to= (ulong*) to_var, *from= (ulong*) from_var;

  while (to != end)
    *(to++)+= *(from++);
}

/*
  Add the difference between two status variable arrays to another one.

  SYNOPSIS
    add_diff_to_status
    to_var       add to this array
    from_var     from this array
    dec_var      minus this array

  NOTE
    This function assumes that all variables are long/ulong.
*/

void add_diff_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var,
                        STATUS_VAR *dec_var)
{
  ulong *end= (ulong*) ((unsigned char*) to_var + offsetof(STATUS_VAR,
						  last_system_status_var) +
			sizeof(ulong));
  ulong *to= (ulong*) to_var, *from= (ulong*) from_var, *dec= (ulong*) dec_var;

  while (to != end)
    *(to++)+= *(from++) - *(dec++);
}


void Session::awake(Session::killed_state state_to_set)
{
  Session_CHECK_SENTRY(this);
  safe_mutex_assert_owner(&LOCK_delete);

  killed= state_to_set;
  if (state_to_set != Session::KILL_QUERY)
  {
    thr_alarm_kill(thread_id);
    if (!slave_thread)
      thread_scheduler.post_kill_notification(this);
  }
  if (mysys_var)
  {
    pthread_mutex_lock(&mysys_var->mutex);
    if (!system_thread)		// Don't abort locks
      mysys_var->abort=1;
    /*
      This broadcast could be up in the air if the victim thread
      exits the cond in the time between read and broadcast, but that is
      ok since all we want to do is to make the victim thread get out
      of waiting on current_cond.
      If we see a non-zero current_cond: it cannot be an old value (because
      then exit_cond() should have run and it can't because we have mutex); so
      it is the true value but maybe current_mutex is not yet non-zero (we're
      in the middle of enter_cond() and there is a "memory order
      inversion"). So we test the mutex too to not lock 0.

      Note that there is a small chance we fail to kill. If victim has locked
      current_mutex, but hasn't yet entered enter_cond() (which means that
      current_cond and current_mutex are 0), then the victim will not get
      a signal and it may wait "forever" on the cond (until
      we issue a second KILL or the status it's waiting for happens).
      It's true that we have set its session->killed but it may not
      see it immediately and so may have time to reach the cond_wait().
    */
    if (mysys_var->current_cond && mysys_var->current_mutex)
    {
      pthread_mutex_lock(mysys_var->current_mutex);
      pthread_cond_broadcast(mysys_var->current_cond);
      pthread_mutex_unlock(mysys_var->current_mutex);
    }
    pthread_mutex_unlock(&mysys_var->mutex);
  }
  return;
}

/*
  Remember the location of thread info, the structure needed for
  sql_alloc() and the structure for the net buffer
*/

bool Session::store_globals()
{
  /*
    Assert that thread_stack is initialized: it's necessary to be able
    to track stack overrun.
  */
  assert(thread_stack);

  if (pthread_setspecific(THR_Session,  this) ||
      pthread_setspecific(THR_Mem_root, &mem_root))
    return 1;
  mysys_var=my_thread_var;
  /*
    Let mysqld define the thread id (not mysys)
    This allows us to move Session to different threads if needed.
  */
  mysys_var->id= thread_id;
  real_id= pthread_self();                      // For debugging

  /*
    We have to call thr_lock_info_init() again here as Session may have been
    created in another thread
  */
  thr_lock_info_init(&lock_info);
  return 0;
}


/*
  Cleanup after query.

  SYNOPSIS
    Session::cleanup_after_query()

  DESCRIPTION
    This function is used to reset thread data to its default state.

  NOTE
    This function is not suitable for setting thread data to some
    non-default values, as there is only one replication thread, so
    different master threads may overwrite data of each other on
    slave.
*/

void Session::cleanup_after_query()
{
  /*
    Reset rand_used so that detection of calls to rand() will save random
    seeds if needed by the slave.
  */
  {
    /* Forget those values, for next binlogger: */
    auto_inc_intervals_in_cur_stmt_for_binlog.empty();
  }
  if (first_successful_insert_id_in_cur_stmt > 0)
  {
    /* set what LAST_INSERT_ID() will return */
    first_successful_insert_id_in_prev_stmt=
      first_successful_insert_id_in_cur_stmt;
    first_successful_insert_id_in_cur_stmt= 0;
    substitute_null_with_insert_id= true;
  }
  arg_of_last_insert_id_function= 0;
  /* Free Items that were created during this execution */
  free_items();
  /* Reset where. */
  where= Session::DEFAULT_WHERE;
}


/**
  Create a LEX_STRING in this connection.

  @param lex_str  pointer to LEX_STRING object to be initialized
  @param str      initializer to be copied into lex_str
  @param length   length of str, in bytes
  @param allocate_lex_string  if true, allocate new LEX_STRING object,
                              instead of using lex_str value
  @return  NULL on failure, or pointer to the LEX_STRING object
*/
LEX_STRING *Session::make_lex_string(LEX_STRING *lex_str,
                                 const char* str, uint32_t length,
                                 bool allocate_lex_string)
{
  if (allocate_lex_string)
    if (!(lex_str= (LEX_STRING *)alloc(sizeof(LEX_STRING))))
      return 0;
  if (!(lex_str->str= strmake_root(mem_root, str, length)))
    return 0;
  lex_str->length= length;
  return lex_str;
}


/*
  Convert a string to another character set

  SYNOPSIS
    convert_string()
    to				Store new allocated string here
    to_cs			New character set for allocated string
    from			String to convert
    from_length			Length of string to convert
    from_cs			Original character set

  NOTES
    to will be 0-terminated to make it easy to pass to system funcs

  RETURN
    0	ok
    1	End of memory.
        In this case to->str will point to 0 and to->length will be 0.
*/

bool Session::convert_string(LEX_STRING *to, const CHARSET_INFO * const to_cs,
			 const char *from, uint32_t from_length,
			 const CHARSET_INFO * const from_cs)
{
  size_t new_length= to_cs->mbmaxlen * from_length;
  uint32_t dummy_errors;
  if (!(to->str= (char*) alloc(new_length+1)))
  {
    to->length= 0;				// Safety fix
    return(1);				// EOM
  }
  to->length= copy_and_convert((char*) to->str, new_length, to_cs,
			       from, from_length, from_cs, &dummy_errors);
  to->str[to->length]=0;			// Safety
  return(0);
}


/*
  Convert string from source character set to target character set inplace.

  SYNOPSIS
    Session::convert_string

  DESCRIPTION
    Convert string using convert_buffer - buffer for character set
    conversion shared between all protocols.

  RETURN
    0   ok
   !0   out of memory
*/

bool Session::convert_string(String *s, const CHARSET_INFO * const from_cs,
                         const CHARSET_INFO * const to_cs)
{
  uint32_t dummy_errors;
  if (convert_buffer.copy(s->ptr(), s->length(), from_cs, to_cs, &dummy_errors))
    return true;
  /* If convert_buffer >> s copying is more efficient long term */
  if (convert_buffer.alloced_length() >= convert_buffer.length() * 2 ||
      !s->is_alloced())
  {
    return s->copy(convert_buffer);
  }
  s->swap(convert_buffer);
  return false;
}


/*
  Update some cache variables when character set changes
*/

void Session::update_charset()
{
  uint32_t not_used;
  charset_is_system_charset= !String::needs_conversion(0,charset(),
                                                       system_charset_info,
                                                       &not_used);
  charset_is_collation_connection=
    !String::needs_conversion(0,charset(),variables.collation_connection,
                              &not_used);
  charset_is_character_set_filesystem=
    !String::needs_conversion(0, charset(),
                              variables.character_set_filesystem, &not_used);
}


/* routings to adding tables to list of changed in transaction tables */

inline static void list_include(CHANGED_TableList** prev,
				CHANGED_TableList* curr,
				CHANGED_TableList* new_table)
{
  if (new_table)
  {
    *prev = new_table;
    (*prev)->next = curr;
  }
}

/* add table to list of changed in transaction tables */

void Session::add_changed_table(Table *table)
{
  assert((options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) &&
	      table->file->has_transactions());
  add_changed_table(table->s->table_cache_key.str,
                    (long) table->s->table_cache_key.length);
  return;
}


void Session::add_changed_table(const char *key, long key_length)
{
  CHANGED_TableList **prev_changed = &transaction.changed_tables;
  CHANGED_TableList *curr = transaction.changed_tables;

  for (; curr; prev_changed = &(curr->next), curr = curr->next)
  {
    int cmp =  (long)curr->key_length - (long)key_length;
    if (cmp < 0)
    {
      list_include(prev_changed, curr, changed_table_dup(key, key_length));
      return;
    }
    else if (cmp == 0)
    {
      cmp = memcmp(curr->key, key, curr->key_length);
      if (cmp < 0)
      {
	list_include(prev_changed, curr, changed_table_dup(key, key_length));
	return;
      }
      else if (cmp == 0)
      {
	return;
      }
    }
  }
  *prev_changed = changed_table_dup(key, key_length);
  return;
}


CHANGED_TableList* Session::changed_table_dup(const char *key, long key_length)
{
  CHANGED_TableList* new_table =
    (CHANGED_TableList*) trans_alloc(ALIGN_SIZE(sizeof(CHANGED_TableList))+
				      key_length + 1);
  if (!new_table)
  {
    my_error(EE_OUTOFMEMORY, MYF(ME_BELL),
             ALIGN_SIZE(sizeof(TableList)) + key_length + 1);
    killed= KILL_CONNECTION;
    return 0;
  }

  new_table->key= ((char*)new_table)+ ALIGN_SIZE(sizeof(CHANGED_TableList));
  new_table->next = 0;
  new_table->key_length = key_length;
  ::memcpy(new_table->key, key, key_length);
  return new_table;
}


int Session::send_explain_fields(select_result *result)
{
  List<Item> field_list;
  Item *item;
  const CHARSET_INFO * const cs= system_charset_info;
  field_list.push_back(new Item_return_int("id",3, DRIZZLE_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("select_type", 19, cs));
  field_list.push_back(item= new Item_empty_string("table", NAME_CHAR_LEN, cs));
  item->maybe_null= 1;
  field_list.push_back(item= new Item_empty_string("type", 10, cs));
  item->maybe_null= 1;
  field_list.push_back(item=new Item_empty_string("possible_keys",
						  NAME_CHAR_LEN*MAX_KEY, cs));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("key", NAME_CHAR_LEN, cs));
  item->maybe_null=1;
  field_list.push_back(item=
    new Item_empty_string("key_len",
                          MAX_KEY *
                          (MAX_KEY_LENGTH_DECIMAL_WIDTH + 1 /* for comma */),
                          cs));
  item->maybe_null=1;
  field_list.push_back(item=new Item_empty_string("ref",
                                                  NAME_CHAR_LEN*MAX_REF_PARTS,
                                                  cs));
  item->maybe_null=1;
  field_list.push_back(item= new Item_return_int("rows", 10,
                                                 DRIZZLE_TYPE_LONGLONG));
  if (lex->describe & DESCRIBE_EXTENDED)
  {
    field_list.push_back(item= new Item_float("filtered", 0.1234, 2, 4));
    item->maybe_null=1;
  }
  item->maybe_null= 1;
  field_list.push_back(new Item_empty_string("Extra", 255, cs));
  return (result->send_fields(field_list,
                              Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF));
}


struct Item_change_record: public ilink
{
  Item **place;
  Item *old_value;
  /* Placement new was hidden by `new' in ilink (TODO: check): */
  static void *operator new(size_t size __attribute__((unused)),
                            void *mem)
    { return mem; }
  static void operator delete(void *ptr __attribute__((unused)),
                              size_t size __attribute__((unused)))
    {}
  static void operator delete(void *ptr __attribute__((unused)),
                              void *mem __attribute__((unused)))
    { /* never called */ }
};


/*
  Register an item tree tree transformation, performed by the query
  optimizer. We need a pointer to runtime_memroot because it may be !=
  session->mem_root (this may no longer be a true statement)
*/

void Session::nocheck_register_item_tree_change(Item **place, Item *old_value,
                                            MEM_ROOT *runtime_memroot)
{
  Item_change_record *change;
  /*
    Now we use one node per change, which adds some memory overhead,
    but still is rather fast as we use alloc_root for allocations.
    A list of item tree changes of an average query should be short.
  */
  void *change_mem= alloc_root(runtime_memroot, sizeof(*change));
  if (change_mem == 0)
  {
    /*
      OOM, session->fatal_error() is called by the error handler of the
      memroot. Just return.
    */
    return;
  }
  change= new (change_mem) Item_change_record;
  change->place= place;
  change->old_value= old_value;
  change_list.append(change);
}


void Session::rollback_item_tree_changes()
{
  I_List_iterator<Item_change_record> it(change_list);
  Item_change_record *change;

  while ((change= it++))
    *change->place= change->old_value;
  /* We can forget about changes memory: it's allocated in runtime memroot */
  change_list.empty();
  return;
}


/*****************************************************************************
** Functions to provide a interface to select results
*****************************************************************************/

select_result::select_result()
{
  session=current_session;
}

void select_result::send_error(uint32_t errcode,const char *err)
{
  my_message(errcode, err, MYF(0));
}


void select_result::cleanup()
{
  /* do nothing */
}

bool select_result::check_simple_select() const
{
  my_error(ER_SP_BAD_CURSOR_QUERY, MYF(0));
  return true;
}


static String default_line_term("\n",default_charset_info);
static String default_escaped("\\",default_charset_info);
static String default_field_term("\t",default_charset_info);

sql_exchange::sql_exchange(char *name, bool flag,
                           enum enum_filetype filetype_arg)
  :file_name(name), opt_enclosed(0), dumpfile(flag), skip_lines(0)
{
  filetype= filetype_arg;
  field_term= &default_field_term;
  enclosed=   line_start= &my_empty_string;
  line_term=  &default_line_term;
  escaped=    &default_escaped;
  cs= NULL;
}

bool select_send::send_fields(List<Item> &list, uint32_t flags)
{
  bool res;
  if (!(res= session->protocol->send_fields(&list, flags)))
    is_result_set_started= 1;
  return res;
}

void select_send::abort()
{
  return;
}


/**
  Cleanup an instance of this class for re-use
  at next execution of a prepared statement/
  stored procedure statement.
*/

void select_send::cleanup()
{
  is_result_set_started= false;
}

/* Send data to client. Returns 0 if ok */

bool select_send::send_data(List<Item> &items)
{
  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }

  /*
    We may be passing the control from mysqld to the client: release the
    InnoDB adaptive hash S-latch to avoid thread deadlocks if it was reserved
    by session
  */
  ha_release_temporary_latches(session);

  List_iterator_fast<Item> li(items);
  Protocol *protocol= session->protocol;
  char buff[MAX_FIELD_WIDTH];
  String buffer(buff, sizeof(buff), &my_charset_bin);

  protocol->prepare_for_resend();
  Item *item;
  while ((item=li++))
  {
    if (item->send(protocol, &buffer))
    {
      protocol->free();				// Free used buffer
      my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
      break;
    }
  }
  session->sent_row_count++;
  if (session->is_error())
  {
    protocol->remove_last_row();
    return(1);
  }
  if (session->vio_ok())
    return(protocol->write());
  return(0);
}

bool select_send::send_eof()
{
  /*
    We may be passing the control from mysqld to the client: release the
    InnoDB adaptive hash S-latch to avoid thread deadlocks if it was reserved
    by session
  */
  ha_release_temporary_latches(session);

  /* Unlock tables before sending packet to gain some speed */
  if (session->lock)
  {
    mysql_unlock_tables(session, session->lock);
    session->lock=0;
  }
  ::my_eof(session);
  is_result_set_started= 0;
  return false;
}


/************************************************************************
  Handling writing to file
************************************************************************/

void select_to_file::send_error(uint32_t errcode,const char *err)
{
  my_message(errcode, err, MYF(0));
  if (file > 0)
  {
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    (void) my_delete(path,MYF(0));		// Delete file on error
    file= -1;
  }
}


bool select_to_file::send_eof()
{
  int error= test(end_io_cache(&cache));
  if (my_close(file,MYF(MY_WME)))
    error= 1;
  if (!error)
  {
    /*
      In order to remember the value of affected rows for ROW_COUNT()
      function, SELECT INTO has to have an own SQLCOM.
      TODO: split from SQLCOM_SELECT
    */
    ::my_ok(session,row_count);
  }
  file= -1;
  return error;
}


void select_to_file::cleanup()
{
  /* In case of error send_eof() may be not called: close the file here. */
  if (file >= 0)
  {
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    file= -1;
  }
  path[0]= '\0';
  row_count= 0;
}


select_to_file::~select_to_file()
{
  if (file >= 0)
  {					// This only happens in case of error
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    file= -1;
  }
}

/***************************************************************************
** Export of select to textfile
***************************************************************************/

select_export::~select_export()
{
  session->sent_row_count=row_count;
}


/*
  Create file with IO cache

  SYNOPSIS
    create_file()
    session			Thread handle
    path		File name
    exchange		Excange class
    cache		IO cache

  RETURN
    >= 0 	File handle
   -1		Error
*/


static File create_file(Session *session, char *path, sql_exchange *exchange,
			IO_CACHE *cache)
{
  File file;
  uint32_t option= MY_UNPACK_FILENAME | MY_RELATIVE_PATH;

#ifdef DONT_ALLOW_FULL_LOAD_DATA_PATHS
  option|= MY_REPLACE_DIR;			// Force use of db directory
#endif

  if (!dirname_length(exchange->file_name))
  {
    strcpy(path, drizzle_real_data_home);
    if (session->db)
      strncat(path, session->db, FN_REFLEN-strlen(drizzle_real_data_home)-1);
    (void) fn_format(path, exchange->file_name, path, "", option);
  }
  else
    (void) fn_format(path, exchange->file_name, drizzle_real_data_home, "", option);

  if (opt_secure_file_priv &&
      strncmp(opt_secure_file_priv, path, strlen(opt_secure_file_priv)))
  {
    /* Write only allowed to dir or subdir specified by secure_file_priv */
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--secure-file-priv");
    return -1;
  }

  if (!access(path, F_OK))
  {
    my_error(ER_FILE_EXISTS_ERROR, MYF(0), exchange->file_name);
    return -1;
  }
  /* Create the file world readable */
  if ((file= my_create(path, 0666, O_WRONLY|O_EXCL, MYF(MY_WME))) < 0)
    return file;
#ifdef HAVE_FCHMOD
  (void) fchmod(file, 0666);			// Because of umask()
#else
  (void) chmod(path, 0666);
#endif
  if (init_io_cache(cache, file, 0L, WRITE_CACHE, 0L, 1, MYF(MY_WME)))
  {
    my_close(file, MYF(0));
    my_delete(path, MYF(0));  // Delete file on error, it was just created
    return -1;
  }
  return file;
}


int
select_export::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  bool blob_flag=0;
  bool string_results= false, non_string_results= false;
  unit= u;
  if ((uint) strlen(exchange->file_name) + NAME_LEN >= FN_REFLEN)
    strncpy(path,exchange->file_name,FN_REFLEN-1);

  /* Check if there is any blobs in data */
  {
    List_iterator_fast<Item> li(list);
    Item *item;
    while ((item=li++))
    {
      if (item->max_length >= MAX_BLOB_WIDTH)
      {
	blob_flag=1;
	break;
      }
      if (item->result_type() == STRING_RESULT)
        string_results= true;
      else
        non_string_results= true;
    }
  }
  field_term_length=exchange->field_term->length();
  field_term_char= field_term_length ?
                   (int) (unsigned char) (*exchange->field_term)[0] : INT_MAX;
  if (!exchange->line_term->length())
    exchange->line_term=exchange->field_term;	// Use this if it exists
  field_sep_char= (exchange->enclosed->length() ?
                  (int) (unsigned char) (*exchange->enclosed)[0] : field_term_char);
  escape_char=	(exchange->escaped->length() ?
                (int) (unsigned char) (*exchange->escaped)[0] : -1);
  is_ambiguous_field_sep= test(strchr(ESCAPE_CHARS, field_sep_char));
  is_unsafe_field_sep= test(strchr(NUMERIC_CHARS, field_sep_char));
  line_sep_char= (exchange->line_term->length() ?
                 (int) (unsigned char) (*exchange->line_term)[0] : INT_MAX);
  if (!field_term_length)
    exchange->opt_enclosed=0;
  if (!exchange->enclosed->length())
    exchange->opt_enclosed=1;			// A little quicker loop
  fixed_row_size= (!field_term_length && !exchange->enclosed->length() &&
		   !blob_flag);
  if ((is_ambiguous_field_sep && exchange->enclosed->is_empty() &&
       (string_results || is_unsafe_field_sep)) ||
      (exchange->opt_enclosed && non_string_results &&
       field_term_length && strchr(NUMERIC_CHARS, field_term_char)))
  {
    my_error(ER_AMBIGUOUS_FIELD_TERM, MYF(0));
    return 1;
  }

  if ((file= create_file(session, path, exchange, &cache)) < 0)
    return 1;

  return 0;
}


#define NEED_ESCAPING(x) ((int) (unsigned char) (x) == escape_char    || \
                          (enclosed ? (int) (unsigned char) (x) == field_sep_char      \
                                    : (int) (unsigned char) (x) == field_term_char) || \
                          (int) (unsigned char) (x) == line_sep_char  || \
                          !(x))

bool select_export::send_data(List<Item> &items)
{
  char buff[MAX_FIELD_WIDTH],null_buff[2],space[MAX_FIELD_WIDTH];
  bool space_inited=0;
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  tmp.length(0);

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return(0);
  }
  row_count++;
  Item *item;
  uint32_t used_length=0,items_left=items.elements;
  List_iterator_fast<Item> li(items);

  if (my_b_write(&cache,(unsigned char*) exchange->line_start->ptr(),
		 exchange->line_start->length()))
    goto err;
  while ((item=li++))
  {
    Item_result result_type=item->result_type();
    bool enclosed = (exchange->enclosed->length() &&
                     (!exchange->opt_enclosed || result_type == STRING_RESULT));
    res=item->str_result(&tmp);
    if (res && enclosed)
    {
      if (my_b_write(&cache,(unsigned char*) exchange->enclosed->ptr(),
		     exchange->enclosed->length()))
	goto err;
    }
    if (!res)
    {						// NULL
      if (!fixed_row_size)
      {
	if (escape_char != -1)			// Use \N syntax
	{
	  null_buff[0]=escape_char;
	  null_buff[1]='N';
	  if (my_b_write(&cache,(unsigned char*) null_buff,2))
	    goto err;
	}
	else if (my_b_write(&cache,(unsigned char*) "NULL",4))
	  goto err;
      }
      else
      {
	used_length=0;				// Fill with space
      }
    }
    else
    {
      if (fixed_row_size)
	used_length=cmin(res->length(),item->max_length);
      else
	used_length=res->length();
      if ((result_type == STRING_RESULT || is_unsafe_field_sep) &&
           escape_char != -1)
      {
        char *pos, *start, *end;
        const CHARSET_INFO * const res_charset= res->charset();
        const CHARSET_INFO * const character_set_client= session->variables.
                                                            character_set_client;
        bool check_second_byte= (res_charset == &my_charset_bin) &&
                                 character_set_client->
                                 escape_with_backslash_is_dangerous;
        assert(character_set_client->mbmaxlen == 2 ||
                    !character_set_client->escape_with_backslash_is_dangerous);
	for (start=pos=(char*) res->ptr(),end=pos+used_length ;
	     pos != end ;
	     pos++)
	{
#ifdef USE_MB
	  if (use_mb(res_charset))
	  {
	    int l;
	    if ((l=my_ismbchar(res_charset, pos, end)))
	    {
	      pos += l-1;
	      continue;
	    }
	  }
#endif

          /*
            Special case when dumping BINARY/VARBINARY/BLOB values
            for the clients with character sets big5, cp932, gbk and sjis,
            which can have the escape character (0x5C "\" by default)
            as the second byte of a multi-byte sequence.

            If
            - pos[0] is a valid multi-byte head (e.g 0xEE) and
            - pos[1] is 0x00, which will be escaped as "\0",

            then we'll get "0xEE + 0x5C + 0x30" in the output file.

            If this file is later loaded using this sequence of commands:

            mysql> create table t1 (a varchar(128)) character set big5;
            mysql> LOAD DATA INFILE 'dump.txt' INTO Table t1;

            then 0x5C will be misinterpreted as the second byte
            of a multi-byte character "0xEE + 0x5C", instead of
            escape character for 0x00.

            To avoid this confusion, we'll escape the multi-byte
            head character too, so the sequence "0xEE + 0x00" will be
            dumped as "0x5C + 0xEE + 0x5C + 0x30".

            Note, in the condition below we only check if
            mbcharlen is equal to 2, because there are no
            character sets with mbmaxlen longer than 2
            and with escape_with_backslash_is_dangerous set.
            assert before the loop makes that sure.
          */

          if ((NEED_ESCAPING(*pos) ||
               (check_second_byte &&
                my_mbcharlen(character_set_client, (unsigned char) *pos) == 2 &&
                pos + 1 < end &&
                NEED_ESCAPING(pos[1]))) &&
              /*
               Don't escape field_term_char by doubling - doubling is only
               valid for ENCLOSED BY characters:
              */
              (enclosed || !is_ambiguous_field_term ||
               (int) (unsigned char) *pos != field_term_char))
          {
	    char tmp_buff[2];
            tmp_buff[0]= ((int) (unsigned char) *pos == field_sep_char &&
                          is_ambiguous_field_sep) ?
                          field_sep_char : escape_char;
	    tmp_buff[1]= *pos ? *pos : '0';
	    if (my_b_write(&cache,(unsigned char*) start,(uint) (pos-start)) ||
		my_b_write(&cache,(unsigned char*) tmp_buff,2))
	      goto err;
	    start=pos+1;
	  }
	}
	if (my_b_write(&cache,(unsigned char*) start,(uint) (pos-start)))
	  goto err;
      }
      else if (my_b_write(&cache,(unsigned char*) res->ptr(),used_length))
	goto err;
    }
    if (fixed_row_size)
    {						// Fill with space
      if (item->max_length > used_length)
      {
	/* QQ:  Fix by adding a my_b_fill() function */
	if (!space_inited)
	{
	  space_inited=1;
	  memset(space, ' ', sizeof(space));
	}
	uint32_t length=item->max_length-used_length;
	for (; length > sizeof(space) ; length-=sizeof(space))
	{
	  if (my_b_write(&cache,(unsigned char*) space,sizeof(space)))
	    goto err;
	}
	if (my_b_write(&cache,(unsigned char*) space,length))
	  goto err;
      }
    }
    if (res && enclosed)
    {
      if (my_b_write(&cache, (unsigned char*) exchange->enclosed->ptr(),
                     exchange->enclosed->length()))
        goto err;
    }
    if (--items_left)
    {
      if (my_b_write(&cache, (unsigned char*) exchange->field_term->ptr(),
                     field_term_length))
        goto err;
    }
  }
  if (my_b_write(&cache,(unsigned char*) exchange->line_term->ptr(),
		 exchange->line_term->length()))
    goto err;
  return(0);
err:
  return(1);
}


/***************************************************************************
** Dump  of select to a binary file
***************************************************************************/


int
select_dump::prepare(List<Item> &list __attribute__((unused)),
		     SELECT_LEX_UNIT *u)
{
  unit= u;
  return (int) ((file= create_file(session, path, exchange, &cache)) < 0);
}


bool select_dump::send_data(List<Item> &items)
{
  List_iterator_fast<Item> li(items);
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  tmp.length(0);
  Item *item;

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return(0);
  }
  if (row_count++ > 1)
  {
    my_message(ER_TOO_MANY_ROWS, ER(ER_TOO_MANY_ROWS), MYF(0));
    goto err;
  }
  while ((item=li++))
  {
    res=item->str_result(&tmp);
    if (!res)					// If NULL
    {
      if (my_b_write(&cache,(unsigned char*) "",1))
	goto err;
    }
    else if (my_b_write(&cache,(unsigned char*) res->ptr(),res->length()))
    {
      my_error(ER_ERROR_ON_WRITE, MYF(0), path, my_errno);
      goto err;
    }
  }
  return(0);
err:
  return(1);
}


select_subselect::select_subselect(Item_subselect *item_arg)
{
  item= item_arg;
}


bool select_singlerow_subselect::send_data(List<Item> &items)
{
  Item_singlerow_subselect *it= (Item_singlerow_subselect *)item;
  if (it->assigned())
  {
    my_message(ER_SUBQUERY_NO_1_ROW, ER(ER_SUBQUERY_NO_1_ROW), MYF(0));
    return(1);
  }
  if (unit->offset_limit_cnt)
  {				          // Using limit offset,count
    unit->offset_limit_cnt--;
    return(0);
  }
  List_iterator_fast<Item> li(items);
  Item *val_item;
  for (uint32_t i= 0; (val_item= li++); i++)
    it->store(i, val_item);
  it->assigned(1);
  return(0);
}


void select_max_min_finder_subselect::cleanup()
{
  cache= 0;
  return;
}


bool select_max_min_finder_subselect::send_data(List<Item> &items)
{
  Item_maxmin_subselect *it= (Item_maxmin_subselect *)item;
  List_iterator_fast<Item> li(items);
  Item *val_item= li++;
  it->register_value();
  if (it->assigned())
  {
    cache->store(val_item);
    if ((this->*op)())
      it->store(0, cache);
  }
  else
  {
    if (!cache)
    {
      cache= Item_cache::get_cache(val_item);
      switch (val_item->result_type())
      {
      case REAL_RESULT:
	op= &select_max_min_finder_subselect::cmp_real;
	break;
      case INT_RESULT:
	op= &select_max_min_finder_subselect::cmp_int;
	break;
      case STRING_RESULT:
	op= &select_max_min_finder_subselect::cmp_str;
	break;
      case DECIMAL_RESULT:
        op= &select_max_min_finder_subselect::cmp_decimal;
        break;
      case ROW_RESULT:
        // This case should never be choosen
	assert(0);
	op= 0;
      }
    }
    cache->store(val_item);
    it->store(0, cache);
  }
  it->assigned(1);
  return(0);
}

bool select_max_min_finder_subselect::cmp_real()
{
  Item *maxmin= ((Item_singlerow_subselect *)item)->element_index(0);
  double val1= cache->val_real(), val2= maxmin->val_real();
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       val1 > val2);
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     val1 < val2);
}

bool select_max_min_finder_subselect::cmp_int()
{
  Item *maxmin= ((Item_singlerow_subselect *)item)->element_index(0);
  int64_t val1= cache->val_int(), val2= maxmin->val_int();
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       val1 > val2);
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     val1 < val2);
}

bool select_max_min_finder_subselect::cmp_decimal()
{
  Item *maxmin= ((Item_singlerow_subselect *)item)->element_index(0);
  my_decimal cval, *cvalue= cache->val_decimal(&cval);
  my_decimal mval, *mvalue= maxmin->val_decimal(&mval);
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       my_decimal_cmp(cvalue, mvalue) > 0) ;
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     my_decimal_cmp(cvalue,mvalue) < 0);
}

bool select_max_min_finder_subselect::cmp_str()
{
  String *val1, *val2, buf1, buf2;
  Item *maxmin= ((Item_singlerow_subselect *)item)->element_index(0);
  /*
    as far as both operand is Item_cache buf1 & buf2 will not be used,
    but added for safety
  */
  val1= cache->val_str(&buf1);
  val2= maxmin->val_str(&buf1);
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       sortcmp(val1, val2, cache->collation.collation) > 0) ;
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     sortcmp(val1, val2, cache->collation.collation) < 0);
}

bool select_exists_subselect::send_data(List<Item> &items __attribute__((unused)))
{
  Item_exists_subselect *it= (Item_exists_subselect *)item;
  if (unit->offset_limit_cnt)
  { // Using limit offset,count
    unit->offset_limit_cnt--;
    return(0);
  }
  it->value= 1;
  it->assigned(1);
  return(0);
}


/***************************************************************************
  Dump of select to variables
***************************************************************************/

int select_dumpvar::prepare(List<Item> &list, SELECT_LEX_UNIT *u)
{
  unit= u;

  if (var_list.elements != list.elements)
  {
    my_message(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT,
               ER(ER_WRONG_NUMBER_OF_COLUMNS_IN_SELECT), MYF(0));
    return 1;
  }
  return 0;
}


bool select_dumpvar::check_simple_select() const
{
  my_error(ER_SP_BAD_CURSOR_SELECT, MYF(0));
  return true;
}


void select_dumpvar::cleanup()
{
  row_count= 0;
}


void Query_arena::free_items()
{
  Item *next;
  /* This works because items are allocated with sql_alloc() */
  for (; free_list; free_list= next)
  {
    next= free_list->next;
    free_list->delete_self();
  }
  /* Postcondition: free_list is 0 */
  return;
}


/*
  Statement functions
*/

Statement::Statement(LEX *lex_arg, MEM_ROOT *mem_root_arg, ulong id_arg)
  :Query_arena(mem_root_arg),
  id(id_arg),
  mark_used_columns(MARK_COLUMNS_READ),
  lex(lex_arg),
  query(0),
  query_length(0),
  db(NULL),
  db_length(0)
{
}


/*
  Don't free mem_root, as mem_root is freed in the end of dispatch_command
  (once for any command).
*/
void Session::end_statement()
{
  /* Cleanup SQL processing state to reuse this statement in next query. */
  lex_end(lex);
}


bool Session::copy_db_to(char **p_db, size_t *p_db_length)
{
  if (db == NULL)
  {
    my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));
    return true;
  }
  *p_db= strmake(db, db_length);
  *p_db_length= db_length;
  return false;
}


bool select_dumpvar::send_data(List<Item> &items)
{
  List_iterator_fast<my_var> var_li(var_list);
  List_iterator<Item> it(items);
  Item *item;
  my_var *mv;

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return(0);
  }
  if (row_count++)
  {
    my_message(ER_TOO_MANY_ROWS, ER(ER_TOO_MANY_ROWS), MYF(0));
    return(1);
  }
  while ((mv= var_li++) && (item= it++))
  {
    if (mv->local == 0)
    {
      Item_func_set_user_var *suv= new Item_func_set_user_var(mv->s, item);
      suv->fix_fields(session, 0);
      suv->check(0);
      suv->update();
    }
  }
  return(session->is_error());
}

bool select_dumpvar::send_eof()
{
  if (! row_count)
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                 ER_SP_FETCH_NO_DATA, ER(ER_SP_FETCH_NO_DATA));
  /*
    In order to remember the value of affected rows for ROW_COUNT()
    function, SELECT INTO has to have an own SQLCOM.
    TODO: split from SQLCOM_SELECT
  */
  ::my_ok(session,row_count);
  return 0;
}

/****************************************************************************
  TMP_TABLE_PARAM
****************************************************************************/

void TMP_TABLE_PARAM::init()
{
  field_count= sum_func_count= func_count= hidden_field_count= 0;
  group_parts= group_length= group_null_parts= 0;
  quick_group= 1;
  table_charset= 0;
  precomputed_group_by= 0;
  bit_fields_as_long= 0;
  return;
}

void TMP_TABLE_PARAM::cleanup(void)
{
  /* Fix for Intel compiler */
  if (copy_field)
  {
    delete [] copy_field;
    save_copy_field= copy_field= 0;
  }
}


void session_increment_bytes_sent(ulong length)
{
  Session *session=current_session;
  if (likely(session != 0))
  { /* current_session==0 when close_connection() calls net_send_error() */
    session->status_var.bytes_sent+= length;
  }
}


void session_increment_bytes_received(ulong length)
{
  current_session->status_var.bytes_received+= length;
}


void session_increment_net_big_packet_count(ulong length)
{
  current_session->status_var.net_big_packet_count+= length;
}

void Session::send_kill_message() const
{
  int err= killed_errno();
  if (err)
    my_message(err, ER(err), MYF(0));
}

void Session::set_status_var_init()
{
  memset(&status_var, 0, sizeof(status_var));
}

void Security_context::skip_grants()
{
  /* privileges for the user are unknown everything is allowed */
}


/****************************************************************************
  Handling of open and locked tables states.

  This is used when we want to open/lock (and then close) some tables when
  we already have a set of tables open and locked. We use these methods for
  access to mysql.proc table to find definitions of stored routines.
****************************************************************************/

void Session::reset_n_backup_open_tables_state(Open_tables_state *backup)
{
  backup->set_open_tables_state(this);
  reset_open_tables_state();
  state_flags|= Open_tables_state::BACKUPS_AVAIL;
  return;
}


void Session::restore_backup_open_tables_state(Open_tables_state *backup)
{
  /*
    Before we will throw away current open tables state we want
    to be sure that it was properly cleaned up.
  */
  assert(open_tables == 0 && temporary_tables == 0 &&
              handler_tables == 0 && derived_tables == 0 &&
              lock == 0 && locked_tables == 0);
  set_open_tables_state(backup);
  return;
}


bool Session::set_db(const char *new_db, size_t new_db_len)
{
  /* Do not reallocate memory if current chunk is big enough. */
  if (db && new_db && db_length >= new_db_len)
    memcpy(db, new_db, new_db_len+1);
  else
  {
    if (db)
      free(db);
    if (new_db)
    {
      db= (char *)malloc(new_db_len + 1);
      if (db != NULL)
      {
        memcpy(db, new_db, new_db_len);
        db[new_db_len]= 0;
      }
    }
    else
      db= NULL;
  }
  db_length= db ? new_db_len : 0;
  return new_db && !db;
}


/**
  Check the killed state of a user thread
  @param session  user thread
  @retval 0 the user thread is active
  @retval 1 the user thread has been killed
*/
extern "C" int session_killed(const Session *session)
{
  return(session->killed);
}

/**
  Return the thread id of a user thread
  @param session user thread
  @return thread id
*/
extern "C" unsigned long session_get_thread_id(const Session *session)
{
  return((unsigned long)session->thread_id);
}


extern "C"
LEX_STRING *session_make_lex_string(Session *session, LEX_STRING *lex_str,
                                const char *str, unsigned int size,
                                int allocate_lex_string)
{
  return session->make_lex_string(lex_str, str, size,
                              (bool) allocate_lex_string);
}

extern "C" const struct charset_info_st *session_charset(Session *session)
{
  return(session->charset());
}

extern "C" char **session_query(Session *session)
{
  return(&session->query);
}

extern "C" int session_slave_thread(const Session *session)
{
  return(session->slave_thread);
}

extern "C" int session_non_transactional_update(const Session *session)
{
  return(session->transaction.all.modified_non_trans_table);
}

extern "C" void session_mark_transaction_to_rollback(Session *session, bool all)
{
  mark_transaction_to_rollback(session, all);
}


/**
  Mark transaction to rollback and mark error as fatal to a sub-statement.

  @param  session   Thread handle
  @param  all   true <=> rollback main transaction.
*/

void mark_transaction_to_rollback(Session *session, bool all)
{
  if (session)
  {
    session->is_fatal_sub_stmt_error= true;
    session->transaction_rollback_request= all;
  }
}
/***************************************************************************
  Handling of XA id cacheing
***************************************************************************/

pthread_mutex_t LOCK_xid_cache;
HASH xid_cache;

extern "C" unsigned char *xid_get_hash_key(const unsigned char *, size_t *, bool);
extern "C" void xid_free_hash(void *);

unsigned char *xid_get_hash_key(const unsigned char *ptr, size_t *length,
                        bool not_used __attribute__((unused)))
{
  *length=((XID_STATE*)ptr)->xid.key_length();
  return ((XID_STATE*)ptr)->xid.key();
}

void xid_free_hash(void *ptr)
{
  if (!((XID_STATE*)ptr)->in_session)
    free((unsigned char*)ptr);
}

bool xid_cache_init()
{
  pthread_mutex_init(&LOCK_xid_cache, MY_MUTEX_INIT_FAST);
  return hash_init(&xid_cache, &my_charset_bin, 100, 0, 0,
                   xid_get_hash_key, xid_free_hash, 0) != 0;
}

void xid_cache_free()
{
  if (hash_inited(&xid_cache))
  {
    hash_free(&xid_cache);
    pthread_mutex_destroy(&LOCK_xid_cache);
  }
}

XID_STATE *xid_cache_search(XID *xid)
{
  pthread_mutex_lock(&LOCK_xid_cache);
  XID_STATE *res=(XID_STATE *)hash_search(&xid_cache, xid->key(), xid->key_length());
  pthread_mutex_unlock(&LOCK_xid_cache);
  return res;
}


bool xid_cache_insert(XID *xid, enum xa_states xa_state)
{
  XID_STATE *xs;
  bool res;
  pthread_mutex_lock(&LOCK_xid_cache);
  if (hash_search(&xid_cache, xid->key(), xid->key_length()))
    res=0;
  else if (!(xs=(XID_STATE *)malloc(sizeof(*xs))))
    res=1;
  else
  {
    xs->xa_state=xa_state;
    xs->xid.set(xid);
    xs->in_session=0;
    res=my_hash_insert(&xid_cache, (unsigned char*)xs);
  }
  pthread_mutex_unlock(&LOCK_xid_cache);
  return res;
}


bool xid_cache_insert(XID_STATE *xid_state)
{
  pthread_mutex_lock(&LOCK_xid_cache);
  assert(hash_search(&xid_cache, xid_state->xid.key(),
                          xid_state->xid.key_length())==0);
  bool res=my_hash_insert(&xid_cache, (unsigned char*)xid_state);
  pthread_mutex_unlock(&LOCK_xid_cache);
  return res;
}


void xid_cache_delete(XID_STATE *xid_state)
{
  pthread_mutex_lock(&LOCK_xid_cache);
  hash_delete(&xid_cache, (unsigned char *)xid_state);
  pthread_mutex_unlock(&LOCK_xid_cache);
}

namespace {
  /**
     Class to handle temporary allocation of memory for row data.

     The responsibilities of the class is to provide memory for
     packing one or two rows of packed data (depending on what
     constructor is called).

     In order to make the allocation more efficient for "simple" rows,
     i.e., rows that do not contain any blobs, a pointer to the
     allocated memory is of memory is stored in the table structure
     for simple rows.  If memory for a table containing a blob field
     is requested, only memory for that is allocated, and subsequently
     released when the object is destroyed.

   */
  class Row_data_memory {
  public:
    /**
      Build an object to keep track of a block-local piece of memory
      for storing a row of data.

      @param table
      Table where the pre-allocated memory is stored.

      @param length
      Length of data that is needed, if the record contain blobs.
     */
    Row_data_memory(Table *table, size_t const len1)
      : m_memory(0)
    {
      m_alloc_checked= false;
      allocate_memory(table, len1);
      m_ptr[0]= has_memory() ? m_memory : 0;
      m_ptr[1]= 0;
    }

    Row_data_memory(Table *table, size_t const len1, size_t const len2)
      : m_memory(0)
    {
      m_alloc_checked= false;
      allocate_memory(table, len1 + len2);
      m_ptr[0]= has_memory() ? m_memory        : 0;
      m_ptr[1]= has_memory() ? m_memory + len1 : 0;
    }

    ~Row_data_memory()
    {
      if (m_memory != 0 && m_release_memory_on_destruction)
        free((unsigned char*) m_memory);
    }

    /**
       Is there memory allocated?

       @retval true There is memory allocated
       @retval false Memory allocation failed
     */
    bool has_memory() const {
      m_alloc_checked= true;
      return m_memory != 0;
    }

    unsigned char *slot(uint32_t s)
    {
      assert(s < sizeof(m_ptr)/sizeof(*m_ptr));
      assert(m_ptr[s] != 0);
      assert(m_alloc_checked == true);
      return m_ptr[s];
    }

  private:
    void allocate_memory(Table *const table, size_t const total_length)
    {
      if (table->s->blob_fields == 0)
      {
        /*
          The maximum length of a packed record is less than this
          length. We use this value instead of the supplied length
          when allocating memory for records, since we don't know how
          the memory will be used in future allocations.

          Since table->s->reclength is for unpacked records, we have
          to add two bytes for each field, which can potentially be
          added to hold the length of a packed field.
        */
        size_t const maxlen= table->s->reclength + 2 * table->s->fields;

        /*
          Allocate memory for two records if memory hasn't been
          allocated. We allocate memory for two records so that it can
          be used when processing update rows as well.
        */
        if (table->write_row_record == 0)
          table->write_row_record=
            (unsigned char *) alloc_root(&table->mem_root, 2 * maxlen);
        m_memory= table->write_row_record;
        m_release_memory_on_destruction= false;
      }
      else
      {
        m_memory= (unsigned char *) malloc(total_length);
        m_release_memory_on_destruction= true;
      }
    }

    mutable bool m_alloc_checked;
    bool m_release_memory_on_destruction;
    unsigned char *m_memory;
    unsigned char *m_ptr[2];
  };
}

bool Discrete_intervals_list::append(uint64_t start, uint64_t val,
                                 uint64_t incr)
{
  /* first, see if this can be merged with previous */
  if ((head == NULL) || tail->merge_if_contiguous(start, val, incr))
  {
    /* it cannot, so need to add a new interval */
    Discrete_interval *new_interval= new Discrete_interval(start, val, incr);
    return(append(new_interval));
  }
  return(0);
}

bool Discrete_intervals_list::append(Discrete_interval *new_interval)
{
  if (unlikely(new_interval == NULL))
    return(1);
  if (head == NULL)
    head= current= new_interval;
  else
    tail->next= new_interval;
  tail= new_interval;
  elements++;
  return(0);
}

/**
  Close a connection.

  @param session		Thread handle
  @param errcode	Error code to print to console
  @param lock	        1 if we have have to lock LOCK_thread_count

  @note
    For the connection that is doing shutdown, this is called twice
*/
void Session::close_connection(uint32_t errcode, bool lock)
{
  st_vio *vio;
  if (lock)
    (void) pthread_mutex_lock(&LOCK_thread_count);
  killed= Session::KILL_CONNECTION;
  if ((vio= net.vio) != 0)
  {
    if (errcode)
      net_send_error(this, errcode, ER(errcode)); /* purecov: inspected */
    net_close(&net);		/* vio is freed in delete session */
  }
  if (lock)
    (void) pthread_mutex_unlock(&LOCK_thread_count);
}



/**
 Reset Session part responsible for command processing state.

   This needs to be called before execution of every statement
   (prepared or conventional).
   It is not called by substatements of routines.

  @todo
   Make it a method of Session and align its name with the rest of
   reset/end/start/init methods.
  @todo
   Call it after we use Session for queries, not before.
*/

void Session::reset_for_next_command()
{
  free_list= 0;
  select_number= 1;
  /*
    Those two lines below are theoretically unneeded as
    Session::cleanup_after_query() should take care of this already.
  */
  auto_inc_intervals_in_cur_stmt_for_binlog.empty();

  is_fatal_error= 0;
  server_status&= ~ (SERVER_MORE_RESULTS_EXISTS |
                          SERVER_QUERY_NO_INDEX_USED |
                          SERVER_QUERY_NO_GOOD_INDEX_USED);
  /*
    If in autocommit mode and not in a transaction, reset
    OPTION_STATUS_NO_TRANS_UPDATE | OPTION_KEEP_LOG to not get warnings
    in ha_rollback_trans() about some tables couldn't be rolled back.
  */
  if (!(options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
  {
    options&= ~OPTION_KEEP_LOG;
    transaction.all.modified_non_trans_table= false;
  }
  thread_specific_used= false;

  if (opt_bin_log)
  {
    reset_dynamic(&user_var_events);
    user_var_events_alloc= mem_root;
  }
  clear_error();
  main_da.reset_diagnostics_area();
  total_warn_count=0;			// Warnings for this query
  sent_row_count= examined_row_count= 0;

  return;
}
