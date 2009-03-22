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

#include <drizzled/server_includes.h>
#include <drizzled/session.h>
#include <sys/stat.h>
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
#include <drizzled/show.h>
#include <drizzled/plugin_scheduling.h>
#include <libdrizzleclient/errmsg.h>

extern scheduling_st thread_scheduler;
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
                              bool )
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

Session::Session()
   :Statement(&main_lex, &main_mem_root,
              /* statement id */ 0),
   Open_tables_state(refresh_version),
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
   m_lip(NULL),
   scheduler(0)
{
  uint64_t tmp;

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
  thread_specific_used= false;
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
  memset(&variables, 0, sizeof(variables));
  thread_id= 0;
  file_id = 0;
  query_id= 0;
  warn_id= 0;
  db_charset= global_system_variables.collation_database;
  memset(ha_data, 0, sizeof(ha_data));
  replication_data= 0;
  mysys_var=0;
  dbug_sentry=Session_SENTRY_MAGIC;
  net.vio= 0;
  client_capabilities= 0;                       // minimalistic client
  system_thread= NON_SYSTEM_THREAD;
  cleanup_done= abort_on_warning= no_warnings_for_error= false;
  peer_port= 0;					// For SHOW PROCESSLIST
  transaction.on= 1;
  pthread_mutex_init(&LOCK_delete, MY_MUTEX_INIT_FAST);

  /* Variables with default values */
  proc_info="login";
  where= Session::DEFAULT_WHERE;
  server_id = ::server_id;
  command=COM_CONNECT;
  *scramble= '\0';

  init();
  /* Initialize sub structures */
  init_sql_alloc(&warn_root, WARN_ALLOC_BLOCK_SIZE, WARN_ALLOC_PREALLOC_SIZE);
  hash_init(&user_vars, system_charset_info, USER_VARS_HASH_SIZE, 0, 0,
	    (hash_get_key) get_var_key,
	    (hash_free_key) free_user_var, 0);

  /* Protocol */
  protocol= &protocol_text;			// Default protocol
  protocol_text.init(this);

  const Query_id& local_query_id= Query_id::get_query_id();
  tablespace_op= false;
  tmp= sql_rnd();
  drizzleclient_randominit(&rand, tmp + (uint64_t) &rand,
                           tmp + (uint64_t)local_query_id.value());
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
  assert(cleanup_done == false);

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
  hash_free(&user_vars);
  close_temporary_tables();

  if (global_read_lock)
    unlock_global_read_lock(this);

  cleanup_done= true;
  return;
}

Session::~Session()
{
  Session_CHECK_SENTRY(this);
  add_to_status(&global_status_var, &status_var);

  if (drizzleclient_vio_ok())
  {
    if (global_system_variables.log_warnings)
        errmsg_printf(ERRMSG_LVL_WARN, ER(ER_FORCING_CLOSE),my_progname,
                      thread_id,
                      (security_ctx.user.c_str() ?
                       security_ctx.user.c_str() : ""));
    disconnect(0, false);
  }

  /* Close connection */
  if (net.vio)
  {
    drizzleclient_net_close(&net);
    drizzleclient_net_end(&net);
  }
  if (cleanup_done == false)
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
  dbug_sentry= Session_SENTRY_GONE;

  free_root(&main_mem_root, MYF(0));
  pthread_setspecific(THR_Session,  0);


  /* Ensure that no one is using Session */
  pthread_mutex_unlock(&LOCK_delete);
  pthread_mutex_destroy(&LOCK_delete);
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

void Session::prepareForQueries()
{
  if (variables.max_join_size == HA_POS_ERROR)
    options |= OPTION_BIG_SELECTS;
  if (client_capabilities & CLIENT_COMPRESS)
    net.compress= true;

  version= refresh_version;
  set_proc_info(0);
  command= COM_SLEEP;
  set_time();
  init_for_queries();

  /* In the past this would only run of the user did not have SUPER_ACL */
  if (sys_init_connect.value_length)
  {
    execute_init_command(this, &sys_init_connect, &LOCK_sys_init_connect);
    if (is_error())
    {
      Security_context *sctx= &security_ctx;
      killed= Session::KILL_CONNECTION;
      errmsg_printf(ERRMSG_LVL_WARN
                  , ER(ER_NEW_ABORTING_CONNECTION)
                  , thread_id
                  , (db ? db : "unconnected")
                  , sctx->user.empty() == false ? sctx->user.c_str() : "unauthenticated"
                  , sctx->ip.c_str(), "init_connect command failed");
      errmsg_printf(ERRMSG_LVL_WARN, "%s", main_da.message());
    }
    set_proc_info(0);
    set_time();
    init_for_queries();
  }
}

bool Session::initGlobals()
{
  if (store_globals())
  {
    disconnect(ER_OUT_OF_RESOURCES, true);
    statistic_increment(aborted_connects, &LOCK_status);
    thread_scheduler.end_thread(this, 0);
    return false;
  }
  return true;
}

bool Session::authenticate()
{
  /* Use "connect_timeout" value during connection phase */
  drizzleclient_net_set_read_timeout(&net, connect_timeout);
  drizzleclient_net_set_write_timeout(&net, connect_timeout);

  lex_start(this);

  bool connection_is_valid= check_connection();
  drizzleclient_net_end_statement(this);

  if (! connection_is_valid)
  {	
    /* We got wrong permissions from check_connection() */
    statistic_increment(aborted_connects, &LOCK_status);
    return false;
  }

  /* Connect completed, set read/write timeouts back to default */
  drizzleclient_net_set_read_timeout(&net, variables.net_read_timeout);
  drizzleclient_net_set_write_timeout(&net, variables.net_write_timeout);
  return true;
}

bool Session::check_connection()
{
  uint32_t pkt_len= 0;
  char *end;

  // TCP/IP connection
  {
    char ip[NI_MAXHOST];

    if (drizzleclient_net_peer_addr(&net, ip, &peer_port, NI_MAXHOST))
    {
      my_error(ER_BAD_HOST_ERROR, MYF(0), security_ctx.ip.c_str());
      return false;
    }

    security_ctx.ip.assign(ip);
  }
  drizzleclient_net_keepalive(&net, true);

  uint32_t server_capabilites;
  {
    /* buff[] needs to big enough to hold the server_version variable */
    char buff[SERVER_VERSION_LENGTH + SCRAMBLE_LENGTH + 64];

    server_capabilites= CLIENT_BASIC_FLAGS;

#ifdef HAVE_COMPRESS
    server_capabilites|= CLIENT_COMPRESS;
#endif /* HAVE_COMPRESS */

    end= buff + strlen(server_version);
    if ((end - buff) >= SERVER_VERSION_LENGTH)
      end= buff + (SERVER_VERSION_LENGTH - 1);
    memcpy(buff, server_version, end - buff);
    *end= 0;
    end++;

    int4store((unsigned char*) end, thread_id);
    end+= 4;
    /*
      So as check_connection is the only entry point to authorization
      procedure, scramble is set here. This gives us new scramble for
      each handshake.
    */
    drizzleclient_create_random_string(scramble, SCRAMBLE_LENGTH, &rand);
    /*
      Old clients does not understand long scrambles, but can ignore packet
      tail: that's why first part of the scramble is placed here, and second
      part at the end of packet.
    */
    end= strncpy(end, scramble, SCRAMBLE_LENGTH_323);
    end+= SCRAMBLE_LENGTH_323;

    *end++= 0; /* an empty byte for some reason */

    int2store(end, server_capabilites);
    /* write server characteristics: up to 16 bytes allowed */
    end[2]=(char) default_charset_info->number;
    int2store(end+3, server_status);
    memset(end+5, 0, 13);
    end+= 18;
    /* write scramble tail */
    size_t scramble_len= SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323;
    end= strncpy(end, scramble + SCRAMBLE_LENGTH_323, scramble_len);
    end+= scramble_len;

    *end++= 0; /* an empty byte for some reason */

    /* At this point we write connection message and read reply */
    if (drizzleclient_net_write_command(&net
          , (unsigned char) protocol_version
          , (unsigned char*) ""
          , 0
          , (unsigned char*) buff
          , (size_t) (end-buff)) 
        ||	(pkt_len= drizzleclient_net_read(&net)) == packet_error 
        || pkt_len < MIN_HANDSHAKE_SIZE)
    {
      my_error(ER_HANDSHAKE_ERROR, MYF(0), security_ctx.ip.c_str());
      return false;
    }
  }
  if (packet.alloc(variables.net_buffer_length))
    return false; /* The error is set by alloc(). */

  client_capabilities= uint2korr(net.read_pos);


  client_capabilities|= ((uint32_t) uint2korr(net.read_pos + 2)) << 16;
  max_client_packet_length= uint4korr(net.read_pos + 4);
  update_charset();
  end= (char*) net.read_pos + 32;

  /*
    Disable those bits which are not supported by the server.
    This is a precautionary measure, if the client lies. See Bug#27944.
  */
  client_capabilities&= server_capabilites;

  if (end >= (char*) net.read_pos + pkt_len + 2)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), security_ctx.ip.c_str());
    return false;
  }

  net.return_status= &server_status;

  char *user= end;
  char *passwd= strchr(user, '\0')+1;
  uint32_t user_len= passwd - user - 1;
  char *l_db= passwd;
  char db_buff[NAME_LEN + 1];           // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH + 1];	// buffer to store user in utf8
  uint32_t dummy_errors;

  /*
    Old clients send null-terminated string as password; new clients send
    the size (1 byte) + string (not null-terminated). Hence in case of empty
    password both send '\0'.

    This strlen() can't be easily deleted without changing protocol.

    Cast *passwd to an unsigned char, so that it doesn't extend the sign for
    *passwd > 127 and become 2**32-127+ after casting to uint.
  */
  uint32_t passwd_len= client_capabilities & CLIENT_SECURE_CONNECTION ?
    (unsigned char)(*passwd++) : strlen(passwd);
  l_db= client_capabilities & CLIENT_CONNECT_WITH_DB ? l_db + passwd_len + 1 : 0;

  /* strlen() can't be easily deleted without changing protocol */
  uint32_t db_len= l_db ? strlen(l_db) : 0;

  if (passwd + passwd_len + db_len > (char *) net.read_pos + pkt_len)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), security_ctx.ip.c_str());
    return false;
  }

  /* Since 4.1 all database names are stored in utf8 */
  if (l_db)
  {
    db_buff[copy_and_convert(db_buff, sizeof(db_buff)-1,
                             system_charset_info,
                             l_db, db_len,
                             charset(), &dummy_errors)]= 0;
    l_db= db_buff;
  }

  user_buff[user_len= copy_and_convert(user_buff, sizeof(user_buff)-1,
                                       system_charset_info, user, user_len,
                                       charset(), &dummy_errors)]= '\0';
  user= user_buff;

  /* If username starts and ends in "'", chop them off */
  if (user_len > 1 && user[0] == '\'' && user[user_len - 1] == '\'')
  {
    user[user_len-1]= 0;
    user++;
    user_len-= 2;
  }

  security_ctx.user.assign(user);

  return check_user(passwd, passwd_len, l_db);
}

bool Session::check_user(const char *passwd, uint32_t passwd_len, const char *in_db)
{
  LEX_STRING db_str= { (char *) in_db, in_db ? strlen(in_db) : 0 };
  bool is_authenticated;

  /*
    Clear session->db as it points to something, that will be freed when
    connection is closed. We don't want to accidentally free a wrong
    pointer if connect failed. Also in case of 'CHANGE USER' failure,
    current database will be switched to 'no database selected'.
  */
  reset_db(NULL, 0);

  if (passwd_len != 0 && passwd_len != SCRAMBLE_LENGTH)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), security_ctx.ip.c_str());
    return false;
  }

  is_authenticated= authenticate_user(this, passwd);

  if (is_authenticated != true)
  {
    my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
             security_ctx.user.c_str(),
             security_ctx.ip.c_str(),
             passwd_len ? ER(ER_YES) : ER(ER_NO));

    return false;
  }

  security_ctx.skip_grants();

  /* Change database if necessary */
  if (in_db && in_db[0])
  {
    if (mysql_change_db(this, &db_str, false))
    {
      /* mysql_change_db() has pushed the error message. */
      return false;
    }
  }
  my_ok();
  password= test(passwd_len);          // remember for error messages

  /* Ready to handle queries */
  return true;
}

bool Session::executeStatement()
{
  bool return_value;
  char *l_packet= 0;
  uint32_t packet_length;

  enum enum_server_command l_command;

  /*
    indicator of uninitialized lex => normal flow of errors handling
    (see my_message_sql)
  */
  lex->current_select= 0;

  /*
    This thread will do a blocking read from the client which
    will be interrupted when the next command is received from
    the client, the connection is closed or "net_wait_timeout"
    number of seconds has passed
  */
  drizzleclient_net_set_read_timeout(&net, variables.net_wait_timeout);

  /*
    XXX: this code is here only to clear possible errors of init_connect.
    Consider moving to init_connect() instead.
  */
  clear_error();				// Clear error message
  main_da.reset_diagnostics_area();

  net_new_transaction(&net);

  packet_length= drizzleclient_net_read(&net);
  if (packet_length == packet_error)
  {
    /* Check if we can continue without closing the connection */

    if(net.last_errno== CR_NET_PACKET_TOO_LARGE)
      my_error(ER_NET_PACKET_TOO_LARGE, MYF(0));
    /* Assert is invalid for dirty connection shutdown
     *     assert(session->is_error());
     */
    drizzleclient_net_end_statement(this);

    if (net.error != 3)
    {
      return_value= false;                       // We have to close it.
      goto out;
    }

    net.error= 0;
    return_value= true;
    goto out;
  }

  l_packet= (char*) net.read_pos;
  /*
    'packet_length' contains length of data, as it was stored in packet
    header. In case of malformed header, drizzleclient_net_read returns zero.
    If packet_length is not zero, drizzleclient_net_read ensures that the returned
    number of bytes was actually read from network.
    There is also an extra safety measure in drizzleclient_net_read:
    it sets packet[packet_length]= 0, but only for non-zero packets.
  */
  if (packet_length == 0)                       /* safety */
  {
    /* Initialize with COM_SLEEP packet */
    l_packet[0]= (unsigned char) COM_SLEEP;
    packet_length= 1;
  }
  /* Do not rely on drizzleclient_net_read, extra safety against programming errors. */
  l_packet[packet_length]= '\0';                  /* safety */

  l_command= (enum enum_server_command) (unsigned char) l_packet[0];

  if (command >= COM_END)
    command= COM_END;                           // Wrong command

  /* Restore read timeout value */
  drizzleclient_net_set_read_timeout(&net, variables.net_read_timeout);

  assert(packet_length);
  return_value= ! dispatch_command(l_command, this, l_packet+1, (uint32_t) (packet_length-1));

out:
  return return_value;
}

bool Session::readAndStoreQuery(const char *in_packet, uint32_t in_packet_length)
{
  /* Remove garbage at start and end of query */
  while (in_packet_length > 0 && my_isspace(charset(), in_packet[0]))
  {
    in_packet++;
    in_packet_length--;
  }
  const char *pos= in_packet + in_packet_length; /* Point at end null */
  while (in_packet_length > 0 &&
	 (pos[-1] == ';' || my_isspace(charset() ,pos[-1])))
  {
    pos--;
    in_packet_length--;
  }

  /* We must allocate some extra memory for the cached query string */
  query_length= 0; /* Extra safety: Avoid races */
  query= (char*) memdup_w_gap((unsigned char*) in_packet, in_packet_length, db_length + 1);
  if (! query)
    return false;

  query[in_packet_length]=0;
  query_length= in_packet_length;

  /* Reclaim some memory */
  packet.shrink(variables.net_buffer_length);
  convert_buffer.shrink(variables.net_buffer_length);

  return true;
}

bool Session::endTransaction(enum enum_mysql_completiontype completion)
{
  bool do_release= 0;
  bool result= true;

  if (transaction.xid_state.xa_state != XA_NOTR)
  {
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[transaction.xid_state.xa_state]);
    return false;
  }
  switch (completion) 
  {
    case COMMIT:
      /*
       * We don't use endActiveTransaction() here to ensure that this works
       * even if there is a problem with the OPTION_AUTO_COMMIT flag
       * (Which of course should never happen...)
       */
      server_status&= ~SERVER_STATUS_IN_TRANS;
      if (ha_commit(this))
        result= false;
      options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
      transaction.all.modified_non_trans_table= false;
      break;
    case COMMIT_RELEASE:
      do_release= 1; /* fall through */
    case COMMIT_AND_CHAIN:
      result= endActiveTransaction();
      if (result == true && completion == COMMIT_AND_CHAIN)
        result= startTransaction();
      break;
    case ROLLBACK_RELEASE:
      do_release= 1; /* fall through */
    case ROLLBACK:
    case ROLLBACK_AND_CHAIN:
    {
      server_status&= ~SERVER_STATUS_IN_TRANS;
      if (ha_rollback(this))
        result= false;
      options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
      transaction.all.modified_non_trans_table= false;
      if (result == true && (completion == ROLLBACK_AND_CHAIN))
        result= startTransaction();
      break;
    }
    default:
      my_error(ER_UNKNOWN_COM_ERROR, MYF(0));
      return false;
  }

  if (result == false)
    my_error(killed_errno(), MYF(0));
  else if ((result == true) && do_release)
    killed= Session::KILL_CONNECTION;

  return result;
}

bool Session::endActiveTransaction()
{
  bool result= true;

  if (transaction.xid_state.xa_state != XA_NOTR)
  {
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[transaction.xid_state.xa_state]);
    return false;
  }
  if (options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN | OPTION_TABLE_LOCK))
  {
    /* Safety if one did "drop table" on locked tables */
    if (! locked_tables)
      options&= ~OPTION_TABLE_LOCK;
    server_status&= ~SERVER_STATUS_IN_TRANS;
    if (ha_commit(this))
      result= false;
  }
  options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
  transaction.all.modified_non_trans_table= false;
  return result;
}

bool Session::startTransaction()
{
  bool result= true;

  if (locked_tables)
  {
    lock= locked_tables;
    locked_tables= 0;			// Will be automatically closed
    close_thread_tables(this);			// Free tables
  }
  if (! endActiveTransaction())
    result= false;
  else
  {
    options|= OPTION_BEGIN;
    server_status|= SERVER_STATUS_IN_TRANS;
    if (lex->start_transaction_opt & DRIZZLE_START_TRANS_OPT_WITH_CONS_SNAPSHOT)
      if (ha_start_consistent_snapshot(this))
        result= false;
  }
  return result;
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
    !String::needs_conversion(0,charset(),variables.getCollation(),
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
    session->my_ok(row_count);
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


static File create_file(Session *session, char *path, file_exchange *exchange, IO_CACHE *cache)
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
select_export::prepare(List<Item> &list, Select_Lex_Unit *u)
{
  bool blob_flag=0;
  bool string_results= false, non_string_results= false;
  unit= u;
  if ((uint32_t) strlen(exchange->file_name) + NAME_LEN >= FN_REFLEN)
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
        const CHARSET_INFO * const character_set_client= default_charset_info;

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
	    if (my_b_write(&cache,(unsigned char*) start,(uint32_t) (pos-start)) ||
		my_b_write(&cache,(unsigned char*) tmp_buff,2))
	      goto err;
	    start=pos+1;
	  }
	}
	if (my_b_write(&cache,(unsigned char*) start,(uint32_t) (pos-start)))
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
select_dump::prepare(List<Item> &, Select_Lex_Unit *u)
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

bool select_exists_subselect::send_data(List<Item> &)
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


/****************************************************************************
  Tmp_Table_Param
****************************************************************************/

void Tmp_Table_Param::init()
{
  field_count= sum_func_count= func_count= hidden_field_count= 0;
  group_parts= group_length= group_null_parts= 0;
  quick_group= 1;
  table_charset= 0;
  precomputed_group_by= 0;
  bit_fields_as_long= 0;
  return;
}

void Tmp_Table_Param::cleanup(void)
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
  { /* current_session==0 when disconnect() calls net_send_error() */
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

void Session::disconnect(uint32_t errcode, bool should_lock)
{
  /* Allow any plugins to cleanup their session variables */
  plugin_sessionvar_cleanup(this);

  /* If necessary, log any aborted or unauthorized connections */
  if (killed || (net.error && net.vio != 0))
    statistic_increment(aborted_threads, &LOCK_status);

  if (net.error && net.vio != 0)
  {
    if (! killed && variables.log_warnings > 1)
    {
      Security_context *sctx= &security_ctx;

      errmsg_printf(ERRMSG_LVL_WARN, ER(ER_NEW_ABORTING_CONNECTION)
                  , thread_id
                  , (db ? db : "unconnected")
                  , sctx->user.empty() == false ? sctx->user.c_str() : "unauthenticated"
                  , sctx->ip.c_str()
                  , (main_da.is_error() ? main_da.message() : ER(ER_UNKNOWN_ERROR)));
    }
  }

  /* Close out our connection to the client */
  st_vio *vio;
  if (should_lock)
    (void) pthread_mutex_lock(&LOCK_thread_count);
  killed= Session::KILL_CONNECTION;
  if ((vio= net.vio) != 0)
  {
    if (errcode)
      net_send_error(this, errcode, ER(errcode)); /* purecov: inspected */
    drizzleclient_net_close(&net);		/* vio is freed in delete session */
  }
  if (should_lock)
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

  clear_error();
  main_da.reset_diagnostics_area();
  total_warn_count=0;			// Warnings for this query
  sent_row_count= examined_row_count= 0;

  return;
}

/*
  Close all temporary tables created by 'CREATE TEMPORARY TABLE' for thread
  creates one DROP TEMPORARY Table binlog event for each pseudo-thread
*/

void Session::close_temporary_tables()
{
  Table *table;
  Table *tmp_next;

  if (!temporary_tables)
    return;

  for (table= temporary_tables; table; table= tmp_next)
  {
    tmp_next= table->next;
    close_temporary(table, 1, 1);
  }
  temporary_tables= 0;

  return;
}


/** Clear most status variables. */
extern time_t flush_status_time;
extern uint32_t max_used_connections;

void Session::refresh_status()
{
  pthread_mutex_lock(&LOCK_status);

  /* Add thread's status variabes to global status */
  add_to_status(&global_status_var, &status_var);

  /* Reset thread's status variables */
  memset(&status_var, 0, sizeof(status_var));

  /* Reset some global variables */
  reset_status_vars();

  /* Reset the counters of all key caches (default and named). */
  process_key_caches(reset_key_cache_counters);
  flush_status_time= time((time_t*) 0);
  max_used_connections= 1; /* We set it to one, because we know we exist */
  pthread_mutex_unlock(&LOCK_status);
}
