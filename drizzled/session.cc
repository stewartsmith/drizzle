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
 * @file Implementation of the Session class and API
 */

#include "config.h"
#include "drizzled/session.h"
#include "drizzled/session_list.h"
#include <sys/stat.h>
#include "drizzled/error.h"
#include "drizzled/gettext.h"
#include "drizzled/query_id.h"
#include "drizzled/data_home.h"
#include "drizzled/sql_base.h"
#include "drizzled/lock.h"
#include "drizzled/item/cache.h"
#include "drizzled/item/float.h"
#include "drizzled/item/return_int.h"
#include "drizzled/item/empty_string.h"
#include "drizzled/show.h"
#include "drizzled/plugin/client.h"
#include "drizzled/plugin/scheduler.h"
#include "drizzled/plugin/authentication.h"
#include "drizzled/plugin/logging.h"
#include "drizzled/plugin/transactional_storage_engine.h"
#include "drizzled/probes.h"
#include "drizzled/table_proto.h"
#include "drizzled/db.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/transaction_services.h"
#include "drizzled/drizzled.h"

#include "drizzled/table/instance.h"

#include "plugin/myisam/myisam.h"
#include "drizzled/internal/iocache.h"
#include "drizzled/internal/thread_var.h"
#include "drizzled/plugin/event_observer.h"

#include "drizzled/util/functors.h"

#include "drizzled/display.h"

#include <fcntl.h>
#include <algorithm>
#include <climits>
#include <boost/filesystem.hpp>

using namespace std;

namespace fs=boost::filesystem;
namespace drizzled
{

/*
  The following is used to initialise Table_ident with a internal
  table name
*/
char internal_table_name[2]= "*";
char empty_c_string[1]= {0};    /* used for not defined db */

const char * const Session::DEFAULT_WHERE= "field list";

bool Key_part_spec::operator==(const Key_part_spec& other) const
{
  return length == other.length &&
         field_name.length == other.field_name.length &&
         !strcmp(field_name.str, other.field_name.str);
}

Open_tables_state::Open_tables_state(uint64_t version_arg) :
  version(version_arg)
{
  open_tables= temporary_tables= derived_tables= NULL;
  extra_lock= lock= NULL;
}

/*
  The following functions form part of the C plugin API
*/
int mysql_tmpfile(const char *prefix)
{
  char filename[FN_REFLEN];
  int fd = internal::create_temp_file(filename, drizzle_tmpdir.c_str(), prefix, MYF(MY_WME));
  if (fd >= 0) {
    unlink(filename);
  }

  return fd;
}

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
void set_session_proc_info(Session *session, const char *info)
{
  session->set_proc_info(info);
}

const char *get_session_proc_info(Session *session)
{
  return session->get_proc_info();
}

void **Session::getEngineData(const plugin::MonitoredInTransaction *monitored)
{
  return static_cast<void **>(&ha_data[monitored->getId()].ha_ptr);
}

ResourceContext *Session::getResourceContext(const plugin::MonitoredInTransaction *monitored,
                                             size_t index)
{
  return &ha_data[monitored->getId()].resource_context[index];
}

int64_t session_test_options(const Session *session, int64_t test_options)
{
  return session->options & test_options;
}

int session_sql_command(const Session *session)
{
  return (int) session->lex->sql_command;
}

enum_tx_isolation session_tx_isolation(const Session *session)
{
  return (enum_tx_isolation)session->variables.tx_isolation;
}

Session::Session(plugin::Client *client_arg) :
  Open_tables_state(refresh_version),
  mem_root(&main_mem_root),
  xa_id(0),
  lex(&main_lex),
  query(new std::string),
  catalog("LOCAL"),
  client(client_arg),
  scheduler(NULL),
  scheduler_arg(NULL),
  lock_id(&main_lock_id),
  user_time(0),
  ha_data(plugin::num_trx_monitored_objects),
  concurrent_execute_allowed(true),
  arg_of_last_insert_id_function(false),
  first_successful_insert_id_in_prev_stmt(0),
  first_successful_insert_id_in_cur_stmt(0),
  limit_found_rows(0),
  _global_read_lock(NONE),
  _killed(NOT_KILLED),
  some_tables_deleted(false),
  no_errors(false),
  password(false),
  is_fatal_error(false),
  transaction_rollback_request(false),
  is_fatal_sub_stmt_error(0),
  derived_tables_processing(false),
  tablespace_op(false),
  m_lip(NULL),
  cached_table(0),
  transaction_message(NULL),
  statement_message(NULL),
  session_event_observers(NULL),
  use_usage(false)
{
  client->setSession(this);

  /*
    Pass nominal parameters to init_alloc_root only to ensure that
    the destructor works OK in case of an error. The main_mem_root
    will be re-initialized in init_for_queries().
  */
  memory::init_sql_alloc(&main_mem_root, memory::ROOT_MIN_BLOCK_SIZE, 0);
  thread_stack= NULL;
  count_cuted_fields= CHECK_FIELD_ERROR_FOR_NULL;
  col_access= 0;
  tmp_table= 0;
  used_tables= 0;
  cuted_fields= sent_row_count= row_count= 0L;
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
  warn_query_id= 0;
  mysys_var= 0;
  scoreboard_index= -1;
  dbug_sentry=Session_SENTRY_MAGIC;
  cleanup_done= abort_on_warning= no_warnings_for_error= false;  

  /* query_cache init */
  query_cache_key= "";
  resultset= NULL;

  /* Variables with default values */
  proc_info="login";
  where= Session::DEFAULT_WHERE;
  command= COM_CONNECT;

  plugin_sessionvar_init(this);
  /*
    variables= global_system_variables above has reset
    variables.pseudo_thread_id to 0. We need to correct it here to
    avoid temporary tables replication failure.
  */
  variables.pseudo_thread_id= thread_id;
  server_status= SERVER_STATUS_AUTOCOMMIT;
  options= session_startup_options;

  if (variables.max_join_size == HA_POS_ERROR)
    options |= OPTION_BIG_SELECTS;
  else
    options &= ~OPTION_BIG_SELECTS;

  open_options=ha_open_options;
  update_lock_default= TL_WRITE;
  session_tx_isolation= (enum_tx_isolation) variables.tx_isolation;
  warn_list.empty();
  memset(warn_count, 0, sizeof(warn_count));
  total_warn_count= 0;
  memset(&status_var, 0, sizeof(status_var));

  /* Initialize sub structures */
  memory::init_sql_alloc(&warn_root, WARN_ALLOC_BLOCK_SIZE, WARN_ALLOC_PREALLOC_SIZE);

  substitute_null_with_insert_id = false;
  lock_info.init(); /* safety: will be reset after start */
  thr_lock_owner_init(&main_lock_id, &lock_info);

  m_internal_handler= NULL;
  
  plugin::EventObserver::registerSessionEvents(*this); 
}

void Session::free_items()
{
  Item *next;
  /* This works because items are allocated with memory::sql_alloc() */
  for (; free_list; free_list= next)
  {
    next= free_list->next;
    free_list->delete_self();
  }
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

void Session::setAbort(bool arg)
{
  mysys_var->abort= arg;
}

void Session::lockOnSys()
{
  if (not mysys_var)
    return;

  setAbort(true);
  boost_unique_lock_t scopedLock(mysys_var->mutex);
  if (mysys_var->current_cond)
  {
    mysys_var->current_mutex->lock();
    mysys_var->current_cond->notify_all();
    mysys_var->current_mutex->unlock();
  }
}

void Session::pop_internal_handler()
{
  assert(m_internal_handler != NULL);
  m_internal_handler= NULL;
}

void Session::get_xid(DRIZZLE_XID *xid)
{
  *xid = *(DRIZZLE_XID *) &transaction.xid_state.xid;
}

/* Do operations that may take a long time */

void Session::cleanup(void)
{
  assert(cleanup_done == false);

  setKilled(KILL_CONNECTION);
#ifdef ENABLE_WHEN_BINLOG_WILL_BE_ABLE_TO_PREPARE
  if (transaction.xid_state.xa_state == XA_PREPARED)
  {
#error xid_state in the cache should be replaced by the allocated value
  }
#endif
  {
    TransactionServices &transaction_services= TransactionServices::singleton();
    transaction_services.rollbackTransaction(this, true);
    xid_cache_delete(&transaction.xid_state);
  }

  for (UserVars::iterator iter= user_vars.begin();
       iter != user_vars.end();
       iter++)
  {
    user_var_entry *entry= (*iter).second;
    delete entry;
  }
  user_vars.clear();


  close_temporary_tables();

  if (global_read_lock)
  {
    unlockGlobalReadLock();
  }

  cleanup_done= true;
}

Session::~Session()
{
  this->checkSentry();

  if (client->isConnected())
  {
    if (global_system_variables.log_warnings)
        errmsg_printf(ERRMSG_LVL_WARN, ER(ER_FORCING_CLOSE),internal::my_progname,
                      thread_id,
                      (getSecurityContext().getUser().c_str() ?
                       getSecurityContext().getUser().c_str() : ""));
    disconnect(0, false);
  }

  /* Close connection */
  client->close();
  delete client;

  if (cleanup_done == false)
    cleanup();

  plugin::StorageEngine::closeConnection(this);
  plugin_sessionvar_cleanup(this);

  warn_root.free_root(MYF(0));
  mysys_var=0;					// Safety (shouldn't be needed)
  dbug_sentry= Session_SENTRY_GONE;

  main_mem_root.free_root(MYF(0));
  currentMemRoot().release();
  currentSession().release();

  plugin::Logging::postEndDo(this);
  plugin::EventObserver::deregisterSessionEvents(*this); 

  for (PropertyMap::iterator iter= life_properties.begin(); iter != life_properties.end(); iter++)
  {
    delete (*iter).second;
  }
  life_properties.clear();
}

void Session::setClient(plugin::Client *client_arg)
{
  client= client_arg;
  client->setSession(this);
}

void Session::awake(Session::killed_state_t state_to_set)
{
  this->checkSentry();

  setKilled(state_to_set);
  if (state_to_set != Session::KILL_QUERY)
  {
    scheduler->killSession(this);
    DRIZZLE_CONNECTION_DONE(thread_id);
  }
  if (mysys_var)
  {
    boost_unique_lock_t scopedLock(mysys_var->mutex);
    /*
      "
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
      mysys_var->current_mutex->lock();
      mysys_var->current_cond->notify_all();
      mysys_var->current_mutex->unlock();
    }
  }
}

/*
  Remember the location of thread info, the structure needed for
  memory::sql_alloc() and the structure for the net buffer
*/
bool Session::storeGlobals()
{
  /*
    Assert that thread_stack is initialized: it's necessary to be able
    to track stack overrun.
  */
  assert(thread_stack);

  currentSession().release();
  currentSession().reset(this);

  currentMemRoot().release();
  currentMemRoot().reset(&mem_root);

  mysys_var=my_thread_var;

  /*
    Let mysqld define the thread id (not mysys)
    This allows us to move Session to different threads if needed.
  */
  mysys_var->id= thread_id;

  /*
    We have to call thr_lock_info_init() again here as Session may have been
    created in another thread
  */
  lock_info.init();

  return false;
}

/*
  Init Session for query processing.
  This has to be called once before we call mysql_parse.
  See also comments in session.h.
*/

void Session::prepareForQueries()
{
  if (variables.max_join_size == HA_POS_ERROR)
    options |= OPTION_BIG_SELECTS;

  version= refresh_version;
  set_proc_info(NULL);
  command= COM_SLEEP;
  set_time();

  mem_root->reset_root_defaults(variables.query_alloc_block_size,
                                variables.query_prealloc_size);
  transaction.xid_state.xid.null();
  transaction.xid_state.in_session=1;
  if (use_usage)
    resetUsage();
}

bool Session::initGlobals()
{
  if (storeGlobals())
  {
    disconnect(ER_OUT_OF_RESOURCES, true);
    status_var.aborted_connects++;
    return true;
  }
  return false;
}

void Session::run()
{
  if (initGlobals() || authenticate())
  {
    disconnect(0, true);
    return;
  }

  prepareForQueries();

  while (not client->haveError() && getKilled() != KILL_CONNECTION)
  {
    if (not executeStatement())
      break;
  }

  disconnect(0, true);
}

bool Session::schedule(Session::shared_ptr arg)
{
  arg->scheduler= plugin::Scheduler::getScheduler();
  assert(arg->scheduler);

  connection_count.increment();

  if (connection_count > current_global_counters.max_used_connections)
  {
    current_global_counters.max_used_connections= connection_count;
  }

  current_global_counters.connections++;
  arg->thread_id= arg->variables.pseudo_thread_id= global_thread_id++;

  session::Cache::singleton().insert(arg);

  if (unlikely(plugin::EventObserver::connectSession(*arg)))
  {
    // We should do something about an error...
  }

  if (unlikely(plugin::EventObserver::connectSession(*arg)))
  {
    // We should do something about an error...
  }

  if (plugin::Scheduler::getScheduler()->addSession(arg.get()))
  {
    DRIZZLE_CONNECTION_START(thread_id);
    char error_message_buff[DRIZZLE_ERRMSG_SIZE];

    arg->setKilled(Session::KILL_CONNECTION);

    arg->status_var.aborted_connects++;

    /* Can't use my_error() since store_globals has not been called. */
    /* TODO replace will better error message */
    snprintf(error_message_buff, sizeof(error_message_buff),
             ER(ER_CANT_CREATE_THREAD), 1);
    arg->client->sendError(ER_CANT_CREATE_THREAD, error_message_buff);
    return true;
  }

  return false;
}


/*
  Is this session viewable by the current user?
*/
bool Session::isViewable() const
{
  return plugin::Authorization::isAuthorized(current_session->getSecurityContext(),
                                             this,
                                             false);
}


const char* Session::enter_cond(boost::condition_variable_any &cond, boost::mutex &mutex, const char* msg)
{
  const char* old_msg = get_proc_info();
  safe_mutex_assert_owner(mutex);
  mysys_var->current_mutex = &mutex;
  mysys_var->current_cond = &cond;
  this->set_proc_info(msg);
  return old_msg;
}

void Session::exit_cond(const char* old_msg)
{
  /*
    Putting the mutex unlock in exit_cond() ensures that
    mysys_var->current_mutex is always unlocked _before_ mysys_var->mutex is
    locked (if that would not be the case, you'll get a deadlock if someone
    does a Session::awake() on you).
  */
  mysys_var->current_mutex->unlock();
  boost_unique_lock_t scopedLock(mysys_var->mutex);
  mysys_var->current_mutex = 0;
  mysys_var->current_cond = 0;
  this->set_proc_info(old_msg);
}

bool Session::authenticate()
{
  lex->start(this);
  if (client->authenticate())
    return false;

  status_var.aborted_connects++;

  return true;
}

bool Session::checkUser(const std::string &passwd_str,
                        const std::string &in_db)
{
  bool is_authenticated=
    plugin::Authentication::isAuthenticated(getSecurityContext(),
                                            passwd_str);

  if (is_authenticated != true)
  {
    status_var.access_denied++;
    /* isAuthenticated has pushed the error message */
    return false;
  }

  /* Change database if necessary */
  if (not in_db.empty())
  {
    SchemaIdentifier identifier(in_db);
    if (mysql_change_db(this, identifier))
    {
      /* mysql_change_db() has pushed the error message. */
      return false;
    }
  }
  my_ok();
  password= not passwd_str.empty();

  /* Ready to handle queries */
  return true;
}

bool Session::executeStatement()
{
  char *l_packet= 0;
  uint32_t packet_length;

  enum enum_server_command l_command;

  /*
    indicator of uninitialized lex => normal flow of errors handling
    (see my_message_sql)
  */
  lex->current_select= 0;
  clear_error();
  main_da.reset_diagnostics_area();

  if (client->readCommand(&l_packet, &packet_length) == false)
  {
    return false;
  }

  if (getKilled() == KILL_CONNECTION)
    return false;

  if (packet_length == 0)
    return true;

  l_command= static_cast<enum_server_command>(l_packet[0]);

  if (command >= COM_END)
    command= COM_END;                           // Wrong command

  assert(packet_length);
  return not dispatch_command(l_command, this, l_packet+1, (uint32_t) (packet_length-1));
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
  while (in_packet_length > 0 && (pos[-1] == ';' || my_isspace(charset() ,pos[-1])))
  {
    pos--;
    in_packet_length--;
  }

  query.reset(new std::string(in_packet, in_packet + in_packet_length));

  return true;
}

bool Session::endTransaction(enum enum_mysql_completiontype completion)
{
  bool do_release= 0;
  bool result= true;
  TransactionServices &transaction_services= TransactionServices::singleton();

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
      if (transaction_services.commitTransaction(this, true))
        result= false;
      options&= ~(OPTION_BEGIN);
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
      if (transaction_services.rollbackTransaction(this, true))
        result= false;
      options&= ~(OPTION_BEGIN);
      if (result == true && (completion == ROLLBACK_AND_CHAIN))
        result= startTransaction();
      break;
    }
    default:
      my_error(ER_UNKNOWN_COM_ERROR, MYF(0));
      return false;
  }

  if (result == false)
  {
    my_error(killed_errno(), MYF(0));
  }
  else if ((result == true) && do_release)
  {
    setKilled(Session::KILL_CONNECTION);
  }

  return result;
}

bool Session::endActiveTransaction()
{
  bool result= true;
  TransactionServices &transaction_services= TransactionServices::singleton();

  if (transaction.xid_state.xa_state != XA_NOTR)
  {
    my_error(ER_XAER_RMFAIL, MYF(0), xa_state_names[transaction.xid_state.xa_state]);
    return false;
  }
  if (options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
  {
    server_status&= ~SERVER_STATUS_IN_TRANS;
    if (transaction_services.commitTransaction(this, true))
      result= false;
  }
  options&= ~(OPTION_BEGIN);
  return result;
}

bool Session::startTransaction(start_transaction_option_t opt)
{
  bool result= true;

  if (! endActiveTransaction())
  {
    result= false;
  }
  else
  {
    options|= OPTION_BEGIN;
    server_status|= SERVER_STATUS_IN_TRANS;

    if (plugin::TransactionalStorageEngine::notifyStartTransaction(this, opt))
    {
      result= false;
    }
  }

  return result;
}

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
    first_successful_insert_id_in_prev_stmt= first_successful_insert_id_in_cur_stmt;
    first_successful_insert_id_in_cur_stmt= 0;
    substitute_null_with_insert_id= true;
  }
  arg_of_last_insert_id_function= false;
  /* Free Items that were created during this execution */
  free_items();
  /* Reset where. */
  where= Session::DEFAULT_WHERE;

  /* Reset the temporary shares we built */
  for_each(temporary_shares.begin(),
           temporary_shares.end(),
           DeletePtr());
  temporary_shares.clear();
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
                                     const std::string &str,
                                     bool allocate_lex_string)
{
  return make_lex_string(lex_str, str.c_str(), str.length(), allocate_lex_string);
}

LEX_STRING *Session::make_lex_string(LEX_STRING *lex_str,
                                     const char* str, uint32_t length,
                                     bool allocate_lex_string)
{
  if (allocate_lex_string)
    if (!(lex_str= (LEX_STRING *)alloc(sizeof(LEX_STRING))))
      return 0;
  if (!(lex_str->str= mem_root->strmake_root(str, length)))
    return 0;
  lex_str->length= length;
  return lex_str;
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
  return (result->send_fields(field_list));
}

void select_result::send_error(uint32_t errcode, const char *err)
{
  my_message(errcode, err, MYF(0));
}

/************************************************************************
  Handling writing to file
************************************************************************/

void select_to_file::send_error(uint32_t errcode,const char *err)
{
  my_message(errcode, err, MYF(0));
  if (file > 0)
  {
    (void) cache->end_io_cache();
    (void) internal::my_close(file, MYF(0));
    (void) internal::my_delete(path.file_string().c_str(), MYF(0));		// Delete file on error
    file= -1;
  }
}


bool select_to_file::send_eof()
{
  int error= test(cache->end_io_cache());
  if (internal::my_close(file, MYF(MY_WME)))
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
    (void) cache->end_io_cache();
    (void) internal::my_close(file, MYF(0));
    file= -1;
  }
  path= "";
  row_count= 0;
}

select_to_file::select_to_file(file_exchange *ex)
  : exchange(ex),
    file(-1),
    cache(static_cast<internal::IO_CACHE *>(memory::sql_calloc(sizeof(internal::IO_CACHE)))),
    row_count(0L)
{
  path= "";
}

select_to_file::~select_to_file()
{
  cleanup();
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


static int create_file(Session *session,
                       fs::path &target_path,
                       file_exchange *exchange,
                       internal::IO_CACHE *cache)
{
  fs::path to_file(exchange->file_name);
  int file;

  if (not to_file.has_root_directory())
  {
    target_path= fs::system_complete(getDataHomeCatalog());
    if (not session->db.empty())
    {
      int count_elements= 0;
      for (fs::path::iterator iter= to_file.begin();
           iter != to_file.end();
           ++iter, ++count_elements)
      { }

      if (count_elements == 1)
      {
        target_path /= session->db;
      }
    }
    target_path /= to_file;
  }
  else
  {
    target_path = exchange->file_name;
  }

  if (not secure_file_priv.string().empty())
  {
    if (target_path.file_string().substr(0, secure_file_priv.file_string().size()) != secure_file_priv.file_string())
    {
      /* Write only allowed to dir or subdir specified by secure_file_priv */
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--secure-file-priv");
      return -1;
    }
  }

  if (!access(target_path.file_string().c_str(), F_OK))
  {
    my_error(ER_FILE_EXISTS_ERROR, MYF(0), exchange->file_name);
    return -1;
  }
  /* Create the file world readable */
  if ((file= internal::my_create(target_path.file_string().c_str(), 0666, O_WRONLY|O_EXCL, MYF(MY_WME))) < 0)
    return file;
  (void) fchmod(file, 0666);			// Because of umask()
  if (cache->init_io_cache(file, 0L, internal::WRITE_CACHE, 0L, 1, MYF(MY_WME)))
  {
    internal::my_close(file, MYF(0));
    internal::my_delete(target_path.file_string().c_str(), MYF(0));  // Delete file on error, it was just created
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
  {
    path= exchange->file_name;
  }

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

  if ((file= create_file(session, path, exchange, cache)) < 0)
    return 1;

  return 0;
}

bool select_export::send_data(List<Item> &items)
{
  char buff[MAX_FIELD_WIDTH],null_buff[2],space[MAX_FIELD_WIDTH];
  bool space_inited=0;
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  tmp.length(0);

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return false;
  }
  row_count++;
  Item *item;
  uint32_t used_length=0,items_left=items.elements;
  List_iterator_fast<Item> li(items);

  if (my_b_write(cache,(unsigned char*) exchange->line_start->ptr(),
                 exchange->line_start->length()))
    return true;

  while ((item=li++))
  {
    Item_result result_type=item->result_type();
    bool enclosed = (exchange->enclosed->length() &&
                     (!exchange->opt_enclosed || result_type == STRING_RESULT));
    res=item->str_result(&tmp);
    if (res && enclosed)
    {
      if (my_b_write(cache,(unsigned char*) exchange->enclosed->ptr(),
                     exchange->enclosed->length()))
        return true;
    }
    if (!res)
    {						// NULL
      if (!fixed_row_size)
      {
        if (escape_char != -1)			// Use \N syntax
        {
          null_buff[0]=escape_char;
          null_buff[1]='N';
          if (my_b_write(cache,(unsigned char*) null_buff,2))
            return true;
        }
        else if (my_b_write(cache,(unsigned char*) "NULL",4))
          return true;
      }
      else
      {
        used_length=0;				// Fill with space
      }
    }
    else
    {
      if (fixed_row_size)
        used_length= min(res->length(), static_cast<size_t>(item->max_length));
      else
        used_length= res->length();

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
          if (use_mb(res_charset))
          {
            int l;
            if ((l=my_ismbchar(res_charset, pos, end)))
            {
              pos += l-1;
              continue;
            }
          }

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

          if ((needs_escaping(*pos, enclosed) ||
               (check_second_byte &&
                my_mbcharlen(character_set_client, (unsigned char) *pos) == 2 &&
                pos + 1 < end &&
                needs_escaping(pos[1], enclosed))) &&
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
            if (my_b_write(cache,(unsigned char*) start,(uint32_t) (pos-start)) ||
                my_b_write(cache,(unsigned char*) tmp_buff,2))
              return true;
            start=pos+1;
          }
        }
        if (my_b_write(cache,(unsigned char*) start,(uint32_t) (pos-start)))
          return true;
      }
      else if (my_b_write(cache,(unsigned char*) res->ptr(),used_length))
        return true;
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
          if (my_b_write(cache,(unsigned char*) space,sizeof(space)))
            return true;
        }
        if (my_b_write(cache,(unsigned char*) space,length))
          return true;
      }
    }
    if (res && enclosed)
    {
      if (my_b_write(cache, (unsigned char*) exchange->enclosed->ptr(),
                     exchange->enclosed->length()))
        return true;
    }
    if (--items_left)
    {
      if (my_b_write(cache, (unsigned char*) exchange->field_term->ptr(),
                     field_term_length))
        return true;
    }
  }
  if (my_b_write(cache,(unsigned char*) exchange->line_term->ptr(),
                 exchange->line_term->length()))
  {
    return true;
  }

  return false;
}


/***************************************************************************
** Dump  of select to a binary file
***************************************************************************/


int
select_dump::prepare(List<Item> &, Select_Lex_Unit *u)
{
  unit= u;
  return (int) ((file= create_file(session, path, exchange, cache)) < 0);
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
    return 1;
  }
  while ((item=li++))
  {
    res=item->str_result(&tmp);
    if (!res)					// If NULL
    {
      if (my_b_write(cache,(unsigned char*) "",1))
        return 1;
    }
    else if (my_b_write(cache,(unsigned char*) res->ptr(),res->length()))
    {
      my_error(ER_ERROR_ON_WRITE, MYF(0), path.file_string().c_str(), errno);
      return 1;
    }
  }
  return(0);
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
  Don't free mem_root, as mem_root is freed in the end of dispatch_command
  (once for any command).
*/
void Session::end_statement()
{
  /* Cleanup SQL processing state to reuse this statement in next query. */
  lex->end();
  query_cache_key= ""; // reset the cache key
  resetResultsetMessage();
}

bool Session::copy_db_to(char **p_db, size_t *p_db_length)
{
  if (db.empty())
  {
    my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));
    return true;
  }
  *p_db= strmake(db.c_str(), db.length());
  *p_db_length= db.length();
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


bool Session::set_db(const std::string &new_db)
{
  /* Do not reallocate memory if current chunk is big enough. */
  if (new_db.length())
    db= new_db;
  else
    db.clear();

  return false;
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
  if (getKilled() || client->wasAborted())
  {
    status_var.aborted_threads++;
  }

  if (client->wasAborted())
  {
    if (not getKilled() && variables.log_warnings > 1)
    {
      SecurityContext *sctx= &security_ctx;

      errmsg_printf(ERRMSG_LVL_WARN, ER(ER_NEW_ABORTING_CONNECTION)
                  , thread_id
                  , (db.empty() ? "unconnected" : db.c_str())
                  , sctx->getUser().empty() == false ? sctx->getUser().c_str() : "unauthenticated"
                  , sctx->getIp().c_str()
                  , (main_da.is_error() ? main_da.message() : ER(ER_UNKNOWN_ERROR)));
    }
  }

  /* Close out our connection to the client */
  if (should_lock)
    session::Cache::singleton().mutex().lock();

  setKilled(Session::KILL_CONNECTION);

  if (client->isConnected())
  {
    if (errcode)
    {
      /*my_error(errcode, ER(errcode));*/
      client->sendError(errcode, ER(errcode));
    }
    client->close();
  }

  if (should_lock)
  {
    session::Cache::singleton().mutex().unlock();
  }
}

void Session::reset_for_next_command()
{
  free_list= 0;
  select_number= 1;
  /*
    Those two lines below are theoretically unneeded as
    Session::cleanup_after_query() should take care of this already.
  */
  auto_inc_intervals_in_cur_stmt_for_binlog.empty();

  is_fatal_error= false;
  server_status&= ~ (SERVER_MORE_RESULTS_EXISTS |
                          SERVER_QUERY_NO_INDEX_USED |
                          SERVER_QUERY_NO_GOOD_INDEX_USED);

  clear_error();
  main_da.reset_diagnostics_area();
  total_warn_count=0;			// Warnings for this query
  sent_row_count= examined_row_count= 0;
}

/*
  Close all temporary tables created by 'CREATE TEMPORARY TABLE' for thread
*/

void Open_tables_state::close_temporary_tables()
{
  Table *table;
  Table *tmp_next;

  if (not temporary_tables)
    return;

  for (table= temporary_tables; table; table= tmp_next)
  {
    tmp_next= table->getNext();
    nukeTable(table);
  }
  temporary_tables= NULL;
}

/*
  unlink from session->temporary tables and close temporary table
*/

void Open_tables_state::close_temporary_table(Table *table)
{
  if (table->getPrev())
  {
    table->getPrev()->setNext(table->getNext());
    if (table->getPrev()->getNext())
    {
      table->getNext()->setPrev(table->getPrev());
    }
  }
  else
  {
    /* removing the item from the list */
    assert(table == temporary_tables);
    /*
      slave must reset its temporary list pointer to zero to exclude
      passing non-zero value to end_slave via rli->save_temporary_tables
      when no temp tables opened, see an invariant below.
    */
    temporary_tables= table->getNext();
    if (temporary_tables)
    {
      table->getNext()->setPrev(NULL);
    }
  }
  nukeTable(table);
}

/*
  Close and drop a temporary table

  NOTE
  This dosn't unlink table from session->temporary
  If this is needed, use close_temporary_table()
*/

void Open_tables_state::nukeTable(Table *table)
{
  plugin::StorageEngine *table_type= table->getShare()->db_type();

  table->free_io_cache();
  table->delete_table();

  TableIdentifier identifier(table->getShare()->getSchemaName(), table->getShare()->getTableName(), table->getShare()->getPath());
  rm_temporary_table(table_type, identifier);

  delete table->getMutableShare();

  /* This makes me sad, but we're allocating it via malloc */
  delete table;
}

/** Clear most status variables. */
extern time_t flush_status_time;

void Session::refresh_status()
{
  /* Reset thread's status variables */
  memset(&status_var, 0, sizeof(status_var));

  flush_status_time= time((time_t*) 0);
  current_global_counters.max_used_connections= 1; /* We set it to one, because we know we exist */
  current_global_counters.connections= 0;
}

user_var_entry *Session::getVariable(LEX_STRING &name, bool create_if_not_exists)
{
  return getVariable(std::string(name.str, name.length), create_if_not_exists);
}

user_var_entry *Session::getVariable(const std::string  &name, bool create_if_not_exists)
{
  UserVarsRange ppp= user_vars.equal_range(name);

  for (UserVars::iterator iter= ppp.first;
       iter != ppp.second; ++iter)
  {
    return (*iter).second;
  }

  if (not create_if_not_exists)
    return NULL;

  user_var_entry *entry= NULL;
  entry= new (nothrow) user_var_entry(name.c_str(), query_id);

  if (entry == NULL)
    return NULL;

  std::pair<UserVars::iterator, bool> returnable= user_vars.insert(make_pair(name, entry));

  if (not returnable.second)
  {
    delete entry;
  }

  return entry;
}

void Session::setVariable(const std::string &name, const std::string &value)
{
  user_var_entry *updateable_var= getVariable(name.c_str(), true);

  updateable_var->update_hash(false,
                              (void*)value.c_str(),
                              static_cast<uint32_t>(value.length()), STRING_RESULT,
                              &my_charset_bin,
                              DERIVATION_IMPLICIT, false);
}

void Open_tables_state::mark_temp_tables_as_free_for_reuse()
{
  for (Table *table= temporary_tables ; table ; table= table->getNext())
  {
    if (table->query_id == getQueryId())
    {
      table->query_id= 0;
      table->cursor->ha_reset();
    }
  }
}

void Session::mark_used_tables_as_free_for_reuse(Table *table)
{
  for (; table ; table= table->getNext())
  {
    if (table->query_id == getQueryId())
    {
      table->query_id= 0;
      table->cursor->ha_reset();
    }
  }
}

/*
  Unlocks tables and frees derived tables.
  Put all normal tables used by thread in free list.

  It will only close/mark as free for reuse tables opened by this
  substatement, it will also check if we are closing tables after
  execution of complete query (i.e. we are on upper level) and will
  leave prelocked mode if needed.
*/
void Session::close_thread_tables()
{
  clearDerivedTables();

  /*
    Mark all temporary tables used by this statement as free for reuse.
  */
  mark_temp_tables_as_free_for_reuse();
  /*
    Let us commit transaction for statement. Since in 5.0 we only have
    one statement transaction and don't allow several nested statement
    transactions this call will do nothing if we are inside of stored
    function or trigger (i.e. statement transaction is already active and
    does not belong to statement for which we do close_thread_tables()).
    TODO: This should be fixed in later releases.
   */
  {
    TransactionServices &transaction_services= TransactionServices::singleton();
    main_da.can_overwrite_status= true;
    transaction_services.autocommitOrRollback(this, is_error());
    main_da.can_overwrite_status= false;
    transaction.stmt.reset();
  }

  if (lock)
  {
    /*
      For RBR we flush the pending event just before we unlock all the
      tables.  This means that we are at the end of a topmost
      statement, so we ensure that the STMT_END_F flag is set on the
      pending event.  For statements that are *inside* stored
      functions, the pending event will not be flushed: that will be
      handled either before writing a query log event (inside
      binlog_query()) or when preparing a pending event.
     */
    unlockTables(lock);
    lock= 0;
  }
  /*
    Note that we need to hold LOCK_open while changing the
    open_tables list. Another thread may work on it.
    (See: table::Cache::singleton().removeTable(), mysql_wait_completed_table())
    Closing a MERGE child before the parent would be fatal if the
    other thread tries to abort the MERGE lock in between.
  */
  if (open_tables)
    close_open_tables();
}

void Session::close_tables_for_reopen(TableList **tables)
{
  /*
    If table list consists only from tables from prelocking set, table list
    for new attempt should be empty, so we have to update list's root pointer.
  */
  if (lex->first_not_own_table() == *tables)
    *tables= 0;
  lex->chop_off_not_own_tables();
  for (TableList *tmp= *tables; tmp; tmp= tmp->next_global)
    tmp->table= 0;
  close_thread_tables();
}

bool Session::openTablesLock(TableList *tables)
{
  uint32_t counter;
  bool need_reopen;

  for ( ; ; )
  {
    if (open_tables_from_list(&tables, &counter))
      return true;

    if (not lock_tables(tables, counter, &need_reopen))
      break;
    if (not need_reopen)
      return true;
    close_tables_for_reopen(&tables);
  }
  if ((mysql_handle_derived(lex, &mysql_derived_prepare) ||
       (
        mysql_handle_derived(lex, &mysql_derived_filling))))
    return true;

  return false;
}

/*
  @note "best_effort" is used in cases were if a failure occurred on this
  operation it would not be surprising because we are only removing because there
  might be an issue (lame engines).
*/

bool Open_tables_state::rm_temporary_table(TableIdentifier &identifier, bool best_effort)
{
  if (plugin::StorageEngine::dropTable(*static_cast<Session *>(this), identifier))
  {
    if (not best_effort)
    {
      errmsg_printf(ERRMSG_LVL_WARN, _("Could not remove temporary table: '%s', error: %d"),
                    identifier.getSQLPath().c_str(), errno);
    }

    return true;
  }

  return false;
}

bool Open_tables_state::rm_temporary_table(plugin::StorageEngine *base, TableIdentifier &identifier)
{
  assert(base);

  if (plugin::StorageEngine::dropTable(*static_cast<Session *>(this), *base, identifier))
  {
    errmsg_printf(ERRMSG_LVL_WARN, _("Could not remove temporary table: '%s', error: %d"),
                  identifier.getSQLPath().c_str(), errno);

    return true;
  }

  return false;
}

/**
  @note this will be removed, I am looking through Hudson to see if it is finding
  any tables that are missed during cleanup.
*/
void Open_tables_state::dumpTemporaryTableNames(const char *foo)
{
  Table *table;

  if (not temporary_tables)
    return;

  cerr << "Begin Run: " << foo << "\n";
  for (table= temporary_tables; table; table= table->getNext())
  {
    bool have_proto= false;

    message::Table *proto= table->getShare()->getTableProto();
    if (table->getShare()->getTableProto())
      have_proto= true;

    const char *answer= have_proto ? "true" : "false";

    if (have_proto)
    {
      cerr << "\tTable Name " << table->getShare()->getSchemaName() << "." << table->getShare()->getTableName() << " : " << answer << "\n";
      cerr << "\t\t Proto " << proto->schema() << " " << proto->name() << "\n";
    }
    else
      cerr << "\tTabl;e Name " << table->getShare()->getSchemaName() << "." << table->getShare()->getTableName() << " : " << answer << "\n";
  }
}

bool Session::TableMessages::storeTableMessage(const TableIdentifier &identifier, message::Table &table_message)
{
  table_message_cache.insert(make_pair(identifier.getPath(), table_message));

  return true;
}

bool Session::TableMessages::removeTableMessage(const TableIdentifier &identifier)
{
  TableMessageCache::iterator iter;

  iter= table_message_cache.find(identifier.getPath());

  if (iter == table_message_cache.end())
    return false;

  table_message_cache.erase(iter);

  return true;
}

bool Session::TableMessages::getTableMessage(const TableIdentifier &identifier, message::Table &table_message)
{
  TableMessageCache::iterator iter;

  iter= table_message_cache.find(identifier.getPath());

  if (iter == table_message_cache.end())
    return false;

  table_message.CopyFrom(((*iter).second));

  return true;
}

bool Session::TableMessages::doesTableMessageExist(const TableIdentifier &identifier)
{
  TableMessageCache::iterator iter;

  iter= table_message_cache.find(identifier.getPath());

  if (iter == table_message_cache.end())
  {
    return false;
  }

  return true;
}

bool Session::TableMessages::renameTableMessage(const TableIdentifier &from, const TableIdentifier &to)
{
  TableMessageCache::iterator iter;

  table_message_cache[to.getPath()]= table_message_cache[from.getPath()];

  iter= table_message_cache.find(to.getPath());

  if (iter == table_message_cache.end())
  {
    return false;
  }

  (*iter).second.set_schema(to.getSchemaName());
  (*iter).second.set_name(to.getTableName());

  return true;
}

table::Instance *Session::getInstanceTable()
{
  temporary_shares.push_back(new table::Instance()); // This will not go into the tableshare cache, so no key is used.

  table::Instance *tmp_share= temporary_shares.back();

  assert(tmp_share);

  return tmp_share;
}


/**
  Create a reduced Table object with properly set up Field list from a
  list of field definitions.

    The created table doesn't have a table Cursor associated with
    it, has no keys, no group/distinct, no copy_funcs array.
    The sole purpose of this Table object is to use the power of Field
    class to read/write data to/from table->getInsertRecord(). Then one can store
    the record in any container (RB tree, hash, etc).
    The table is created in Session mem_root, so are the table's fields.
    Consequently, if you don't BLOB fields, you don't need to free it.

  @param session         connection handle
  @param field_list  list of column definitions

  @return
    0 if out of memory, Table object in case of success
*/
table::Instance *Session::getInstanceTable(List<CreateField> &field_list)
{
  temporary_shares.push_back(new table::Instance(this, field_list)); // This will not go into the tableshare cache, so no key is used.

  table::Instance *tmp_share= temporary_shares.back();

  assert(tmp_share);

  return tmp_share;
}

} /* namespace drizzled */
