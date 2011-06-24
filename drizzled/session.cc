/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>

#include <boost/checked_delete.hpp>
#include <boost/filesystem.hpp>
#include <drizzled/copy_field.h>
#include <drizzled/data_home.h>
#include <drizzled/diagnostics_area.h>
#include <drizzled/display.h>
#include <drizzled/drizzled.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/ha_data.h>
#include <drizzled/identifier.h>
#include <drizzled/internal/iocache.h>
#include <drizzled/internal/thread_var.h>
#include <drizzled/item/cache.h>
#include <drizzled/item/empty_string.h>
#include <drizzled/item/float.h>
#include <drizzled/item/return_int.h>
#include <drizzled/item/subselect.h>
#include <drizzled/lock.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/plugin/authentication.h>
#include <drizzled/plugin/authorization.h>
#include <drizzled/plugin/client.h>
#include <drizzled/plugin/event_observer.h>
#include <drizzled/plugin/logging.h>
#include <drizzled/plugin/query_rewrite.h>
#include <drizzled/plugin/scheduler.h>
#include <drizzled/plugin/transactional_storage_engine.h>
#include <drizzled/probes.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/query_id.h>
#include <drizzled/schema.h>
#include <drizzled/select_dump.h>
#include <drizzled/select_exists_subselect.h>
#include <drizzled/select_export.h>
#include <drizzled/select_max_min_finder_subselect.h>
#include <drizzled/select_singlerow_subselect.h>
#include <drizzled/select_subselect.h>
#include <drizzled/select_to_file.h>
#include <drizzled/session.h>
#include <drizzled/session/cache.h>
#include <drizzled/session/state.h>
#include <drizzled/session/table_messages.h>
#include <drizzled/session/times.h>
#include <drizzled/session/transactions.h>
#include <drizzled/show.h>
#include <drizzled/sql_base.h>
#include <drizzled/sql_lex.h>
#include <drizzled/system_variables.h>
#include <drizzled/statement.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/table/singular.h>
#include <drizzled/table_proto.h>
#include <drizzled/tmp_table_param.h>
#include <drizzled/transaction_services.h>
#include <drizzled/user_var_entry.h>
#include <drizzled/util/backtrace.h>
#include <drizzled/util/find_ptr.h>
#include <drizzled/util/functors.h>
#include <drizzled/util/storable.h>
#include <plugin/myisam/myisam.h>

#include <algorithm>
#include <climits>
#include <fcntl.h>
#include <sys/stat.h>

using namespace std;

namespace fs= boost::filesystem;

namespace drizzled {

/*
  The following is used to initialise Table_ident with a internal
  table name
*/
char internal_table_name[2]= "*";
char empty_c_string[1]= {0};    /* used for not defined db */

const char* const Session::DEFAULT_WHERE= "field list";

uint64_t g_refresh_version = 1;

bool Key_part_spec::operator==(const Key_part_spec& other) const
{
  return length == other.length &&
         field_name.length == other.field_name.length &&
    !my_strcasecmp(system_charset_info, field_name.str, other.field_name.str);
}

Open_tables_state::Open_tables_state(Session& session, uint64_t version_arg) :
  version(version_arg),
  session_(session)
{
  open_tables_= temporary_tables= derived_tables= NULL;
  extra_lock= lock= NULL;
}

/*
  The following functions form part of the C plugin API
*/
int tmpfile(const char *prefix)
{
  char filename[FN_REFLEN];
  int fd = internal::create_temp_file(filename, drizzle_tmpdir.c_str(), prefix, MYF(MY_WME));
  if (fd >= 0)
    unlink(filename);
  return fd;
}

void **Session::getEngineData(const plugin::MonitoredInTransaction *monitored)
{
  return static_cast<void **>(&ha_data[monitored->getId()].ha_ptr);
}

ResourceContext& Session::getResourceContext(const plugin::MonitoredInTransaction& monitored, size_t index)
{
  return ha_data[monitored.getId()].resource_context[index];
}

int64_t session_test_options(const Session *session, int64_t test_options)
{
  return session->options & test_options;
}

class Session::impl_c
{
public:
  typedef boost::unordered_map<std::string, util::Storable*, util::insensitive_hash, util::insensitive_equal_to> properties_t;
  typedef std::map<std::string, plugin::EventObserverList*> schema_event_observers_t;

  impl_c(Session& session) :
    open_tables(session, g_refresh_version),
    schema(boost::make_shared<std::string>())
  {
  }

  ~impl_c()
  {
    BOOST_FOREACH(properties_t::reference it, properties)
      delete it.second;
  }

  Diagnostics_area diagnostics;
  memory::Root mem_root;

  /**
    The lex to hold the parsed tree of conventional (non-prepared) queries.
    Whereas for prepared and stored procedure statements we use an own lex
    instance for each new query, for conventional statements we reuse
    the same lex. (@see mysql_parse for details).
  */
  LEX lex;
  Open_tables_state open_tables;
  properties_t properties;
  schema_event_observers_t schema_event_observers;
  system_status_var status_var;
  session::TableMessages table_message_cache;
  util::string::mptr schema;
  boost::shared_ptr<session::State> state;
  std::vector<table::Singular*> temporary_shares;
	session::Times times;
	session::Transactions transaction;
  drizzle_system_variables variables;
};

Session::Session(plugin::Client *client_arg, catalog::Instance::shared_ptr catalog_arg) :
  impl_(new impl_c(*this)),
  mem(impl_->mem_root),
  mem_root(&impl_->mem_root),
  query(new std::string),
  scheduler(NULL),
  variables(impl_->variables),
  status_var(impl_->status_var),
  lock_id(&main_lock_id),
  thread_stack(NULL),
  _where(Session::DEFAULT_WHERE),
  mysys_var(0),
  command(COM_CONNECT),
  ha_data(plugin::num_trx_monitored_objects),
  query_id(0),
  warn_query_id(0),
	transaction(impl_->transaction),
  open_tables(impl_->open_tables),
	times(impl_->times),
  first_successful_insert_id_in_prev_stmt(0),
  first_successful_insert_id_in_cur_stmt(0),
  limit_found_rows(0),
  options(session_startup_options),
  row_count_func(-1),
  sent_row_count(0),
  examined_row_count(0),
  used_tables(0),
  total_warn_count(0),
  row_count(0),
  thread_id(0),
  tmp_table(0),
  _global_read_lock(NONE),
  count_cuted_fields(CHECK_FIELD_ERROR_FOR_NULL),
  _killed(NOT_KILLED),
  no_errors(false),
  is_fatal_error(false),
  transaction_rollback_request(false),
  is_fatal_sub_stmt_error(0),
  derived_tables_processing(false),
  m_lip(NULL),
  arg_of_last_insert_id_function(false),
  _catalog(catalog_arg),
  transaction_message(NULL),
  statement_message(NULL),
  session_event_observers(NULL),
  xa_id(0),
  concurrent_execute_allowed(true),
  tablespace_op(false),
  use_usage(false),
  security_ctx(identifier::User::make_shared()),
  originating_server_uuid_set(false),
  client(client_arg)
{
  client->setSession(this);

  /*
    Pass nominal parameters to init only to ensure that
    the destructor works OK in case of an error. The main_mem_root
    will be re-initialized in init_for_queries().
  */
  mem.init(memory::ROOT_MIN_BLOCK_SIZE);
  cuted_fields= sent_row_count= row_count= 0L;
  // Must be reset to handle error with Session's created for init of mysqld
  lex().current_select= 0;
  memset(&variables, 0, sizeof(variables));
  scoreboard_index= -1;
  originating_server_uuid= "";
  originating_commit_id= 0;
  cleanup_done= abort_on_warning= no_warnings_for_error= false;

  /* query_cache init */
  query_cache_key= "";
  resultset= NULL;

  /* Variables with default values */
  proc_info="login";

  plugin_sessionvar_init(this);
  /*
    variables= global_system_variables above has reset
    variables.pseudo_thread_id to 0. We need to correct it here to
    avoid temporary tables replication failure.
  */
  variables.pseudo_thread_id= thread_id;
  server_status= SERVER_STATUS_AUTOCOMMIT;

  if (variables.max_join_size == HA_POS_ERROR)
    options |= OPTION_BIG_SELECTS;
  else
    options &= ~OPTION_BIG_SELECTS;

  open_options=ha_open_options;
  update_lock_default= TL_WRITE;
  session_tx_isolation= (enum_tx_isolation) variables.tx_isolation;
  memset(warn_count, 0, sizeof(warn_count));
  memset(&status_var, 0, sizeof(status_var));

  /* Initialize sub structures */
  warn_root.init(WARN_ALLOC_BLOCK_SIZE);

  substitute_null_with_insert_id = false;
  lock_info.init(); /* safety: will be reset after start */
  thr_lock_owner_init(&main_lock_id, &lock_info);

  plugin::EventObserver::registerSessionEvents(*this);
}

Diagnostics_area& Session::main_da()
{
  return impl_->diagnostics;
}

const LEX& Session::lex() const
{
  return impl_->lex;
}

LEX& Session::lex()
{
  return impl_->lex;
}

enum_sql_command Session::getSqlCommand() const
{
  return lex().sql_command;
}

session::TableMessages& Session::getMessageCache()
{
  return impl_->table_message_cache;
}

void statement::Statement::set_command(enum_sql_command v)
{
	session().lex().sql_command= v;
}

LEX& statement::Statement::lex()
{
	return session().lex();
}

session::Transactions& statement::Statement::transaction()
{
	return session().transaction;
}

void Session::add_item_to_list(Item *item)
{
  lex().current_select->add_item_to_list(this, item);
}

void Session::add_value_to_list(Item *value)
{
	lex().value_list.push_back(value);
}

void Session::add_order_to_list(Item *item, bool asc)
{
  lex().current_select->add_order_to_list(this, item, asc);
}

void Session::add_group_to_list(Item *item, bool asc)
{
  lex().current_select->add_group_to_list(this, item, asc);
}

void Session::free_items()
{
  /* This works because items are allocated with memory::sql_alloc() */
  for (Item* next; free_list; free_list= next)
  {
    next= free_list->next;
    free_list->delete_self();
  }
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
  boost::mutex::scoped_lock scopedLock(mysys_var->mutex);
  if (mysys_var->current_cond)
  {
    mysys_var->current_mutex->lock();
    mysys_var->current_cond->notify_all();
    mysys_var->current_mutex->unlock();
  }
}

void Session::get_xid(DrizzleXid *xid)
{
  *xid = *(DrizzleXid *) &transaction.xid_state.xid;
}

/* Do operations that may take a long time */

void Session::cleanup()
{
  assert(not cleanup_done);

  setKilled(KILL_CONNECTION);
#ifdef ENABLE_WHEN_BINLOG_WILL_BE_ABLE_TO_PREPARE
  if (transaction.xid_state.xa_state == XA_PREPARED)
  {
#error xid_state in the cache should be replaced by the allocated value
  }
#endif
  {
    TransactionServices::rollbackTransaction(*this, true);
  }

  BOOST_FOREACH(UserVars::reference iter, user_vars)
    boost::checked_delete(iter.second);
  user_vars.clear();

  open_tables.close_temporary_tables();

  if (global_read_lock)
    unlockGlobalReadLock();

  cleanup_done= true;
}

Session::~Session()
{
  if (client and client->isConnected())
  {
    assert(security_ctx);
    if (global_system_variables.log_warnings)
    {
      errmsg_printf(error::WARN, ER(ER_FORCING_CLOSE),
                    internal::my_progname,
                    thread_id,
                    security_ctx->username().c_str());
    }

    disconnect();
  }

  /* Close connection */
  if (client)
  {
    client->close();
    boost::checked_delete(client);
    client= NULL;
  }

  if (not cleanup_done)
    cleanup();

  plugin::StorageEngine::closeConnection(*this);
  plugin_sessionvar_cleanup(this);

  warn_root.free_root(MYF(0));
  mysys_var=0;					// Safety (shouldn't be needed)

  impl_->mem_root.free_root(MYF(0));
  setCurrentMemRoot(NULL);
  setCurrentSession(NULL);

  plugin::Logging::postEndDo(this);
  plugin::EventObserver::deregisterSessionEvents(session_event_observers); 

	BOOST_FOREACH(impl_c::schema_event_observers_t::reference it, impl_->schema_event_observers)
    plugin::EventObserver::deregisterSchemaEvents(it.second);
}

void Session::setClient(plugin::Client *client_arg)
{
  client= client_arg;
  client->setSession(this);
}

void Session::awake(Session::killed_state_t state_to_set)
{
  if (state_to_set == Session::KILL_QUERY && command == COM_SLEEP)
    return;

  setKilled(state_to_set);
  scheduler->killSession(this);

  if (state_to_set != Session::KILL_QUERY)
  {
    DRIZZLE_CONNECTION_DONE(thread_id);
  }

  if (mysys_var)
  {
    boost::mutex::scoped_lock scopedLock(mysys_var->mutex);
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
void Session::storeGlobals()
{
  /*
    Assert that thread_stack is initialized: it's necessary to be able
    to track stack overrun.
  */
  assert(thread_stack);
  setCurrentSession(this);
  setCurrentMemRoot(&mem);

  mysys_var= my_thread_var;

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

  open_tables.version= g_refresh_version;
  set_proc_info(NULL);
  command= COM_SLEEP;
  times.set_time();

  mem.reset_defaults(variables.query_alloc_block_size, variables.query_prealloc_size);
  transaction.xid_state.xid.set_null();
  transaction.xid_state.in_session=1;
  if (use_usage)
    resetUsage();
}

void Session::run()
{
  storeGlobals();
  if (authenticate())
  {
    disconnect();
    return;
  }
  prepareForQueries();
  while (not client->haveError() && getKilled() != KILL_CONNECTION)
  {
    if (not executeStatement())
      break;
  }
  disconnect();
}

bool Session::schedule(const shared_ptr& arg)
{
  arg->scheduler= plugin::Scheduler::getScheduler();
  assert(arg->scheduler);

  ++connection_count;

  long current_connections= connection_count;

  if (current_connections > 0 and static_cast<uint64_t>(current_connections) > current_global_counters.max_used_connections)
  {
    current_global_counters.max_used_connections= static_cast<uint64_t>(connection_count);
  }

  current_global_counters.connections++;
  arg->thread_id= arg->variables.pseudo_thread_id= global_thread_id++;

  session::Cache::insert(arg);

  if (unlikely(plugin::EventObserver::connectSession(*arg)))
  {
    // We should do something about an error...
  }

  if (plugin::Scheduler::getScheduler()->addSession(arg))
  {
    DRIZZLE_CONNECTION_START(arg->getSessionId());
    char error_message_buff[DRIZZLE_ERRMSG_SIZE];

    arg->setKilled(Session::KILL_CONNECTION);

    arg->status_var.aborted_connects++;

    /* Can't use my_error() since store_globals has not been called. */
    /* TODO replace will better error message */
    snprintf(error_message_buff, sizeof(error_message_buff), ER(ER_CANT_CREATE_THREAD), 1);
    arg->client->sendError(ER_CANT_CREATE_THREAD, error_message_buff);
    return true;
  }
  return false;
}


/*
  Is this session viewable by the current user?
*/
bool Session::isViewable(const identifier::User& user_arg) const
{
  return plugin::Authorization::isAuthorized(user_arg, *this, false);
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
  boost::mutex::scoped_lock scopedLock(mysys_var->mutex);
  mysys_var->current_mutex = 0;
  mysys_var->current_cond = 0;
  this->set_proc_info(old_msg);
}

bool Session::authenticate()
{
  if (client->authenticate())
    return false;

  status_var.aborted_connects++;

  return true;
}

bool Session::checkUser(const std::string &passwd_str, const std::string &in_db)
{
  if (not plugin::Authentication::isAuthenticated(*user(), passwd_str))
  {
    status_var.access_denied++;
    /* isAuthenticated has pushed the error message */
    return false;
  }

  /* Change database if necessary */
  if (not in_db.empty() && schema::change(*this, identifier::Schema(in_db)))
    return false; // change() has pushed the error message
  my_ok();

  /* Ready to handle queries */
  return true;
}

bool Session::executeStatement()
{
  /*
    indicator of uninitialized lex => normal flow of errors handling
    (see my_message_sql)
  */
  lex().current_select= 0;
  clear_error();
  main_da().reset_diagnostics_area();
  char *l_packet= 0;
  uint32_t packet_length;
  if (not client->readCommand(&l_packet, packet_length))
    return false;

  if (getKilled() == KILL_CONNECTION)
    return false;

  if (packet_length == 0)
    return true;

  enum_server_command l_command= static_cast<enum_server_command>(l_packet[0]);

  if (command >= COM_END)
    command= COM_END;                           // Wrong command

  assert(packet_length);
  return not dispatch_command(l_command, this, l_packet+1, (uint32_t) (packet_length-1));
}

void Session::readAndStoreQuery(const char *in_packet, uint32_t in_packet_length)
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

  util::string::mptr new_query= boost::make_shared<std::string>(in_packet, in_packet_length);
  plugin::QueryRewriter::rewriteQuery(*impl_->schema, *new_query);
  query= new_query;
  impl_->state= boost::make_shared<session::State>(in_packet, in_packet_length);
}

bool Session::endTransaction(enum_mysql_completiontype completion)
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
      if (TransactionServices::commitTransaction(*this, true))
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
      if (TransactionServices::rollbackTransaction(*this, true))
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

  if (not result)
  {
    my_error(static_cast<drizzled::error_t>(killed_errno()), MYF(0));
  }
  else if (result && do_release)
  {
    setKilled(Session::KILL_CONNECTION);
  }

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
  if (options & (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
  {
    server_status&= ~SERVER_STATUS_IN_TRANS;
    if (TransactionServices::commitTransaction(*this, true))
      result= false;
  }
  options&= ~(OPTION_BEGIN);
  return result;
}

bool Session::startTransaction(start_transaction_option_t opt)
{
  assert(not inTransaction());

  options|= OPTION_BEGIN;
  server_status|= SERVER_STATUS_IN_TRANS;

  if (plugin::TransactionalStorageEngine::notifyStartTransaction(this, opt))
    return false;
  return true;
}

void Session::cleanup_after_query()
{
  /*
    Reset rand_used so that detection of calls to rand() will save random
    seeds if needed by the slave.
  */
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

  /* Reset _where. */
  _where= Session::DEFAULT_WHERE;

  /* Reset the temporary shares we built */
  for_each(impl_->temporary_shares.begin(), impl_->temporary_shares.end(), DeletePtr());
  impl_->temporary_shares.clear();
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
    lex_str= new (mem) LEX_STRING;
  lex_str->str= mem_root->strmake(str, length);
  lex_str->length= length;
  return lex_str;
}

void Session::send_explain_fields(select_result *result)
{
  List<Item> field_list;
  Item *item;
  const charset_info_st* cs= system_charset_info;
  field_list.push_back(new Item_return_int("id",3, DRIZZLE_TYPE_LONGLONG));
  field_list.push_back(new Item_empty_string("select_type", 19, cs));
  field_list.push_back(item= new Item_empty_string("table", NAME_CHAR_LEN, cs));
  item->maybe_null= 1;
  field_list.push_back(item= new Item_empty_string("type", 10, cs));
  item->maybe_null= 1;
  field_list.push_back(item= new Item_empty_string("possible_keys", NAME_CHAR_LEN*MAX_KEY, cs));
  item->maybe_null=1;
  field_list.push_back(item= new Item_empty_string("key", NAME_CHAR_LEN, cs));
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
  if (lex().describe & DESCRIBE_EXTENDED)
  {
    field_list.push_back(item= new Item_float("filtered", 0.1234, 2, 4));
    item->maybe_null=1;
  }
  item->maybe_null= 1;
  field_list.push_back(new Item_empty_string("Extra", 255, cs));
  result->send_fields(field_list);
}

void select_result::send_error(drizzled::error_t errcode, const char *err)
{
  my_message(errcode, err, MYF(0));
}

/************************************************************************
  Handling writing to file
************************************************************************/

void select_to_file::send_error(drizzled::error_t errcode,const char *err)
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
    cache(static_cast<internal::io_cache_st *>(memory::sql_calloc(sizeof(internal::io_cache_st)))),
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


static int create_file(Session& session,
                       fs::path &target_path,
                       file_exchange *exchange,
                       internal::io_cache_st *cache)
{
  fs::path to_file(exchange->file_name);

  if (not to_file.has_root_directory())
  {
    target_path= fs::system_complete(getDataHomeCatalog());
    util::string::ptr schema(session.schema());
    if (not schema->empty())
    {
      int count_elements= 0;
      for (fs::path::iterator it= to_file.begin(); it != to_file.end(); it++)
        count_elements++;
      if (count_elements == 1)
        target_path /= *schema;
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
  int file= internal::my_create(target_path.file_string().c_str(), 0666, O_WRONLY|O_EXCL, MYF(MY_WME));
  if (file < 0)
    return file;
  (void) fchmod(file, 0666);			// Because of umask()
  if (cache->init_io_cache(file, 0, internal::WRITE_CACHE, 0L, 1, MYF(MY_WME)))
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
    List<Item>::iterator li(list.begin());
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

  if ((file= create_file(*session, path, exchange, cache)) < 0)
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
  uint32_t used_length=0,items_left=items.size();
  List<Item>::iterator li(items.begin());

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
        const charset_info_st * const res_charset= res->charset();
        const charset_info_st * const character_set_client= default_charset_info;

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
  return (file= create_file(*session, path, exchange, cache)) < 0;
}


bool select_dump::send_data(List<Item> &items)
{
  List<Item>::iterator li(items.begin());
  char buff[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff),&my_charset_bin),*res;
  tmp.length(0);
  Item *item;

  if (unit->offset_limit_cnt)
  {						// using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
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
  return 0;
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
    return 1;
  }
  if (unit->offset_limit_cnt)
  {				          // Using limit offset,count
    unit->offset_limit_cnt--;
    return 0;
  }
  List<Item>::iterator li(items.begin());
  Item *val_item;
  for (uint32_t i= 0; (val_item= li++); i++)
    it->store(i, val_item);
  it->assigned(1);
  return 0;
}


void select_max_min_finder_subselect::cleanup()
{
  cache= 0;
}


bool select_max_min_finder_subselect::send_data(List<Item> &items)
{
  Item_maxmin_subselect *it= (Item_maxmin_subselect *)item;
  List<Item>::iterator li(items.begin());
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
  return 0;
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
  type::Decimal cval, *cvalue= cache->val_decimal(&cval);
  type::Decimal mval, *mvalue= maxmin->val_decimal(&mval);
  if (fmax)
    return (cache->null_value && !maxmin->null_value) ||
      (!cache->null_value && !maxmin->null_value &&
       class_decimal_cmp(cvalue, mvalue) > 0) ;
  return (maxmin->null_value && !cache->null_value) ||
    (!cache->null_value && !maxmin->null_value &&
     class_decimal_cmp(cvalue,mvalue) < 0);
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
    return 0;
  }
  it->value= 1;
  it->assigned(1);
  return 0;
}

/*
  Don't free mem_root, as mem_root is freed in the end of dispatch_command
  (once for any command).
*/
void Session::end_statement()
{
  /* Cleanup SQL processing state to reuse this statement in next query. */
  lex().end();
  query_cache_key= ""; // reset the cache key
  resetResultsetMessage();
}

bool Session::copy_db_to(char **p_db, size_t *p_db_length)
{
  if (impl_->schema->empty())
  {
    my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR), MYF(0));
    return true;
  }
  *p_db= mem.strmake(*impl_->schema);
  *p_db_length= impl_->schema->size();
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
    boost::checked_array_delete(copy_field);
    save_copy_field= save_copy_field_end= copy_field= copy_field_end= 0;
  }
}

void Session::send_kill_message() const
{
  drizzled::error_t err= static_cast<drizzled::error_t>(killed_errno());
  if (err != EE_OK)
    my_message(err, ER(err), MYF(0));
}

void Session::set_db(const std::string& new_db)
{
  impl_->schema = boost::make_shared<std::string>(new_db);
}


/**
  Mark transaction to rollback and mark error as fatal to a sub-statement.

  @param  session   Thread handle
  @param  all   true <=> rollback main transaction().
*/
void Session::markTransactionForRollback(bool all)
{
  is_fatal_sub_stmt_error= true;
  transaction_rollback_request= all;
}

void Session::disconnect(error_t errcode)
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
      errmsg_printf(error::WARN, ER(ER_NEW_ABORTING_CONNECTION)
                  , thread_id
                  , (impl_->schema->empty() ? "unconnected" : impl_->schema->c_str())
                  , security_ctx->username().empty() ? "unauthenticated" : security_ctx->username().c_str()
                  , security_ctx->address().c_str()
                  , (main_da().is_error() ? main_da().message() : ER(ER_UNKNOWN_ERROR)));
    }
  }

  setKilled(Session::KILL_CONNECTION);

  if (client->isConnected())
  {
    if (errcode != EE_OK)
    {
      /*my_error(errcode, ER(errcode));*/
      client->sendError(errcode, ER(errcode));
    }
    client->close();
  }
}

void Session::reset_for_next_command()
{
  free_list= 0;
  select_number= 1;

  is_fatal_error= false;
  server_status&= ~ (SERVER_MORE_RESULTS_EXISTS |
                          SERVER_QUERY_NO_INDEX_USED |
                          SERVER_QUERY_NO_GOOD_INDEX_USED);

  clear_error();
  main_da().reset_diagnostics_area();
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
  plugin::StorageEngine& table_type= *table->getShare()->db_type();
  table->free_io_cache();
  table->delete_table();
  rm_temporary_table(table_type, identifier::Table(table->getShare()->getSchemaName(), table->getShare()->getTableName(), table->getShare()->getPath()));
  boost::checked_delete(table->getMutableShare());
  boost::checked_delete(table);
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
  if (cleanup_done)
    return NULL;

  if (UserVars::mapped_type* iter= find_ptr(user_vars, name))
    return *iter;

  if (not create_if_not_exists)
    return NULL;

  user_var_entry *entry= new user_var_entry(name.c_str(), query_id);

  std::pair<UserVars::iterator, bool> returnable= user_vars.insert(make_pair(name, entry));

  if (not returnable.second)
  {
    boost::checked_delete(entry);
  }

  return entry;
}

void Session::setVariable(const std::string &name, const std::string &value)
{
  user_var_entry *updateable_var= getVariable(name.c_str(), true);
  if (updateable_var)
  {
    updateable_var->update_hash(false,
                                (void*)value.c_str(),
                                static_cast<uint32_t>(value.length()), STRING_RESULT,
                                &my_charset_bin,
                                DERIVATION_IMPLICIT, false);
  }
}

void Open_tables_state::mark_temp_tables_as_free_for_reuse()
{
  for (Table *table= temporary_tables ; table ; table= table->getNext())
  {
    if (table->query_id == session_.getQueryId())
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
  open_tables.clearDerivedTables();

  /*
    Mark all temporary tables used by this statement as free for reuse.
  */
  open_tables.mark_temp_tables_as_free_for_reuse();
  /*
    Let us commit transaction for statement. Since in 5.0 we only have
    one statement transaction and don't allow several nested statement
    transactions this call will do nothing if we are inside of stored
    function or trigger (i.e. statement transaction is already active and
    does not belong to statement for which we do close_thread_tables()).
    TODO: This should be fixed in later releases.
   */
  {
    main_da().can_overwrite_status= true;
    TransactionServices::autocommitOrRollback(*this, is_error());
    main_da().can_overwrite_status= false;
    transaction.stmt.reset();
  }

  if (open_tables.lock)
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
    unlockTables(open_tables.lock);
    open_tables.lock= 0;
  }
  /*
    Note that we need to hold table::Cache::mutex() while changing the
    open_tables list. Another thread may work on it.
    (See: table::Cache::removeTable(), wait_completed_table())
    Closing a MERGE child before the parent would be fatal if the
    other thread tries to abort the MERGE lock in between.
  */
  if (open_tables.open_tables_)
    open_tables.close_open_tables();
}

void Session::close_tables_for_reopen(TableList **tables)
{
  /*
    If table list consists only from tables from prelocking set, table list
    for new attempt should be empty, so we have to update list's root pointer.
  */
  if (lex().first_not_own_table() == *tables)
    *tables= 0;
  lex().chop_off_not_own_tables();
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

  return handle_derived(&lex(), &derived_prepare) || handle_derived(&lex(), &derived_filling);
}

/*
  @note "best_effort" is used in cases were if a failure occurred on this
  operation it would not be surprising because we are only removing because there
  might be an issue (lame engines).
*/

bool Open_tables_state::rm_temporary_table(const identifier::Table &identifier, bool best_effort)
{
  if (plugin::StorageEngine::dropTable(session_, identifier))
		return false;
  if (not best_effort)
    errmsg_printf(error::WARN, _("Could not remove temporary table: '%s', error: %d"), identifier.getSQLPath().c_str(), errno);
  return true;
}

bool Open_tables_state::rm_temporary_table(plugin::StorageEngine& base, const identifier::Table &identifier)
{
  drizzled::error_t error;
  if (plugin::StorageEngine::dropTable(session_, base, identifier, error))
		return false;
  errmsg_printf(error::WARN, _("Could not remove temporary table: '%s', error: %d"), identifier.getSQLPath().c_str(), error);
  return true;
}

table::Singular& Session::getInstanceTable()
{
  impl_->temporary_shares.push_back(new table::Singular); // This will not go into the tableshare cache, so no key is used.
  return *impl_->temporary_shares.back();
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
table::Singular& Session::getInstanceTable(std::list<CreateField>& field_list)
{
  impl_->temporary_shares.push_back(new table::Singular(this, field_list)); // This will not go into the tableshare cache, so no key is used.
  return *impl_->temporary_shares.back();
}

void Session::clear_error(bool full)
{
  if (main_da().is_error())
    main_da().reset_diagnostics_area();

  if (full)
  {
    drizzle_reset_errors(this, true);
  }
}

void Session::clearDiagnostics()
{
  main_da().reset_diagnostics_area();
}

/**
  true if there is an error in the error stack.

  Please use this method instead of direct access to
  net.report_error.

  If true, the current (sub)-statement should be aborted.
  The main difference between this member and is_fatal_error
  is that a fatal error can not be handled by a stored
  procedure continue handler, whereas a normal error can.

  To raise this flag, use my_error().
*/
bool Session::is_error() const 
{ 
  return impl_->diagnostics.is_error(); 
}

/** A short cut for session->main_da().set_ok_status(). */
void Session::my_ok(ha_rows affected_rows, ha_rows found_rows_arg, uint64_t passed_id, const char *message)
{
  main_da().set_ok_status(this, affected_rows, found_rows_arg, passed_id, message);
}

/** A short cut for session->main_da().set_eof_status(). */

void Session::my_eof()
{
  main_da().set_eof_status(this);
}

plugin::StorageEngine* Session::getDefaultStorageEngine()
{
  return variables.storage_engine ? variables.storage_engine : global_system_variables.storage_engine;
}

enum_tx_isolation Session::getTxIsolation()
{
  return (enum_tx_isolation)variables.tx_isolation;
}

drizzled::util::Storable* Session::getProperty0(const std::string& arg)
{
  return impl_->properties[arg];
}

void Session::setProperty0(const std::string& arg, drizzled::util::Storable* value)
{
  // assert(not _properties.count(arg));
  impl_->properties[arg]= value;
}

plugin::EventObserverList* Session::getSchemaObservers(const std::string &db_name)
{
  if (impl_c::schema_event_observers_t::mapped_type* i= find_ptr(impl_->schema_event_observers, db_name))
    return *i;
  return NULL;
}

plugin::EventObserverList* Session::setSchemaObservers(const std::string &db_name, plugin::EventObserverList* observers)
{
  impl_->schema_event_observers.erase(db_name);
  if (observers)
    impl_->schema_event_observers[db_name] = observers;
	return observers;
}

util::string::ptr Session::schema() const
{
  return impl_->schema;
}

void Session::resetQueryString()
{
  query.reset();
  impl_->state.reset();
}

const boost::shared_ptr<session::State>& Session::state()
{
  return impl_->state;
}

const std::string& display::type(drizzled::Session::global_read_lock_t type)
{
  static const std::string NONE= "NONE";
  static const std::string GOT_GLOBAL_READ_LOCK= "HAS GLOBAL READ LOCK";
  static const std::string MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT= "HAS GLOBAL READ LOCK WITH BLOCKING COMMIT";

  switch (type) 
  {
    default:
    case Session::NONE:
      return NONE;
    case Session::GOT_GLOBAL_READ_LOCK:
      return GOT_GLOBAL_READ_LOCK;
    case Session::MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT:
      return MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT;
  }
}

size_t display::max_string_length(drizzled::Session::global_read_lock_t)
{
  return display::type(Session::MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT).size();
}

} /* namespace drizzled */
