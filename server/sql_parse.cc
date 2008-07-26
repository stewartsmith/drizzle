/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define MYSQL_LEX 1
#include "mysql_priv.h"
#include "sql_repl.h"
#include "rpl_filter.h"
#include "repl_failsafe.h"
#include <m_ctype.h>
#include <myisam.h>
#include <my_dir.h>

/**
  @defgroup Runtime_Environment Runtime Environment
  @{
*/

static bool execute_sqlcom_select(THD *thd, TABLE_LIST *all_tables);

const char *any_db="*any*";	// Special symbol for check_access

const LEX_STRING command_name[]={
  { C_STRING_WITH_LEN("Sleep") },
  { C_STRING_WITH_LEN("Quit") },
  { C_STRING_WITH_LEN("Init DB") },
  { C_STRING_WITH_LEN("Query") },
  { C_STRING_WITH_LEN("Field List") },
  { C_STRING_WITH_LEN("Create DB") },
  { C_STRING_WITH_LEN("Drop DB") },
  { C_STRING_WITH_LEN("Refresh") },
  { C_STRING_WITH_LEN("Shutdown") },
  { C_STRING_WITH_LEN("Statistics") },
  { C_STRING_WITH_LEN("Processlist") },
  { C_STRING_WITH_LEN("Connect") },
  { C_STRING_WITH_LEN("Kill") },
  { C_STRING_WITH_LEN("Debug") },
  { C_STRING_WITH_LEN("Ping") },
  { C_STRING_WITH_LEN("Time") },
  { C_STRING_WITH_LEN("Delayed insert") },
  { C_STRING_WITH_LEN("Change user") },
  { C_STRING_WITH_LEN("Binlog Dump") },
  { C_STRING_WITH_LEN("Table Dump") },
  { C_STRING_WITH_LEN("Connect Out") },
  { C_STRING_WITH_LEN("Register Slave") },
  { C_STRING_WITH_LEN("Prepare") },
  { C_STRING_WITH_LEN("Execute") },
  { C_STRING_WITH_LEN("Long Data") },
  { C_STRING_WITH_LEN("Close stmt") },
  { C_STRING_WITH_LEN("Reset stmt") },
  { C_STRING_WITH_LEN("Set option") },
  { C_STRING_WITH_LEN("Fetch") },
  { C_STRING_WITH_LEN("Daemon") },
  { C_STRING_WITH_LEN("Error") }  // Last command number
};

const char *xa_state_names[]={
  "NON-EXISTING", "ACTIVE", "IDLE", "PREPARED"
};

static void unlock_locked_tables(THD *thd)
{
  if (thd->locked_tables)
  {
    thd->lock=thd->locked_tables;
    thd->locked_tables=0;			// Will be automatically closed
    close_thread_tables(thd);			// Free tables
  }
}


bool end_active_trans(THD *thd)
{
  int error=0;
  if (unlikely(thd->in_sub_stmt))
  {
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    return(1);
  }
  if (thd->transaction.xid_state.xa_state != XA_NOTR)
  {
    my_error(ER_XAER_RMFAIL, MYF(0),
             xa_state_names[thd->transaction.xid_state.xa_state]);
    return(1);
  }
  if (thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN |
		      OPTION_TABLE_LOCK))
  {
    /* Safety if one did "drop table" on locked tables */
    if (!thd->locked_tables)
      thd->options&= ~OPTION_TABLE_LOCK;
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    if (ha_commit(thd))
      error=1;
  }
  thd->options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
  thd->transaction.all.modified_non_trans_table= false;
  return(error);
}


bool begin_trans(THD *thd)
{
  int error=0;
  if (unlikely(thd->in_sub_stmt))
  {
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    return 1;
  }
  if (thd->locked_tables)
  {
    thd->lock=thd->locked_tables;
    thd->locked_tables=0;			// Will be automatically closed
    close_thread_tables(thd);			// Free tables
  }
  if (end_active_trans(thd))
    error= -1;
  else
  {
    LEX *lex= thd->lex;
    thd->options|= OPTION_BEGIN;
    thd->server_status|= SERVER_STATUS_IN_TRANS;
    if (lex->start_transaction_opt & MYSQL_START_TRANS_OPT_WITH_CONS_SNAPSHOT)
      error= ha_start_consistent_snapshot(thd);
  }
  return error;
}

/**
  Returns true if all tables should be ignored.
*/
inline bool all_tables_not_ok(THD *thd, TABLE_LIST *tables)
{
  return rpl_filter->is_on() && tables &&
         !rpl_filter->tables_ok(thd->db, tables);
}


static bool some_non_temp_table_to_be_updated(THD *thd, TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table; table= table->next_global)
  {
    assert(table->db && table->table_name);
    if (table->updating &&
        !find_temporary_table(thd, table->db, table->table_name))
      return 1;
  }
  return 0;
}


/**
  Mark all commands that somehow changes a table.

  This is used to check number of updates / hour.

  sql_command is actually set to SQLCOM_END sometimes
  so we need the +1 to include it in the array.

  See COMMAND_FLAG_xxx for different type of commands
     2  - query that returns meaningful ROW_COUNT() -
          a number of modified rows
*/

uint sql_command_flags[SQLCOM_END+1];

void init_update_queries(void)
{
  bzero((uchar*) &sql_command_flags, sizeof(sql_command_flags));

  sql_command_flags[SQLCOM_CREATE_TABLE]=   CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_CREATE_INDEX]=   CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_ALTER_TABLE]=    CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND;
  sql_command_flags[SQLCOM_TRUNCATE]=       CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND;
  sql_command_flags[SQLCOM_DROP_TABLE]=     CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_LOAD]=           CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_CREATE_DB]=      CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_DROP_DB]=        CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_RENAME_TABLE]=   CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_DROP_INDEX]=     CF_CHANGES_DATA;

  sql_command_flags[SQLCOM_UPDATE]=	    CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_UPDATE_MULTI]=   CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_INSERT]=	    CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_INSERT_SELECT]=  CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_DELETE]=         CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_DELETE_MULTI]=   CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_REPLACE]=        CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_REPLACE_SELECT]= CF_CHANGES_DATA | CF_HAS_ROW_COUNT;

  sql_command_flags[SQLCOM_SHOW_STATUS]=      CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_DATABASES]=   CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_OPEN_TABLES]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_FIELDS]=      CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_KEYS]=        CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_VARIABLES]=   CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CHARSETS]=    CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_COLLATIONS]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_BINLOGS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_HOSTS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_BINLOG_EVENTS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_WARNS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ERRORS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_STATUS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROCESSLIST]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_DB]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_MASTER_STAT]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_STAT]=  CF_STATUS_COMMAND;

   sql_command_flags[SQLCOM_SHOW_TABLES]=       (CF_STATUS_COMMAND |
                                               CF_SHOW_TABLE_COMMAND);
  sql_command_flags[SQLCOM_SHOW_TABLE_STATUS]= (CF_STATUS_COMMAND |
                                                CF_SHOW_TABLE_COMMAND);
  /*
    The following admin table operations are allowed
    on log tables.
  */
  sql_command_flags[SQLCOM_REPAIR]=           CF_WRITE_LOGS_COMMAND;
  sql_command_flags[SQLCOM_OPTIMIZE]=         CF_WRITE_LOGS_COMMAND;
  sql_command_flags[SQLCOM_ANALYZE]=          CF_WRITE_LOGS_COMMAND;
}


bool is_update_query(enum enum_sql_command command)
{
  assert(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_CHANGES_DATA) != 0;
}

void execute_init_command(THD *thd, sys_var_str *init_command_var,
			  rw_lock_t *var_mutex)
{
  Vio* save_vio;
  ulong save_client_capabilities;

  thd_proc_info(thd, "Execution of init_command");
  /*
    We need to lock init_command_var because
    during execution of init_command_var query
    values of init_command_var can't be changed
  */
  rw_rdlock(var_mutex);
  save_client_capabilities= thd->client_capabilities;
  thd->client_capabilities|= CLIENT_MULTI_QUERIES;
  /*
    We don't need return result of execution to client side.
    To forbid this we should set thd->net.vio to 0.
  */
  save_vio= thd->net.vio;
  thd->net.vio= 0;
  dispatch_command(COM_QUERY, thd,
                   init_command_var->value,
                   init_command_var->value_length);
  rw_unlock(var_mutex);
  thd->client_capabilities= save_client_capabilities;
  thd->net.vio= save_vio;
}

/**
  Ends the current transaction and (maybe) begin the next.

  @param thd            Current thread
  @param completion     Completion type

  @retval
    0   OK
*/

int end_trans(THD *thd, enum enum_mysql_completiontype completion)
{
  bool do_release= 0;
  int res= 0;

  if (unlikely(thd->in_sub_stmt))
  {
    my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
    return(1);
  }
  if (thd->transaction.xid_state.xa_state != XA_NOTR)
  {
    my_error(ER_XAER_RMFAIL, MYF(0),
             xa_state_names[thd->transaction.xid_state.xa_state]);
    return(1);
  }
  switch (completion) {
  case COMMIT:
    /*
     We don't use end_active_trans() here to ensure that this works
     even if there is a problem with the OPTION_AUTO_COMMIT flag
     (Which of course should never happen...)
    */
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    res= ha_commit(thd);
    thd->options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
    thd->transaction.all.modified_non_trans_table= false;
    break;
  case COMMIT_RELEASE:
    do_release= 1; /* fall through */
  case COMMIT_AND_CHAIN:
    res= end_active_trans(thd);
    if (!res && completion == COMMIT_AND_CHAIN)
      res= begin_trans(thd);
    break;
  case ROLLBACK_RELEASE:
    do_release= 1; /* fall through */
  case ROLLBACK:
  case ROLLBACK_AND_CHAIN:
  {
    thd->server_status&= ~SERVER_STATUS_IN_TRANS;
    if (ha_rollback(thd))
      res= -1;
    thd->options&= ~(OPTION_BEGIN | OPTION_KEEP_LOG);
    thd->transaction.all.modified_non_trans_table= false;
    if (!res && (completion == ROLLBACK_AND_CHAIN))
      res= begin_trans(thd);
    break;
  }
  default:
    res= -1;
    my_error(ER_UNKNOWN_COM_ERROR, MYF(0));
    return(-1);
  }

  if (res < 0)
    my_error(thd->killed_errno(), MYF(0));
  else if ((res == 0) && do_release)
    thd->killed= THD::KILL_CONNECTION;

  return(res);
}


/**
  Read one command from connection and execute it (query or simple command).
  This function is called in loop from thread function.

  For profiling to work, it must never be called recursively.

  @retval
    0  success
  @retval
    1  request of thread shutdown (see dispatch_command() description)
*/

bool do_command(THD *thd)
{
  bool return_value;
  char *packet= 0;
  ulong packet_length;
  NET *net= &thd->net;
  enum enum_server_command command;

  /*
    indicator of uninitialized lex => normal flow of errors handling
    (see my_message_sql)
  */
  thd->lex->current_select= 0;

  /*
    This thread will do a blocking read from the client which
    will be interrupted when the next command is received from
    the client, the connection is closed or "net_wait_timeout"
    number of seconds has passed
  */
  my_net_set_read_timeout(net, thd->variables.net_wait_timeout);

  /*
    XXX: this code is here only to clear possible errors of init_connect. 
    Consider moving to init_connect() instead.
  */
  thd->clear_error();				// Clear error message
  thd->main_da.reset_diagnostics_area();

  net_new_transaction(net);

  packet_length= my_net_read(net);
  if (packet_length == packet_error)
  {
    /* Check if we can continue without closing the connection */

    /* The error must be set. */
    assert(thd->is_error());
    net_end_statement(thd);

    if (net->error != 3)
    {
      return_value= true;                       // We have to close it.
      goto out;
    }

    net->error= 0;
    return_value= false;
    goto out;
  }

  packet= (char*) net->read_pos;
  /*
    'packet_length' contains length of data, as it was stored in packet
    header. In case of malformed header, my_net_read returns zero.
    If packet_length is not zero, my_net_read ensures that the returned
    number of bytes was actually read from network.
    There is also an extra safety measure in my_net_read:
    it sets packet[packet_length]= 0, but only for non-zero packets.
  */
  if (packet_length == 0)                       /* safety */
  {
    /* Initialize with COM_SLEEP packet */
    packet[0]= (uchar) COM_SLEEP;
    packet_length= 1;
  }
  /* Do not rely on my_net_read, extra safety against programming errors. */
  packet[packet_length]= '\0';                  /* safety */

  command= (enum enum_server_command) (uchar) packet[0];

  if (command >= COM_END)
    command= COM_END;                           // Wrong command

  /* Restore read timeout value */
  my_net_set_read_timeout(net, thd->variables.net_read_timeout);

  assert(packet_length);
  return_value= dispatch_command(command, thd, packet+1, (uint) (packet_length-1));

out:
  return(return_value);
}

/**
  Determine if an attempt to update a non-temporary table while the
  read-only option was enabled has been made.

  This is a helper function to mysql_execute_command.

  @note SQLCOM_MULTI_UPDATE is an exception and dealt with elsewhere.

  @see mysql_execute_command

  @returns Status code
    @retval true The statement should be denied.
    @retval false The statement isn't updating any relevant tables.
*/

static bool deny_updates_if_read_only_option(THD *thd,
                                                TABLE_LIST *all_tables)
{
  if (!opt_readonly)
    return(false);

  LEX *lex= thd->lex;

  if (!(sql_command_flags[lex->sql_command] & CF_CHANGES_DATA))
    return(false);

  /* Multi update is an exception and is dealt with later. */
  if (lex->sql_command == SQLCOM_UPDATE_MULTI)
    return(false);

  const bool create_temp_tables= 
    (lex->sql_command == SQLCOM_CREATE_TABLE) &&
    (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE);

  const bool drop_temp_tables= 
    (lex->sql_command == SQLCOM_DROP_TABLE) &&
    lex->drop_temporary;

  const bool update_real_tables=
    some_non_temp_table_to_be_updated(thd, all_tables) &&
    !(create_temp_tables || drop_temp_tables);


  const bool create_or_drop_databases=
    (lex->sql_command == SQLCOM_CREATE_DB) ||
    (lex->sql_command == SQLCOM_DROP_DB);

  if (update_real_tables || create_or_drop_databases)
  {
      /*
        An attempt was made to modify one or more non-temporary tables.
      */
      return(true);
  }


  /* Assuming that only temporary tables are modified. */
  return(false);
}

/**
  Perform one connection-level (COM_XXXX) command.

  @param command         type of command to perform
  @param thd             connection handle
  @param packet          data for the command, packet is always null-terminated
  @param packet_length   length of packet + 1 (to show that data is
                         null-terminated) except for COM_SLEEP, where it
                         can be zero.

  @todo
    set thd->lex->sql_command to SQLCOM_END here.
  @todo
    The following has to be changed to an 8 byte integer

  @retval
    0   ok
  @retval
    1   request of thread shutdown, i. e. if command is
        COM_QUIT/COM_SHUTDOWN
*/
bool dispatch_command(enum enum_server_command command, THD *thd,
		      char* packet, uint packet_length)
{
  NET *net= &thd->net;
  bool error= 0;

  thd->command=command;
  /*
    Commands which always take a long time are logged into
    the slow log only if opt_log_slow_admin_statements is set.
  */
  thd->enable_slow_log= true;
  thd->lex->sql_command= SQLCOM_END; /* to avoid confusing VIEW detectors */
  thd->set_time();
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query_id= global_query_id;

  switch( command ) {
  /* Ignore these statements. */
  case COM_STATISTICS:
  case COM_PING:
    break;
  /* Increase id and count all other statements. */
  default:
    statistic_increment(thd->status_var.questions, &LOCK_status);
    next_query_id();
  }

  thread_running++;
  /* TODO: set thd->lex->sql_command to SQLCOM_END here */
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  thd->server_status&=
           ~(SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED);
  switch (command) {
  case COM_INIT_DB:
  {
    LEX_STRING tmp;
    status_var_increment(thd->status_var.com_stat[SQLCOM_CHANGE_DB]);
    thd->convert_string(&tmp, system_charset_info,
                        packet, packet_length, thd->charset());
    if (!mysql_change_db(thd, &tmp, false))
    {
      general_log_write(thd, command, thd->db, thd->db_length);
      my_ok(thd);
    }
    break;
  }
  case COM_REGISTER_SLAVE:
  {
    if (!register_slave(thd, (uchar*)packet, packet_length))
      my_ok(thd);
    break;
  }
  case COM_CHANGE_USER:
  {
    status_var_increment(thd->status_var.com_other);
    char *user= (char*) packet, *packet_end= packet + packet_length;
    /* Safe because there is always a trailing \0 at the end of the packet */
    char *passwd= strend(user)+1;

    thd->clear_error();                         // if errors from rollback

    /*
      Old clients send null-terminated string ('\0' for empty string) for
      password.  New clients send the size (1 byte) + string (not null
      terminated, so also '\0' for empty string).

      Cast *passwd to an unsigned char, so that it doesn't extend the sign
      for *passwd > 127 and become 2**32-127 after casting to uint.
    */
    char db_buff[NAME_LEN+1];                 // buffer to store db in utf8
    char *db= passwd;
    char *save_db;
    /*
      If there is no password supplied, the packet must contain '\0',
      in any type of handshake (4.1 or pre-4.1).
     */
    if (passwd >= packet_end)
    {
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      break;
    }
    uint passwd_len= (thd->client_capabilities & CLIENT_SECURE_CONNECTION ?
                      (uchar)(*passwd++) : strlen(passwd));
    uint dummy_errors, save_db_length, db_length;
    int res;
    Security_context save_security_ctx= *thd->security_ctx;
    USER_CONN *save_user_connect;

    db+= passwd_len + 1;
    /*
      Database name is always NUL-terminated, so in case of empty database
      the packet must contain at least the trailing '\0'.
    */
    if (db >= packet_end)
    {
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      break;
    }
    db_length= strlen(db);

    char *ptr= db + db_length + 1;
    uint cs_number= 0;

    if (ptr < packet_end)
    {
      if (ptr + 2 > packet_end)
      {
        my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
        break;
      }

      cs_number= uint2korr(ptr);
    }

    /* Convert database name to utf8 */
    db_buff[copy_and_convert(db_buff, sizeof(db_buff)-1,
                             system_charset_info, db, db_length,
                             thd->charset(), &dummy_errors)]= 0;
    db= db_buff;

    /* Save user and privileges */
    save_db_length= thd->db_length;
    save_db= thd->db;
    save_user_connect= thd->user_connect;

    if (!(thd->security_ctx->user= my_strdup(user, MYF(0))))
    {
      thd->security_ctx->user= save_security_ctx.user;
      my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES), MYF(0));
      break;
    }

    /* Clear variables that are allocated */
    thd->user_connect= 0;
    thd->security_ctx->priv_user= thd->security_ctx->user;
    res= check_user(thd, COM_CHANGE_USER, passwd, passwd_len, db, false);

    if (res)
    {
      x_free(thd->security_ctx->user);
      *thd->security_ctx= save_security_ctx;
      thd->user_connect= save_user_connect;
      thd->db= save_db;
      thd->db_length= save_db_length;
    }
    else
    {
      x_free(save_db);
      x_free(save_security_ctx.user);

      if (cs_number)
      {
        thd_init_client_charset(thd, cs_number);
        thd->update_charset();
      }
    }
    break;
  }
  case COM_QUERY:
  {
    if (alloc_query(thd, packet, packet_length))
      break;					// fatal error is set
    char *packet_end= thd->query + thd->query_length;
    const char* end_of_stmt= NULL;

    general_log_write(thd, command, thd->query, thd->query_length);

    mysql_parse(thd, thd->query, thd->query_length, &end_of_stmt);

    while (!thd->killed && (end_of_stmt != NULL) && ! thd->is_error())
    {
      char *beginning_of_next_stmt= (char*) end_of_stmt;

      net_end_statement(thd);
      /*
        Multiple queries exits, execute them individually
      */
      close_thread_tables(thd);
      ulong length= (ulong)(packet_end - beginning_of_next_stmt);

      log_slow_statement(thd);

      /* Remove garbage at start of query */
      while (length > 0 && my_isspace(thd->charset(), *beginning_of_next_stmt))
      {
        beginning_of_next_stmt++;
        length--;
      }

      VOID(pthread_mutex_lock(&LOCK_thread_count));
      thd->query_length= length;
      thd->query= beginning_of_next_stmt;
      /*
        Count each statement from the client.
      */
      statistic_increment(thd->status_var.questions, &LOCK_status);
      thd->query_id= next_query_id();
      thd->set_time(); /* Reset the query start time. */
      /* TODO: set thd->lex->sql_command to SQLCOM_END here */
      VOID(pthread_mutex_unlock(&LOCK_thread_count));

      mysql_parse(thd, beginning_of_next_stmt, length, &end_of_stmt);
    }
    break;
  }
  case COM_FIELD_LIST:				// This isn't actually needed
  {
    char *fields, *packet_end= packet + packet_length, *arg_end;
    /* Locked closure of all tables */
    TABLE_LIST table_list;
    LEX_STRING conv_name;

    /* used as fields initializator */
    lex_start(thd);

    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_FIELDS]);
    bzero((char*) &table_list,sizeof(table_list));
    if (thd->copy_db_to(&table_list.db, &table_list.db_length))
      break;
    /*
      We have name + wildcard in packet, separated by endzero
    */
    arg_end= strend(packet);
    thd->convert_string(&conv_name, system_charset_info,
			packet, (uint) (arg_end - packet), thd->charset());
    table_list.alias= table_list.table_name= conv_name.str;
    packet= arg_end + 1;

    if (!my_strcasecmp(system_charset_info, table_list.db,
                       INFORMATION_SCHEMA_NAME.str))
    {
      ST_SCHEMA_TABLE *schema_table= find_schema_table(thd, table_list.alias);
      if (schema_table)
        table_list.schema_table= schema_table;
    }

    thd->query_length= (uint) (packet_end - packet); // Don't count end \0
    if (!(thd->query=fields= (char*) thd->memdup(packet,thd->query_length+1)))
      break;
    general_log_print(thd, command, "%s %s", table_list.table_name, fields);
    if (lower_case_table_names)
      my_casedn_str(files_charset_info, table_list.table_name);

    /* init structures for VIEW processing */
    table_list.select_lex= &(thd->lex->select_lex);

    lex_start(thd);
    mysql_reset_thd_for_next_command(thd);

    thd->lex->
      select_lex.table_list.link_in_list((uchar*) &table_list,
                                         (uchar**) &table_list.next_local);
    thd->lex->add_to_query_tables(&table_list);

    /* switch on VIEW optimisation: do not fill temporary tables */
    thd->lex->sql_command= SQLCOM_SHOW_FIELDS;
    mysqld_list_fields(thd,&table_list,fields);
    thd->lex->unit.cleanup();
    thd->cleanup_after_query();
    break;
  }
  case COM_QUIT:
    /* We don't calculate statistics for this command */
    general_log_print(thd, command, NullS);
    net->error=0;				// Don't give 'abort' message
    thd->main_da.disable_status();              // Don't send anything back
    error=true;					// End server
    break;
  case COM_BINLOG_DUMP:
    {
      ulong pos;
      ushort flags;
      uint32_t slave_server_id;

      status_var_increment(thd->status_var.com_other);
      thd->enable_slow_log= opt_log_slow_admin_statements;
      /* TODO: The following has to be changed to an 8 byte integer */
      pos = uint4korr(packet);
      flags = uint2korr(packet + 4);
      thd->server_id=0; /* avoid suicide */
      if ((slave_server_id= uint4korr(packet+6))) // mysqlbinlog.server_id==0
	kill_zombie_dump_threads(slave_server_id);
      thd->server_id = slave_server_id;

      general_log_print(thd, command, "Log: '%s'  Pos: %ld", packet+10,
                      (long) pos);
      mysql_binlog_send(thd, thd->strdup(packet + 10), (my_off_t) pos, flags);
      unregister_slave(thd,1,1);
      /*  fake COM_QUIT -- if we get here, the thread needs to terminate */
      error = true;
      break;
    }
  case COM_SHUTDOWN:
  {
    status_var_increment(thd->status_var.com_other);
    /*
      If the client is < 4.1.3, it is going to send us no argument; then
      packet_length is 0, packet[0] is the end 0 of the packet. Note that
      SHUTDOWN_DEFAULT is 0. If client is >= 4.1.3, the shutdown level is in
      packet[0].
    */
    enum drizzle_enum_shutdown_level level=
      (enum drizzle_enum_shutdown_level) (uchar) packet[0];
    if (level == SHUTDOWN_DEFAULT)
      level= SHUTDOWN_WAIT_ALL_BUFFERS; // soon default will be configurable
    else if (level != SHUTDOWN_WAIT_ALL_BUFFERS)
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "this shutdown level");
      break;
    }
    general_log_print(thd, command, NullS);
    my_eof(thd);
    close_thread_tables(thd);			// Free before kill
    kill_mysql();
    error=true;
    break;
  }
  case COM_STATISTICS:
  {
    STATUS_VAR current_global_status_var;
    ulong uptime;
    uint length;
    uint64_t queries_per_second1000;
    char buff[250];
    uint buff_len= sizeof(buff);

    general_log_print(thd, command, NullS);
    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_STATUS]);
    calc_sum_of_all_status(&current_global_status_var);
    if (!(uptime= (ulong) (thd->start_time - server_start_time)))
      queries_per_second1000= 0;
    else
      queries_per_second1000= thd->query_id * 1000LL / uptime;

    length= snprintf((char*) buff, buff_len - 1,
                     "Uptime: %lu  Threads: %d  Questions: %lu  "
                     "Slow queries: %lu  Opens: %lu  Flush tables: %lu  "
                     "Open tables: %u  Queries per second avg: %u.%u",
                     uptime,
                     (int) thread_count, (ulong) thd->query_id,
                     current_global_status_var.long_query_count,
                     current_global_status_var.opened_tables,
                     refresh_version,
                     cached_open_tables(),
                     (uint) (queries_per_second1000 / 1000),
                     (uint) (queries_per_second1000 % 1000));
    /* Store the buffer in permanent memory */
    my_ok(thd, 0, 0, buff);
    VOID(my_net_write(net, (uchar*) buff, length));
    VOID(net_flush(net));
    thd->main_da.disable_status();
    break;
  }
  case COM_PING:
    status_var_increment(thd->status_var.com_other);
    my_ok(thd);				// Tell client we are alive
    break;
  case COM_PROCESS_INFO:
    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_PROCESSLIST]);
    general_log_print(thd, command, NullS);
    mysqld_list_processes(thd, NullS, 0);
    break;
  case COM_PROCESS_KILL:
  {
    status_var_increment(thd->status_var.com_stat[SQLCOM_KILL]);
    ulong id=(ulong) uint4korr(packet);
    sql_kill(thd,id,false);
    break;
  }
  case COM_SET_OPTION:
  {
    status_var_increment(thd->status_var.com_stat[SQLCOM_SET_OPTION]);
    uint opt_command= uint2korr(packet);

    switch (opt_command) {
    case (int) MYSQL_OPTION_MULTI_STATEMENTS_ON:
      thd->client_capabilities|= CLIENT_MULTI_STATEMENTS;
      my_eof(thd);
      break;
    case (int) MYSQL_OPTION_MULTI_STATEMENTS_OFF:
      thd->client_capabilities&= ~CLIENT_MULTI_STATEMENTS;
      my_eof(thd);
      break;
    default:
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      break;
    }
    break;
  }
  case COM_SLEEP:
  case COM_CONNECT:				// Impossible here
  case COM_TIME:				// Impossible from client
  case COM_END:
  default:
    my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
    break;
  }

  /* If commit fails, we should be able to reset the OK status. */
  thd->main_da.can_overwrite_status= true;
  ha_autocommit_or_rollback(thd, thd->is_error());
  thd->main_da.can_overwrite_status= false;

  thd->transaction.stmt.reset();


  /* report error issued during command execution */
  if (thd->killed_errno())
  {
    if (! thd->main_da.is_set())
      thd->send_kill_message();
  }
  if (thd->killed == THD::KILL_QUERY || thd->killed == THD::KILL_BAD_DATA)
  {
    thd->killed= THD::NOT_KILLED;
    thd->mysys_var->abort= 0;
  }

  net_end_statement(thd);

  thd->proc_info= "closing tables";
  /* Free tables */
  close_thread_tables(thd);

  log_slow_statement(thd);

  thd_proc_info(thd, "cleaning up");
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For process list
  thd_proc_info(thd, 0);
  thd->command=COM_SLEEP;
  thd->query=0;
  thd->query_length=0;
  thread_running--;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd->packet.shrink(thd->variables.net_buffer_length);	// Reclaim some memory
  free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
  return(error);
}


void log_slow_statement(THD *thd)
{
  /*
    The following should never be true with our current code base,
    but better to keep this here so we don't accidently try to log a
    statement in a trigger or stored function
  */
  if (unlikely(thd->in_sub_stmt))
    return;                           // Don't set time for sub stmt

  /*
    Do not log administrative statements unless the appropriate option is
    set; do not log into slow log if reading from backup.
  */
  if (thd->enable_slow_log && !thd->user_time)
  {
    thd_proc_info(thd, "logging slow query");
    uint64_t end_utime_of_query= thd->current_utime();

    if (((end_utime_of_query - thd->utime_after_lock) >
         thd->variables.long_query_time ||
         ((thd->server_status &
           (SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED)) &&
          opt_log_queries_not_using_indexes &&
           !(sql_command_flags[thd->lex->sql_command] & CF_STATUS_COMMAND))) &&
        thd->examined_row_count >= thd->variables.min_examined_row_limit)
    {
      thd_proc_info(thd, "logging slow query");
      thd->status_var.long_query_count++;
      slow_log_print(thd, thd->query, thd->query_length, end_utime_of_query);
    }
  }
  return;
}


/**
  Create a TABLE_LIST object for an INFORMATION_SCHEMA table.

    This function is used in the parser to convert a SHOW or DESCRIBE
    table_name command to a SELECT from INFORMATION_SCHEMA.
    It prepares a SELECT_LEX and a TABLE_LIST object to represent the
    given command as a SELECT parse tree.

  @param thd              thread handle
  @param lex              current lex
  @param table_ident      table alias if it's used
  @param schema_table_idx the type of the INFORMATION_SCHEMA table to be
                          created

  @note
    Due to the way this function works with memory and LEX it cannot
    be used outside the parser (parse tree transformations outside
    the parser break PS and SP).

  @retval
    0                 success
  @retval
    1                 out of memory or SHOW commands are not allowed
                      in this version of the server.
*/

int prepare_schema_table(THD *thd, LEX *lex, Table_ident *table_ident,
                         enum enum_schema_tables schema_table_idx)
{
  SELECT_LEX *schema_select_lex= NULL;

  switch (schema_table_idx) {
  case SCH_SCHEMATA:
    break;
  case SCH_TABLE_NAMES:
  case SCH_TABLES:
    {
      LEX_STRING db;
      size_t dummy;
      if (lex->select_lex.db == NULL &&
          lex->copy_db_to(&lex->select_lex.db, &dummy))
      {
        return(1);
      }
      schema_select_lex= new SELECT_LEX();
      db.str= schema_select_lex->db= lex->select_lex.db;
      schema_select_lex->table_list.first= NULL;
      db.length= strlen(db.str);

      if (check_db_name(&db))
      {
        my_error(ER_WRONG_DB_NAME, MYF(0), db.str);
        return(1);
      }
      break;
    }
  case SCH_COLUMNS:
  case SCH_STATISTICS:
  {
    assert(table_ident);
    TABLE_LIST **query_tables_last= lex->query_tables_last;
    schema_select_lex= new SELECT_LEX();
    /* 'parent_lex' is used in init_query() so it must be before it. */
    schema_select_lex->parent_lex= lex;
    schema_select_lex->init_query();
    if (!schema_select_lex->add_table_to_list(thd, table_ident, 0, 0, TL_READ))
      return(1);
    lex->query_tables_last= query_tables_last;
    break;
  }
  case SCH_OPEN_TABLES:
  case SCH_VARIABLES:
  case SCH_STATUS:
  case SCH_CHARSETS:
  case SCH_COLLATIONS:
  case SCH_COLLATION_CHARACTER_SET_APPLICABILITY:
  case SCH_TABLE_CONSTRAINTS:
  case SCH_KEY_COLUMN_USAGE:
  default:
    break;
  }
  
  SELECT_LEX *select_lex= lex->current_select;
  assert(select_lex);
  if (make_schema_select(thd, select_lex, schema_table_idx))
  {
    return(1);
  }
  TABLE_LIST *table_list= (TABLE_LIST*) select_lex->table_list.first;
  assert(table_list);
  table_list->schema_select_lex= schema_select_lex;
  table_list->schema_table_reformed= 1;
  return(0);
}


/**
  Read query from packet and store in thd->query.
  Used in COM_QUERY and COM_STMT_PREPARE.

    Sets the following THD variables:
  - query
  - query_length

  @retval
    false ok
  @retval
    true  error;  In this case thd->fatal_error is set
*/

bool alloc_query(THD *thd, const char *packet, uint packet_length)
{
  /* Remove garbage at start and end of query */
  while (packet_length > 0 && my_isspace(thd->charset(), packet[0]))
  {
    packet++;
    packet_length--;
  }
  const char *pos= packet + packet_length;     // Point at end null
  while (packet_length > 0 &&
	 (pos[-1] == ';' || my_isspace(thd->charset() ,pos[-1])))
  {
    pos--;
    packet_length--;
  }
  /* We must allocate some extra memory for query cache */
  thd->query_length= 0;                        // Extra safety: Avoid races
  if (!(thd->query= (char*) thd->memdup_w_gap((uchar*) (packet),
					      packet_length,
					      thd->db_length+ 1)))
    return true;
  thd->query[packet_length]=0;
  thd->query_length= packet_length;

  /* Reclaim some memory */
  thd->packet.shrink(thd->variables.net_buffer_length);
  thd->convert_buffer.shrink(thd->variables.net_buffer_length);

  return false;
}

static void reset_one_shot_variables(THD *thd) 
{
  thd->variables.character_set_client=
    global_system_variables.character_set_client;
  thd->variables.collation_connection=
    global_system_variables.collation_connection;
  thd->variables.collation_database=
    global_system_variables.collation_database;
  thd->variables.collation_server=
    global_system_variables.collation_server;
  thd->update_charset();
  thd->variables.time_zone=
    global_system_variables.time_zone;
  thd->variables.lc_time_names= &my_locale_en_US;
  thd->one_shot_set= 0;
}


/**
  Execute command saved in thd and lex->sql_command.

    Before every operation that can request a write lock for a table
    wait if a global read lock exists. However do not wait if this
    thread has locked tables already. No new locks can be requested
    until the other locks are released. The thread that requests the
    global read lock waits for write locked tables to become unlocked.

    Note that wait_if_global_read_lock() sets a protection against a new
    global read lock when it succeeds. This needs to be released by
    start_waiting_global_read_lock() after the operation.

  @param thd                       Thread handle

  @todo
    - Invalidate the table in the query cache if something changed
    after unlocking when changes become visible.
    TODO: this is workaround. right way will be move invalidating in
    the unlock procedure.
    - TODO: use check_change_password()
    - JOIN is not supported yet. TODO
    - SUSPEND and FOR MIGRATE are not supported yet. TODO

  @retval
    false       OK
  @retval
    true        Error
*/

int
mysql_execute_command(THD *thd)
{
  int res= false;
  bool need_start_waiting= false; // have protection against global read lock
  int  up_result= 0;
  LEX  *lex= thd->lex;
  /* first SELECT_LEX (have special meaning for many of non-SELECTcommands) */
  SELECT_LEX *select_lex= &lex->select_lex;
  /* first table of first SELECT_LEX */
  TABLE_LIST *first_table= (TABLE_LIST*) select_lex->table_list.first;
  /* list of all tables in query */
  TABLE_LIST *all_tables;
  /* most outer SELECT_LEX_UNIT of query */
  SELECT_LEX_UNIT *unit= &lex->unit;
  /* Saved variable value */

  /*
    In many cases first table of main SELECT_LEX have special meaning =>
    check that it is first table in global list and relink it first in 
    queries_tables list if it is necessary (we need such relinking only
    for queries with subqueries in select list, in this case tables of
    subqueries will go to global list first)

    all_tables will differ from first_table only if most upper SELECT_LEX
    do not contain tables.

    Because of above in place where should be at least one table in most
    outer SELECT_LEX we have following check:
    assert(first_table == all_tables);
    assert(first_table == all_tables && first_table != 0);
  */
  lex->first_lists_tables_same();
  /* should be assigned after making first tables same */
  all_tables= lex->query_tables;
  /* set context for commands which do not use setup_tables */
  select_lex->
    context.resolve_in_table_list_only((TABLE_LIST*)select_lex->
                                       table_list.first);

  /*
    Reset warning count for each query that uses tables
    A better approach would be to reset this for any commands
    that is not a SHOW command or a select that only access local
    variables, but for now this is probably good enough.
    Don't reset warnings when executing a stored routine.
  */
  if (all_tables || !lex->is_single_level_stmt())
    mysql_reset_errors(thd, 0);

  if (unlikely(thd->slave_thread))
  {
    /*
      Check if statment should be skipped because of slave filtering
      rules

      Exceptions are:
      - UPDATE MULTI: For this statement, we want to check the filtering
        rules later in the code
      - SET: we always execute it (Not that many SET commands exists in
        the binary log anyway -- only 4.1 masters write SET statements,
	in 5.0 there are no SET statements in the binary log)
      - DROP TEMPORARY TABLE IF EXISTS: we always execute it (otherwise we
        have stale files on slave caused by exclusion of one tmp table).
    */
    if (!(lex->sql_command == SQLCOM_UPDATE_MULTI) &&
	!(lex->sql_command == SQLCOM_SET_OPTION) &&
	!(lex->sql_command == SQLCOM_DROP_TABLE &&
          lex->drop_temporary && lex->drop_if_exists) &&
        all_tables_not_ok(thd, all_tables))
    {
      /* we warn the slave SQL thread */
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      if (thd->one_shot_set)
      {
        /*
          It's ok to check thd->one_shot_set here:

          The charsets in a MySQL 5.0 slave can change by both a binlogged
          SET ONE_SHOT statement and the event-internal charset setting, 
          and these two ways to change charsets do not seems to work
          together.

          At least there seems to be problems in the rli cache for
          charsets if we are using ONE_SHOT.  Note that this is normally no
          problem because either the >= 5.0 slave reads a 4.1 binlog (with
          ONE_SHOT) *or* or 5.0 binlog (without ONE_SHOT) but never both."
        */
        reset_one_shot_variables(thd);
      }
      return(0);
    }
  }
  else
  {
    /*
      When option readonly is set deny operations which change non-temporary
      tables. Except for the replication thread and the 'super' users.
    */
    if (deny_updates_if_read_only_option(thd, all_tables))
    {
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
      return(-1);
    }
  } /* endif unlikely slave */
  status_var_increment(thd->status_var.com_stat[lex->sql_command]);

  assert(thd->transaction.stmt.modified_non_trans_table == false);
  
  switch (lex->sql_command) {
  case SQLCOM_SHOW_STATUS:
  {
    system_status_var old_status_var= thd->status_var;
    thd->initial_status_var= &old_status_var;
    res= execute_sqlcom_select(thd, all_tables);
    /* Don't log SHOW STATUS commands to slow query log */
    thd->server_status&= ~(SERVER_QUERY_NO_INDEX_USED |
                           SERVER_QUERY_NO_GOOD_INDEX_USED);
    /*
      restore status variables, as we don't want 'show status' to cause
      changes
    */
    pthread_mutex_lock(&LOCK_status);
    add_diff_to_status(&global_status_var, &thd->status_var,
                       &old_status_var);
    thd->status_var= old_status_var;
    pthread_mutex_unlock(&LOCK_status);
    break;
  }
  case SQLCOM_SHOW_DATABASES:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_TABLE_STATUS:
  case SQLCOM_SHOW_OPEN_TABLES:
  case SQLCOM_SHOW_FIELDS:
  case SQLCOM_SHOW_KEYS:
  case SQLCOM_SHOW_VARIABLES:
  case SQLCOM_SHOW_CHARSETS:
  case SQLCOM_SHOW_COLLATIONS:
  case SQLCOM_SELECT:
  {
    thd->status_var.last_query_cost= 0.0;
    res= execute_sqlcom_select(thd, all_tables);
    break;
  }
  case SQLCOM_EMPTY_QUERY:
    my_ok(thd);
    break;

  case SQLCOM_PURGE:
  {
    res = purge_master_logs(thd, lex->to_log);
    break;
  }
  case SQLCOM_PURGE_BEFORE:
  {
    Item *it;

    /* PURGE MASTER LOGS BEFORE 'data' */
    it= (Item *)lex->value_list.head();
    if ((!it->fixed && it->fix_fields(lex->thd, &it)) ||
        it->check_cols(1))
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "PURGE LOGS BEFORE");
      goto error;
    }
    it= new Item_func_unix_timestamp(it);
    /*
      it is OK only emulate fix_fieds, because we need only
      value of constant
    */
    it->quick_fix_field();
    res = purge_master_logs_before_date(thd, (ulong)it->val_int());
    break;
  }
  case SQLCOM_SHOW_WARNS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      ((1L << (uint) MYSQL_ERROR::WARN_LEVEL_NOTE) |
			       (1L << (uint) MYSQL_ERROR::WARN_LEVEL_WARN) |
			       (1L << (uint) MYSQL_ERROR::WARN_LEVEL_ERROR)
			       ));
    break;
  }
  case SQLCOM_SHOW_ERRORS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      (1L << (uint) MYSQL_ERROR::WARN_LEVEL_ERROR));
    break;
  }
  case SQLCOM_SHOW_SLAVE_HOSTS:
  {
    res = show_slave_hosts(thd);
    break;
  }
  case SQLCOM_SHOW_BINLOG_EVENTS:
  {
    res = mysql_show_binlog_events(thd);
    break;
  }

  case SQLCOM_ASSIGN_TO_KEYCACHE:
  {
    assert(first_table == all_tables && first_table != 0);
    res= mysql_assign_to_keycache(thd, first_table, &lex->ident);
    break;
  }
  case SQLCOM_CHANGE_MASTER:
  {
    pthread_mutex_lock(&LOCK_active_mi);
    res = change_master(thd,active_mi);
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_SLAVE_STAT:
  {
    pthread_mutex_lock(&LOCK_active_mi);
    if (active_mi != NULL)
    {
      res = show_master_info(thd, active_mi);
    }
    else
    {
      push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                   "the master info structure does not exist");
      my_ok(thd);
    }
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_MASTER_STAT:
  {
    res = show_binlog_info(thd);
    break;
  }

  case SQLCOM_SHOW_ENGINE_STATUS:
    {
      res = ha_show_status(thd, lex->create_info.db_type, HA_ENGINE_STATUS);
      break;
    }
  case SQLCOM_CREATE_TABLE:
  {
    /* If CREATE TABLE of non-temporary table, do implicit commit */
    if (!(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE))
    {
      if (end_active_trans(thd))
      {
	res= -1;
	break;
      }
    }
    assert(first_table == all_tables && first_table != 0);
    bool link_to_local;
    // Skip first table, which is the table we are creating
    TABLE_LIST *create_table= lex->unlink_first_table(&link_to_local);
    TABLE_LIST *select_tables= lex->query_tables;
    /*
      Code below (especially in mysql_create_table() and select_create
      methods) may modify HA_CREATE_INFO structure in LEX, so we have to
      use a copy of this structure to make execution prepared statement-
      safe. A shallow copy is enough as this code won't modify any memory
      referenced from this structure.
    */
    HA_CREATE_INFO create_info(lex->create_info);
    /*
      We need to copy alter_info for the same reasons of re-execution
      safety, only in case of Alter_info we have to do (almost) a deep
      copy.
    */
    Alter_info alter_info(lex->alter_info, thd->mem_root);

    if (thd->is_fatal_error)
    {
      /* If out of memory when creating a copy of alter_info. */
      res= 1;
      goto end_with_restore_list;
    }

    if ((res= create_table_precheck(thd, select_tables, create_table)))
      goto end_with_restore_list;

    /* Might have been updated in create_table_precheck */
    create_info.alias= create_table->alias;

#ifdef HAVE_READLINK
    /* Fix names if symlinked tables */
    if (append_file_to_dir(thd, &create_info.data_file_name,
			   create_table->table_name) ||
	append_file_to_dir(thd, &create_info.index_file_name,
			   create_table->table_name))
      goto end_with_restore_list;
#endif
    /*
      If we are using SET CHARSET without DEFAULT, add an implicit
      DEFAULT to not confuse old users. (This may change).
    */
    if ((create_info.used_fields &
	 (HA_CREATE_USED_DEFAULT_CHARSET | HA_CREATE_USED_CHARSET)) ==
	HA_CREATE_USED_CHARSET)
    {
      create_info.used_fields&= ~HA_CREATE_USED_CHARSET;
      create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
      create_info.default_table_charset= create_info.table_charset;
      create_info.table_charset= 0;
    }
    /*
      The create-select command will open and read-lock the select table
      and then create, open and write-lock the new table. If a global
      read lock steps in, we get a deadlock. The write lock waits for
      the global read lock, while the global read lock waits for the
      select table to be closed. So we wait until the global readlock is
      gone before starting both steps. Note that
      wait_if_global_read_lock() sets a protection against a new global
      read lock when it succeeds. This needs to be released by
      start_waiting_global_read_lock(). We protect the normal CREATE
      TABLE in the same way. That way we avoid that a new table is
      created during a gobal read lock.
    */
    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      goto end_with_restore_list;
    }
    if (select_lex->item_list.elements)		// With select
    {
      select_result *result;

      select_lex->options|= SELECT_NO_UNLOCK;
      unit->set_limit(select_lex);

      /*
        Disable non-empty MERGE tables with CREATE...SELECT. Too
        complicated. See Bug #26379. Empty MERGE tables are read-only
        and don't allow CREATE...SELECT anyway.
      */
      if (create_info.used_fields & HA_CREATE_USED_UNION)
      {
        my_error(ER_WRONG_OBJECT, MYF(0), create_table->db,
                 create_table->table_name, "BASE TABLE");
        res= 1;
        goto end_with_restore_list;
      }

      if (!(create_info.options & HA_LEX_CREATE_TMP_TABLE))
      {
        lex->link_first_table_back(create_table, link_to_local);
        create_table->create= true;
      }

      if (!(res= open_and_lock_tables(thd, lex->query_tables)))
      {
        /*
          Is table which we are changing used somewhere in other parts
          of query
        */
        if (!(create_info.options & HA_LEX_CREATE_TMP_TABLE))
        {
          TABLE_LIST *duplicate;
          create_table= lex->unlink_first_table(&link_to_local);
          if ((duplicate= unique_table(thd, create_table, select_tables, 0)))
          {
            update_non_unique_table_error(create_table, "CREATE", duplicate);
            res= 1;
            goto end_with_restore_list;
          }
        }
        /* If we create merge table, we have to test tables in merge, too */
        if (create_info.used_fields & HA_CREATE_USED_UNION)
        {
          TABLE_LIST *tab;
          for (tab= (TABLE_LIST*) create_info.merge_list.first;
               tab;
               tab= tab->next_local)
          {
            TABLE_LIST *duplicate;
            if ((duplicate= unique_table(thd, tab, select_tables, 0)))
            {
              update_non_unique_table_error(tab, "CREATE", duplicate);
              res= 1;
              goto end_with_restore_list;
            }
          }
        }

        /*
          select_create is currently not re-execution friendly and
          needs to be created for every execution of a PS/SP.
        */
        if ((result= new select_create(create_table,
                                       &create_info,
                                       &alter_info,
                                       select_lex->item_list,
                                       lex->duplicates,
                                       lex->ignore,
                                       select_tables)))
        {
          /*
            CREATE from SELECT give its SELECT_LEX for SELECT,
            and item_list belong to SELECT
          */
          res= handle_select(thd, lex, result, 0);
          delete result;
        }
      }
      else if (!(create_info.options & HA_LEX_CREATE_TMP_TABLE))
        create_table= lex->unlink_first_table(&link_to_local);

    }
    else
    {
      /* So that CREATE TEMPORARY TABLE gets to binlog at commit/rollback */
      if (create_info.options & HA_LEX_CREATE_TMP_TABLE)
        thd->options|= OPTION_KEEP_LOG;
      /* regular create */
      if (create_info.options & HA_LEX_CREATE_TABLE_LIKE)
        res= mysql_create_like_table(thd, create_table, select_tables,
                                     &create_info);
      else
      {
        res= mysql_create_table(thd, create_table->db,
                                create_table->table_name, &create_info,
                                &alter_info, 0, 0);
      }
      if (!res)
	my_ok(thd);
    }

    /* put tables back for PS rexecuting */
end_with_restore_list:
    lex->link_first_table_back(create_table, link_to_local);
    break;
  }
  case SQLCOM_CREATE_INDEX:
    /* Fall through */
  case SQLCOM_DROP_INDEX:
  /*
    CREATE INDEX and DROP INDEX are implemented by calling ALTER
    TABLE with proper arguments.

    In the future ALTER TABLE will notice that the request is to
    only add indexes and create these one by one for the existing
    table without having to do a full rebuild.
  */
  {
    /* Prepare stack copies to be re-execution safe */
    HA_CREATE_INFO create_info;
    Alter_info alter_info(lex->alter_info, thd->mem_root);

    if (thd->is_fatal_error) /* out of memory creating a copy of alter_info */
      goto error;

    assert(first_table == all_tables && first_table != 0);
    if (end_active_trans(thd))
      goto error;
    /*
      Currently CREATE INDEX or DROP INDEX cause a full table rebuild
      and thus classify as slow administrative statements just like
      ALTER TABLE.
    */
    thd->enable_slow_log= opt_log_slow_admin_statements;

    bzero((char*) &create_info, sizeof(create_info));
    create_info.db_type= 0;
    create_info.row_type= ROW_TYPE_NOT_USED;
    create_info.default_table_charset= thd->variables.collation_database;

    res= mysql_alter_table(thd, first_table->db, first_table->table_name,
                           &create_info, first_table, &alter_info,
                           0, (ORDER*) 0, 0);
    break;
  }
  case SQLCOM_SLAVE_START:
  {
    pthread_mutex_lock(&LOCK_active_mi);
    start_slave(thd,active_mi,1 /* net report*/);
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SLAVE_STOP:
  /*
    If the client thread has locked tables, a deadlock is possible.
    Assume that
    - the client thread does LOCK TABLE t READ.
    - then the master updates t.
    - then the SQL slave thread wants to update t,
      so it waits for the client thread because t is locked by it.
    - then the client thread does SLAVE STOP.
      SLAVE STOP waits for the SQL slave thread to terminate its
      update t, which waits for the client thread because t is locked by it.
    To prevent that, refuse SLAVE STOP if the
    client thread has locked tables
  */
  if (thd->locked_tables || thd->active_transaction() || thd->global_read_lock)
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    goto error;
  }
  {
    pthread_mutex_lock(&LOCK_active_mi);
    stop_slave(thd,active_mi,1/* net report*/);
    pthread_mutex_unlock(&LOCK_active_mi);
    break;
  }

  case SQLCOM_ALTER_TABLE:
    assert(first_table == all_tables && first_table != 0);
    {
      /*
        Code in mysql_alter_table() may modify its HA_CREATE_INFO argument,
        so we have to use a copy of this structure to make execution
        prepared statement- safe. A shallow copy is enough as no memory
        referenced from this structure will be modified.
      */
      HA_CREATE_INFO create_info(lex->create_info);
      Alter_info alter_info(lex->alter_info, thd->mem_root);

      if (thd->is_fatal_error) /* out of memory creating a copy of alter_info */
      {
        goto error;
      }

      /* Must be set in the parser */
      assert(select_lex->db);

      { // Rename of table
          TABLE_LIST tmp_table;
          bzero((char*) &tmp_table,sizeof(tmp_table));
          tmp_table.table_name= lex->name.str;
          tmp_table.db=select_lex->db;
      }

      /* Don't yet allow changing of symlinks with ALTER TABLE */
      if (create_info.data_file_name)
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                     "DATA DIRECTORY option ignored");
      if (create_info.index_file_name)
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                     "INDEX DIRECTORY option ignored");
      create_info.data_file_name= create_info.index_file_name= NULL;
      /* ALTER TABLE ends previous transaction */
      if (end_active_trans(thd))
	goto error;

      if (!thd->locked_tables &&
          !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
      {
        res= 1;
        break;
      }

      thd->enable_slow_log= opt_log_slow_admin_statements;
      res= mysql_alter_table(thd, select_lex->db, lex->name.str,
                             &create_info,
                             first_table,
                             &alter_info,
                             select_lex->order_list.elements,
                             (ORDER *) select_lex->order_list.first,
                             lex->ignore);
      break;
    }
  case SQLCOM_RENAME_TABLE:
  {
    assert(first_table == all_tables && first_table != 0);
    TABLE_LIST *table;
    for (table= first_table; table; table= table->next_local->next_local)
    {
      TABLE_LIST old_list, new_list;
      /*
        we do not need initialize old_list and new_list because we will
        come table[0] and table->next[0] there
      */
      old_list= table[0];
      new_list= table->next_local[0];
    }

    if (end_active_trans(thd) || mysql_rename_tables(thd, first_table, 0))
      {
        goto error;
      }
    break;
  }
  case SQLCOM_SHOW_BINLOGS:
    {
      res = show_binlogs(thd);
      break;
    }
  case SQLCOM_SHOW_CREATE:
    assert(first_table == all_tables && first_table != 0);
    {
      res= mysqld_show_create(thd, first_table);
      break;
    }
  case SQLCOM_CHECKSUM:
  {
    assert(first_table == all_tables && first_table != 0);
    res = mysql_checksum_table(thd, first_table, &lex->check_opt);
    break;
  }
  case SQLCOM_REPAIR:
  {
    assert(first_table == all_tables && first_table != 0);
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res= mysql_repair_table(thd, first_table, &lex->check_opt);
    /* ! we write after unlocking the table */
    if (!res && !lex->no_write_to_binlog)
    {
      /*
        Presumably, REPAIR and binlog writing doesn't require synchronization
      */
      write_bin_log(thd, true, thd->query, thd->query_length);
    }
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_CHECK:
  {
    assert(first_table == all_tables && first_table != 0);
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res = mysql_check_table(thd, first_table, &lex->check_opt);
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_ANALYZE:
  {
    assert(first_table == all_tables && first_table != 0);
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res= mysql_analyze_table(thd, first_table, &lex->check_opt);
    /* ! we write after unlocking the table */
    if (!res && !lex->no_write_to_binlog)
    {
      /*
        Presumably, ANALYZE and binlog writing doesn't require synchronization
      */
      write_bin_log(thd, true, thd->query, thd->query_length);
    }
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }

  case SQLCOM_OPTIMIZE:
  {
    assert(first_table == all_tables && first_table != 0);
    thd->enable_slow_log= opt_log_slow_admin_statements;
    res= (specialflag & (SPECIAL_SAFE_MODE | SPECIAL_NO_NEW_FUNC)) ?
      mysql_recreate_table(thd, first_table) :
      mysql_optimize_table(thd, first_table, &lex->check_opt);
    /* ! we write after unlocking the table */
    if (!res && !lex->no_write_to_binlog)
    {
      /*
        Presumably, OPTIMIZE and binlog writing doesn't require synchronization
      */
      write_bin_log(thd, true, thd->query, thd->query_length);
    }
    select_lex->table_list.first= (uchar*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_UPDATE:
    assert(first_table == all_tables && first_table != 0);
    if (update_precheck(thd, all_tables))
      break;
    assert(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);
    res= (up_result= mysql_update(thd, all_tables,
                                  select_lex->item_list,
                                  lex->value_list,
                                  select_lex->where,
                                  select_lex->order_list.elements,
                                  (ORDER *) select_lex->order_list.first,
                                  unit->select_limit_cnt,
                                  lex->duplicates, lex->ignore));
    /* mysql_update return 2 if we need to switch to multi-update */
    if (up_result != 2)
      break;
    /* Fall through */
  case SQLCOM_UPDATE_MULTI:
  {
    assert(first_table == all_tables && first_table != 0);
    /* if we switched from normal update, rights are checked */
    if (up_result != 2)
    {
      if ((res= multi_update_precheck(thd, all_tables)))
        break;
    }
    else
      res= 0;

    res= mysql_multi_update_prepare(thd);

    /* Check slave filtering rules */
    if (unlikely(thd->slave_thread))
    {
      if (all_tables_not_ok(thd, all_tables))
      {
        if (res!= 0)
        {
          res= 0;             /* don't care of prev failure  */
          thd->clear_error(); /* filters are of highest prior */
        }
        /* we warn the slave SQL thread */
        my_error(ER_SLAVE_IGNORED_TABLE, MYF(0));
        break;
      }
      if (res)
        break;
    }
    else
    {
      if (res)
        break;
      if (opt_readonly &&
	  some_non_temp_table_to_be_updated(thd, all_tables))
      {
	my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
	break;
      }
    }  /* unlikely */

    res= mysql_multi_update(thd, all_tables,
                            &select_lex->item_list,
                            &lex->value_list,
                            select_lex->where,
                            select_lex->options,
                            lex->duplicates, lex->ignore, unit, select_lex);
    break;
  }
  case SQLCOM_REPLACE:
  case SQLCOM_INSERT:
  {
    assert(first_table == all_tables && first_table != 0);
    if ((res= insert_precheck(thd, all_tables)))
      break;

    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      break;
    }

    res= mysql_insert(thd, all_tables, lex->field_list, lex->many_values,
		      lex->update_list, lex->value_list,
                      lex->duplicates, lex->ignore);

    break;
  }
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  {
    select_result *sel_result;
    assert(first_table == all_tables && first_table != 0);
    if ((res= insert_precheck(thd, all_tables)))
      break;

    /* Fix lock for first table */
    if (first_table->lock_type == TL_WRITE_DELAYED)
      first_table->lock_type= TL_WRITE;

    /* Don't unlock tables until command is written to binary log */
    select_lex->options|= SELECT_NO_UNLOCK;

    unit->set_limit(select_lex);

    if (! thd->locked_tables &&
        ! (need_start_waiting= ! wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      break;
    }

    if (!(res= open_and_lock_tables(thd, all_tables)))
    {
      /* Skip first table, which is the table we are inserting in */
      TABLE_LIST *second_table= first_table->next_local;
      select_lex->table_list.first= (uchar*) second_table;
      select_lex->context.table_list= 
        select_lex->context.first_name_resolution_table= second_table;
      res= mysql_insert_select_prepare(thd);
      if (!res && (sel_result= new select_insert(first_table,
                                                 first_table->table,
                                                 &lex->field_list,
                                                 &lex->update_list,
                                                 &lex->value_list,
                                                 lex->duplicates,
                                                 lex->ignore)))
      {
	res= handle_select(thd, lex, sel_result, OPTION_SETUP_TABLES_DONE);
        /*
          Invalidate the table in the query cache if something changed
          after unlocking when changes become visible.
          TODO: this is workaround. right way will be move invalidating in
          the unlock procedure.
        */
        if (first_table->lock_type ==  TL_WRITE_CONCURRENT_INSERT &&
            thd->lock)
        {
          /* INSERT ... SELECT should invalidate only the very first table */
          TABLE_LIST *save_table= first_table->next_local;
          first_table->next_local= 0;
          first_table->next_local= save_table;
        }
        delete sel_result;
      }
      /* revert changes for SP */
      select_lex->table_list.first= (uchar*) first_table;
    }

    break;
  }
  case SQLCOM_TRUNCATE:
    if (end_active_trans(thd))
    {
      res= -1;
      break;
    }
    assert(first_table == all_tables && first_table != 0);
    /*
      Don't allow this within a transaction because we want to use
      re-generate table
    */
    if (thd->locked_tables || thd->active_transaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }

    res= mysql_truncate(thd, first_table, 0);

    break;
  case SQLCOM_DELETE:
  {
    assert(first_table == all_tables && first_table != 0);
    assert(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);

    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      break;
    }

    res = mysql_delete(thd, all_tables, select_lex->where,
                       &select_lex->order_list,
                       unit->select_limit_cnt, select_lex->options,
                       false);
    break;
  }
  case SQLCOM_DELETE_MULTI:
  {
    assert(first_table == all_tables && first_table != 0);
    TABLE_LIST *aux_tables=
      (TABLE_LIST *)thd->lex->auxiliary_table_list.first;
    multi_delete *del_result;

    if (!thd->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(thd, 0, 1)))
    {
      res= 1;
      break;
    }

    if ((res= multi_delete_precheck(thd, all_tables)))
      break;

    /* condition will be true on SP re-excuting */
    if (select_lex->item_list.elements != 0)
      select_lex->item_list.empty();
    if (add_item_to_list(thd, new Item_null()))
      goto error;

    thd_proc_info(thd, "init");
    if ((res= open_and_lock_tables(thd, all_tables)))
      break;

    if ((res= mysql_multi_delete_prepare(thd)))
      goto error;

    if (!thd->is_fatal_error &&
        (del_result= new multi_delete(aux_tables, lex->table_count)))
    {
      res= mysql_select(thd, &select_lex->ref_pointer_array,
			select_lex->get_table_list(),
			select_lex->with_wild,
			select_lex->item_list,
			select_lex->where,
			0, (ORDER *)NULL, (ORDER *)NULL, (Item *)NULL,
			(ORDER *)NULL,
			select_lex->options | thd->options |
			SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK |
                        OPTION_SETUP_TABLES_DONE,
			del_result, unit, select_lex);
      res|= thd->is_error();
      if (res)
        del_result->abort();
      delete del_result;
    }
    else
      res= true;                                // Error
    break;
  }
  case SQLCOM_DROP_TABLE:
  {
    assert(first_table == all_tables && first_table != 0);
    if (!lex->drop_temporary)
    {
      if (end_active_trans(thd))
        goto error;
    }
    else
    {
      /*
	If this is a slave thread, we may sometimes execute some 
	DROP / * 40005 TEMPORARY * / TABLE
	that come from parts of binlogs (likely if we use RESET SLAVE or CHANGE
	MASTER TO), while the temporary table has already been dropped.
	To not generate such irrelevant "table does not exist errors",
	we silently add IF EXISTS if TEMPORARY was used.
      */
      if (thd->slave_thread)
        lex->drop_if_exists= 1;

      /* So that DROP TEMPORARY TABLE gets to binlog at commit/rollback */
      thd->options|= OPTION_KEEP_LOG;
    }
    /* DDL and binlog write order protected by LOCK_open */
    res= mysql_rm_table(thd, first_table, lex->drop_if_exists, lex->drop_temporary);
  }
  break;
  case SQLCOM_SHOW_PROCESSLIST:
    mysqld_list_processes(thd, NullS, lex->verbose);
    break;
  case SQLCOM_SHOW_ENGINE_LOGS:
    {
      res= ha_show_status(thd, lex->create_info.db_type, HA_ENGINE_LOGS);
      break;
    }
  case SQLCOM_CHANGE_DB:
  {
    LEX_STRING db_str= { (char *) select_lex->db, strlen(select_lex->db) };

    if (!mysql_change_db(thd, &db_str, false))
      my_ok(thd);

    break;
  }

  case SQLCOM_LOAD:
  {
    assert(first_table == all_tables && first_table != 0);
    if (lex->local_file)
    {
      if (!(thd->client_capabilities & CLIENT_LOCAL_FILES) ||
          !opt_local_infile)
      {
	my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND), MYF(0));
	goto error;
      }
    }

    res= mysql_load(thd, lex->exchange, first_table, lex->field_list,
                    lex->update_list, lex->value_list, lex->duplicates,
                    lex->ignore, (bool) lex->local_file);
    break;
  }

  case SQLCOM_SET_OPTION:
  {
    List<set_var_base> *lex_var_list= &lex->var_list;

    if (lex->autocommit && end_active_trans(thd))
      goto error;

    if (open_and_lock_tables(thd, all_tables))
      goto error;
    if (lex->one_shot_set && not_all_support_one_shot(lex_var_list))
    {
      my_error(ER_RESERVED_SYNTAX, MYF(0), "SET ONE_SHOT");
      goto error;
    }
    if (!(res= sql_set_variables(thd, lex_var_list)))
    {
      /*
        If the previous command was a SET ONE_SHOT, we don't want to forget
        about the ONE_SHOT property of that SET. So we use a |= instead of = .
      */
      thd->one_shot_set|= lex->one_shot_set;
      my_ok(thd);
    }
    else
    {
      /*
        We encountered some sort of error, but no message was sent.
        Send something semi-generic here since we don't know which
        assignment in the list caused the error.
      */
      if (!thd->is_error())
        my_error(ER_WRONG_ARGUMENTS,MYF(0),"SET");
      goto error;
    }

    break;
  }

  case SQLCOM_UNLOCK_TABLES:
    /*
      It is critical for mysqldump --single-transaction --master-data that
      UNLOCK TABLES does not implicitely commit a connection which has only
      done FLUSH TABLES WITH READ LOCK + BEGIN. If this assumption becomes
      false, mysqldump will not work.
    */
    unlock_locked_tables(thd);
    if (thd->options & OPTION_TABLE_LOCK)
    {
      end_active_trans(thd);
      thd->options&= ~(OPTION_TABLE_LOCK);
    }
    if (thd->global_read_lock)
      unlock_global_read_lock(thd);
    my_ok(thd);
    break;
  case SQLCOM_LOCK_TABLES:
    /*
      We try to take transactional locks if
      - only transactional locks are requested (lex->lock_transactional) and
      - no non-transactional locks exist (!thd->locked_tables).
    */
    if (lex->lock_transactional && !thd->locked_tables)
    {
      int rc;
      /*
        All requested locks are transactional and no non-transactional
        locks exist.
      */
      if ((rc= try_transactional_lock(thd, all_tables)) == -1)
        goto error;
      if (rc == 0)
      {
        my_ok(thd);
        break;
      }
      /*
        Non-transactional locking has been requested or
        non-transactional locks exist already or transactional locks are
        not supported by all storage engines. Take non-transactional
        locks.
      */
    }
    /*
      One or more requested locks are non-transactional and/or
      non-transactional locks exist or a storage engine does not support
      transactional locks. Check if at least one transactional lock is
      requested. If yes, warn about the conversion to non-transactional
      locks or abort in strict mode.
    */
    if (check_transactional_lock(thd, all_tables))
      goto error;
    unlock_locked_tables(thd);
    /* we must end the trasaction first, regardless of anything */
    if (end_active_trans(thd))
      goto error;
    thd->in_lock_tables=1;
    thd->options|= OPTION_TABLE_LOCK;

    if (!(res= simple_open_n_lock_tables(thd, all_tables)))
    {
      thd->locked_tables=thd->lock;
      thd->lock=0;
      (void) set_handler_table_locks(thd, all_tables, false);
      my_ok(thd);
    }
    else
    {
      /* 
        Need to end the current transaction, so the storage engine (InnoDB)
        can free its locks if LOCK TABLES locked some tables before finding
        that it can't lock a table in its list
      */
      ha_autocommit_or_rollback(thd, 1);
      end_active_trans(thd);
      thd->options&= ~(OPTION_TABLE_LOCK);
    }
    thd->in_lock_tables=0;
    break;
  case SQLCOM_CREATE_DB:
  {
    /*
      As mysql_create_db() may modify HA_CREATE_INFO structure passed to
      it, we need to use a copy of LEX::create_info to make execution
      prepared statement- safe.
    */
    HA_CREATE_INFO create_info(lex->create_info);
    if (end_active_trans(thd))
    {
      res= -1;
      break;
    }
    char *alias;
    if (!(alias=thd->strmake(lex->name.str, lex->name.length)) ||
        check_db_name(&lex->name))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->name.str);
      break;
    }
    /*
      If in a slave thread :
      CREATE DATABASE DB was certainly not preceded by USE DB.
      For that reason, db_ok() in sql/slave.cc did not check the
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
    if (thd->slave_thread && 
	(!rpl_filter->db_ok(lex->name.str) ||
	 !rpl_filter->db_ok_with_wild_table(lex->name.str)))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
    res= mysql_create_db(thd,(lower_case_table_names == 2 ? alias :
                              lex->name.str), &create_info, 0);
    break;
  }
  case SQLCOM_DROP_DB:
  {
    if (end_active_trans(thd))
    {
      res= -1;
      break;
    }
    if (check_db_name(&lex->name))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->name.str);
      break;
    }
    /*
      If in a slave thread :
      DROP DATABASE DB may not be preceded by USE DB.
      For that reason, maybe db_ok() in sql/slave.cc did not check the 
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
    if (thd->slave_thread && 
	(!rpl_filter->db_ok(lex->name.str) ||
	 !rpl_filter->db_ok_with_wild_table(lex->name.str)))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
    if (thd->locked_tables || thd->active_transaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }
    res= mysql_rm_db(thd, lex->name.str, lex->drop_if_exists, 0);
    break;
  }
  case SQLCOM_ALTER_DB_UPGRADE:
  {
    LEX_STRING *db= & lex->name;
    if (end_active_trans(thd))
    {
      res= 1;
      break;
    }
    if (thd->slave_thread && 
       (!rpl_filter->db_ok(db->str) ||
        !rpl_filter->db_ok_with_wild_table(db->str)))
    {
      res= 1;
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
    if (check_db_name(db))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), db->str);
      break;
    }
    if (thd->locked_tables || thd->active_transaction())
    {
      res= 1;
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }

    res= mysql_upgrade_db(thd, db);
    if (!res)
      my_ok(thd);
    break;
  }
  case SQLCOM_ALTER_DB:
  {
    LEX_STRING *db= &lex->name;
    HA_CREATE_INFO create_info(lex->create_info);
    if (check_db_name(db))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), db->str);
      break;
    }
    /*
      If in a slave thread :
      ALTER DATABASE DB may not be preceded by USE DB.
      For that reason, maybe db_ok() in sql/slave.cc did not check the
      do_db/ignore_db. And as this query involves no tables, tables_ok()
      above was not called. So we have to check rules again here.
    */
    if (thd->slave_thread &&
	(!rpl_filter->db_ok(db->str) ||
	 !rpl_filter->db_ok_with_wild_table(db->str)))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
    if (thd->locked_tables || thd->active_transaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
                 ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }
    res= mysql_alter_db(thd, db->str, &create_info);
    break;
  }
  case SQLCOM_SHOW_CREATE_DB:
  {
    if (check_db_name(&lex->name))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->name.str);
      break;
    }
    res= mysqld_show_create_db(thd, lex->name.str, &lex->create_info);
    break;
  }
  case SQLCOM_RESET:
    /*
      RESET commands are never written to the binary log, so we have to
      initialize this variable because RESET shares the same code as FLUSH
    */
    lex->no_write_to_binlog= 1;
  case SQLCOM_FLUSH:
  {
    bool write_to_binlog;

    /*
      reload_cache() will tell us if we are allowed to write to the
      binlog or not.
    */
    if (!reload_cache(thd, lex->type, first_table, &write_to_binlog))
    {
      /*
        We WANT to write and we CAN write.
        ! we write after unlocking the table.
      */
      /*
        Presumably, RESET and binlog writing doesn't require synchronization
      */
      if (!lex->no_write_to_binlog && write_to_binlog)
      {
        write_bin_log(thd, false, thd->query, thd->query_length);
      }
      my_ok(thd);
    } 
    
    break;
  }
  case SQLCOM_KILL:
  {
    Item *it= (Item *)lex->value_list.head();

    if (lex->table_or_sp_used())
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Usage of subqueries or stored "
               "function calls as part of this statement");
      break;
    }

    if ((!it->fixed && it->fix_fields(lex->thd, &it)) || it->check_cols(1))
    {
      my_message(ER_SET_CONSTANTS_ONLY, ER(ER_SET_CONSTANTS_ONLY),
		 MYF(0));
      goto error;
    }
    sql_kill(thd, (ulong)it->val_int(), lex->type & ONLY_KILL_QUERY);
    break;
  }
  case SQLCOM_BEGIN:
    if (thd->transaction.xid_state.xa_state != XA_NOTR)
    {
      my_error(ER_XAER_RMFAIL, MYF(0),
               xa_state_names[thd->transaction.xid_state.xa_state]);
      break;
    }
    /*
      Breakpoints for backup testing.
    */
    if (begin_trans(thd))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_COMMIT:
    if (end_trans(thd, lex->tx_release ? COMMIT_RELEASE :
                              lex->tx_chain ? COMMIT_AND_CHAIN : COMMIT))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_ROLLBACK:
    if (end_trans(thd, lex->tx_release ? ROLLBACK_RELEASE :
                              lex->tx_chain ? ROLLBACK_AND_CHAIN : ROLLBACK))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_RELEASE_SAVEPOINT:
  {
    SAVEPOINT *sv;
    for (sv=thd->transaction.savepoints; sv; sv=sv->prev)
    {
      if (my_strnncoll(system_charset_info,
                       (uchar *)lex->ident.str, lex->ident.length,
                       (uchar *)sv->name, sv->length) == 0)
        break;
    }
    if (sv)
    {
      if (ha_release_savepoint(thd, sv))
        res= true; // cannot happen
      else
        my_ok(thd);
      thd->transaction.savepoints=sv->prev;
    }
    else
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SAVEPOINT", lex->ident.str);
    break;
  }
  case SQLCOM_ROLLBACK_TO_SAVEPOINT:
  {
    SAVEPOINT *sv;
    for (sv=thd->transaction.savepoints; sv; sv=sv->prev)
    {
      if (my_strnncoll(system_charset_info,
                       (uchar *)lex->ident.str, lex->ident.length,
                       (uchar *)sv->name, sv->length) == 0)
        break;
    }
    if (sv)
    {
      if (ha_rollback_to_savepoint(thd, sv))
        res= true; // cannot happen
      else
      {
        if (((thd->options & OPTION_KEEP_LOG) || 
             thd->transaction.all.modified_non_trans_table) &&
            !thd->slave_thread)
          push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                       ER_WARNING_NOT_COMPLETE_ROLLBACK,
                       ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
        my_ok(thd);
      }
      thd->transaction.savepoints=sv;
    }
    else
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SAVEPOINT", lex->ident.str);
    break;
  }
  case SQLCOM_SAVEPOINT:
    if (!(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN) ||
          thd->in_sub_stmt) || !opt_using_transactions)
      my_ok(thd);
    else
    {
      SAVEPOINT **sv, *newsv;
      for (sv=&thd->transaction.savepoints; *sv; sv=&(*sv)->prev)
      {
        if (my_strnncoll(system_charset_info,
                         (uchar *)lex->ident.str, lex->ident.length,
                         (uchar *)(*sv)->name, (*sv)->length) == 0)
          break;
      }
      if (*sv) /* old savepoint of the same name exists */
      {
        newsv=*sv;
        ha_release_savepoint(thd, *sv); // it cannot fail
        *sv=(*sv)->prev;
      }
      else if ((newsv=(SAVEPOINT *) alloc_root(&thd->transaction.mem_root,
                                               savepoint_alloc_size)) == 0)
      {
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
        break;
      }
      newsv->name=strmake_root(&thd->transaction.mem_root,
                               lex->ident.str, lex->ident.length);
      newsv->length=lex->ident.length;
      /*
        if we'll get an error here, don't add new savepoint to the list.
        we'll lose a little bit of memory in transaction mem_root, but it'll
        be free'd when transaction ends anyway
      */
      if (ha_savepoint(thd, newsv))
        res= true;
      else
      {
        newsv->prev=thd->transaction.savepoints;
        thd->transaction.savepoints=newsv;
        my_ok(thd);
      }
    }
    break;
  case SQLCOM_BINLOG_BASE64_EVENT:
  {
    mysql_client_binlog_statement(thd);
    break;
  }
  default:
    assert(0);                             /* Impossible */
    my_ok(thd);
    break;
  }
  thd_proc_info(thd, "query end");

  /*
    Binlog-related cleanup:
    Reset system variables temporarily modified by SET ONE SHOT.

    Exception: If this is a SET, do nothing. This is to allow
    mysqlbinlog to print many SET commands (in this case we want the
    charset temp setting to live until the real query). This is also
    needed so that SET CHARACTER_SET_CLIENT... does not cancel itself
    immediately.
  */
  if (thd->one_shot_set && lex->sql_command != SQLCOM_SET_OPTION)
    reset_one_shot_variables(thd);

  /*
    The return value for ROW_COUNT() is "implementation dependent" if the
    statement is not DELETE, INSERT or UPDATE, but -1 is what JDBC and ODBC
    wants. We also keep the last value in case of SQLCOM_CALL or
    SQLCOM_EXECUTE.
  */
  if (!(sql_command_flags[lex->sql_command] & CF_HAS_ROW_COUNT))
    thd->row_count_func= -1;

  goto finish;

error:
  res= true;

finish:
  if (need_start_waiting)
  {
    /*
      Release the protection against the global read lock and wake
      everyone, who might want to set a global read lock.
    */
    start_waiting_global_read_lock(thd);
  }
  return(res || thd->is_error());
}


static bool execute_sqlcom_select(THD *thd, TABLE_LIST *all_tables)
{
  LEX	*lex= thd->lex;
  select_result *result=lex->result;
  bool res;
  /* assign global limit variable if limit is not given */
  {
    SELECT_LEX *param= lex->unit.global_parameters;
    if (!param->explicit_limit)
      param->select_limit=
        new Item_int((uint64_t) thd->variables.select_limit);
  }
  if (!(res= open_and_lock_tables(thd, all_tables)))
  {
    if (lex->describe)
    {
      /*
        We always use select_send for EXPLAIN, even if it's an EXPLAIN
        for SELECT ... INTO OUTFILE: a user application should be able
        to prepend EXPLAIN to any query and receive output for it,
        even if the query itself redirects the output.
      */
      if (!(result= new select_send()))
        return 1;                               /* purecov: inspected */
      thd->send_explain_fields(result);
      res= mysql_explain_union(thd, &thd->lex->unit, result);
      if (lex->describe & DESCRIBE_EXTENDED)
      {
        char buff[1024];
        String str(buff,(uint32_t) sizeof(buff), system_charset_info);
        str.length(0);
        thd->lex->unit.print(&str, QT_ORDINARY);
        str.append('\0');
        push_warning(thd, MYSQL_ERROR::WARN_LEVEL_NOTE,
                     ER_YES, str.ptr());
      }
      if (res)
        result->abort();
      else
        result->send_eof();
      delete result;
    }
    else
    {
      if (!result && !(result= new select_send()))
        return 1;                               /* purecov: inspected */
      res= handle_select(thd, lex, result, 0);
      if (result != lex->result)
        delete result;
    }
  }
  return res;
}

/****************************************************************************
	Check stack size; Send error if there isn't enough stack to continue
****************************************************************************/
#if STACK_DIRECTION < 0
#define used_stack(A,B) (long) (A - B)
#else
#define used_stack(A,B) (long) (B - A)
#endif

/**
  @note
  Note: The 'buf' parameter is necessary, even if it is unused here.
  - fix_fields functions has a "dummy" buffer large enough for the
    corresponding exec. (Thus we only have to check in fix_fields.)
  - Passing to check_stack_overrun() prevents the compiler from removing it.
*/
bool check_stack_overrun(THD *thd, long margin,
			 uchar *buf __attribute__((unused)))
{
  long stack_used;
  assert(thd == current_thd);
  if ((stack_used=used_stack(thd->thread_stack,(char*) &stack_used)) >=
      (long) (my_thread_stack_size - margin))
  {
    sprintf(errbuff[0],ER(ER_STACK_OVERRUN_NEED_MORE),
            stack_used,my_thread_stack_size,margin);
    my_message(ER_STACK_OVERRUN_NEED_MORE,errbuff[0],MYF(ME_FATALERROR));
    return 1;
  }
  return 0;
}

#define MY_YACC_INIT 1000			// Start with big alloc
#define MY_YACC_MAX  32000			// Because of 'short'

bool my_yyoverflow(short **yyss, YYSTYPE **yyvs, ulong *yystacksize)
{
  LEX	*lex= current_thd->lex;
  ulong old_info=0;
  if ((uint) *yystacksize >= MY_YACC_MAX)
    return 1;
  if (!lex->yacc_yyvs)
    old_info= *yystacksize;
  *yystacksize= set_zone((*yystacksize)*2,MY_YACC_INIT,MY_YACC_MAX);
  if (!(lex->yacc_yyvs= (uchar*)
	my_realloc(lex->yacc_yyvs,
		   *yystacksize*sizeof(**yyvs),
		   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))) ||
      !(lex->yacc_yyss= (uchar*)
	my_realloc(lex->yacc_yyss,
		   *yystacksize*sizeof(**yyss),
		   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))))
    return 1;
  if (old_info)
  {						// Copy old info from stack
    memcpy(lex->yacc_yyss, (uchar*) *yyss, old_info*sizeof(**yyss));
    memcpy(lex->yacc_yyvs, (uchar*) *yyvs, old_info*sizeof(**yyvs));
  }
  *yyss=(short*) lex->yacc_yyss;
  *yyvs=(YYSTYPE*) lex->yacc_yyvs;
  return 0;
}


/**
 Reset THD part responsible for command processing state.

   This needs to be called before execution of every statement
   (prepared or conventional).
   It is not called by substatements of routines.

  @todo
   Make it a method of THD and align its name with the rest of
   reset/end/start/init methods.
  @todo
   Call it after we use THD for queries, not before.
*/

void mysql_reset_thd_for_next_command(THD *thd)
{
  assert(! thd->in_sub_stmt);
  thd->free_list= 0;
  thd->select_number= 1;
  /*
    Those two lines below are theoretically unneeded as
    THD::cleanup_after_query() should take care of this already.
  */
  thd->auto_inc_intervals_in_cur_stmt_for_binlog.empty();
  thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt= 0;

  thd->query_start_used= 0;
  thd->is_fatal_error= thd->time_zone_used= 0;
  thd->server_status&= ~ (SERVER_MORE_RESULTS_EXISTS | 
                          SERVER_QUERY_NO_INDEX_USED |
                          SERVER_QUERY_NO_GOOD_INDEX_USED);
  /*
    If in autocommit mode and not in a transaction, reset
    OPTION_STATUS_NO_TRANS_UPDATE | OPTION_KEEP_LOG to not get warnings
    in ha_rollback_trans() about some tables couldn't be rolled back.
  */
  if (!(thd->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
  {
    thd->options&= ~OPTION_KEEP_LOG;
    thd->transaction.all.modified_non_trans_table= false;
  }
  assert(thd->security_ctx== &thd->main_security_ctx);
  thd->thread_specific_used= false;

  if (opt_bin_log)
  {
    reset_dynamic(&thd->user_var_events);
    thd->user_var_events_alloc= thd->mem_root;
  }
  thd->clear_error();
  thd->main_da.reset_diagnostics_area();
  thd->total_warn_count=0;			// Warnings for this query
  thd->rand_used= 0;
  thd->sent_row_count= thd->examined_row_count= 0;

  /*
    Because we come here only for start of top-statements, binlog format is
    constant inside a complex statement (using stored functions) etc.
  */
  thd->reset_current_stmt_binlog_row_based();

  return;
}


void
mysql_init_select(LEX *lex)
{
  SELECT_LEX *select_lex= lex->current_select;
  select_lex->init_select();
  lex->wild= 0;
  if (select_lex == &lex->select_lex)
  {
    assert(lex->result == 0);
    lex->exchange= 0;
  }
}


bool
mysql_new_select(LEX *lex, bool move_down)
{
  SELECT_LEX *select_lex;
  THD *thd= lex->thd;

  if (!(select_lex= new (thd->mem_root) SELECT_LEX()))
    return(1);
  select_lex->select_number= ++thd->select_number;
  select_lex->parent_lex= lex; /* Used in init_query. */
  select_lex->init_query();
  select_lex->init_select();
  lex->nest_level++;
  if (lex->nest_level > (int) MAX_SELECT_NESTING)
  {
    my_error(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT,MYF(0),MAX_SELECT_NESTING);
    return(1);
  }
  select_lex->nest_level= lex->nest_level;
  if (move_down)
  {
    SELECT_LEX_UNIT *unit;
    lex->subqueries= true;
    /* first select_lex of subselect or derived table */
    if (!(unit= new (thd->mem_root) SELECT_LEX_UNIT()))
      return(1);

    unit->init_query();
    unit->init_select();
    unit->thd= thd;
    unit->include_down(lex->current_select);
    unit->link_next= 0;
    unit->link_prev= 0;
    unit->return_to= lex->current_select;
    select_lex->include_down(unit);
    /*
      By default we assume that it is usual subselect and we have outer name
      resolution context, if no we will assign it to 0 later
    */
    select_lex->context.outer_context= &select_lex->outer_select()->context;
  }
  else
  {
    if (lex->current_select->order_list.first && !lex->current_select->braces)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "UNION", "ORDER BY");
      return(1);
    }
    select_lex->include_neighbour(lex->current_select);
    SELECT_LEX_UNIT *unit= select_lex->master_unit();                              
    if (!unit->fake_select_lex && unit->add_fake_select_lex(lex->thd))
      return(1);
    select_lex->context.outer_context= 
                unit->first_select()->context.outer_context;
  }

  select_lex->master_unit()->global_parameters= select_lex;
  select_lex->include_global((st_select_lex_node**)&lex->all_selects_list);
  lex->current_select= select_lex;
  /*
    in subquery is SELECT query and we allow resolution of names in SELECT
    list
  */
  select_lex->context.resolve_in_select_list= true;
  return(0);
}

/**
  Create a select to return the same output as 'SELECT @@var_name'.

  Used for SHOW COUNT(*) [ WARNINGS | ERROR].

  This will crash with a core dump if the variable doesn't exists.

  @param var_name		Variable name
*/

void create_select_for_variable(const char *var_name)
{
  THD *thd;
  LEX *lex;
  LEX_STRING tmp, null_lex_string;
  Item *var;
  char buff[MAX_SYS_VAR_LENGTH*2+4+8], *end;

  thd= current_thd;
  lex= thd->lex;
  mysql_init_select(lex);
  lex->sql_command= SQLCOM_SELECT;
  tmp.str= (char*) var_name;
  tmp.length=strlen(var_name);
  bzero((char*) &null_lex_string.str, sizeof(null_lex_string));
  /*
    We set the name of Item to @@session.var_name because that then is used
    as the column name in the output.
  */
  if ((var= get_system_var(thd, OPT_SESSION, tmp, null_lex_string)))
  {
    end= strxmov(buff, "@@session.", var_name, NullS);
    var->set_name(buff, end-buff, system_charset_info);
    add_item_to_list(thd, var);
  }
  return;
}


void mysql_init_multi_delete(LEX *lex)
{
  lex->sql_command=  SQLCOM_DELETE_MULTI;
  mysql_init_select(lex);
  lex->select_lex.select_limit= 0;
  lex->unit.select_limit_cnt= HA_POS_ERROR;
  lex->select_lex.table_list.save_and_clear(&lex->auxiliary_table_list);
  lex->lock_option= using_update_log ? TL_READ_NO_INSERT : TL_READ;
  lex->query_tables= 0;
  lex->query_tables_last= &lex->query_tables;
}


/*
  When you modify mysql_parse(), you may need to mofify
  mysql_test_parse_for_slave() in this same file.
*/

/**
  Parse a query.

  @param       thd     Current thread
  @param       inBuf   Begining of the query text
  @param       length  Length of the query text
  @param[out]  found_semicolon For multi queries, position of the character of
                               the next query in the query text.
*/

void mysql_parse(THD *thd, const char *inBuf, uint length,
                 const char ** found_semicolon)
{
  /*
    Warning.
    The purpose of query_cache_send_result_to_client() is to lookup the
    query in the query cache first, to avoid parsing and executing it.
    So, the natural implementation would be to:
    - first, call query_cache_send_result_to_client,
    - second, if caching failed, initialise the lexical and syntactic parser.
    The problem is that the query cache depends on a clean initialization
    of (among others) lex->safe_to_cache_query and thd->server_status,
    which are reset respectively in
    - lex_start()
    - mysql_reset_thd_for_next_command()
    So, initializing the lexical analyser *before* using the query cache
    is required for the cache to work properly.
    FIXME: cleanup the dependencies in the code to simplify this.
  */
  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);

  {
    LEX *lex= thd->lex;

    Lex_input_stream lip(thd, inBuf, length);

    bool err= parse_sql(thd, &lip, NULL);
    *found_semicolon= lip.found_semicolon;

    if (!err)
    {
      {
	if (! thd->is_error())
	{
          /*
            Binlog logs a string starting from thd->query and having length
            thd->query_length; so we set thd->query_length correctly (to not
            log several statements in one event, when we executed only first).
            We set it to not see the ';' (otherwise it would get into binlog
            and Query_log_event::print() would give ';;' output).
            This also helps display only the current query in SHOW
            PROCESSLIST.
            Note that we don't need LOCK_thread_count to modify query_length.
          */
          if (*found_semicolon &&
              (thd->query_length= (ulong)(*found_semicolon - thd->query)))
            thd->query_length--;
          /* Actually execute the query */
          mysql_execute_command(thd);
	}
      }
    }
    else
    {
      assert(thd->is_error());
    }
    lex->unit.cleanup();
    thd_proc_info(thd, "freeing items");
    thd->end_statement();
    thd->cleanup_after_query();
    assert(thd->change_list.is_empty());
  }

  return;
}


/*
  Usable by the replication SQL thread only: just parse a query to know if it
  can be ignored because of replicate-*-table rules.

  @retval
    0	cannot be ignored
  @retval
    1	can be ignored
*/

bool mysql_test_parse_for_slave(THD *thd, char *inBuf, uint length)
{
  LEX *lex= thd->lex;
  bool error= 0;

  Lex_input_stream lip(thd, inBuf, length);
  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);

  if (!parse_sql(thd, &lip, NULL) &&
      all_tables_not_ok(thd,(TABLE_LIST*) lex->select_lex.table_list.first))
    error= 1;                  /* Ignore question */
  thd->end_statement();
  thd->cleanup_after_query();
  return(error);
}



/**
  Store field definition for create.

  @return
    Return 0 if ok
*/

bool add_field_to_list(THD *thd, LEX_STRING *field_name, enum_field_types type,
		       char *length, char *decimals,
		       uint type_modifier,
                       enum column_format_type column_format,
		       Item *default_value, Item *on_update_value,
                       LEX_STRING *comment,
		       char *change,
                       List<String> *interval_list, CHARSET_INFO *cs)
{
  register Create_field *new_field;
  LEX  *lex= thd->lex;

  if (check_identifier_name(field_name, ER_TOO_LONG_IDENT))
    return(1);				/* purecov: inspected */

  if (type_modifier & PRI_KEY_FLAG)
  {
    Key *key;
    lex->col_list.push_back(new Key_part_spec(*field_name, 0));
    key= new Key(Key::PRIMARY, null_lex_str,
                      &default_key_create_info,
                      0, lex->col_list);
    lex->alter_info.key_list.push_back(key);
    lex->col_list.empty();
  }
  if (type_modifier & (UNIQUE_FLAG | UNIQUE_KEY_FLAG))
  {
    Key *key;
    lex->col_list.push_back(new Key_part_spec(*field_name, 0));
    key= new Key(Key::UNIQUE, null_lex_str,
                 &default_key_create_info, 0,
                 lex->col_list);
    lex->alter_info.key_list.push_back(key);
    lex->col_list.empty();
  }

  if (default_value)
  {
    /* 
      Default value should be literal => basic constants =>
      no need fix_fields()
      
      We allow only one function as part of default value - 
      NOW() as default for TIMESTAMP type.
    */
    if (default_value->type() == Item::FUNC_ITEM && 
        !(((Item_func*)default_value)->functype() == Item_func::NOW_FUNC &&
         type == MYSQL_TYPE_TIMESTAMP))
    {
      my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
      return(1);
    }
    else if (default_value->type() == Item::NULL_ITEM)
    {
      default_value= 0;
      if ((type_modifier & (NOT_NULL_FLAG | AUTO_INCREMENT_FLAG)) ==
	  NOT_NULL_FLAG)
      {
	my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
	return(1);
      }
    }
    else if (type_modifier & AUTO_INCREMENT_FLAG)
    {
      my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
      return(1);
    }
  }

  if (on_update_value && type != MYSQL_TYPE_TIMESTAMP)
  {
    my_error(ER_INVALID_ON_UPDATE, MYF(0), field_name->str);
    return(1);
  }

  if (!(new_field= new Create_field()) ||
      new_field->init(thd, field_name->str, type, length, decimals, type_modifier,
                      default_value, on_update_value, comment, change,
                      interval_list, cs, 0, column_format))
    return(1);

  lex->alter_info.create_list.push_back(new_field);
  lex->last_field=new_field;
  return(0);
}


/** Store position for column in ALTER TABLE .. ADD column. */

void store_position_for_column(const char *name)
{
  current_thd->lex->last_field->after=my_const_cast(char*) (name);
}

bool
add_proc_to_list(THD* thd, Item *item)
{
  ORDER *order;
  Item	**item_ptr;

  if (!(order = (ORDER *) thd->alloc(sizeof(ORDER)+sizeof(Item*))))
    return 1;
  item_ptr = (Item**) (order+1);
  *item_ptr= item;
  order->item=item_ptr;
  order->free_me=0;
  thd->lex->proc_list.link_in_list((uchar*) order,(uchar**) &order->next);
  return 0;
}


/**
  save order by and tables in own lists.
*/

bool add_to_list(THD *thd, SQL_LIST &list,Item *item,bool asc)
{
  ORDER *order;
  if (!(order = (ORDER *) thd->alloc(sizeof(ORDER))))
    return(1);
  order->item_ptr= item;
  order->item= &order->item_ptr;
  order->asc = asc;
  order->free_me=0;
  order->used=0;
  order->counter_used= 0;
  list.link_in_list((uchar*) order,(uchar**) &order->next);
  return(0);
}


/**
  Add a table to list of used tables.

  @param table		Table to add
  @param alias		alias for table (or null if no alias)
  @param table_options	A set of the following bits:
                         - TL_OPTION_UPDATING : Table will be updated
                         - TL_OPTION_FORCE_INDEX : Force usage of index
                         - TL_OPTION_ALIAS : an alias in multi table DELETE
  @param lock_type	How table should be locked
  @param use_index	List of indexed used in USE INDEX
  @param ignore_index	List of indexed used in IGNORE INDEX

  @retval
      0		Error
  @retval
    \#	Pointer to TABLE_LIST element added to the total table list
*/

TABLE_LIST *st_select_lex::add_table_to_list(THD *thd,
					     Table_ident *table,
					     LEX_STRING *alias,
					     uint32_t table_options,
					     thr_lock_type lock_type,
					     List<Index_hint> *index_hints_arg,
                                             LEX_STRING *option)
{
  register TABLE_LIST *ptr;
  TABLE_LIST *previous_table_ref; /* The table preceding the current one. */
  char *alias_str;
  LEX *lex= thd->lex;

  if (!table)
    return(0);				// End of memory
  alias_str= alias ? alias->str : table->table.str;
  if (!test(table_options & TL_OPTION_ALIAS) && 
      check_table_name(table->table.str, table->table.length))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), table->table.str);
    return(0);
  }

  if (table->is_derived_table() == false && table->db.str &&
      check_db_name(&table->db))
  {
    my_error(ER_WRONG_DB_NAME, MYF(0), table->db.str);
    return(0);
  }

  if (!alias)					/* Alias is case sensitive */
  {
    if (table->sel)
    {
      my_message(ER_DERIVED_MUST_HAVE_ALIAS,
                 ER(ER_DERIVED_MUST_HAVE_ALIAS), MYF(0));
      return(0);
    }
    if (!(alias_str= (char*) thd->memdup(alias_str,table->table.length+1)))
      return(0);
  }
  if (!(ptr = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
    return(0);				/* purecov: inspected */
  if (table->db.str)
  {
    ptr->is_fqtn= true;
    ptr->db= table->db.str;
    ptr->db_length= table->db.length;
  }
  else if (lex->copy_db_to(&ptr->db, &ptr->db_length))
    return(0);
  else
    ptr->is_fqtn= false;

  ptr->alias= alias_str;
  ptr->is_alias= alias ? true : false;
  if (lower_case_table_names && table->table.length)
    table->table.length= my_casedn_str(files_charset_info, table->table.str);
  ptr->table_name=table->table.str;
  ptr->table_name_length=table->table.length;
  ptr->lock_type=   lock_type;
  ptr->lock_timeout= -1;      /* default timeout */
  ptr->lock_transactional= 1; /* allow transactional locks */
  ptr->updating=    test(table_options & TL_OPTION_UPDATING);
  ptr->force_index= test(table_options & TL_OPTION_FORCE_INDEX);
  ptr->ignore_leaves= test(table_options & TL_OPTION_IGNORE_LEAVES);
  ptr->derived=	    table->sel;
  if (!ptr->derived && !my_strcasecmp(system_charset_info, ptr->db,
                                      INFORMATION_SCHEMA_NAME.str))
  {
    ST_SCHEMA_TABLE *schema_table= find_schema_table(thd, ptr->table_name);
    if (!schema_table ||
        (schema_table->hidden && 
         ((sql_command_flags[lex->sql_command] & CF_STATUS_COMMAND) == 0 || 
          /*
            this check is used for show columns|keys from I_S hidden table
          */
          lex->sql_command == SQLCOM_SHOW_FIELDS ||
          lex->sql_command == SQLCOM_SHOW_KEYS)))
    {
      my_error(ER_UNKNOWN_TABLE, MYF(0),
               ptr->table_name, INFORMATION_SCHEMA_NAME.str);
      return(0);
    }
    ptr->schema_table_name= ptr->table_name;
    ptr->schema_table= schema_table;
  }
  ptr->select_lex=  lex->current_select;
  ptr->cacheable_table= 1;
  ptr->index_hints= index_hints_arg;
  ptr->option= option ? option->str : 0;
  /* check that used name is unique */
  if (lock_type != TL_IGNORE)
  {
    TABLE_LIST *first_table= (TABLE_LIST*) table_list.first;
    for (TABLE_LIST *tables= first_table ;
	 tables ;
	 tables=tables->next_local)
    {
      if (!my_strcasecmp(table_alias_charset, alias_str, tables->alias) &&
	  !strcmp(ptr->db, tables->db))
      {
	my_error(ER_NONUNIQ_TABLE, MYF(0), alias_str); /* purecov: tested */
	return(0);				/* purecov: tested */
      }
    }
  }
  /* Store the table reference preceding the current one. */
  if (table_list.elements > 0)
  {
    /*
      table_list.next points to the last inserted TABLE_LIST->next_local'
      element
      We don't use the offsetof() macro here to avoid warnings from gcc
    */
    previous_table_ref= (TABLE_LIST*) ((char*) table_list.next -
                                       ((char*) &(ptr->next_local) -
                                        (char*) ptr));
    /*
      Set next_name_resolution_table of the previous table reference to point
      to the current table reference. In effect the list
      TABLE_LIST::next_name_resolution_table coincides with
      TABLE_LIST::next_local. Later this may be changed in
      store_top_level_join_columns() for NATURAL/USING joins.
    */
    previous_table_ref->next_name_resolution_table= ptr;
  }

  /*
    Link the current table reference in a local list (list for current select).
    Notice that as a side effect here we set the next_local field of the
    previous table reference to 'ptr'. Here we also add one element to the
    list 'table_list'.
  */
  table_list.link_in_list((uchar*) ptr, (uchar**) &ptr->next_local);
  ptr->next_name_resolution_table= NULL;
  /* Link table in global list (all used tables) */
  lex->add_to_query_tables(ptr);
  return(ptr);
}


/**
  Initialize a new table list for a nested join.

    The function initializes a structure of the TABLE_LIST type
    for a nested join. It sets up its nested join list as empty.
    The created structure is added to the front of the current
    join list in the st_select_lex object. Then the function
    changes the current nest level for joins to refer to the newly
    created empty list after having saved the info on the old level
    in the initialized structure.

  @param thd         current thread

  @retval
    0   if success
  @retval
    1   otherwise
*/

bool st_select_lex::init_nested_join(THD *thd)
{
  TABLE_LIST *ptr;
  NESTED_JOIN *nested_join;

  if (!(ptr= (TABLE_LIST*) thd->calloc(ALIGN_SIZE(sizeof(TABLE_LIST))+
                                       sizeof(NESTED_JOIN))))
    return(1);
  nested_join= ptr->nested_join=
    ((NESTED_JOIN*) ((uchar*) ptr + ALIGN_SIZE(sizeof(TABLE_LIST))));

  join_list->push_front(ptr);
  ptr->embedding= embedding;
  ptr->join_list= join_list;
  ptr->alias= (char*) "(nested_join)";
  embedding= ptr;
  join_list= &nested_join->join_list;
  join_list->empty();
  return(0);
}


/**
  End a nested join table list.

    The function returns to the previous join nest level.
    If the current level contains only one member, the function
    moves it one level up, eliminating the nest.

  @param thd         current thread

  @return
    - Pointer to TABLE_LIST element added to the total table list, if success
    - 0, otherwise
*/

TABLE_LIST *st_select_lex::end_nested_join(THD *thd __attribute__((__unused__)))
{
  TABLE_LIST *ptr;
  NESTED_JOIN *nested_join;

  assert(embedding);
  ptr= embedding;
  join_list= ptr->join_list;
  embedding= ptr->embedding;
  nested_join= ptr->nested_join;
  if (nested_join->join_list.elements == 1)
  {
    TABLE_LIST *embedded= nested_join->join_list.head();
    join_list->pop();
    embedded->join_list= join_list;
    embedded->embedding= embedding;
    join_list->push_front(embedded);
    ptr= embedded;
  }
  else if (nested_join->join_list.elements == 0)
  {
    join_list->pop();
    ptr= 0;                                     // return value
  }
  return(ptr);
}


/**
  Nest last join operation.

    The function nest last join operation as if it was enclosed in braces.

  @param thd         current thread

  @retval
    0  Error
  @retval
    \#  Pointer to TABLE_LIST element created for the new nested join
*/

TABLE_LIST *st_select_lex::nest_last_join(THD *thd)
{
  TABLE_LIST *ptr;
  NESTED_JOIN *nested_join;
  List<TABLE_LIST> *embedded_list;

  if (!(ptr= (TABLE_LIST*) thd->calloc(ALIGN_SIZE(sizeof(TABLE_LIST))+
                                       sizeof(NESTED_JOIN))))
    return(0);
  nested_join= ptr->nested_join=
    ((NESTED_JOIN*) ((uchar*) ptr + ALIGN_SIZE(sizeof(TABLE_LIST))));

  ptr->embedding= embedding;
  ptr->join_list= join_list;
  ptr->alias= (char*) "(nest_last_join)";
  embedded_list= &nested_join->join_list;
  embedded_list->empty();

  for (uint i=0; i < 2; i++)
  {
    TABLE_LIST *table= join_list->pop();
    table->join_list= embedded_list;
    table->embedding= ptr;
    embedded_list->push_back(table);
    if (table->natural_join)
    {
      ptr->is_natural_join= true;
      /*
        If this is a JOIN ... USING, move the list of joined fields to the
        table reference that describes the join.
      */
      if (prev_join_using)
        ptr->join_using_fields= prev_join_using;
    }
  }
  join_list->push_front(ptr);
  nested_join->used_tables= nested_join->not_null_tables= (table_map) 0;
  return(ptr);
}


/**
  Add a table to the current join list.

    The function puts a table in front of the current join list
    of st_select_lex object.
    Thus, joined tables are put into this list in the reverse order
    (the most outer join operation follows first).

  @param table       the table to add

  @return
    None
*/

void st_select_lex::add_joined_table(TABLE_LIST *table)
{
  join_list->push_front(table);
  table->join_list= join_list;
  table->embedding= embedding;
  return;
}


/**
  Convert a right join into equivalent left join.

    The function takes the current join list t[0],t[1] ... and
    effectively converts it into the list t[1],t[0] ...
    Although the outer_join flag for the new nested table contains
    JOIN_TYPE_RIGHT, it will be handled as the inner table of a left join
    operation.

  EXAMPLES
  @verbatim
    SELECT * FROM t1 RIGHT JOIN t2 ON on_expr =>
      SELECT * FROM t2 LEFT JOIN t1 ON on_expr

    SELECT * FROM t1,t2 RIGHT JOIN t3 ON on_expr =>
      SELECT * FROM t1,t3 LEFT JOIN t2 ON on_expr

    SELECT * FROM t1,t2 RIGHT JOIN (t3,t4) ON on_expr =>
      SELECT * FROM t1,(t3,t4) LEFT JOIN t2 ON on_expr

    SELECT * FROM t1 LEFT JOIN t2 ON on_expr1 RIGHT JOIN t3  ON on_expr2 =>
      SELECT * FROM t3 LEFT JOIN (t1 LEFT JOIN t2 ON on_expr2) ON on_expr1
   @endverbatim

  @param thd         current thread

  @return
    - Pointer to the table representing the inner table, if success
    - 0, otherwise
*/

TABLE_LIST *st_select_lex::convert_right_join()
{
  TABLE_LIST *tab2= join_list->pop();
  TABLE_LIST *tab1= join_list->pop();

  join_list->push_front(tab2);
  join_list->push_front(tab1);
  tab1->outer_join|= JOIN_TYPE_RIGHT;

  return(tab1);
}

/**
  Set lock for all tables in current select level.

  @param lock_type			Lock to set for tables

  @note
    If lock is a write lock, then tables->updating is set 1
    This is to get tables_ok to know that the table is updated by the
    query
*/

void st_select_lex::set_lock_for_tables(thr_lock_type lock_type)
{
  bool for_update= lock_type >= TL_READ_NO_INSERT;

  for (TABLE_LIST *tables= (TABLE_LIST*) table_list.first;
       tables;
       tables= tables->next_local)
  {
    tables->lock_type= lock_type;
    tables->updating=  for_update;
  }
  return;
}


/**
  Create a fake SELECT_LEX for a unit.

    The method create a fake SELECT_LEX object for a unit.
    This object is created for any union construct containing a union
    operation and also for any single select union construct of the form
    @verbatim
    (SELECT ... ORDER BY order_list [LIMIT n]) ORDER BY ... 
    @endvarbatim
    or of the form
    @varbatim
    (SELECT ... ORDER BY LIMIT n) ORDER BY ...
    @endvarbatim
  
  @param thd_arg		   thread handle

  @note
    The object is used to retrieve rows from the temporary table
    where the result on the union is obtained.

  @retval
    1     on failure to create the object
  @retval
    0     on success
*/

bool st_select_lex_unit::add_fake_select_lex(THD *thd_arg)
{
  SELECT_LEX *first_sl= first_select();
  assert(!fake_select_lex);

  if (!(fake_select_lex= new (thd_arg->mem_root) SELECT_LEX()))
      return(1);
  fake_select_lex->include_standalone(this, 
                                      (SELECT_LEX_NODE**)&fake_select_lex);
  fake_select_lex->select_number= INT_MAX;
  fake_select_lex->parent_lex= thd_arg->lex; /* Used in init_query. */
  fake_select_lex->make_empty_select();
  fake_select_lex->linkage= GLOBAL_OPTIONS_TYPE;
  fake_select_lex->select_limit= 0;

  fake_select_lex->context.outer_context=first_sl->context.outer_context;
  /* allow item list resolving in fake select for ORDER BY */
  fake_select_lex->context.resolve_in_select_list= true;
  fake_select_lex->context.select_lex= fake_select_lex;

  if (!is_union())
  {
    /* 
      This works only for 
      (SELECT ... ORDER BY list [LIMIT n]) ORDER BY order_list [LIMIT m],
      (SELECT ... LIMIT n) ORDER BY order_list [LIMIT m]
      just before the parser starts processing order_list
    */ 
    global_parameters= fake_select_lex;
    fake_select_lex->no_table_names_allowed= 1;
    thd_arg->lex->current_select= fake_select_lex;
  }
  thd_arg->lex->pop_context();
  return(0);
}


/**
  Push a new name resolution context for a JOIN ... ON clause to the
  context stack of a query block.

    Create a new name resolution context for a JOIN ... ON clause,
    set the first and last leaves of the list of table references
    to be used for name resolution, and push the newly created
    context to the stack of contexts of the query.

  @param thd       pointer to current thread
  @param left_op   left  operand of the JOIN
  @param right_op  rigth operand of the JOIN

  @retval
    false  if all is OK
  @retval
    true   if a memory allocation error occured
*/

bool
push_new_name_resolution_context(THD *thd,
                                 TABLE_LIST *left_op, TABLE_LIST *right_op)
{
  Name_resolution_context *on_context;
  if (!(on_context= new (thd->mem_root) Name_resolution_context))
    return true;
  on_context->init();
  on_context->first_name_resolution_table=
    left_op->first_leaf_for_name_resolution();
  on_context->last_name_resolution_table=
    right_op->last_leaf_for_name_resolution();
  return thd->lex->push_context(on_context);
}


/**
  Add an ON condition to the second operand of a JOIN ... ON.

    Add an ON condition to the right operand of a JOIN ... ON clause.

  @param b     the second operand of a JOIN ... ON
  @param expr  the condition to be added to the ON clause

  @retval
    false  if there was some error
  @retval
    true   if all is OK
*/

void add_join_on(TABLE_LIST *b, Item *expr)
{
  if (expr)
  {
    if (!b->on_expr)
      b->on_expr= expr;
    else
    {
      /*
        If called from the parser, this happens if you have both a
        right and left join. If called later, it happens if we add more
        than one condition to the ON clause.
      */
      b->on_expr= new Item_cond_and(b->on_expr,expr);
    }
    b->on_expr->top_level_item();
  }
}


/**
  Mark that there is a NATURAL JOIN or JOIN ... USING between two
  tables.

    This function marks that table b should be joined with a either via
    a NATURAL JOIN or via JOIN ... USING. Both join types are special
    cases of each other, so we treat them together. The function
    setup_conds() creates a list of equal condition between all fields
    of the same name for NATURAL JOIN or the fields in 'using_fields'
    for JOIN ... USING. The list of equality conditions is stored
    either in b->on_expr, or in JOIN::conds, depending on whether there
    was an outer join.

  EXAMPLE
  @verbatim
    SELECT * FROM t1 NATURAL LEFT JOIN t2
     <=>
    SELECT * FROM t1 LEFT JOIN t2 ON (t1.i=t2.i and t1.j=t2.j ... )

    SELECT * FROM t1 NATURAL JOIN t2 WHERE <some_cond>
     <=>
    SELECT * FROM t1, t2 WHERE (t1.i=t2.i and t1.j=t2.j and <some_cond>)

    SELECT * FROM t1 JOIN t2 USING(j) WHERE <some_cond>
     <=>
    SELECT * FROM t1, t2 WHERE (t1.j=t2.j and <some_cond>)
   @endverbatim

  @param a		  Left join argument
  @param b		  Right join argument
  @param using_fields    Field names from USING clause
*/

void add_join_natural(TABLE_LIST *a, TABLE_LIST *b, List<String> *using_fields,
                      SELECT_LEX *lex)
{
  b->natural_join= a;
  lex->prev_join_using= using_fields;
}


/**
  Reload/resets privileges and the different caches.

  @param thd Thread handler (can be NULL!)
  @param options What should be reset/reloaded (tables, privileges, slave...)
  @param tables Tables to flush (if any)
  @param write_to_binlog True if we can write to the binlog.
               
  @note Depending on 'options', it may be very bad to write the
    query to the binlog (e.g. FLUSH SLAVE); this is a
    pointer where reload_cache() will put 0 if
    it thinks we really should not write to the binlog.
    Otherwise it will put 1.

  @return Error status code
    @retval 0 Ok
    @retval !=0  Error; thd->killed is set or thd->is_error() is true
*/

bool reload_cache(THD *thd, ulong options, TABLE_LIST *tables,
                          bool *write_to_binlog)
{
  bool result=0;
  select_errors=0;				/* Write if more errors */
  bool tmp_write_to_binlog= 1;

  assert(!thd || !thd->in_sub_stmt);

  if (options & REFRESH_LOG)
  {
    /*
      Flush the normal query log, the update log, the binary log,
      the slow query log, the relay log (if it exists) and the log
      tables.
    */

    /*
      Writing this command to the binlog may result in infinite loops
      when doing mysqlbinlog|mysql, and anyway it does not really make
      sense to log it automatically (would cause more trouble to users
      than it would help them)
    */
    tmp_write_to_binlog= 0;
    if( mysql_bin_log.is_open() )
    {
      mysql_bin_log.rotate_and_purge(RP_FORCE_ROTATE);
    }
    pthread_mutex_lock(&LOCK_active_mi);
    rotate_relay_log(active_mi);
    pthread_mutex_unlock(&LOCK_active_mi);

    /* flush slow and general logs */
    logger.flush_logs(thd);

    if (ha_flush_logs(NULL))
      result=1;
    if (flush_error_log())
      result=1;
  }
  /*
    Note that if REFRESH_READ_LOCK bit is set then REFRESH_TABLES is set too
    (see sql_yacc.yy)
  */
  if (options & (REFRESH_TABLES | REFRESH_READ_LOCK)) 
  {
    if ((options & REFRESH_READ_LOCK) && thd)
    {
      /*
        We must not try to aspire a global read lock if we have a write
        locked table. This would lead to a deadlock when trying to
        reopen (and re-lock) the table after the flush.
      */
      if (thd->locked_tables)
      {
        THR_LOCK_DATA **lock_p= thd->locked_tables->locks;
        THR_LOCK_DATA **end_p= lock_p + thd->locked_tables->lock_count;

        for (; lock_p < end_p; lock_p++)
        {
          if ((*lock_p)->type >= TL_WRITE_ALLOW_WRITE)
          {
            my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
            return 1;
          }
        }
      }
      /*
	Writing to the binlog could cause deadlocks, as we don't log
	UNLOCK TABLES
      */
      tmp_write_to_binlog= 0;
      if (lock_global_read_lock(thd))
	return 1;                               // Killed
      result= close_cached_tables(thd, tables, false, (options & REFRESH_FAST) ?
                                  false : true, true);
      if (make_global_read_lock_block_commit(thd)) // Killed
      {
        /* Don't leave things in a half-locked state */
        unlock_global_read_lock(thd);
        return 1;
      }
    }
    else
      result= close_cached_tables(thd, tables, false, (options & REFRESH_FAST) ?
                                  false : true, false);
    my_dbopt_cleanup();
  }
  if (thd && (options & REFRESH_STATUS))
    refresh_status(thd);
  if (options & REFRESH_THREADS)
    flush_thread_cache();
  if (options & REFRESH_MASTER)
  {
    assert(thd);
    tmp_write_to_binlog= 0;
    if (reset_master(thd))
    {
      result=1;
    }
  }
 if (options & REFRESH_SLAVE)
 {
   tmp_write_to_binlog= 0;
   pthread_mutex_lock(&LOCK_active_mi);
   if (reset_slave(thd, active_mi))
     result=1;
   pthread_mutex_unlock(&LOCK_active_mi);
 }
 *write_to_binlog= tmp_write_to_binlog;
 return result;
}


/**
  kill on thread.

  @param thd			Thread class
  @param id			Thread id
  @param only_kill_query        Should it kill the query or the connection

  @note
    This is written such that we have a short lock on LOCK_thread_count
*/

uint kill_one_thread(THD *thd __attribute__((__unused__)),
                     ulong id, bool only_kill_query)
{
  THD *tmp;
  uint error=ER_NO_SUCH_THREAD;
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    if (tmp->command == COM_DAEMON)
      continue;
    if (tmp->thread_id == id)
    {
      pthread_mutex_lock(&tmp->LOCK_delete);	// Lock from delete
      break;
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  if (tmp)
  {
    tmp->awake(only_kill_query ? THD::KILL_QUERY : THD::KILL_CONNECTION);
    error=0;
    pthread_mutex_unlock(&tmp->LOCK_delete);
  }
  return(error);
}


/*
  kills a thread and sends response

  SYNOPSIS
    sql_kill()
    thd			Thread class
    id			Thread id
    only_kill_query     Should it kill the query or the connection
*/

void sql_kill(THD *thd, ulong id, bool only_kill_query)
{
  uint error;
  if (!(error= kill_one_thread(thd, id, only_kill_query)))
    my_ok(thd);
  else
    my_error(error, MYF(0), id);
}


/** If pointer is not a null pointer, append filename to it. */

bool append_file_to_dir(THD *thd, const char **filename_ptr,
                        const char *table_name)
{
  char buff[FN_REFLEN],*ptr, *end;
  if (!*filename_ptr)
    return 0;					// nothing to do

  /* Check that the filename is not too long and it's a hard path */
  if (strlen(*filename_ptr)+strlen(table_name) >= FN_REFLEN-1 ||
      !test_if_hard_path(*filename_ptr))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), *filename_ptr);
    return 1;
  }
  /* Fix is using unix filename format on dos */
  strmov(buff,*filename_ptr);
  end=convert_dirname(buff, *filename_ptr, NullS);
  if (!(ptr= (char*) thd->alloc((size_t) (end-buff) + strlen(table_name)+1)))
    return 1;					// End of memory
  *filename_ptr=ptr;
  strxmov(ptr,buff,table_name,NullS);
  return 0;
}


/**
  Check if the select is a simple select (not an union).

  @retval
    0	ok
  @retval
    1	error	; In this case the error messege is sent to the client
*/

bool check_simple_select()
{
  THD *thd= current_thd;
  LEX *lex= thd->lex;
  if (lex->current_select != &lex->select_lex)
  {
    char command[80];
    Lex_input_stream *lip= thd->m_lip;
    strmake(command, lip->yylval->symbol.str,
	    min(lip->yylval->symbol.length, sizeof(command)-1));
    my_error(ER_CANT_USE_OPTION_HERE, MYF(0), command);
    return 1;
  }
  return 0;
}


Comp_creator *comp_eq_creator(bool invert)
{
  return invert?(Comp_creator *)&ne_creator:(Comp_creator *)&eq_creator;
}


Comp_creator *comp_ge_creator(bool invert)
{
  return invert?(Comp_creator *)&lt_creator:(Comp_creator *)&ge_creator;
}


Comp_creator *comp_gt_creator(bool invert)
{
  return invert?(Comp_creator *)&le_creator:(Comp_creator *)&gt_creator;
}


Comp_creator *comp_le_creator(bool invert)
{
  return invert?(Comp_creator *)&gt_creator:(Comp_creator *)&le_creator;
}


Comp_creator *comp_lt_creator(bool invert)
{
  return invert?(Comp_creator *)&ge_creator:(Comp_creator *)&lt_creator;
}


Comp_creator *comp_ne_creator(bool invert)
{
  return invert?(Comp_creator *)&eq_creator:(Comp_creator *)&ne_creator;
}


/**
  Construct ALL/ANY/SOME subquery Item.

  @param left_expr   pointer to left expression
  @param cmp         compare function creator
  @param all         true if we create ALL subquery
  @param select_lex  pointer on parsed subquery structure

  @return
    constructed Item (or 0 if out of memory)
*/
Item * all_any_subquery_creator(Item *left_expr,
				chooser_compare_func_creator cmp,
				bool all,
				SELECT_LEX *select_lex)
{
  if ((cmp == &comp_eq_creator) && !all)       //  = ANY <=> IN
    return new Item_in_subselect(left_expr, select_lex);

  if ((cmp == &comp_ne_creator) && all)        // <> ALL <=> NOT IN
    return new Item_func_not(new Item_in_subselect(left_expr, select_lex));

  Item_allany_subselect *it=
    new Item_allany_subselect(left_expr, cmp, select_lex, all);
  if (all)
    return it->upper_item= new Item_func_not_all(it);	/* ALL */

  return it->upper_item= new Item_func_nop_all(it);      /* ANY/SOME */
}


/**
  Multi update query pre-check.

  @param thd		Thread handler
  @param tables	Global/local table list (have to be the same)

  @retval
    false OK
  @retval
    true  Error
*/

bool multi_update_precheck(THD *thd,
                           TABLE_LIST *tables __attribute__((__unused__)))
{
  const char *msg= 0;
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;

  if (select_lex->item_list.elements != lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    return(true);
  }

  if (select_lex->order_list.elements)
    msg= "ORDER BY";
  else if (select_lex->select_limit)
    msg= "LIMIT";
  if (msg)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", msg);
    return(true);
  }
  return(false);
}

/**
  Multi delete query pre-check.

  @param thd			Thread handler
  @param tables		Global/local table list

  @retval
    false OK
  @retval
    true  error
*/

bool multi_delete_precheck(THD *thd,
                           TABLE_LIST *tables __attribute__((__unused__)))
{
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  TABLE_LIST **save_query_tables_own_last= thd->lex->query_tables_own_last;

  thd->lex->query_tables_own_last= 0;
  thd->lex->query_tables_own_last= save_query_tables_own_last;

  if ((thd->options & OPTION_SAFE_UPDATES) && !select_lex->where)
  {
    my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
               ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
    return(true);
  }
  return(false);
}


/*
  Given a table in the source list, find a correspondent table in the
  table references list.

  @param lex Pointer to LEX representing multi-delete.
  @param src Source table to match.
  @param ref Table references list.

  @remark The source table list (tables listed before the FROM clause
  or tables listed in the FROM clause before the USING clause) may
  contain table names or aliases that must match unambiguously one,
  and only one, table in the target table list (table references list,
  after FROM/USING clause).

  @return Matching table, NULL otherwise.
*/

static TABLE_LIST *multi_delete_table_match(LEX *lex __attribute__((__unused__)),
                                            TABLE_LIST *tbl,
                                            TABLE_LIST *tables)
{
  TABLE_LIST *match= NULL;

  for (TABLE_LIST *elem= tables; elem; elem= elem->next_local)
  {
    int cmp;

    if (tbl->is_fqtn && elem->is_alias)
      continue; /* no match */
    if (tbl->is_fqtn && elem->is_fqtn)
      cmp= my_strcasecmp(table_alias_charset, tbl->table_name, elem->table_name) ||
           strcmp(tbl->db, elem->db);
    else if (elem->is_alias)
      cmp= my_strcasecmp(table_alias_charset, tbl->alias, elem->alias);
    else
      cmp= my_strcasecmp(table_alias_charset, tbl->table_name, elem->table_name) ||
           strcmp(tbl->db, elem->db);

    if (cmp)
      continue;

    if (match)
    {
      my_error(ER_NONUNIQ_TABLE, MYF(0), elem->alias);
      return(NULL);
    }

    match= elem;
  }

  if (!match)
    my_error(ER_UNKNOWN_TABLE, MYF(0), tbl->table_name, "MULTI DELETE");

  return(match);
}


/**
  Link tables in auxilary table list of multi-delete with corresponding
  elements in main table list, and set proper locks for them.

  @param lex   pointer to LEX representing multi-delete

  @retval
    false   success
  @retval
    true    error
*/

bool multi_delete_set_locks_and_link_aux_tables(LEX *lex)
{
  TABLE_LIST *tables= (TABLE_LIST*)lex->select_lex.table_list.first;
  TABLE_LIST *target_tbl;

  lex->table_count= 0;

  for (target_tbl= (TABLE_LIST *)lex->auxiliary_table_list.first;
       target_tbl; target_tbl= target_tbl->next_local)
  {
    lex->table_count++;
    /* All tables in aux_tables must be found in FROM PART */
    TABLE_LIST *walk= multi_delete_table_match(lex, target_tbl, tables);
    if (!walk)
      return(true);
    if (!walk->derived)
    {
      target_tbl->table_name= walk->table_name;
      target_tbl->table_name_length= walk->table_name_length;
    }
    walk->updating= target_tbl->updating;
    walk->lock_type= target_tbl->lock_type;
    target_tbl->correspondent_table= walk;	// Remember corresponding table
  }
  return(false);
}


/**
  simple UPDATE query pre-check.

  @param thd		Thread handler
  @param tables	Global table list

  @retval
    false OK
  @retval
    true  Error
*/

bool update_precheck(THD *thd, TABLE_LIST *tables __attribute__((__unused__)))
{
  if (thd->lex->select_lex.item_list.elements != thd->lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    return(true);
  }
  return(false);
}


/**
  simple INSERT query pre-check.

  @param thd		Thread handler
  @param tables	Global table list

  @retval
    false  OK
  @retval
    true   error
*/

bool insert_precheck(THD *thd, TABLE_LIST *tables __attribute__((__unused__)))
{
  LEX *lex= thd->lex;

  /*
    Check that we have modify privileges for the first table and
    select privileges for the rest
  */
  if (lex->update_list.elements != lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    return(true);
  }
  return(false);
}


/**
  CREATE TABLE query pre-check.

  @param thd			Thread handler
  @param tables		Global table list
  @param create_table	        Table which will be created

  @retval
    false   OK
  @retval
    true   Error
*/

bool create_table_precheck(THD *thd,
                           TABLE_LIST *tables __attribute__((__unused__)),
                           TABLE_LIST *create_table)
{
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  bool error= true;                                 // Error message is given

  if (create_table && (strcmp(create_table->db, "information_schema") == 0))
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), "", "", INFORMATION_SCHEMA_NAME.str);
    return(true);
  }

  if (select_lex->item_list.elements)
  {
    /* Check permissions for used tables in CREATE TABLE ... SELECT */

#ifdef NOT_NECESSARY_TO_CHECK_CREATE_TABLE_EXIST_WHEN_PREPARING_STATEMENT
    /* This code throws an ill error for CREATE TABLE t1 SELECT * FROM t1 */
    /*
      Only do the check for PS, because we on execute we have to check that
      against the opened tables to ensure we don't use a table that is part
      of the view (which can only be done after the table has been opened).
    */
    if (thd->stmt_arena->is_stmt_prepare_or_first_sp_execute())
    {
      /*
        For temporary tables we don't have to check if the created table exists
      */
      if (!(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) &&
          find_table_in_global_list(tables, create_table->db,
                                    create_table->table_name))
      {
	error= false;
        goto err;
      }
    }
#endif
  }
  error= false;

  return(error);
}


/**
  negate given expression.

  @param thd  thread handler
  @param expr expression for negation

  @return
    negated expression
*/

Item *negate_expression(THD *thd, Item *expr)
{
  Item *negated;
  if (expr->type() == Item::FUNC_ITEM &&
      ((Item_func *) expr)->functype() == Item_func::NOT_FUNC)
  {
    /* it is NOT(NOT( ... )) */
    Item *arg= ((Item_func *) expr)->arguments()[0];
    enum_parsing_place place= thd->lex->current_select->parsing_place;
    if (arg->is_bool_func() || place == IN_WHERE || place == IN_HAVING)
      return arg;
    /*
      if it is not boolean function then we have to emulate value of
      not(not(a)), it will be a != 0
    */
    return new Item_func_ne(arg, new Item_int((char*) "0", 0, 1));
  }

  if ((negated= expr->neg_transformer(thd)) != 0)
    return negated;
  return new Item_func_not(expr);
}


/**
  Check that byte length of a string does not exceed some limit.

  @param str         string to be checked
  @param err_msg     error message to be displayed if the string is too long
  @param max_length  max length

  @retval
    false   the passed string is not longer than max_length
  @retval
    true    the passed string is longer than max_length

  NOTE
    The function is not used in existing code but can be useful later?
*/

bool check_string_byte_length(LEX_STRING *str, const char *err_msg,
                              uint max_byte_length)
{
  if (str->length <= max_byte_length)
    return false;

  my_error(ER_WRONG_STRING_LENGTH, MYF(0), str->str, err_msg, max_byte_length);

  return true;
}


/*
  Check that char length of a string does not exceed some limit.

  SYNOPSIS
  check_string_char_length()
      str              string to be checked
      err_msg          error message to be displayed if the string is too long
      max_char_length  max length in symbols
      cs               string charset

  RETURN
    false   the passed string is not longer than max_char_length
    true    the passed string is longer than max_char_length
*/


bool check_string_char_length(LEX_STRING *str, const char *err_msg,
                              uint max_char_length, CHARSET_INFO *cs,
                              bool no_error)
{
  int well_formed_error;
  uint res= cs->cset->well_formed_len(cs, str->str, str->str + str->length,
                                      max_char_length, &well_formed_error);

  if (!well_formed_error &&  str->length == res)
    return false;

  if (!no_error)
    my_error(ER_WRONG_STRING_LENGTH, MYF(0), str->str, err_msg, max_char_length);
  return true;
}


bool check_identifier_name(LEX_STRING *str, uint max_char_length,
                           uint err_code, const char *param_for_err_msg)
{
#ifdef HAVE_CHARSET_utf8mb3
  /*
    We don't support non-BMP characters in identifiers at the moment,
    so they should be prohibited until such support is done.
    This is why we use the 3-byte utf8 to check well-formedness here.
  */
  CHARSET_INFO *cs= &my_charset_utf8mb3_general_ci;
#else
  CHARSET_INFO *cs= system_charset_info;
#endif
  int well_formed_error;
  uint res= cs->cset->well_formed_len(cs, str->str, str->str + str->length,
                                      max_char_length, &well_formed_error);

  if (well_formed_error)
  {
    my_error(ER_INVALID_CHARACTER_STRING, MYF(0), "identifier", str->str);
    return true;
  }
  
  if (str->length == res)
    return false;

  switch (err_code)
  {
  case 0:
    break;
  case ER_WRONG_STRING_LENGTH:
    my_error(err_code, MYF(0), str->str, param_for_err_msg, max_char_length);
    break;
  case ER_TOO_LONG_IDENT:
    my_error(err_code, MYF(0), str->str);
    break;
  default:
    assert(0);
    break;
  }
  return true;
}


/*
  Check if path does not contain mysql data home directory
  SYNOPSIS
    test_if_data_home_dir()
    dir                     directory
    conv_home_dir           converted data home directory
    home_dir_len            converted data home directory length

  RETURN VALUES
    0	ok
    1	error  
*/

bool test_if_data_home_dir(const char *dir)
{
  char path[FN_REFLEN], conv_path[FN_REFLEN];
  uint dir_len, home_dir_len= strlen(mysql_unpacked_real_data_home);

  if (!dir)
    return(0);

  (void) fn_format(path, dir, "", "",
                   (MY_RETURN_REAL_PATH|MY_RESOLVE_SYMLINKS));
  dir_len= unpack_dirname(conv_path, dir);

  if (home_dir_len < dir_len)
  {
    if (lower_case_file_system)
    {
      if (!my_strnncoll(character_set_filesystem,
                        (const uchar*) conv_path, home_dir_len,
                        (const uchar*) mysql_unpacked_real_data_home,
                        home_dir_len))
        return(1);
    }
    else if (!memcmp(conv_path, mysql_unpacked_real_data_home, home_dir_len))
      return(1);
  }
  return(0);
}


extern int MYSQLparse(void *thd); // from sql_yacc.cc


/**
  This is a wrapper of MYSQLparse(). All the code should call parse_sql()
  instead of MYSQLparse().

  @param thd Thread context.
  @param lip Lexer context.
  @param creation_ctx Object creation context.

  @return Error status.
    @retval false on success.
    @retval true on parsing error.
*/

bool parse_sql(THD *thd,
               Lex_input_stream *lip,
               Object_creation_ctx *creation_ctx)
{
  assert(thd->m_lip == NULL);

  /* Backup creation context. */

  Object_creation_ctx *backup_ctx= NULL;

  if (creation_ctx)
    backup_ctx= creation_ctx->set_n_backup(thd);

  /* Set Lex_input_stream. */

  thd->m_lip= lip;

  /* Parse the query. */

  bool mysql_parse_status= MYSQLparse(thd) != 0;

  /* Check that if MYSQLparse() failed, thd->is_error() is set. */

  assert(!mysql_parse_status || thd->is_error());

  /* Reset Lex_input_stream. */

  thd->m_lip= NULL;

  /* Restore creation context. */

  if (creation_ctx)
    creation_ctx->restore_env(thd, backup_ctx);

  /* That's it. */

  return mysql_parse_status || thd->is_fatal_error;
}

/**
  @} (end of group Runtime_Environment)
*/
