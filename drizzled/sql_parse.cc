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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>

#define DRIZZLE_LEX 1

#include <drizzled/item/num.h>
#include <drizzled/abort_exception.h>
#include <drizzled/error.h>
#include <drizzled/nested_join.h>
#include <drizzled/query_id.h>
#include <drizzled/transaction_services.h>
#include <drizzled/sql_parse.h>
#include <drizzled/data_home.h>
#include <drizzled/sql_base.h>
#include <drizzled/show.h>
#include <drizzled/function/time/unix_timestamp.h>
#include <drizzled/function/get_system_var.h>
#include <drizzled/item/cmpfunc.h>
#include <drizzled/item/null.h>
#include <drizzled/session.h>
#include <drizzled/session/cache.h>
#include <drizzled/sql_load.h>
#include <drizzled/lock.h>
#include <drizzled/select_send.h>
#include <drizzled/plugin/client.h>
#include <drizzled/statement.h>
#include <drizzled/statement/alter_table.h>
#include <drizzled/probes.h>
#include <drizzled/charset.h>
#include <drizzled/plugin/logging.h>
#include <drizzled/plugin/query_rewrite.h>
#include <drizzled/plugin/query_cache.h>
#include <drizzled/plugin/authorization.h>
#include <drizzled/optimizer/explain_plan.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/plugin/event_observer.h>
#include <drizzled/display.h>
#include <drizzled/visibility.h>
#include <drizzled/kill.h>
#include <drizzled/schema.h>
#include <drizzled/item/subselect.h>
#include <drizzled/diagnostics_area.h>
#include <drizzled/table_ident.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/system_variables.h>
#include <drizzled/session/times.h>
#include <drizzled/session/transactions.h>

#include <limits.h>

#include <bitset>
#include <algorithm>
#include <boost/date_time.hpp>
#include <drizzled/internal/my_sys.h>

using namespace std;

extern int base_sql_parse(drizzled::Session *session); // from sql_yacc.cc

namespace drizzled {

/* Prototypes */
bool my_yyoverflow(short **a, ParserType **b, ulong *yystacksize);
static bool parse_sql(Session *session, Lex_input_stream *lip);
void parse(Session&, const char *inBuf, uint32_t length);

/**
  @defgroup Runtime_Environment Runtime Environment
  @{
*/

extern size_t my_thread_stack_size;
extern const charset_info_st *character_set_filesystem;

namespace
{

static const std::string command_name[COM_END+1]={
  "Sleep",
  "Quit",
  "Init DB",
  "Query",
  "Shutdown",
  "Connect",
  "Ping",
  "Kill",
  "Error"  // Last command number
};

}

const char *xa_state_names[]={
  "NON-EXISTING", "ACTIVE", "IDLE", "PREPARED"
};

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

const std::string &getCommandName(const enum_server_command& command)
{
  return command_name[command];
}

void init_update_queries(void)
{
  for (uint32_t x= uint32_t(SQLCOM_SELECT); 
       x <= uint32_t(SQLCOM_END); x++)
  {
    sql_command_flags[x].reset();
  }

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
  sql_command_flags[SQLCOM_INSERT]=	    CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_INSERT_SELECT]=  CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_DELETE]=         CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_REPLACE]=        CF_CHANGES_DATA | CF_HAS_ROW_COUNT;
  sql_command_flags[SQLCOM_REPLACE_SELECT]= CF_CHANGES_DATA | CF_HAS_ROW_COUNT;

  sql_command_flags[SQLCOM_SHOW_WARNS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ERRORS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_DB]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE]=  CF_STATUS_COMMAND;

  /*
    The following admin table operations are allowed
    on log tables.
  */
  sql_command_flags[SQLCOM_ANALYZE]=          CF_WRITE_LOGS_COMMAND;
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
    set session->lex().sql_command to SQLCOM_END here.
  @todo
    The following has to be changed to an 8 byte integer

  @retval
    0   ok
  @retval
    1   request of thread shutdown, i. e. if command is
        COM_QUIT/COM_SHUTDOWN
*/
bool dispatch_command(enum_server_command command, Session *session,
                      const char* packet, uint32_t packet_length)
{
  bool error= false;
  Query_id &query_id= Query_id::get_query_id();

  DRIZZLE_COMMAND_START(session->thread_id, command);

  session->command= command;
  session->lex().sql_command= SQLCOM_END; /* to avoid confusing VIEW detectors */
  session->times.set_time();
  session->setQueryId(query_id.value());

  switch (command)
  {
  /* Ignore these statements. */
  case COM_PING:
    break;

  /* Increase id and count all other statements. */
  default:
    session->status_var.questions++;
    query_id.next();
  }

  /* @todo set session->lex().sql_command to SQLCOM_END here */

  plugin::Logging::preDo(session);
  if (unlikely(plugin::EventObserver::beforeStatement(*session)))
  {
    // We should do something about an error...
  }

  session->server_status&=
           ~(SERVER_QUERY_NO_INDEX_USED | SERVER_QUERY_NO_GOOD_INDEX_USED);

  switch (command)
  {
  case COM_USE_SCHEMA:
    {
      if (packet_length == 0)
      {
        my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));
        break;
      }
      if (not schema::change(*session, identifier::Schema(string(packet, packet_length))))
      {
        session->my_ok();
      }
      break;
    }

  case COM_QUERY:
    {
      session->readAndStoreQuery(packet, packet_length);
      DRIZZLE_QUERY_START(session->getQueryString()->c_str(), session->thread_id, session->schema()->c_str());
      parse(*session, session->getQueryString()->c_str(), session->getQueryString()->length());
      break;
    }

  case COM_QUIT:
    /* We don't calculate statistics for this command */
    session->main_da().disable_status();              // Don't send anything back
    error= true;					// End server
    break;

  case COM_KILL:
    {
      if (packet_length != 4)
      {
        my_error(ER_NO_SUCH_THREAD, MYF(0), 0);
        break;
      }
      else
      {
        uint32_t kill_id;
        memcpy(&kill_id, packet, sizeof(uint32_t));

        kill_id= ntohl(kill_id);
        (void)drizzled::kill(*session->user(), kill_id, true);
      }
      session->my_ok();
      break;
    }

  case COM_SHUTDOWN:
    {
      session->status_var.com_other++;
      session->my_eof();
      session->close_thread_tables();			// Free before kill
      kill_drizzle();
      error= true;
      break;
    }

  case COM_PING:
    session->status_var.com_other++;
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
  session->main_da().can_overwrite_status= true;
  TransactionServices::autocommitOrRollback(*session, session->is_error());
  session->main_da().can_overwrite_status= false;

  session->transaction.stmt.reset();


  /* report error issued during command execution */
  if (session->killed_errno())
  {
    if (not session->main_da().is_set())
      session->send_kill_message();
  }
  if (session->getKilled() == Session::KILL_QUERY || session->getKilled() == Session::KILL_BAD_DATA)
  {
    session->setKilled(Session::NOT_KILLED);
    session->setAbort(false);
  }

  /* Can not be true, but do not take chances in production. */
  assert(! session->main_da().is_sent);

  switch (session->main_da().status())
  {
  case Diagnostics_area::DA_ERROR:
    /* The query failed, send error to log and abort bootstrap. */
    session->getClient()->sendError(session->main_da().sql_errno(),
                               session->main_da().message());
    break;

  case Diagnostics_area::DA_EOF:
    session->getClient()->sendEOF();
    break;

  case Diagnostics_area::DA_OK:
    session->getClient()->sendOK();
    break;

  case Diagnostics_area::DA_DISABLED:
    break;

  case Diagnostics_area::DA_EMPTY:
  default:
    session->getClient()->sendOK();
    break;
  }

  session->main_da().is_sent= true;

  session->set_proc_info("closing tables");
  /* Free tables */
  session->close_thread_tables();

  plugin::Logging::postDo(session);
  if (unlikely(plugin::EventObserver::afterStatement(*session)))
  {
    // We should do something about an error...
  }

  /* Store temp state for processlist */
  session->set_proc_info("cleaning up");
  session->command= COM_SLEEP;
  session->resetQueryString();

  session->set_proc_info(NULL);
  session->mem_root->free_root(MYF(memory::KEEP_PREALLOC));

  if (DRIZZLE_QUERY_DONE_ENABLED() || DRIZZLE_COMMAND_DONE_ENABLED())
  {
    if (command == COM_QUERY)
    {
      DRIZZLE_QUERY_DONE(session->is_error());
    }
    DRIZZLE_COMMAND_DONE(session->is_error());
  }

  return error;
}


/**
  Create a TableList object for an INFORMATION_SCHEMA table.

    This function is used in the parser to convert a SHOW or DESCRIBE
    table_name command to a SELECT from INFORMATION_SCHEMA.
    It prepares a Select_Lex and a TableList object to represent the
    given command as a SELECT parse tree.

  @param session           thread handle
  @param lex               current lex
  @param table_ident       table alias if it's used
  @param schema_table_name the name of the INFORMATION_SCHEMA table to be
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
static bool _schema_select(Session& session, Select_Lex& sel, const string& schema_table_name)
{
  LEX_STRING db, table;
  bitset<NUM_OF_TABLE_OPTIONS> table_options;
  /*
     We have to make non const db_name & table_name
     because of lower_case_table_names
  */
  session.make_lex_string(&db, "data_dictionary", sizeof("data_dictionary"), false);
  session.make_lex_string(&table, schema_table_name, false);
  return not sel.add_table_to_list(&session, new Table_ident(db, table), NULL, table_options, TL_READ);
}

int prepare_new_schema_table(Session *session, LEX& lex0, const string& schema_table_name)
{
  Select_Lex& lex= *lex0.current_select;
  if (_schema_select(*session, lex, schema_table_name))
    return 1;
  TableList *table_list= (TableList*)lex.table_list.first;
  table_list->schema_select_lex= NULL;
  return 0;
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

static int execute_command(Session *session)
{
  bool res= false;

  /* first Select_Lex (have special meaning for many of non-SELECTcommands) */
  Select_Lex *select_lex= &session->lex().select_lex;

  /* list of all tables in query */
  TableList *all_tables;

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
  session->lex().first_lists_tables_same();

  /* should be assigned after making first tables same */
  all_tables= session->lex().query_tables;

  /* set context for commands which do not use setup_tables */
  select_lex->context.resolve_in_table_list_only((TableList*)select_lex->table_list.first);

  /*
    Reset warning count for each query that uses tables
    A better approach would be to reset this for any commands
    that is not a SHOW command or a select that only access local
    variables, but for now this is probably good enough.
    Don't reset warnings when executing a stored routine.
  */
  if (all_tables || ! session->lex().is_single_level_stmt())
  {
    drizzle_reset_errors(session, 0);
  }

  assert(not session->transaction.stmt.hasModifiedNonTransData());

  if (! (session->server_status & SERVER_STATUS_AUTOCOMMIT)
      && ! session->inTransaction()
      && session->lex().statement->isTransactional())
  {
    if (not session->startTransaction())
    {
      my_error(drizzled::ER_UNKNOWN_ERROR, MYF(0));
      return true;
    }
  }

  /* now we are ready to execute the statement */
  res= session->lex().statement->execute();
  session->set_proc_info("query end");
  /*
    The return value for ROW_COUNT() is "implementation dependent" if the
    statement is not DELETE, INSERT or UPDATE, but -1 is what JDBC and ODBC
    wants. We also keep the last value in case of SQLCOM_CALL or
    SQLCOM_EXECUTE.
  */
  if (! (sql_command_flags[session->lex().sql_command].test(CF_BIT_HAS_ROW_COUNT)))
  {
    session->row_count_func= -1;
  }

  return (res || session->is_error());
}
bool execute_sqlcom_select(Session *session, TableList *all_tables)
{
  LEX	*lex= &session->lex();
  select_result *result=lex->result;
  bool res= false;
  /* assign global limit variable if limit is not given */
  {
    Select_Lex *param= lex->unit.global_parameters;
    if (!param->explicit_limit)
      param->select_limit=
        new Item_int((uint64_t) session->variables.select_limit);
  }

  if (all_tables
      && ! (session->server_status & SERVER_STATUS_AUTOCOMMIT)
      && ! session->inTransaction()
      && ! lex->statement->isShow())
  {
    if (not session->startTransaction())
    {
      my_error(drizzled::ER_UNKNOWN_ERROR, MYF(0));
      return true;
    }
  }

  if (not (res= session->openTablesLock(all_tables)))
  {
    if (lex->describe)
    {
      /*
        We always use select_send for EXPLAIN, even if it's an EXPLAIN
        for SELECT ... INTO OUTFILE: a user application should be able
        to prepend EXPLAIN to any query and receive output for it,
        even if the query itself redirects the output.
      */
      result= new select_send();
      session->send_explain_fields(result);
      optimizer::ExplainPlan planner;
      res= planner.explainUnion(session, &session->lex().unit, result);
      if (lex->describe & DESCRIBE_EXTENDED)
      {
        char buff[1024];
        String str(buff,(uint32_t) sizeof(buff), system_charset_info);
        str.length(0);
        session->lex().unit.print(&str);
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
      if (not result)
        result= new select_send();

      /* Init the Query Cache plugin */
      plugin::QueryCache::prepareResultset(session);
      res= handle_select(session, lex, result, 0);
      /* Send the Resultset to the cache */
      plugin::QueryCache::setResultset(session);

      if (result != lex->result)
        delete result;
    }
  }
  return res;
}


#define MY_YACC_INIT 1000			// Start with big alloc
#define MY_YACC_MAX  32000			// Because of 'short'

bool my_yyoverflow(short **yyss, ParserType **yyvs, ulong *yystacksize)
{
  LEX	*lex= &current_session->lex();
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
  *yyvs=(ParserType*) lex->yacc_yyvs;
  return 0;
}


void
init_select(LEX *lex)
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


bool new_select(LEX *lex, bool move_down)
{
  Session* session= lex->session;
  Select_Lex* select_lex= new (session->mem_root) Select_Lex;

  select_lex->select_number= ++session->select_number;
  select_lex->parent_lex= lex; /* Used in init_query. */
  select_lex->init_query();
  select_lex->init_select();
  lex->nest_level++;

  if (lex->nest_level > (int) MAX_SELECT_NESTING)
  {
    my_error(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT,MYF(0),MAX_SELECT_NESTING);
    return 1;
  }

  select_lex->nest_level= lex->nest_level;
  if (move_down)
  {
    /* first select_lex of subselect or derived table */
    Select_Lex_Unit* unit= new (session->mem_root) Select_Lex_Unit();

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
      return true;
    }

    select_lex->include_neighbour(lex->current_select);
    Select_Lex_Unit *unit= select_lex->master_unit();

    if (not unit->fake_select_lex && unit->add_fake_select_lex(lex->session))
      return true;

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

  return false;
}

/**
  Create a select to return the same output as 'SELECT @@var_name'.

  Used for SHOW COUNT(*) [ WARNINGS | ERROR].

  This will crash with a core dump if the variable doesn't exists.

  @param var_name		Variable name
*/

void create_select_for_variable(Session *session, const char *var_name)
{
  LEX& lex= session->lex();
  init_select(&lex);
  lex.sql_command= SQLCOM_SELECT;
  LEX_STRING tmp;
  tmp.str= (char*) var_name;
  tmp.length=strlen(var_name);
  LEX_STRING null_lex_string;
  memset(&null_lex_string.str, 0, sizeof(null_lex_string));
  /*
    We set the name of Item to @@session.var_name because that then is used
    as the column name in the output.
  */
  if (Item* var= get_system_var(session, OPT_SESSION, tmp, null_lex_string))
  {
    char buff[MAX_SYS_VAR_LENGTH*2+4+8];
    char *end= buff;
    end+= snprintf(buff, sizeof(buff), "@@session.%s", var_name);
    var->set_name(buff, end-buff, system_charset_info);
    session->add_item_to_list(var);
  }
}


/**
  Parse a query.

  @param       session     Current thread
  @param       inBuf   Begining of the query text
  @param       length  Length of the query text
*/

void parse(Session& session, const char *inBuf, uint32_t length)
{
  session.lex().start(&session);
  session.reset_for_next_command();
  /* Check if the Query is Cached if and return true if yes
   * TODO the plugin has to make sure that the query is cacheble
   * by setting the query_safe_cache param to TRUE
   */
  if (plugin::QueryCache::isCached(&session) && not plugin::QueryCache::sendCachedResultset(&session))
      return;
  Lex_input_stream lip(&session, inBuf, length);
  if (parse_sql(&session, &lip))
    assert(session.is_error());
  else if (not session.is_error())
  {
    DRIZZLE_QUERY_EXEC_START(session.getQueryString()->c_str(), session.thread_id,
                             const_cast<const char *>(session.schema()->c_str()));
    // Implement Views here --Brian
    /* Actually execute the query */
    try
    {
      execute_command(&session);
    }
    catch (...)
    {
      // Just try to catch any random failures that could have come
      // during execution.
      DRIZZLE_ABORT;
    }
    DRIZZLE_QUERY_EXEC_DONE(0);
  }
  session.lex().unit.cleanup();
  session.set_proc_info("freeing items");
  session.end_statement();
  session.cleanup_after_query();
  session.times.set_end_timer(session);
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
                       List<String> *interval_list, const charset_info_st * const cs)
{
  register CreateField *new_field;
  LEX  *lex= &session->lex();
  statement::AlterTable *statement= (statement::AlterTable *)lex->statement;

  if (check_identifier_name(field_name, ER_TOO_LONG_IDENT))
    return true;

  if (type_modifier & PRI_KEY_FLAG)
  {
    Key *key;
    lex->col_list.push_back(new Key_part_spec(*field_name, 0));
    key= new Key(Key::PRIMARY, null_lex_str,
                      &default_key_create_info,
                      0, lex->col_list);
    statement->alter_info.key_list.push_back(key);
    lex->col_list.clear();
  }
  if (type_modifier & (UNIQUE_FLAG | UNIQUE_KEY_FLAG))
  {
    Key *key;
    lex->col_list.push_back(new Key_part_spec(*field_name, 0));
    key= new Key(Key::UNIQUE, null_lex_str,
                 &default_key_create_info, 0,
                 lex->col_list);
    statement->alter_info.key_list.push_back(key);
    lex->col_list.clear();
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
         (type == DRIZZLE_TYPE_TIMESTAMP or type == DRIZZLE_TYPE_MICROTIME)))
    {
      my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
      return true;
    }
    else if (default_value->type() == Item::NULL_ITEM)
    {
      default_value= 0;
      if ((type_modifier & (NOT_NULL_FLAG | AUTO_INCREMENT_FLAG)) == NOT_NULL_FLAG)
      {
	my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
	return true;
      }
    }
    else if (type_modifier & AUTO_INCREMENT_FLAG)
    {
      my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
      return true;
    }
  }

  if (on_update_value && (type != DRIZZLE_TYPE_TIMESTAMP and type != DRIZZLE_TYPE_MICROTIME))
  {
    my_error(ER_INVALID_ON_UPDATE, MYF(0), field_name->str);
    return true;
  }

  new_field= new CreateField;
  if (new_field->init(session, field_name->str, type, length, decimals,
                         type_modifier, comment, change, interval_list, cs, 0, column_format)
      || new_field->setDefaultValue(default_value, on_update_value))
    return true;

  statement->alter_info.create_list.push_back(new_field);
  lex->last_field=new_field;

  return false;
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
                                         const bitset<NUM_OF_TABLE_OPTIONS>& table_options,
                                         thr_lock_type lock_type,
                                         List<Index_hint> *index_hints_arg,
                                         LEX_STRING *option)
{
  TableList *previous_table_ref; /* The table preceding the current one. */
  char *alias_str;
  LEX *lex= &session->lex();

  if (!table)
    return NULL;				// End of memory
  alias_str= alias ? alias->str : table->table.str;
  if (! table_options.test(TL_OPTION_ALIAS) &&
      check_table_name(table->table.str, table->table.length))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), table->table.str);
    return NULL;
  }

  if (not table->is_derived_table() && table->db.str)
  {
    my_casedn_str(files_charset_info, table->db.str);

    identifier::Schema schema_identifier(string(table->db.str));
    if (not schema::check(*session, schema_identifier))
    {

      my_error(ER_WRONG_DB_NAME, MYF(0), table->db.str);
      return NULL;
    }
  }

  if (!alias)					/* Alias is case sensitive */
  {
    if (table->sel)
    {
      my_message(ER_DERIVED_MUST_HAVE_ALIAS,
                 ER(ER_DERIVED_MUST_HAVE_ALIAS), MYF(0));
      return NULL;
    }
    alias_str= (char*) session->mem.memdup(alias_str,table->table.length+1);
  }
  TableList *ptr = (TableList *) session->mem.calloc(sizeof(TableList));

  if (table->db.str)
  {
    ptr->setIsFqtn(true);
    ptr->setSchemaName(table->db.str);
    ptr->db_length= table->db.length;
  }
  else if (lex->copy_db_to(ptr->getSchemaNamePtr(), &ptr->db_length))
    return NULL;
  else
    ptr->setIsFqtn(false);

  ptr->alias= alias_str;
  ptr->setIsAlias(alias ? true : false);
  ptr->setTableName(table->table.str);
  ptr->table_name_length=table->table.length;
  ptr->lock_type=   lock_type;
  ptr->force_index= table_options.test(TL_OPTION_FORCE_INDEX);
  ptr->ignore_leaves= table_options.test(TL_OPTION_IGNORE_LEAVES);
  ptr->derived=	    table->sel;
  ptr->select_lex=  lex->current_select;
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
      if (not my_strcasecmp(table_alias_charset, alias_str, tables->alias) &&
	  not my_strcasecmp(system_charset_info, ptr->getSchemaName(), tables->getSchemaName()))
      {
	my_error(ER_NONUNIQ_TABLE, MYF(0), alias_str);
	return NULL;
      }
    }
  }
  /* Store the table reference preceding the current one. */
  if (table_list.size() > 0)
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
  return ptr;
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

void Select_Lex::init_nested_join(Session& session)
{
  TableList* ptr= (TableList*) session.mem.calloc(ALIGN_SIZE(sizeof(TableList)) + sizeof(NestedJoin));
  ptr->setNestedJoin(((NestedJoin*) ((unsigned char*) ptr + ALIGN_SIZE(sizeof(TableList)))));
  NestedJoin* nested_join= ptr->getNestedJoin();
  join_list->push_front(ptr);
  ptr->setEmbedding(embedding);
  ptr->setJoinList(join_list);
  ptr->alias= (char*) "(nested_join)";
  embedding= ptr;
  join_list= &nested_join->join_list;
  join_list->clear();
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
  NestedJoin *nested_join;

  assert(embedding);
  ptr= embedding;
  join_list= ptr->getJoinList();
  embedding= ptr->getEmbedding();
  nested_join= ptr->getNestedJoin();
  if (nested_join->join_list.size() == 1)
  {
    TableList *embedded= &nested_join->join_list.front();
    join_list->pop();
    embedded->setJoinList(join_list);
    embedded->setEmbedding(embedding);
    join_list->push_front(embedded);
    ptr= embedded;
  }
  else if (nested_join->join_list.size() == 0)
  {
    join_list->pop();
    ptr= NULL;                                     // return value
  }
  return ptr;
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
  TableList* ptr= (TableList*) session->mem.calloc(ALIGN_SIZE(sizeof(TableList)) + sizeof(NestedJoin));
  ptr->setNestedJoin(((NestedJoin*) ((unsigned char*) ptr + ALIGN_SIZE(sizeof(TableList)))));
  NestedJoin* nested_join= ptr->getNestedJoin();
  ptr->setEmbedding(embedding);
  ptr->setJoinList(join_list);
  ptr->alias= (char*) "(nest_last_join)";
  List<TableList>* embedded_list= &nested_join->join_list;
  embedded_list->clear();

  for (uint32_t i=0; i < 2; i++)
  {
    TableList *table= join_list->pop();
    table->setJoinList(embedded_list);
    table->setEmbedding(ptr);
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
  return ptr;
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
  table->setJoinList(join_list);
  table->setEmbedding(embedding);
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

  return tab1;
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
  for (TableList *tables= (TableList*) table_list.first;
       tables;
       tables= tables->next_local)
  {
    tables->lock_type= lock_type;
  }
}


/**
  Create a fake Select_Lex for a unit.

    The method create a fake Select_Lex object for a unit.
    This object is created for any union construct containing a union
    operation and also for any single select union construct of the form
    @verbatim
    (SELECT ... ORDER BY order_list [LIMIT n]) ORDER BY ...
    @endvarbatim
    or of the form
    @varbatim
    (SELECT ... ORDER BY LIMIT n) ORDER BY ...
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

  fake_select_lex= new (session_arg->mem_root) Select_Lex();
  fake_select_lex->include_standalone(this, (Select_Lex_Node**)&fake_select_lex);
  fake_select_lex->select_number= INT_MAX;
  fake_select_lex->parent_lex= &session_arg->lex(); /* Used in init_query. */
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
    session_arg->lex().current_select= fake_select_lex;
  }
  session_arg->lex().pop_context();
  return 0;
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

void push_new_name_resolution_context(Session& session, TableList& left_op, TableList& right_op)
{
  Name_resolution_context *on_context= new (session.mem_root) Name_resolution_context;
  on_context->init();
  on_context->first_name_resolution_table= left_op.first_leaf_for_name_resolution();
  on_context->last_name_resolution_table= right_op.last_leaf_for_name_resolution();
  session.lex().push_context(on_context);
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
  Check if the select is a simple select (not an union).

  @retval
    0	ok
  @retval
    1	error	; In this case the error messege is sent to the client
*/

bool check_simple_select(Session* session)
{
  if (session->lex().current_select != &session->lex().select_lex)
  {
    char command[80];
    Lex_input_stream *lip= session->m_lip;
    strncpy(command, lip->yylval->symbol.str,
            min(lip->yylval->symbol.length, (uint32_t)(sizeof(command)-1)));
    command[min(lip->yylval->symbol.length, (uint32_t)(sizeof(command)-1))]=0;
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
  Select_Lex *select_lex= &session->lex().select_lex;

  if (session->lex().select_lex.item_list.size() != session->lex().value_list.size())
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    return true;
  }

  if (session->lex().select_lex.table_list.size() > 1)
  {
    if (select_lex->order_list.size())
      msg= "ORDER BY";
    else if (select_lex->select_limit)
      msg= "LIMIT";
    if (msg)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", msg);
      return true;
    }
  }
  return false;
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
  /*
    Check that we have modify privileges for the first table and
    select privileges for the rest
  */
  if (session->lex().update_list.size() != session->lex().value_list.size())
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    return true;
  }
  return false;
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
    enum_parsing_place place= session->lex().current_select->parsing_place;
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
                              uint32_t max_char_length, const charset_info_st * const cs,
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


bool check_identifier_name(LEX_STRING *str, error_t err_code,
                           uint32_t max_char_length,
                           const char *param_for_err_msg)
{
  /*
    We don't support non-BMP characters in identifiers at the moment,
    so they should be prohibited until such support is done.
    This is why we use the 3-byte utf8 to check well-formedness here.
  */
  const charset_info_st * const cs= &my_charset_utf8mb4_general_ci;

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
  case EE_OK:
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


/**
  This is a wrapper of DRIZZLEparse(). All the code should call parse_sql()
  instead of DRIZZLEparse().

  @param session Thread context.
  @param lip Lexer context.

  @return Error status.
    @retval false on success.
    @retval true on parsing error.
*/

static bool parse_sql(Session *session, Lex_input_stream *lip)
{
  assert(session->m_lip == NULL);

  DRIZZLE_QUERY_PARSE_START(session->getQueryString()->c_str());

  /* Set Lex_input_stream. */

  session->m_lip= lip;

  /* Parse the query. */

  bool parse_status= base_sql_parse(session) != 0;

  /* Check that if DRIZZLEparse() failed, session->is_error() is set. */

  assert(!parse_status || session->is_error());

  /* Reset Lex_input_stream. */

  session->m_lip= NULL;

  DRIZZLE_QUERY_PARSE_DONE(parse_status || session->is_fatal_error);

  /* That's it. */

  return parse_status || session->is_fatal_error;
}

/**
  @} (end of group Runtime_Environment)
*/

} /* namespace drizzled */
