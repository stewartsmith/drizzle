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

#define DRIZZLE_LEX 1
#include <drizzled/server_includes.h>
#include <mysys/hash.h>
#include <drizzled/logging.h>
#include <drizzled/db.h>
#include <drizzled/error.h>
#include <drizzled/nested_join.h>
#include <drizzled/query_id.h>
#include <drizzled/sql_parse.h>
#include <drizzled/data_home.h>
#include <drizzled/sql_base.h>
#include <drizzled/show.h>
#include <drizzled/rename.h>
#include <drizzled/function/time/unix_timestamp.h>
#include <drizzled/function/get_system_var.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/null.h>
#include <drizzled/session.h>
#include <drizzled/sql_load.h>
#include <drizzled/connect.h>
#include <drizzled/lock.h>
#include <drizzled/select_send.h>
#include <bitset>

using namespace std;

/**
  @defgroup Runtime_Environment Runtime Environment
  @{
*/

extern size_t my_thread_stack_size;
extern const CHARSET_INFO *character_set_filesystem;
const char *any_db="*any*";	// Special symbol for check_access

const LEX_STRING command_name[COM_END+1]={
  { C_STRING_WITH_LEN("Sleep") },
  { C_STRING_WITH_LEN("Quit") },
  { C_STRING_WITH_LEN("Init DB") },
  { C_STRING_WITH_LEN("Query") },
  { C_STRING_WITH_LEN("Shutdown") },
  { C_STRING_WITH_LEN("Connect") },
  { C_STRING_WITH_LEN("Ping") },
  { C_STRING_WITH_LEN("Error") }  // Last command number
};

const char *xa_state_names[]={
  "NON-EXISTING", "ACTIVE", "IDLE", "PREPARED"
};

static void unlock_locked_tables(Session *session)
{
  if (session->locked_tables)
  {
    session->lock=session->locked_tables;
    session->locked_tables=0;			// Will be automatically closed
    close_thread_tables(session);			// Free tables
  }
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
bitset<CF_BIT_SIZE> sql_command_flags[SQLCOM_END+1];

void init_update_queries(void)
{
  uint32_t x;

  for (x= 0; x <= SQLCOM_END; x++)
    sql_command_flags[x].reset();

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
  sql_command_flags[SQLCOM_SHOW_WARNS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ERRORS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_STATUS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROCESSLIST]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_DB]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE]=  CF_STATUS_COMMAND;

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
  return (sql_command_flags[command].test(CF_BIT_CHANGES_DATA));
}

/**
  Perform one connection-level (COM_XXXX) command.

  @param command         type of command to perform
  @param session             connection handle
  @param packet          data for the command, packet is always null-terminated
  @param packet_length   length of packet + 1 (to show that data is
                         null-terminated) except for COM_SLEEP, where it
                         can be zero.

  @todo
    set session->lex->sql_command to SQLCOM_END here.
  @todo
    The following has to be changed to an 8 byte integer

  @retval
    0   ok
  @retval
    1   request of thread shutdown, i. e. if command is
        COM_QUIT/COM_SHUTDOWN
*/
bool dispatch_command(enum enum_server_command command, Session *session,
                      char* packet, uint32_t packet_length)
{
  bool error= 0;
  Query_id &query_id= Query_id::get_query_id();

  session->command=command;
  session->lex->sql_command= SQLCOM_END; /* to avoid confusing VIEW detectors */
  session->set_time();
  session->query_id= query_id.value();

  switch( command ) {
  /* Ignore these statements. */
  case COM_PING:
    break;
  /* Increase id and count all other statements. */
  default:
    statistic_increment(session->status_var.questions, &LOCK_status);
    query_id.next();
  }

  /* TODO: set session->lex->sql_command to SQLCOM_END here */

  logging_pre_do(session);

  session->server_status&=
           ~(SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED);
  switch (command) {
  case COM_INIT_DB:
  {
    LEX_STRING tmp;
    status_var_increment(session->status_var.com_stat[SQLCOM_CHANGE_DB]);
    session->convert_string(&tmp, system_charset_info,
                        packet, packet_length, session->charset());
    if (!mysql_change_db(session, &tmp, false))
    {
      session->my_ok();
    }
    break;
  }
  case COM_QUERY:
  {
    if (! session->readAndStoreQuery(packet, packet_length))
      break;					// fatal error is set
    const char* end_of_stmt= NULL;

    mysql_parse(session, session->query, session->query_length, &end_of_stmt);

    break;
  }
  case COM_QUIT:
    /* We don't calculate statistics for this command */
    session->protocol->setError(0);
    session->main_da.disable_status();              // Don't send anything back
    error=true;					// End server
    break;
  case COM_SHUTDOWN:
  {
    status_var_increment(session->status_var.com_other);
    session->my_eof();
    close_thread_tables(session);			// Free before kill
    kill_drizzle();
    error=true;
    break;
  }
  case COM_PING:
    status_var_increment(session->status_var.com_other);
    session->my_ok();				// Tell client we are alive
    break;
  case COM_SLEEP:
  case COM_CONNECT:				// Impossible here
  case COM_END:
  default:
    my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
    break;
  }

  /* If commit fails, we should be able to reset the OK status. */
  session->main_da.can_overwrite_status= true;
  ha_autocommit_or_rollback(session, session->is_error());
  session->main_da.can_overwrite_status= false;

  session->transaction.stmt.reset();


  /* report error issued during command execution */
  if (session->killed_errno())
  {
    if (! session->main_da.is_set())
      session->send_kill_message();
  }
  if (session->killed == Session::KILL_QUERY || session->killed == Session::KILL_BAD_DATA)
  {
    session->killed= Session::NOT_KILLED;
    session->mysys_var->abort= 0;
  }

  /* Can not be true, but do not take chances in production. */
  assert(! session->main_da.is_sent);

  switch (session->main_da.status())
  {
  case Diagnostics_area::DA_ERROR:
    /* The query failed, send error to log and abort bootstrap. */
    session->protocol->sendError(session->main_da.sql_errno(),
                                 session->main_da.message());
    break;

  case Diagnostics_area::DA_EOF:
    session->protocol->sendEOF();
    break;

  case Diagnostics_area::DA_OK:
    session->protocol->sendOK();
    break;

  case Diagnostics_area::DA_DISABLED:
    break;

  case Diagnostics_area::DA_EMPTY:
  default:
    session->protocol->sendOK();
    break;
  }

  session->main_da.is_sent= true;

  session->set_proc_info("closing tables");
  /* Free tables */
  close_thread_tables(session);

  log_slow_statement(session);

  /* Store temp state for processlist */
  session->set_proc_info("cleaning up");
  session->command=COM_SLEEP;
  session->process_list_info[0]= 0;
  session->query=0;
  session->query_length=0;

  session->set_proc_info(NULL);
  session->packet.shrink(session->variables.net_buffer_length);	// Reclaim some memory
  free_root(session->mem_root,MYF(MY_KEEP_PREALLOC));
  return(error);
}


void log_slow_statement(Session *session)
{
  logging_post_do(session);

  return;
}


/**
  Create a TableList object for an INFORMATION_SCHEMA table.

    This function is used in the parser to convert a SHOW or DESCRIBE
    table_name command to a SELECT from INFORMATION_SCHEMA.
    It prepares a Select_Lex and a TableList object to represent the
    given command as a SELECT parse tree.

  @param session              thread handle
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

int prepare_schema_table(Session *session, LEX *lex, Table_ident *table_ident,
                         enum enum_schema_tables schema_table_idx)
{
  Select_Lex *schema_select_lex= NULL;

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
      schema_select_lex= new Select_Lex();
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
    TableList **query_tables_last= lex->query_tables_last;
    schema_select_lex= new Select_Lex();
    /* 'parent_lex' is used in init_query() so it must be before it. */
    schema_select_lex->parent_lex= lex;
    schema_select_lex->init_query();
    if (!schema_select_lex->add_table_to_list(session, table_ident, 0, 0, TL_READ))
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

  Select_Lex *select_lex= lex->current_select;
  assert(select_lex);
  if (make_schema_select(session, select_lex, schema_table_idx))
  {
    return(1);
  }
  TableList *table_list= (TableList*) select_lex->table_list.first;
  assert(table_list);
  table_list->schema_select_lex= schema_select_lex;
  table_list->schema_table_reformed= 1;
  return(0);
}

/**
  Execute command saved in session and lex->sql_command.

    Before every operation that can request a write lock for a table
    wait if a global read lock exists. However do not wait if this
    thread has locked tables already. No new locks can be requested
    until the other locks are released. The thread that requests the
    global read lock waits for write locked tables to become unlocked.

    Note that wait_if_global_read_lock() sets a protection against a new
    global read lock when it succeeds. This needs to be released by
    start_waiting_global_read_lock() after the operation.

  @param session                       Thread handle

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
mysql_execute_command(Session *session)
{
  int res= false;
  bool need_start_waiting= false; // have protection against global read lock
  LEX  *lex= session->lex;
  /* first Select_Lex (have special meaning for many of non-SELECTcommands) */
  Select_Lex *select_lex= &lex->select_lex;
  /* first table of first Select_Lex */
  TableList *first_table= (TableList*) select_lex->table_list.first;
  /* list of all tables in query */
  TableList *all_tables;
  /* most outer Select_Lex_Unit of query */
  Select_Lex_Unit *unit= &lex->unit;
  /* A peek into the query string */
  size_t proc_info_len= session->query_length > PROCESS_LIST_WIDTH ?
                        PROCESS_LIST_WIDTH : session->query_length;

  memcpy(session->process_list_info, session->query, proc_info_len);
  session->process_list_info[proc_info_len]= '\0';

  /*
    In many cases first table of main Select_Lex have special meaning =>
    check that it is first table in global list and relink it first in
    queries_tables list if it is necessary (we need such relinking only
    for queries with subqueries in select list, in this case tables of
    subqueries will go to global list first)

    all_tables will differ from first_table only if most upper Select_Lex
    do not contain tables.

    Because of above in place where should be at least one table in most
    outer Select_Lex we have following check:
    assert(first_table == all_tables);
    assert(first_table == all_tables && first_table != 0);
  */
  lex->first_lists_tables_same();
  /* should be assigned after making first tables same */
  all_tables= lex->query_tables;
  /* set context for commands which do not use setup_tables */
  select_lex->
    context.resolve_in_table_list_only((TableList*)select_lex->
                                       table_list.first);

  /*
    Reset warning count for each query that uses tables
    A better approach would be to reset this for any commands
    that is not a SHOW command or a select that only access local
    variables, but for now this is probably good enough.
    Don't reset warnings when executing a stored routine.
  */
  if (all_tables || !lex->is_single_level_stmt())
    drizzle_reset_errors(session, 0);

  status_var_increment(session->status_var.com_stat[lex->sql_command]);

  assert(session->transaction.stmt.modified_non_trans_table == false);

  switch (lex->sql_command) {
  case SQLCOM_SHOW_STATUS:
  {
    system_status_var old_status_var= session->status_var;
    session->initial_status_var= &old_status_var;
    res= execute_sqlcom_select(session, all_tables);
    /* Don't log SHOW STATUS commands to slow query log */
    session->server_status&= ~(SERVER_QUERY_NO_INDEX_USED |
                           SERVER_QUERY_NO_GOOD_INDEX_USED);
    /*
      restore status variables, as we don't want 'show status' to cause
      changes
    */
    pthread_mutex_lock(&LOCK_status);
    add_diff_to_status(&global_status_var, &session->status_var,
                       &old_status_var);
    session->status_var= old_status_var;
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
  case SQLCOM_SELECT:
  {
    session->status_var.last_query_cost= 0.0;
    res= execute_sqlcom_select(session, all_tables);
    break;
  }
  case SQLCOM_EMPTY_QUERY:
    session->my_ok();
    break;

  case SQLCOM_SHOW_WARNS:
  {
    res= mysqld_show_warnings(session, (uint32_t)
			      ((1L << (uint32_t) DRIZZLE_ERROR::WARN_LEVEL_NOTE) |
			       (1L << (uint32_t) DRIZZLE_ERROR::WARN_LEVEL_WARN) |
			       (1L << (uint32_t) DRIZZLE_ERROR::WARN_LEVEL_ERROR)
			       ));
    break;
  }
  case SQLCOM_SHOW_ERRORS:
  {
    res= mysqld_show_warnings(session, (uint32_t)
			      (1L << (uint32_t) DRIZZLE_ERROR::WARN_LEVEL_ERROR));
    break;
  }
  case SQLCOM_ASSIGN_TO_KEYCACHE:
  {
    assert(first_table == all_tables && first_table != 0);
    res= mysql_assign_to_keycache(session, first_table, &lex->ident);
    break;
  }
  case SQLCOM_SHOW_ENGINE_STATUS:
    {
      res = ha_show_status(session, lex->create_info.db_type, HA_ENGINE_STATUS);
      break;
    }
  case SQLCOM_CREATE_TABLE:
  {
    /* If CREATE TABLE of non-temporary table, do implicit commit */
    if (!(lex->create_info.options & HA_LEX_CREATE_TMP_TABLE))
    {
      if (! session->endActiveTransaction())
      {
        res= -1;
        break;
      }
    }
    assert(first_table == all_tables && first_table != 0);
    bool link_to_local;
    // Skip first table, which is the table we are creating
    TableList *create_table= lex->unlink_first_table(&link_to_local);
    TableList *select_tables= lex->query_tables;
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
    Alter_info alter_info(lex->alter_info, session->mem_root);

    if (session->is_fatal_error)
    {
      /* If out of memory when creating a copy of alter_info. */
      res= 1;
      goto end_with_restore_list;
    }

    if ((res= create_table_precheck(session, select_tables, create_table)))
      goto end_with_restore_list;

    /* Might have been updated in create_table_precheck */
    create_info.alias= create_table->alias;

#ifdef HAVE_READLINK
    /* Fix names if symlinked tables */
    if (append_file_to_dir(session, &create_info.data_file_name,
			   create_table->table_name) ||
	append_file_to_dir(session, &create_info.index_file_name,
			   create_table->table_name))
      goto end_with_restore_list;
#endif
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
    if (!session->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(session, 0, 1)))
    {
      res= 1;
      goto end_with_restore_list;
    }
    if (select_lex->item_list.elements)		// With select
    {
      select_result *result;

      select_lex->options|= SELECT_NO_UNLOCK;
      unit->set_limit(select_lex);

      if (!(create_info.options & HA_LEX_CREATE_TMP_TABLE))
      {
        lex->link_first_table_back(create_table, link_to_local);
        create_table->create= true;
      }

      if (!(res= open_and_lock_tables(session, lex->query_tables)))
      {
        /*
          Is table which we are changing used somewhere in other parts
          of query
        */
        if (!(create_info.options & HA_LEX_CREATE_TMP_TABLE))
        {
          TableList *duplicate;
          create_table= lex->unlink_first_table(&link_to_local);
          if ((duplicate= unique_table(session, create_table, select_tables, 0)))
          {
            update_non_unique_table_error(create_table, "CREATE", duplicate);
            res= 1;
            goto end_with_restore_list;
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
            CREATE from SELECT give its Select_Lex for SELECT,
            and item_list belong to SELECT
          */
          res= handle_select(session, lex, result, 0);
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
        session->options|= OPTION_KEEP_LOG;
      /* regular create */
      if (create_info.options & HA_LEX_CREATE_TABLE_LIKE)
        res= mysql_create_like_table(session, create_table, select_tables,
                                     &create_info);
      else
      {
        res= mysql_create_table(session, create_table->db,
                                create_table->table_name, &create_info,
                                &alter_info, 0, 0);
      }
      if (!res)
	session->my_ok();
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
    Alter_info alter_info(lex->alter_info, session->mem_root);

    if (session->is_fatal_error) /* out of memory creating a copy of alter_info */
      goto error;

    assert(first_table == all_tables && first_table != 0);
    if (! session->endActiveTransaction())
      goto error;

    memset(&create_info, 0, sizeof(create_info));
    create_info.db_type= 0;
    create_info.row_type= ROW_TYPE_NOT_USED;
    create_info.default_table_charset= session->variables.collation_database;

    res= mysql_alter_table(session, first_table->db, first_table->table_name,
                           &create_info, first_table, &alter_info,
                           0, (order_st*) 0, 0);
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
      Alter_info alter_info(lex->alter_info, session->mem_root);

      if (session->is_fatal_error) /* out of memory creating a copy of alter_info */
      {
        goto error;
      }

      /* Must be set in the parser */
      assert(select_lex->db);

      { // Rename of table
          TableList tmp_table;
          memset(&tmp_table, 0, sizeof(tmp_table));
          tmp_table.table_name= lex->name.str;
          tmp_table.db=select_lex->db;
      }

      /* Don't yet allow changing of symlinks with ALTER TABLE */
      if (create_info.data_file_name)
        push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN, 0,
                     "DATA DIRECTORY option ignored");
      if (create_info.index_file_name)
        push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN, 0,
                     "INDEX DIRECTORY option ignored");
      create_info.data_file_name= create_info.index_file_name= NULL;
      /* ALTER TABLE ends previous transaction */
      if (! session->endActiveTransaction())
	goto error;

      if (!session->locked_tables &&
          !(need_start_waiting= !wait_if_global_read_lock(session, 0, 1)))
      {
        res= 1;
        break;
      }

      res= mysql_alter_table(session, select_lex->db, lex->name.str,
                             &create_info,
                             first_table,
                             &alter_info,
                             select_lex->order_list.elements,
                             (order_st *) select_lex->order_list.first,
                             lex->ignore);
      break;
    }
  case SQLCOM_RENAME_TABLE:
  {
    assert(first_table == all_tables && first_table != 0);
    TableList *table;
    for (table= first_table; table; table= table->next_local->next_local)
    {
      TableList old_list, new_list;
      /*
        we do not need initialize old_list and new_list because we will
        come table[0] and table->next[0] there
      */
      old_list= table[0];
      new_list= table->next_local[0];
    }

    if (! session->endActiveTransaction() || drizzle_rename_tables(session, first_table, 0))
    {
      goto error;
    }
    break;
  }
  case SQLCOM_SHOW_CREATE:
    assert(first_table == all_tables && first_table != 0);
    {
      res= drizzled_show_create(session, first_table);
      break;
    }
  case SQLCOM_CHECKSUM:
  {
    assert(first_table == all_tables && first_table != 0);
    res = mysql_checksum_table(session, first_table, &lex->check_opt);
    break;
  }
  case SQLCOM_REPAIR:
  {
    assert(first_table == all_tables && first_table != 0);
    res= mysql_repair_table(session, first_table, &lex->check_opt);
    /* ! we write after unlocking the table */
    /*
      Presumably, REPAIR and binlog writing doesn't require synchronization
    */
    write_bin_log(session, true, session->query, session->query_length);
    select_lex->table_list.first= (unsigned char*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_CHECK:
  {
    assert(first_table == all_tables && first_table != 0);
    res = mysql_check_table(session, first_table, &lex->check_opt);
    select_lex->table_list.first= (unsigned char*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_ANALYZE:
  {
    assert(first_table == all_tables && first_table != 0);
    res= mysql_analyze_table(session, first_table, &lex->check_opt);
    /* ! we write after unlocking the table */
    write_bin_log(session, true, session->query, session->query_length);
    select_lex->table_list.first= (unsigned char*) first_table;
    lex->query_tables=all_tables;
    break;
  }

  case SQLCOM_OPTIMIZE:
  {
    assert(first_table == all_tables && first_table != 0);
    res= mysql_optimize_table(session, first_table, &lex->check_opt);
    /* ! we write after unlocking the table */
    write_bin_log(session, true, session->query, session->query_length);
    select_lex->table_list.first= (unsigned char*) first_table;
    lex->query_tables=all_tables;
    break;
  }
  case SQLCOM_UPDATE:
    assert(first_table == all_tables && first_table != 0);
    if ((res= update_precheck(session, all_tables)))
      break;
    assert(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);
    res= mysql_update(session, all_tables,
                      select_lex->item_list,
                      lex->value_list,
                      select_lex->where,
                      select_lex->order_list.elements,
                      (order_st *) select_lex->order_list.first,
                      unit->select_limit_cnt,
                      lex->duplicates, lex->ignore);
    break;
  case SQLCOM_UPDATE_MULTI:
  {
    assert(first_table == all_tables && first_table != 0);
    if ((res= update_precheck(session, all_tables)))
      break;

    if ((res= mysql_multi_update_prepare(session)))
      break;

    res= mysql_multi_update(session, all_tables,
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
    if ((res= insert_precheck(session, all_tables)))
      break;

    if (!session->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(session, 0, 1)))
    {
      res= 1;
      break;
    }

    res= mysql_insert(session, all_tables, lex->field_list, lex->many_values,
		      lex->update_list, lex->value_list,
                      lex->duplicates, lex->ignore);

    break;
  }
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  {
    select_result *sel_result;
    assert(first_table == all_tables && first_table != 0);
    if ((res= insert_precheck(session, all_tables)))
      break;

    /* Fix lock for first table */
    if (first_table->lock_type == TL_WRITE_DELAYED)
      first_table->lock_type= TL_WRITE;

    /* Don't unlock tables until command is written to binary log */
    select_lex->options|= SELECT_NO_UNLOCK;

    unit->set_limit(select_lex);

    if (! session->locked_tables &&
        ! (need_start_waiting= ! wait_if_global_read_lock(session, 0, 1)))
    {
      res= 1;
      break;
    }

    if (!(res= open_and_lock_tables(session, all_tables)))
    {
      /* Skip first table, which is the table we are inserting in */
      TableList *second_table= first_table->next_local;
      select_lex->table_list.first= (unsigned char*) second_table;
      select_lex->context.table_list=
        select_lex->context.first_name_resolution_table= second_table;
      res= mysql_insert_select_prepare(session);
      if (!res && (sel_result= new select_insert(first_table,
                                                 first_table->table,
                                                 &lex->field_list,
                                                 &lex->update_list,
                                                 &lex->value_list,
                                                 lex->duplicates,
                                                 lex->ignore)))
      {
	res= handle_select(session, lex, sel_result, OPTION_SETUP_TABLES_DONE);
        /*
          Invalidate the table in the query cache if something changed
          after unlocking when changes become visible.
          TODO: this is workaround. right way will be move invalidating in
          the unlock procedure.
        */
        if (first_table->lock_type ==  TL_WRITE_CONCURRENT_INSERT &&
            session->lock)
        {
          /* INSERT ... SELECT should invalidate only the very first table */
          TableList *save_table= first_table->next_local;
          first_table->next_local= 0;
          first_table->next_local= save_table;
        }
        delete sel_result;
      }
      /* revert changes for SP */
      select_lex->table_list.first= (unsigned char*) first_table;
    }

    break;
  }
  case SQLCOM_TRUNCATE:
    if (! session->endActiveTransaction())
    {
      res= -1;
      break;
    }
    assert(first_table == all_tables && first_table != 0);
    /*
      Don't allow this within a transaction because we want to use
      re-generate table
    */
    if (session->locked_tables || session->inTransaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION, ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }

    res= mysql_truncate(session, first_table, 0);

    break;
  case SQLCOM_DELETE:
  {
    assert(first_table == all_tables && first_table != 0);
    assert(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);

    if (!session->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(session, 0, 1)))
    {
      res= 1;
      break;
    }

    res = mysql_delete(session, all_tables, select_lex->where,
                       &select_lex->order_list,
                       unit->select_limit_cnt, select_lex->options,
                       false);
    break;
  }
  case SQLCOM_DELETE_MULTI:
  {
    assert(first_table == all_tables && first_table != 0);
    TableList *aux_tables=
      (TableList *)session->lex->auxiliary_table_list.first;
    multi_delete *del_result;

    if (!session->locked_tables &&
        !(need_start_waiting= !wait_if_global_read_lock(session, 0, 1)))
    {
      res= 1;
      break;
    }

    if ((res= multi_delete_precheck(session, all_tables)))
      break;

    /* condition will be true on SP re-excuting */
    if (select_lex->item_list.elements != 0)
      select_lex->item_list.empty();
    if (session->add_item_to_list(new Item_null()))
      goto error;

    session->set_proc_info("init");
    if ((res= open_and_lock_tables(session, all_tables)))
      break;

    if ((res= mysql_multi_delete_prepare(session)))
      goto error;

    if (!session->is_fatal_error &&
        (del_result= new multi_delete(aux_tables, lex->table_count)))
    {
      res= mysql_select(session, &select_lex->ref_pointer_array,
			select_lex->get_table_list(),
			select_lex->with_wild,
			select_lex->item_list,
			select_lex->where,
			0, (order_st *)NULL, (order_st *)NULL, (Item *)NULL,
			select_lex->options | session->options | SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK | OPTION_SETUP_TABLES_DONE,
			del_result, unit, select_lex);
      res|= session->is_error();
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
      if (! session->endActiveTransaction())
        goto error;
    }
    else
    {
      /* So that DROP TEMPORARY TABLE gets to binlog at commit/rollback */
      session->options|= OPTION_KEEP_LOG;
    }
    /* DDL and binlog write order protected by LOCK_open */
    res= mysql_rm_table(session, first_table, lex->drop_if_exists, lex->drop_temporary);
  }
  break;
  case SQLCOM_SHOW_PROCESSLIST:
    mysqld_list_processes(session, NULL, lex->verbose);
    break;
  case SQLCOM_SHOW_ENGINE_LOGS:
    {
      res= ha_show_status(session, lex->create_info.db_type, HA_ENGINE_LOGS);
      break;
    }
  case SQLCOM_CHANGE_DB:
  {
    LEX_STRING db_str= { (char *) select_lex->db, strlen(select_lex->db) };

    if (!mysql_change_db(session, &db_str, false))
      session->my_ok();

    break;
  }

  case SQLCOM_LOAD:
  {
    assert(first_table == all_tables && first_table != 0);
    res= mysql_load(session, lex->exchange, first_table, lex->field_list,
                    lex->update_list, lex->value_list, lex->duplicates, lex->ignore);
    break;
  }

  case SQLCOM_SET_OPTION:
  {
    List<set_var_base> *lex_var_list= &lex->var_list;

    if (lex->autocommit && ! session->endActiveTransaction())
      goto error;

    if (open_and_lock_tables(session, all_tables))
      goto error;
    if (!(res= sql_set_variables(session, lex_var_list)))
    {
      session->my_ok();
    }
    else
    {
      /*
        We encountered some sort of error, but no message was sent.
        Send something semi-generic here since we don't know which
        assignment in the list caused the error.
      */
      if (!session->is_error())
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
    unlock_locked_tables(session);
    if (session->options & OPTION_TABLE_LOCK)
    {
      (void) session->endActiveTransaction();
      session->options&= ~(OPTION_TABLE_LOCK);
    }
    if (session->global_read_lock)
      unlock_global_read_lock(session);
    session->my_ok();
    break;
  case SQLCOM_LOCK_TABLES:
    /*
      We try to take transactional locks if
      - only transactional locks are requested (lex->lock_transactional) and
      - no non-transactional locks exist (!session->locked_tables).
    */
    if (lex->lock_transactional && !session->locked_tables)
    {
      int rc;
      /*
        All requested locks are transactional and no non-transactional
        locks exist.
      */
      if ((rc= try_transactional_lock(session, all_tables)) == -1)
        goto error;
      if (rc == 0)
      {
        session->my_ok();
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
    if (check_transactional_lock(session, all_tables))
      goto error;
    unlock_locked_tables(session);
    /* we must end the trasaction first, regardless of anything */
    if (! session->endActiveTransaction())
      goto error;
    session->in_lock_tables=1;
    session->options|= OPTION_TABLE_LOCK;

    if (!(res= simple_open_n_lock_tables(session, all_tables)))
    {
      session->locked_tables=session->lock;
      session->lock=0;
      (void) set_handler_table_locks(session, all_tables, false);
      session->my_ok();
    }
    else
    {
      /*
        Need to end the current transaction, so the storage engine (InnoDB)
        can free its locks if LOCK TABLES locked some tables before finding
        that it can't lock a table in its list
      */
      ha_autocommit_or_rollback(session, 1);
      (void) session->endActiveTransaction();
      session->options&= ~(OPTION_TABLE_LOCK);
    }
    session->in_lock_tables=0;
    break;
  case SQLCOM_CREATE_DB:
  {
    /*
      As mysql_create_db() may modify HA_CREATE_INFO structure passed to
      it, we need to use a copy of LEX::create_info to make execution
      prepared statement- safe.
    */
    HA_CREATE_INFO create_info(lex->create_info);
    if (! session->endActiveTransaction())
    {
      res= -1;
      break;
    }
    char *alias;
    if (!(alias=session->strmake(lex->name.str, lex->name.length)) ||
        check_db_name(&lex->name))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->name.str);
      break;
    }
    res= mysql_create_db(session,(lower_case_table_names == 2 ? alias :
                              lex->name.str), &create_info, 0);
    break;
  }
  case SQLCOM_DROP_DB:
  {
    if (! session->endActiveTransaction())
    {
      res= -1;
      break;
    }
    if (check_db_name(&lex->name))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->name.str);
      break;
    }
    if (session->locked_tables || session->inTransaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION, ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }
    res= mysql_rm_db(session, lex->name.str, lex->drop_if_exists, 0);
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
    if (session->locked_tables || session->inTransaction())
    {
      my_message(ER_LOCK_OR_ACTIVE_TRANSACTION, ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
      goto error;
    }
    res= mysql_alter_db(session, db->str, &create_info);
    break;
  }
  case SQLCOM_SHOW_CREATE_DB:
  {
    if (check_db_name(&lex->name))
    {
      my_error(ER_WRONG_DB_NAME, MYF(0), lex->name.str);
      break;
    }
    res= mysqld_show_create_db(session, lex->name.str, &lex->create_info);
    break;
  }
  case SQLCOM_FLUSH:
  {
    bool write_to_binlog;

    /*
      reload_cache() will tell us if we are allowed to write to the
      binlog or not.
    */
    if (!reload_cache(session, lex->type, first_table, &write_to_binlog))
    {
      /*
        We WANT to write and we CAN write.
        ! we write after unlocking the table.
      */
      /*
        Presumably, RESET and binlog writing doesn't require synchronization
      */
      write_bin_log(session, false, session->query, session->query_length);
      session->my_ok();
    }

    break;
  }
  case SQLCOM_KILL:
  {
    Item *it= (Item *)lex->value_list.head();

    if ((!it->fixed && it->fix_fields(lex->session, &it)) || it->check_cols(1))
    {
      my_message(ER_SET_CONSTANTS_ONLY, ER(ER_SET_CONSTANTS_ONLY),
		 MYF(0));
      goto error;
    }
    sql_kill(session, (ulong)it->val_int(), lex->type & ONLY_KILL_QUERY);
    break;
  }
  case SQLCOM_BEGIN:
    if (session->transaction.xid_state.xa_state != XA_NOTR)
    {
      my_error(ER_XAER_RMFAIL, MYF(0),
               xa_state_names[session->transaction.xid_state.xa_state]);
      break;
    }
    /*
      Breakpoints for backup testing.
    */
    if (! session->startTransaction())
      goto error;
    session->my_ok();
    break;
  case SQLCOM_COMMIT:
    if (! session->endTransaction(lex->tx_release ? COMMIT_RELEASE : lex->tx_chain ? COMMIT_AND_CHAIN : COMMIT))
      goto error;
    session->my_ok();
    break;
  case SQLCOM_ROLLBACK:
    if (! session->endTransaction(lex->tx_release ? ROLLBACK_RELEASE : lex->tx_chain ? ROLLBACK_AND_CHAIN : ROLLBACK))
      goto error;
    session->my_ok();
    break;
  case SQLCOM_RELEASE_SAVEPOINT:
  {
    SAVEPOINT *sv;
    for (sv=session->transaction.savepoints; sv; sv=sv->prev)
    {
      if (my_strnncoll(system_charset_info,
                       (unsigned char *)lex->ident.str, lex->ident.length,
                       (unsigned char *)sv->name, sv->length) == 0)
        break;
    }
    if (sv)
    {
      if (ha_release_savepoint(session, sv))
        res= true; // cannot happen
      else
        session->my_ok();
      session->transaction.savepoints=sv->prev;
    }
    else
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SAVEPOINT", lex->ident.str);
    break;
  }
  case SQLCOM_ROLLBACK_TO_SAVEPOINT:
  {
    SAVEPOINT *sv;
    for (sv=session->transaction.savepoints; sv; sv=sv->prev)
    {
      if (my_strnncoll(system_charset_info,
                       (unsigned char *)lex->ident.str, lex->ident.length,
                       (unsigned char *)sv->name, sv->length) == 0)
        break;
    }
    if (sv)
    {
      if (ha_rollback_to_savepoint(session, sv))
        res= true; // cannot happen
      else
      {
        if ((session->options & OPTION_KEEP_LOG) || session->transaction.all.modified_non_trans_table)
          push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                       ER_WARNING_NOT_COMPLETE_ROLLBACK,
                       ER(ER_WARNING_NOT_COMPLETE_ROLLBACK));
        session->my_ok();
      }
      session->transaction.savepoints=sv;
    }
    else
      my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "SAVEPOINT", lex->ident.str);
    break;
  }
  case SQLCOM_SAVEPOINT:
    if (!(session->options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
      session->my_ok();
    else
    {
      SAVEPOINT **sv, *newsv;
      for (sv=&session->transaction.savepoints; *sv; sv=&(*sv)->prev)
      {
        if (my_strnncoll(system_charset_info,
                         (unsigned char *)lex->ident.str, lex->ident.length,
                         (unsigned char *)(*sv)->name, (*sv)->length) == 0)
          break;
      }
      if (*sv) /* old savepoint of the same name exists */
      {
        newsv=*sv;
        ha_release_savepoint(session, *sv); // it cannot fail
        *sv=(*sv)->prev;
      }
      else if ((newsv=(SAVEPOINT *) alloc_root(&session->transaction.mem_root,
                                               savepoint_alloc_size)) == 0)
      {
        my_error(ER_OUT_OF_RESOURCES, MYF(0));
        break;
      }
      newsv->name=strmake_root(&session->transaction.mem_root,
                               lex->ident.str, lex->ident.length);
      newsv->length=lex->ident.length;
      /*
        if we'll get an error here, don't add new savepoint to the list.
        we'll lose a little bit of memory in transaction mem_root, but it'll
        be free'd when transaction ends anyway
      */
      if (ha_savepoint(session, newsv))
        res= true;
      else
      {
        newsv->prev=session->transaction.savepoints;
        session->transaction.savepoints=newsv;
        session->my_ok();
      }
    }
    break;
  default:
    assert(0);                             /* Impossible */
    session->my_ok();
    break;
  }
  session->set_proc_info("query end");

  /*
    The return value for ROW_COUNT() is "implementation dependent" if the
    statement is not DELETE, INSERT or UPDATE, but -1 is what JDBC and ODBC
    wants. We also keep the last value in case of SQLCOM_CALL or
    SQLCOM_EXECUTE.
  */
  if (!(sql_command_flags[lex->sql_command].test(CF_BIT_HAS_ROW_COUNT)))
    session->row_count_func= -1;

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
    start_waiting_global_read_lock(session);
  }
  return(res || session->is_error());
}

bool execute_sqlcom_select(Session *session, TableList *all_tables)
{
  LEX	*lex= session->lex;
  select_result *result=lex->result;
  bool res= false;
  /* assign global limit variable if limit is not given */
  {
    Select_Lex *param= lex->unit.global_parameters;
    if (!param->explicit_limit)
      param->select_limit=
        new Item_int((uint64_t) session->variables.select_limit);
  }
  if (!(res= open_and_lock_tables(session, all_tables)))
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
        return true;                               /* purecov: inspected */
      session->send_explain_fields(result);
      res= mysql_explain_union(session, &session->lex->unit, result);
      if (lex->describe & DESCRIBE_EXTENDED)
      {
        char buff[1024];
        String str(buff,(uint32_t) sizeof(buff), system_charset_info);
        str.length(0);
        session->lex->unit.print(&str, QT_ORDINARY);
        str.append('\0');
        push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_NOTE,
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
        return true;                               /* purecov: inspected */
      res= handle_select(session, lex, result, 0);
      if (result != lex->result)
        delete result;
    }
  }
  return res;
}


#define MY_YACC_INIT 1000			// Start with big alloc
#define MY_YACC_MAX  32000			// Because of 'short'

bool my_yyoverflow(short **yyss, YYSTYPE **yyvs, ulong *yystacksize)
{
  LEX	*lex= current_session->lex;
  ulong old_info=0;
  if ((uint32_t) *yystacksize >= MY_YACC_MAX)
    return 1;
  if (!lex->yacc_yyvs)
    old_info= *yystacksize;
  *yystacksize= set_zone((*yystacksize)*2,MY_YACC_INIT,MY_YACC_MAX);
  unsigned char *tmpptr= NULL;
  if (!(tmpptr= (unsigned char *)realloc(lex->yacc_yyvs,
                                         *yystacksize* sizeof(**yyvs))))
      return 1;
  lex->yacc_yyvs= tmpptr;
  tmpptr= NULL;
  if (!(tmpptr= (unsigned char*)realloc(lex->yacc_yyss,
                                        *yystacksize* sizeof(**yyss))))
      return 1;
  lex->yacc_yyss= tmpptr;
  if (old_info)
  {						// Copy old info from stack
    memcpy(lex->yacc_yyss, *yyss, old_info*sizeof(**yyss));
    memcpy(lex->yacc_yyvs, *yyvs, old_info*sizeof(**yyvs));
  }
  *yyss=(short*) lex->yacc_yyss;
  *yyvs=(YYSTYPE*) lex->yacc_yyvs;
  return 0;
}


void
mysql_init_select(LEX *lex)
{
  Select_Lex *select_lex= lex->current_select;
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
  Select_Lex *select_lex;
  Session *session= lex->session;

  if (!(select_lex= new (session->mem_root) Select_Lex()))
    return(1);
  select_lex->select_number= ++session->select_number;
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
    Select_Lex_Unit *unit;
    lex->subqueries= true;
    /* first select_lex of subselect or derived table */
    if (!(unit= new (session->mem_root) Select_Lex_Unit()))
      return(1);

    unit->init_query();
    unit->init_select();
    unit->session= session;
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
      my_error(ER_WRONG_USAGE, MYF(0), "UNION", "order_st BY");
      return(1);
    }
    select_lex->include_neighbour(lex->current_select);
    Select_Lex_Unit *unit= select_lex->master_unit();
    if (!unit->fake_select_lex && unit->add_fake_select_lex(lex->session))
      return(1);
    select_lex->context.outer_context=
                unit->first_select()->context.outer_context;
  }

  select_lex->master_unit()->global_parameters= select_lex;
  select_lex->include_global((Select_Lex_Node**)&lex->all_selects_list);
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
  Session *session;
  LEX *lex;
  LEX_STRING tmp, null_lex_string;
  Item *var;
  char buff[MAX_SYS_VAR_LENGTH*2+4+8];
  char *end= buff;

  session= current_session;
  lex= session->lex;
  mysql_init_select(lex);
  lex->sql_command= SQLCOM_SELECT;
  tmp.str= (char*) var_name;
  tmp.length=strlen(var_name);
  memset(&null_lex_string.str, 0, sizeof(null_lex_string));
  /*
    We set the name of Item to @@session.var_name because that then is used
    as the column name in the output.
  */
  if ((var= get_system_var(session, OPT_SESSION, tmp, null_lex_string)))
  {
    end+= sprintf(buff, "@@session.%s", var_name);
    var->set_name(buff, end-buff, system_charset_info);
    session->add_item_to_list(var);
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
  lex->lock_option= TL_READ;
  lex->query_tables= 0;
  lex->query_tables_last= &lex->query_tables;
}


/**
  Parse a query.

  @param       session     Current thread
  @param       inBuf   Begining of the query text
  @param       length  Length of the query text
  @param[out]  found_semicolon For multi queries, position of the character of
                               the next query in the query text.
*/

void mysql_parse(Session *session, const char *inBuf, uint32_t length,
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
    of (among others) lex->safe_to_cache_query and session->server_status,
    which are reset respectively in
    - lex_start()
    - mysql_reset_session_for_next_command()
    So, initializing the lexical analyser *before* using the query cache
    is required for the cache to work properly.
    FIXME: cleanup the dependencies in the code to simplify this.
  */
  lex_start(session);
  session->reset_for_next_command();

  {
    LEX *lex= session->lex;

    Lex_input_stream lip(session, inBuf, length);

    bool err= parse_sql(session, &lip);
    *found_semicolon= lip.found_semicolon;

    if (!err)
    {
      {
	if (! session->is_error())
	{
          /*
            Binlog logs a string starting from session->query and having length
            session->query_length; so we set session->query_length correctly (to not
            log several statements in one event, when we executed only first).
            We set it to not see the ';' (otherwise it would get into binlog
            and Query_log_event::print() would give ';;' output).
            This also helps display only the current query in SHOW
            PROCESSLIST.
            Note that we don't need LOCK_thread_count to modify query_length.
          */
          if (*found_semicolon &&
              (session->query_length= (ulong)(*found_semicolon - session->query)))
            session->query_length--;
          /* Actually execute the query */
          mysql_execute_command(session);
	}
      }
    }
    else
    {
      assert(session->is_error());
    }
    lex->unit.cleanup();
    session->set_proc_info("freeing items");
    session->end_statement();
    session->cleanup_after_query();
  }

  return;
}



/**
  Store field definition for create.

  @return
    Return 0 if ok
*/

bool add_field_to_list(Session *session, LEX_STRING *field_name, enum_field_types type,
		       char *length, char *decimals,
		       uint32_t type_modifier,
                       enum column_format_type column_format,
		       Item *default_value, Item *on_update_value,
                       LEX_STRING *comment,
		       char *change,
                       List<String> *interval_list, const CHARSET_INFO * const cs)
{
  register Create_field *new_field;
  LEX  *lex= session->lex;

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
         type == DRIZZLE_TYPE_TIMESTAMP))
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

  if (on_update_value && type != DRIZZLE_TYPE_TIMESTAMP)
  {
    my_error(ER_INVALID_ON_UPDATE, MYF(0), field_name->str);
    return(1);
  }

  if (!(new_field= new Create_field()) ||
      new_field->init(session, field_name->str, type, length, decimals, type_modifier,
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
  current_session->lex->last_field->after=const_cast<char*> (name);
}

/**
  save order by and tables in own lists.
*/

bool add_to_list(Session *session, SQL_LIST &list,Item *item,bool asc)
{
  order_st *order;
  if (!(order = (order_st *) session->alloc(sizeof(order_st))))
    return(1);
  order->item_ptr= item;
  order->item= &order->item_ptr;
  order->asc = asc;
  order->free_me=0;
  order->used=0;
  order->counter_used= 0;
  list.link_in_list((unsigned char*) order,(unsigned char**) &order->next);
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
    \#	Pointer to TableList element added to the total table list
*/

TableList *Select_Lex::add_table_to_list(Session *session,
					     Table_ident *table,
					     LEX_STRING *alias,
					     uint32_t table_options,
					     thr_lock_type lock_type,
					     List<Index_hint> *index_hints_arg,
                                             LEX_STRING *option)
{
  register TableList *ptr;
  TableList *previous_table_ref; /* The table preceding the current one. */
  char *alias_str;
  LEX *lex= session->lex;

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
    if (!(alias_str= (char*) session->memdup(alias_str,table->table.length+1)))
      return(0);
  }
  if (!(ptr = (TableList *) session->calloc(sizeof(TableList))))
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
                                      INFORMATION_SCHEMA_NAME.c_str()))
  {
    InfoSchemaTable *schema_table= find_schema_table(session, ptr->table_name);
    if (!schema_table ||
        (schema_table->hidden &&
         ((sql_command_flags[lex->sql_command].test(CF_BIT_STATUS_COMMAND)) == 0 ||
          /*
            this check is used for show columns|keys from I_S hidden table
          */
          lex->sql_command == SQLCOM_SHOW_FIELDS ||
          lex->sql_command == SQLCOM_SHOW_KEYS)))
    {
      my_error(ER_UNKNOWN_TABLE, MYF(0),
               ptr->table_name, INFORMATION_SCHEMA_NAME.c_str());
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
    TableList *first_table= (TableList*) table_list.first;
    for (TableList *tables= first_table ;
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
      table_list.next points to the last inserted TableList->next_local'
      element
      We don't use the offsetof() macro here to avoid warnings from gcc
    */
    previous_table_ref= (TableList*) ((char*) table_list.next -
                                       ((char*) &(ptr->next_local) -
                                        (char*) ptr));
    /*
      Set next_name_resolution_table of the previous table reference to point
      to the current table reference. In effect the list
      TableList::next_name_resolution_table coincides with
      TableList::next_local. Later this may be changed in
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
  table_list.link_in_list((unsigned char*) ptr, (unsigned char**) &ptr->next_local);
  ptr->next_name_resolution_table= NULL;
  /* Link table in global list (all used tables) */
  lex->add_to_query_tables(ptr);
  return(ptr);
}


/**
  Initialize a new table list for a nested join.

    The function initializes a structure of the TableList type
    for a nested join. It sets up its nested join list as empty.
    The created structure is added to the front of the current
    join list in the Select_Lex object. Then the function
    changes the current nest level for joins to refer to the newly
    created empty list after having saved the info on the old level
    in the initialized structure.

  @param session         current thread

  @retval
    0   if success
  @retval
    1   otherwise
*/

bool Select_Lex::init_nested_join(Session *session)
{
  TableList *ptr;
  nested_join_st *nested_join;

  if (!(ptr= (TableList*) session->calloc(ALIGN_SIZE(sizeof(TableList))+
                                       sizeof(nested_join_st))))
    return(1);
  nested_join= ptr->nested_join=
    ((nested_join_st*) ((unsigned char*) ptr + ALIGN_SIZE(sizeof(TableList))));

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

  @param session         current thread

  @return
    - Pointer to TableList element added to the total table list, if success
    - 0, otherwise
*/

TableList *Select_Lex::end_nested_join(Session *)
{
  TableList *ptr;
  nested_join_st *nested_join;

  assert(embedding);
  ptr= embedding;
  join_list= ptr->join_list;
  embedding= ptr->embedding;
  nested_join= ptr->nested_join;
  if (nested_join->join_list.elements == 1)
  {
    TableList *embedded= nested_join->join_list.head();
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

  @param session         current thread

  @retval
    0  Error
  @retval
    \#  Pointer to TableList element created for the new nested join
*/

TableList *Select_Lex::nest_last_join(Session *session)
{
  TableList *ptr;
  nested_join_st *nested_join;
  List<TableList> *embedded_list;

  if (!(ptr= (TableList*) session->calloc(ALIGN_SIZE(sizeof(TableList))+
                                       sizeof(nested_join_st))))
    return(0);
  nested_join= ptr->nested_join=
    ((nested_join_st*) ((unsigned char*) ptr + ALIGN_SIZE(sizeof(TableList))));

  ptr->embedding= embedding;
  ptr->join_list= join_list;
  ptr->alias= (char*) "(nest_last_join)";
  embedded_list= &nested_join->join_list;
  embedded_list->empty();

  for (uint32_t i=0; i < 2; i++)
  {
    TableList *table= join_list->pop();
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
    of Select_Lex object.
    Thus, joined tables are put into this list in the reverse order
    (the most outer join operation follows first).

  @param table       the table to add

  @return
    None
*/

void Select_Lex::add_joined_table(TableList *table)
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

  @param session         current thread

  @return
    - Pointer to the table representing the inner table, if success
    - 0, otherwise
*/

TableList *Select_Lex::convert_right_join()
{
  TableList *tab2= join_list->pop();
  TableList *tab1= join_list->pop();

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

void Select_Lex::set_lock_for_tables(thr_lock_type lock_type)
{
  bool for_update= lock_type >= TL_READ_NO_INSERT;

  for (TableList *tables= (TableList*) table_list.first;
       tables;
       tables= tables->next_local)
  {
    tables->lock_type= lock_type;
    tables->updating=  for_update;
  }
  return;
}


/**
  Create a fake Select_Lex for a unit.

    The method create a fake Select_Lex object for a unit.
    This object is created for any union construct containing a union
    operation and also for any single select union construct of the form
    @verbatim
    (SELECT ... order_st BY order_list [LIMIT n]) order_st BY ...
    @endvarbatim
    or of the form
    @varbatim
    (SELECT ... order_st BY LIMIT n) order_st BY ...
    @endvarbatim

  @param session_arg		   thread handle

  @note
    The object is used to retrieve rows from the temporary table
    where the result on the union is obtained.

  @retval
    1     on failure to create the object
  @retval
    0     on success
*/

bool Select_Lex_Unit::add_fake_select_lex(Session *session_arg)
{
  Select_Lex *first_sl= first_select();
  assert(!fake_select_lex);

  if (!(fake_select_lex= new (session_arg->mem_root) Select_Lex()))
      return(1);
  fake_select_lex->include_standalone(this,
                                      (Select_Lex_Node**)&fake_select_lex);
  fake_select_lex->select_number= INT_MAX;
  fake_select_lex->parent_lex= session_arg->lex; /* Used in init_query. */
  fake_select_lex->make_empty_select();
  fake_select_lex->linkage= GLOBAL_OPTIONS_TYPE;
  fake_select_lex->select_limit= 0;

  fake_select_lex->context.outer_context=first_sl->context.outer_context;
  /* allow item list resolving in fake select for order_st BY */
  fake_select_lex->context.resolve_in_select_list= true;
  fake_select_lex->context.select_lex= fake_select_lex;

  if (!is_union())
  {
    /*
      This works only for
      (SELECT ... order_st BY list [LIMIT n]) order_st BY order_list [LIMIT m],
      (SELECT ... LIMIT n) order_st BY order_list [LIMIT m]
      just before the parser starts processing order_list
    */
    global_parameters= fake_select_lex;
    fake_select_lex->no_table_names_allowed= 1;
    session_arg->lex->current_select= fake_select_lex;
  }
  session_arg->lex->pop_context();
  return(0);
}


/**
  Push a new name resolution context for a JOIN ... ON clause to the
  context stack of a query block.

    Create a new name resolution context for a JOIN ... ON clause,
    set the first and last leaves of the list of table references
    to be used for name resolution, and push the newly created
    context to the stack of contexts of the query.

  @param session       pointer to current thread
  @param left_op   left  operand of the JOIN
  @param right_op  rigth operand of the JOIN

  @retval
    false  if all is OK
  @retval
    true   if a memory allocation error occured
*/

bool
push_new_name_resolution_context(Session *session,
                                 TableList *left_op, TableList *right_op)
{
  Name_resolution_context *on_context;
  if (!(on_context= new (session->mem_root) Name_resolution_context))
    return true;
  on_context->init();
  on_context->first_name_resolution_table=
    left_op->first_leaf_for_name_resolution();
  on_context->last_name_resolution_table=
    right_op->last_leaf_for_name_resolution();
  return session->lex->push_context(on_context);
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

void add_join_on(TableList *b, Item *expr)
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

void add_join_natural(TableList *a, TableList *b, List<String> *using_fields,
                      Select_Lex *lex)
{
  b->natural_join= a;
  lex->prev_join_using= using_fields;
}


/**
  Reload/resets privileges and the different caches.

  @param session Thread handler (can be NULL!)
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
    @retval !=0  Error; session->killed is set or session->is_error() is true
*/

bool reload_cache(Session *session, ulong options, TableList *tables, bool *write_to_binlog)
{
  bool result=0;
  select_errors=0;				/* Write if more errors */
  bool tmp_write_to_binlog= 1;

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

    if (ha_flush_logs(NULL))
      result=1;
  }
  /*
    Note that if REFRESH_READ_LOCK bit is set then REFRESH_TABLES is set too
    (see sql_yacc.yy)
  */
  if (options & (REFRESH_TABLES | REFRESH_READ_LOCK))
  {
    if ((options & REFRESH_READ_LOCK) && session)
    {
      /*
        We must not try to aspire a global read lock if we have a write
        locked table. This would lead to a deadlock when trying to
        reopen (and re-lock) the table after the flush.
      */
      if (session->locked_tables)
      {
        THR_LOCK_DATA **lock_p= session->locked_tables->locks;
        THR_LOCK_DATA **end_p= lock_p + session->locked_tables->lock_count;

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
      if (lock_global_read_lock(session))
	return 1;                               // Killed
      result= close_cached_tables(session, tables, false, (options & REFRESH_FAST) ?
                                  false : true, true);
      if (make_global_read_lock_block_commit(session)) // Killed
      {
        /* Don't leave things in a half-locked state */
        unlock_global_read_lock(session);
        return 1;
      }
    }
    else
      result= close_cached_tables(session, tables, false, (options & REFRESH_FAST) ?
                                  false : true, false);
  }
  if (session && (options & REFRESH_STATUS))
    session->refresh_status();
 *write_to_binlog= tmp_write_to_binlog;

 return result;
}


/**
  kill on thread.

  @param session			Thread class
  @param id			Thread id
  @param only_kill_query        Should it kill the query or the connection

  @note
    This is written such that we have a short lock on LOCK_thread_count
*/

static unsigned int
kill_one_thread(Session *, ulong id, bool only_kill_query)
{
  Session *tmp;
  uint32_t error=ER_NO_SUCH_THREAD;
  pthread_mutex_lock(&LOCK_thread_count); // For unlink from list
  I_List_iterator<Session> it(session_list);
  while ((tmp=it++))
  {
    if (tmp->thread_id == id)
    {
      pthread_mutex_lock(&tmp->LOCK_delete);	// Lock from delete
      break;
    }
  }
  pthread_mutex_unlock(&LOCK_thread_count);
  if (tmp)
  {
    tmp->awake(only_kill_query ? Session::KILL_QUERY : Session::KILL_CONNECTION);
    error=0;
    pthread_mutex_unlock(&tmp->LOCK_delete);
  }
  return(error);
}


/*
  kills a thread and sends response

  SYNOPSIS
    sql_kill()
    session			Thread class
    id			Thread id
    only_kill_query     Should it kill the query or the connection
*/

void sql_kill(Session *session, ulong id, bool only_kill_query)
{
  uint32_t error;
  if (!(error= kill_one_thread(session, id, only_kill_query)))
    session->my_ok();
  else
    my_error(error, MYF(0), id);
}


/** If pointer is not a null pointer, append filename to it. */

bool append_file_to_dir(Session *session, const char **filename_ptr,
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
  strcpy(buff,*filename_ptr);
  end=convert_dirname(buff, *filename_ptr, NULL);
  if (!(ptr= (char*) session->alloc((size_t) (end-buff) + strlen(table_name)+1)))
    return 1;					// End of memory
  *filename_ptr=ptr;
  sprintf(ptr,"%s%s",buff,table_name);
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
  Session *session= current_session;
  LEX *lex= session->lex;
  if (lex->current_select != &lex->select_lex)
  {
    char command[80];
    Lex_input_stream *lip= session->m_lip;
    strncpy(command, lip->yylval->symbol.str,
            cmin(lip->yylval->symbol.length, sizeof(command)-1));
    command[cmin(lip->yylval->symbol.length, sizeof(command)-1)]=0;
    my_error(ER_CANT_USE_OPTION_HERE, MYF(0), command);
    return 1;
  }
  return 0;
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
                                Select_Lex *select_lex)
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
  Update query pre-check.

  @param session		Thread handler
  @param tables	Global/local table list (have to be the same)

  @retval
    false OK
  @retval
    true  Error
*/

bool update_precheck(Session *session, TableList *)
{
  const char *msg= 0;
  LEX *lex= session->lex;
  Select_Lex *select_lex= &lex->select_lex;

  if (session->lex->select_lex.item_list.elements != session->lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    return(true);
  }

  if (session->lex->select_lex.table_list.elements > 1)
  {
    if (select_lex->order_list.elements)
      msg= "ORDER BY";
    else if (select_lex->select_limit)
      msg= "LIMIT";
    if (msg)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", msg);
      return(true);
    }
  }
  return(false);
}

/**
  Multi delete query pre-check.

  @param session			Thread handler
  @param tables		Global/local table list

  @retval
    false OK
  @retval
    true  error
*/

bool multi_delete_precheck(Session *session, TableList *)
{
  Select_Lex *select_lex= &session->lex->select_lex;
  TableList **save_query_tables_own_last= session->lex->query_tables_own_last;

  session->lex->query_tables_own_last= 0;
  session->lex->query_tables_own_last= save_query_tables_own_last;

  if ((session->options & OPTION_SAFE_UPDATES) && !select_lex->where)
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

static TableList *multi_delete_table_match(LEX *, TableList *tbl,
                                           TableList *tables)
{
  TableList *match= NULL;

  for (TableList *elem= tables; elem; elem= elem->next_local)
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
  TableList *tables= (TableList*)lex->select_lex.table_list.first;
  TableList *target_tbl;

  lex->table_count= 0;

  for (target_tbl= (TableList *)lex->auxiliary_table_list.first;
       target_tbl; target_tbl= target_tbl->next_local)
  {
    lex->table_count++;
    /* All tables in aux_tables must be found in FROM PART */
    TableList *walk= multi_delete_table_match(lex, target_tbl, tables);
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
  simple INSERT query pre-check.

  @param session		Thread handler
  @param tables	Global table list

  @retval
    false  OK
  @retval
    true   error
*/

bool insert_precheck(Session *session, TableList *)
{
  LEX *lex= session->lex;

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

  @param session			Thread handler
  @param tables		Global table list
  @param create_table	        Table which will be created

  @retval
    false   OK
  @retval
    true   Error
*/

bool create_table_precheck(Session *, TableList *,
                           TableList *create_table)
{
  bool error= true;                                 // Error message is given

  if (create_table && (strcmp(create_table->db, "information_schema") == 0))
  {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), "", "", INFORMATION_SCHEMA_NAME.c_str());
    return(true);
  }

  error= false;

  return(error);
}


/**
  negate given expression.

  @param session  thread handler
  @param expr expression for negation

  @return
    negated expression
*/

Item *negate_expression(Session *session, Item *expr)
{
  Item *negated;
  if (expr->type() == Item::FUNC_ITEM &&
      ((Item_func *) expr)->functype() == Item_func::NOT_FUNC)
  {
    /* it is NOT(NOT( ... )) */
    Item *arg= ((Item_func *) expr)->arguments()[0];
    enum_parsing_place place= session->lex->current_select->parsing_place;
    if (arg->is_bool_func() || place == IN_WHERE || place == IN_HAVING)
      return arg;
    /*
      if it is not boolean function then we have to emulate value of
      not(not(a)), it will be a != 0
    */
    return new Item_func_ne(arg, new Item_int((char*) "0", 0, 1));
  }

  if ((negated= expr->neg_transformer(session)) != 0)
    return negated;
  return new Item_func_not(expr);
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
                              uint32_t max_char_length, const CHARSET_INFO * const cs,
                              bool no_error)
{
  int well_formed_error;
  uint32_t res= cs->cset->well_formed_len(cs, str->str, str->str + str->length,
                                      max_char_length, &well_formed_error);

  if (!well_formed_error &&  str->length == res)
    return false;

  if (!no_error)
    my_error(ER_WRONG_STRING_LENGTH, MYF(0), str->str, err_msg, max_char_length);
  return true;
}


bool check_identifier_name(LEX_STRING *str, uint32_t err_code,
                           uint32_t max_char_length,
                           const char *param_for_err_msg)
{
  /*
    We don't support non-BMP characters in identifiers at the moment,
    so they should be prohibited until such support is done.
    This is why we use the 3-byte utf8 to check well-formedness here.
  */
  const CHARSET_INFO * const cs= &my_charset_utf8mb4_general_ci;

  int well_formed_error;
  uint32_t res= cs->cset->well_formed_len(cs, str->str, str->str + str->length,
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
  uint32_t dir_len, home_dir_len= strlen(drizzle_unpacked_real_data_home);

  if (!dir)
    return(0);

  (void) fn_format(path, dir, "", "",
                   (MY_RETURN_REAL_PATH|MY_RESOLVE_SYMLINKS));
  dir_len= unpack_dirname(conv_path, dir);

  if (home_dir_len < dir_len)
  {
    if (!my_strnncoll(character_set_filesystem,
                      (const unsigned char*) conv_path, home_dir_len,
                      (const unsigned char*) drizzle_unpacked_real_data_home,
                      home_dir_len))
      return(1);
  }
  return(0);
}


extern int DRIZZLEparse(void *session); // from sql_yacc.cc


/**
  This is a wrapper of DRIZZLEparse(). All the code should call parse_sql()
  instead of DRIZZLEparse().

  @param session Thread context.
  @param lip Lexer context.

  @return Error status.
    @retval false on success.
    @retval true on parsing error.
*/

bool parse_sql(Session *session, Lex_input_stream *lip)
{
  assert(session->m_lip == NULL);

  /* Set Lex_input_stream. */

  session->m_lip= lip;

  /* Parse the query. */

  bool mysql_parse_status= DRIZZLEparse(session) != 0;

  /* Check that if DRIZZLEparse() failed, session->is_error() is set. */

  assert(!mysql_parse_status || session->is_error());

  /* Reset Lex_input_stream. */

  session->m_lip= NULL;

  /* That's it. */

  return mysql_parse_status || session->is_fatal_error;
}

/**
  @} (end of group Runtime_Environment)
*/
