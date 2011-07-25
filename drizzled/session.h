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

#pragma once

#include <algorithm>
#include <bitset>
#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/thread.hpp>
#include <map>
#include <netdb.h>
#include <string>
#include <sys/resource.h>
#include <sys/time.h>

#include <drizzled/charset.h>
#include <drizzled/base.h>
#include <drizzled/error.h>
#include <drizzled/lock.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/sql_error.h>
#include <drizzled/sql_locale.h>
#include <drizzled/visibility.h>
#include <drizzled/util/find_ptr.h>
#include <drizzled/util/string.h>
#include <drizzled/type/time.h>

namespace drizzled {

extern char internal_table_name[2];
extern char empty_c_string[1];
extern const char **errmesg;
extern uint32_t server_id;
extern std::string server_uuid;

#define TC_HEURISTIC_RECOVER_COMMIT   1
#define TC_HEURISTIC_RECOVER_ROLLBACK 2
extern uint32_t tc_heuristic_recover;

extern DRIZZLED_API struct drizzle_system_variables global_system_variables;

/**
 * Represents a client connection to the database server.
 *
 * Contains the client/server protocol object, the current statement
 * being executed, local-to-session variables and status counters, and
 * a host of other information.
 *
 * @todo
 *
 * The Session class should have a vector of Statement object pointers which
 * comprise the statements executed on the Session. Until this architectural
 * change is done, we can forget about parallel operations inside a session.
 *
 * @todo
 *
 * Make member variables private and have inlined accessors and setters.  Hide
 * all member variables that are not critical to non-internal operations of the
 * session object.
 */

class Open_tables_state;

class DRIZZLED_API Session
{
private:
  class impl_c;

  boost::scoped_ptr<impl_c> impl_;
public:
  typedef boost::shared_ptr<Session> shared_ptr;

  static shared_ptr make_shared(plugin::Client *client, boost::shared_ptr<catalog::Instance> instance_arg)
  {
    assert(instance_arg);
    return boost::make_shared<Session>(client, instance_arg);
  }

  /*
    MARK_COLUMNS_NONE:  Means mark_used_colums is not set and no indicator to
                        handler of fields used is set
    MARK_COLUMNS_READ:  Means a bit in read set is set to inform handler
	                that the field is to be read. If field list contains
                        duplicates, then session->dup_field is set to point
                        to the last found duplicate.
    MARK_COLUMNS_WRITE: Means a bit is set in write set to inform handler
			that it needs to update this field in write_row
                        and update_row.
  */
  enum_mark_columns mark_used_columns;

  /** Frees all items attached to this Statement */
  void free_items();

  /**
   * List of items created in the parser for this query. Every item puts
   * itself to the list on creation (see Item::Item() for details))
   */
  Item *free_list;
  memory::Root& mem;
  memory::Root* mem_root; /**< Pointer to current memroot */

  uint64_t getXaId()
  {
    return xa_id;
  }

  void setXaId(uint64_t in_xa_id)
  {
    xa_id= in_xa_id;
  }

public:
  Diagnostics_area& main_da();
  const LEX& lex() const;
  LEX& lex();
  enum_sql_command getSqlCommand() const;

  /** query associated with this statement */
  typedef boost::shared_ptr<const std::string> QueryString;

private:
  boost::shared_ptr<std::string> query;

  // Never allow for a modification of this outside of the class. c_str()
  // requires under some setup non const, you must copy the QueryString in
  // order to use it.
public:
  void resetQueryString();
  const boost::shared_ptr<session::State>& state();

  QueryString getQueryString() const
  {
    return query;
  }

  const char* getQueryStringCopy(size_t &length)
  {
    QueryString tmp_string(getQueryString());
    if (not tmp_string)
    {
      length= 0;
      return NULL;
    }
    length= tmp_string->length();
    return mem.strmake(*tmp_string);
  }

  util::string::ptr schema() const;

  /* current cache key */
  std::string query_cache_key;
  /**
    Constant for Session::where initialization in the beginning of every query.

    It's needed because we do not save/restore Session::where normally during
    primary (non subselect) query execution.
  */
  static const char* const DEFAULT_WHERE;

  memory::Root warn_root; /**< Allocation area for warnings and errors */
public:
  void setClient(plugin::Client *client_arg);

  plugin::Client *getClient() const
  {
    return client;
  }

  plugin::Scheduler* scheduler; /**< Pointer to scheduler object */

  typedef boost::unordered_map<std::string, user_var_entry*, util::insensitive_hash, util::insensitive_equal_to> UserVars;

private:
  typedef std::pair< UserVars::iterator, UserVars::iterator > UserVarsRange;
  UserVars user_vars; /**< Hash of user variables defined during the session's lifetime */

public:
  const UserVars &getUserVariables() const
  {
    return user_vars;
  }

  drizzle_system_variables& variables; /**< Mutable local variables local to the session */
  enum_tx_isolation getTxIsolation();
  system_status_var& status_var;

  THR_LOCK_INFO lock_info; /**< Locking information for this session */
  THR_LOCK_OWNER main_lock_id; /**< To use for conventional queries */
  THR_LOCK_OWNER *lock_id; /**< If not main_lock_id, points to the lock_id of a cursor. */

  /**
   * A pointer to the stack frame of the scheduler thread
   * which is called first in the thread for handling a client
   */
  char *thread_stack;

  identifier::user::ptr user() const
  {
    return security_ctx ? security_ctx : identifier::user::ptr();
  }

  void setUser(identifier::user::mptr arg)
  {
    security_ctx= arg;
  }

  int32_t getScoreboardIndex()
  {
    return scoreboard_index;
  }

  void setScoreboardIndex(int32_t in_scoreboard_index)
  {
    scoreboard_index= in_scoreboard_index;
  }

  bool isOriginatingServerUUIDSet()
  {
    return originating_server_uuid_set;
  }

  void setOriginatingServerUUID(std::string in_originating_server_uuid)
  {
    originating_server_uuid= in_originating_server_uuid;
    originating_server_uuid_set= true;
  }

  std::string &getOriginatingServerUUID()
  {
    return originating_server_uuid;
  }

  void setOriginatingCommitID(uint64_t in_originating_commit_id)
  {
    originating_commit_id= in_originating_commit_id;
  }

  uint64_t getOriginatingCommitID()
  {
    return originating_commit_id;
  }

  /**
   * Is this session viewable by the current user?
   */
  bool isViewable(const identifier::User&) const;

private:
  /**
    Used in error messages to tell user in what part of MySQL we found an
    error. E. g. when where= "having clause", if fix_fields() fails, user
    will know that the error was in having clause.
  */
  const char *_where;

public:
  const char *where()
  {
    return _where;
  }

  void setWhere(const char *arg)
  {
    _where= arg;
  }

  /*
    One thread can hold up to one named user-level lock. This variable
    points to a lock object if the lock is present. See item_func.cc and
    chapter 'Miscellaneous functions', for functions GET_LOCK, RELEASE_LOCK.
  */

private:
  boost::thread::id boost_thread_id;
  thread_ptr _thread;
  boost::this_thread::disable_interruption *interrupt;

  internal::st_my_thread_var *mysys_var;

public:
  thread_ptr &getThread()
  {
    return _thread;
  }

  void pushInterrupt(boost::this_thread::disable_interruption *interrupt_arg)
  {
    interrupt= interrupt_arg;
  }

  boost::this_thread::disable_interruption &getThreadInterupt()
  {
    assert(interrupt);
    return *interrupt;
  }

  internal::st_my_thread_var *getThreadVar()
  {
    return mysys_var;
  }

  /**
   * Type of current query: COM_STMT_PREPARE, COM_QUERY, etc. Set from
   * first byte of the packet in executeStatement()
   */
  enum_server_command command;

  thr_lock_type update_lock_default;

  /*
    Both of the following container points in session will be converted to an API.
  */

private:
  /* container for handler's private per-connection data */
  std::vector<Ha_data> ha_data;
  /*
    Id of current query. Statement can be reused to execute several queries
    query_id is global in context of the whole MySQL server.
    ID is automatically generated from an atomic counter.
    It's used in Cursor code for various purposes: to check which columns
    from table are necessary for this select, to check if it's necessary to
    update auto-updatable fields (like auto_increment and timestamp).
  */
  query_id_t query_id;
  query_id_t warn_query_id;

public:
  void **getEngineData(const plugin::MonitoredInTransaction *monitored);
  ResourceContext& getResourceContext(const plugin::MonitoredInTransaction&, size_t index= 0);

  session::Transactions& transaction;
  Open_tables_state& open_tables;
	session::Times& times;

  Field *dup_field;
  sigset_t signals;

public:
  // As of right now we do not allow a concurrent execute to launch itself
  void setConcurrentExecute(bool arg)
  {
    concurrent_execute_allowed= arg;
  }

  bool isConcurrentExecuteAllowed() const
  {
    return concurrent_execute_allowed;
  }

  /*
    ALL OVER THIS FILE, "insert_id" means "*automatically generated* value for
    insertion into an auto_increment column".
  */
  /**
    This is the first autogenerated insert id which was *successfully*
    inserted by the previous statement (exactly, if the previous statement
    didn't successfully insert an autogenerated insert id, then it's the one
    of the statement before, etc).
    It can also be set by SET LAST_INSERT_ID=# or SELECT LAST_INSERT_ID(#).
    It is returned by LAST_INSERT_ID().
  */
  uint64_t first_successful_insert_id_in_prev_stmt;
  /**
    This is the first autogenerated insert id which was *successfully*
    inserted by the current statement. It is maintained only to set
    first_successful_insert_id_in_prev_stmt when statement ends.
  */
  uint64_t first_successful_insert_id_in_cur_stmt;
  /**
    We follow this logic:
    - when stmt starts, first_successful_insert_id_in_prev_stmt contains the
    first insert id successfully inserted by the previous stmt.
    - as stmt makes progress, handler::insert_id_for_cur_row changes;
    every time get_auto_increment() is called,
    auto_inc_intervals_in_cur_stmt_for_binlog is augmented with the
    reserved interval (if statement-based binlogging).
    - at first successful insertion of an autogenerated value,
    first_successful_insert_id_in_cur_stmt is set to
    handler::insert_id_for_cur_row.
    - when stmt goes to binlog,
    auto_inc_intervals_in_cur_stmt_for_binlog is binlogged if
    non-empty.
    - when stmt ends, first_successful_insert_id_in_prev_stmt is set to
    first_successful_insert_id_in_cur_stmt.

    List of auto_increment intervals reserved by the thread so far, for
    storage in the statement-based binlog.
    Note that its minimum is not first_successful_insert_id_in_cur_stmt:
    assuming a table with an autoinc column, and this happens:
    INSERT INTO ... VALUES(3);
    SET INSERT_ID=3; INSERT IGNORE ... VALUES (NULL);
    then the latter INSERT will insert no rows
    (first_successful_insert_id_in_cur_stmt == 0), but storing "INSERT_ID=3"
    in the binlog is still needed; the list's minimum will contain 3.
  */

  uint64_t limit_found_rows;
  uint64_t options; /**< Bitmap of options */
  int64_t row_count_func; /**< For the ROW_COUNT() function */

  int64_t rowCount() const
  {
    return row_count_func;
  }

  ha_rows cuted_fields; /**< Count of "cut" or truncated fields. @todo Kill this friggin thing. */

  /**
   * Number of rows we actually sent to the client, including "synthetic"
   * rows in ROLLUP etc.
   */
  ha_rows sent_row_count;

  /**
   * Number of rows we read, sent or not, including in create_sort_index()
   */
  ha_rows examined_row_count;

  /**
   * The set of those tables whose fields are referenced in all subqueries
   * of the query.
   *
   * @todo
   *
   * Possibly this it is incorrect to have used tables in Session because
   * with more than one subquery, it is not clear what does the field mean.
   */
  table_map used_tables;

  /**
    @todo

    This, and some other variables like 'count_cuted_fields'
    maybe should be statement/cursor local, that is, moved to Statement
    class. With current implementation warnings produced in each prepared
    statement/cursor settle here.
  */
  uint32_t warn_count[(uint32_t) DRIZZLE_ERROR::WARN_LEVEL_END];
  uint32_t total_warn_count;

  /**
    Row counter, mainly for errors and warnings. Not increased in
    create_sort_index(); may differ from examined_row_count.
  */
  uint32_t row_count;

  session_id_t thread_id;
  uint32_t tmp_table;
  enum global_read_lock_t
  {
    NONE= 0,
    GOT_GLOBAL_READ_LOCK= 1,
    MADE_GLOBAL_READ_LOCK_BLOCK_COMMIT= 2
  };
private:
  global_read_lock_t _global_read_lock;

public:

  global_read_lock_t isGlobalReadLock() const
  {
    return _global_read_lock;
  }

  void setGlobalReadLock(global_read_lock_t arg)
  {
    _global_read_lock= arg;
  }

  DrizzleLock *lockTables(Table **tables, uint32_t count, uint32_t flags);
  bool lockGlobalReadLock();
  bool lock_table_names(TableList *table_list);
  bool lock_table_names_exclusively(TableList *table_list);
  bool makeGlobalReadLockBlockCommit();
  bool abortLockForThread(Table *table);
  bool wait_if_global_read_lock(bool abort_on_refresh, bool is_not_commit);
  int lock_table_name(TableList *table_list);
  void abortLock(Table *table);
  void removeLock(Table *table);
  void unlockReadTables(DrizzleLock *sql_lock);
  void unlockSomeTables(Table **table, uint32_t count);
  void unlockTables(DrizzleLock *sql_lock);
  void startWaitingGlobalReadLock();
  void unlockGlobalReadLock();

private:
  int unlock_external(Table **table, uint32_t count);
  int lock_external(Table **tables, uint32_t count);
  bool wait_for_locked_table_names(TableList *table_list);
  DrizzleLock *get_lock_data(Table **table_ptr, uint32_t count,
                             bool should_lock, Table **write_lock_used);
public:

  uint32_t server_status;
  uint32_t open_options;
  uint32_t select_number; /**< number of select (used for EXPLAIN) */
  /* variables.transaction_isolation is reset to this after each commit */
  enum_tx_isolation session_tx_isolation;
  enum_check_fields count_cuted_fields;

  enum killed_state_t
  {
    NOT_KILLED,
    KILL_BAD_DATA,
    KILL_CONNECTION,
    KILL_QUERY,
    KILLED_NO_VALUE /* means none of the above states apply */
  };
private:
  killed_state_t volatile _killed;

public:

  void setKilled(killed_state_t arg)
  {
    _killed= arg;
  }

  killed_state_t getKilled()
  {
    return _killed;
  }

  volatile killed_state_t *getKilledPtr() // Do not use this method, it is here for historical convience.
  {
    return &_killed;
  }

  bool is_admin_connection;
  bool no_errors;
  /**
    Set to true if execution of the current compound statement
    can not continue. In particular, disables activation of
    CONTINUE or EXIT handlers of stored routines.
    Reset in the end of processing of the current user request, in
    @see reset_session_for_next_command().
  */
  bool is_fatal_error;
  /**
    Set by a storage engine to request the entire
    transaction (that possibly spans multiple engines) to
    rollback. Reset in ha_rollback.
  */
  bool transaction_rollback_request;
  /**
    true if we are in a sub-statement and the current error can
    not be safely recovered until we left the sub-statement mode.
    In particular, disables activation of CONTINUE and EXIT
    handlers inside sub-statements. E.g. if it is a deadlock
    error and requires a transaction-wide rollback, this flag is
    raised (traditionally, MySQL first has to close all the reads
    via @see handler::ha_index_or_rnd_end() and only then perform
    the rollback).
    Reset to false when we leave the sub-statement mode.
  */
  bool is_fatal_sub_stmt_error;
  /** for IS NULL => = last_insert_id() fix in remove_eq_conds() */
  bool substitute_null_with_insert_id;
  bool cleanup_done;

public:
  bool got_warning; /**< Set on call to push_warning() */
  bool no_warnings_for_error; /**< no warnings on call to my_error() */
  /** set during loop of derived table processing */
  bool derived_tables_processing;

  bool doing_tablespace_operation()
  {
    return tablespace_op;
  }

  void setDoingTablespaceOperation(bool doing)
  {
    tablespace_op= doing;
  }


  /** Used by the sys_var class to store temporary values */
  union
  {
    bool bool_value;
    uint32_t uint32_t_value;
    int32_t int32_t_value;
    uint64_t uint64_t_value;
  } sys_var_tmp;

  /**
    Character input stream consumed by the lexical analyser,
    used during parsing.
    Note that since the parser is not re-entrant, we keep only one input
    stream here. This member is valid only when executing code during parsing,
    and may point to invalid memory after that.
  */
  Lex_input_stream *m_lip;

  /** Place to store various things */
  void *session_marker;

  /**
    Points to info-string that we show in SHOW PROCESSLIST
    You are supposed to call Session_SET_PROC_INFO only if you have coded
    a time-consuming piece that MySQL can get stuck in for a long time.

    Set it using the  session_proc_info(Session *thread, const char *message)
    macro/function.
  */
  inline void set_proc_info(const char *info)
  {
    proc_info= info;
  }
  inline const char* get_proc_info() const
  {
    return proc_info;
  }

  /** Sets this Session's current query ID */
  inline void setQueryId(query_id_t in_query_id)
  {
    query_id= in_query_id;
  }

  /** Returns the current query ID */
  query_id_t getQueryId()  const
  {
    return query_id;
  }


  /** Sets this Session's warning query ID */
  inline void setWarningQueryId(query_id_t in_query_id)
  {
    warn_query_id= in_query_id;
  }

  /** Returns the Session's warning query ID */
  inline query_id_t getWarningQueryId()  const
  {
    return warn_query_id;
  }

  /** Accessor method returning the session's ID. */
  inline session_id_t getSessionId()  const
  {
    return thread_id;
  }

  /** Accessor method returning the server's ID. */
  inline uint32_t getServerId()  const
  {
    /* We return the global server ID. */
    return server_id;
  }

  inline std::string &getServerUUID() const
  {
    return server_uuid;
  }

  /**
    There is BUG#19630 where statement-based replication of stored
    functions/triggers with two auto_increment columns breaks.
    We however ensure that it works when there is 0 or 1 auto_increment
    column; our rules are
    a) on master, while executing a top statement involving substatements,
    first top- or sub- statement to generate auto_increment values wins the
    exclusive right to see its values be written to binlog (the write
    will be done by the statement or its caller), and the losers won't see
    their values be written to binlog.
    b) on slave, while replicating a top statement involving substatements,
    first top- or sub- statement to need to read auto_increment values from
    the master's binlog wins the exclusive right to read them (so the losers
    won't read their values from binlog but instead generate on their own).
    a) implies that we mustn't backup/restore
    auto_inc_intervals_in_cur_stmt_for_binlog.
    b) implies that we mustn't backup/restore auto_inc_intervals_forced.

    If there are more than 1 auto_increment columns, then intervals for
    different columns may mix into the
    auto_inc_intervals_in_cur_stmt_for_binlog list, which is logically wrong,
    but there is no point in preventing this mixing by preventing intervals
    from the secondly inserted column to come into the list, as such
    prevention would be wrong too.
    What will happen in the case of
    INSERT INTO t1 (auto_inc) VALUES(NULL);
    where t1 has a trigger which inserts into an auto_inc column of t2, is
    that in binlog we'll store the interval of t1 and the interval of t2 (when
    we store intervals, soon), then in slave, t1 will use both intervals, t2
    will use none; if t1 inserts the same number of rows as on master,
    normally the 2nd interval will not be used by t1, which is fine. t2's
    values will be wrong if t2's internal auto_increment counter is different
    from what it was on master (which is likely). In 5.1, in mixed binlogging
    mode, row-based binlogging is used for such cases where two
    auto_increment columns are inserted.
  */
  inline void record_first_successful_insert_id_in_cur_stmt(uint64_t id_arg)
  {
    if (first_successful_insert_id_in_cur_stmt == 0)
      first_successful_insert_id_in_cur_stmt= id_arg;
  }
  inline uint64_t read_first_successful_insert_id_in_prev_stmt()
  {
    return first_successful_insert_id_in_prev_stmt;
  }

  Session(plugin::Client*, boost::shared_ptr<catalog::Instance>);
  ~Session();

  void cleanup();
  /**
   * Cleans up after query.
   *
   * @details
   *
   * This function is used to reset thread data to its default state.
   *
   * This function is not suitable for setting thread data to some
   * non-default values, as there is only one replication thread, so
   * different master threads may overwrite data of each other on
   * slave.
   */
  void cleanup_after_query();
  void storeGlobals();
  void awake(Session::killed_state_t state_to_set);

  /**
    Initialize memory roots necessary for query processing and (!)
    pre-allocate memory for it. We can't do that in Session constructor because
    there are use cases where it's vital to not allocate excessive and not used
    memory.
  */
  void prepareForQueries();

  /**
   * Executes a single statement received from the
   * client connection.
   *
   * Returns true if the statement was successful, or false
   * otherwise.
   *
   * @note
   *
   * For profiling to work, it must never be called recursively.
   *
   * In MySQL, this used to be the do_command() C function whic
   * accepted a single parameter of the THD pointer.
   */
  bool executeStatement();

  /**
   * Reads a query from packet and stores it.
   *
   * Returns true if query is read and allocated successfully,
   * false otherwise.  On a return of false, Session::fatal_error
   * is set.
   *
   * @note Used in COM_QUERY and COM_STMT_PREPARE.
   *
   * Sets the following Session variables:
   *  - query
   *  - query_length
   *
   * @param The packet pointer to read from
   * @param The length of the query to read
   */
  void readAndStoreQuery(const char *in_packet, uint32_t in_packet_length);

  /**
   * Ends the current transaction and (maybe) begins the next.
   *
   * Returns true if the transaction completed successfully,
   * otherwise false.
   *
   * @param Completion type
   */
  bool endTransaction(enum enum_mysql_completiontype completion);
  bool endActiveTransaction();
  bool startTransaction(start_transaction_option_t opt= START_TRANS_NO_OPTIONS);
  void markTransactionForRollback(bool all);

  /**
   * Authenticates users, with error reporting.
   *
   * Returns true on success, or false on failure.
   */
  bool authenticate();
  void run();
  static bool schedule(const Session::shared_ptr&);
  static void unlink(session_id_t&);
  static void unlink(const Session::shared_ptr&);

  /*
    For enter_cond() / exit_cond() to work the mutex must be got before
    enter_cond(); this mutex is then released by exit_cond().
    Usage must be: lock mutex; enter_cond(); your code; exit_cond().
  */
  const char* enter_cond(boost::condition_variable_any &cond, boost::mutex &mutex, const char* msg);
  void exit_cond(const char* old_msg);

  uint64_t found_rows() const
  {
    return limit_found_rows;
  }

  /** Returns whether the session is currently inside a transaction */
  bool inTransaction() const
  {
    return server_status & SERVER_STATUS_IN_TRANS;
  }

  LEX_STRING *make_lex_string(LEX_STRING *lex_str,
                              const char* str, uint32_t length,
                              bool allocate_lex_string);

  LEX_STRING *make_lex_string(LEX_STRING *lex_str,
                              const std::string &str,
                              bool allocate_lex_string);

  void send_explain_fields(select_result*);

  void clear_error(bool full= false);
  void clearDiagnostics();
  bool is_error() const;

  static const charset_info_st *charset() { return default_charset_info; }

  /**
    Cleanup statement parse state (parse tree, lex) and execution
    state after execution of a non-prepared SQL statement.

    @todo

    Move this to Statement::~Statement
  */
  void end_statement();
  inline int killed_errno() const
  {
    killed_state_t killed_val; /* to cache the volatile 'killed' */
    return (killed_val= _killed) != KILL_BAD_DATA ? killed_val : 0;
  }
  void send_kill_message() const;
  /* return true if we will abort query if we make a warning now */
  inline bool abortOnWarning()
  {
    return abort_on_warning;
  }

  inline void setAbortOnWarning(bool arg)
  {
    abort_on_warning= arg;
  }

  void setAbort(bool arg);
  void lockOnSys();
  void set_status_var_init();
  /**
    Set the current database; use deep copy of C-string.

    @param new_db     a pointer to the new database name.
    @param new_db_len length of the new database name.

    Initialize the current database from a NULL-terminated string with
    length. If we run out of memory, we free the current database and
    return true.  This way the user will notice the error as there will be
    no current database selected (in addition to the error message set by
    malloc).

    @note This operation just sets {db, db_length}. Switching the current
    database usually involves other actions, like switching other database
    attributes including security context. In the future, this operation
    will be made private and more convenient interface will be provided.
  */
  void set_db(const std::string&);

  /*
    Copy the current database to the argument. Use the current arena to
    allocate memory for a deep copy: current database may be freed after
    a statement is parsed but before it's executed.
  */
  bool copy_db_to(char **p_db, size_t *p_db_length);

public:

  /**
    Resets Session part responsible for command processing state.

    This needs to be called before execution of every statement
    (prepared or conventional).
    It is not called by substatements of routines.

    @todo
    Make it a method of Session and align its name with the rest of
    reset/end/start/init methods.
    @todo
    Call it after we use Session for queries, not before.
  */
  void reset_for_next_command();

  /**
   * Disconnects the session from a client connection and
   * updates any status variables necessary.
   *
   * @param errcode	Error code to print to console
   *
   * @note  For the connection that is doing shutdown, this is called twice
   */
  void disconnect(enum error_t errcode= EE_OK);

  /**
   * Check if user exists and the password supplied is correct.
   *
   * Returns true on success, and false on failure.
   *
   * @note Host, user and passwd may point to communication buffer.
   * Current implementation does not depend on that, but future changes
   * should be done with this in mind;
   *
   * @param passwd Scrambled password received from client
   * @param db Database name to connect to, may be NULL
   */
  bool checkUser(const std::string &passwd, const std::string &db);

  /**
   * Returns a pointer to the active Transaction message for this
   * Session being managed by the ReplicationServices component, or
   * NULL if no active message.
   */
  message::Transaction *getTransactionMessage() const
  {
    return transaction_message;
  }

  /**
   * Returns a pointer to the active Statement message for this
   * Session, or NULL if no active message.
   */
  message::Statement *getStatementMessage() const
  {
    return statement_message;
  }

  /**
   * Returns a pointer to the current Resulset message for this
   * Session, or NULL if no active message.
   */
  message::Resultset *getResultsetMessage() const
  {
    return resultset;
  }
  /**
   * Sets the active transaction message used by the ReplicationServices
   * component.
   *
   * @param[in] Pointer to the message
   */
  void setTransactionMessage(message::Transaction *in_message)
  {
    transaction_message= in_message;
  }

  /**
   * Sets the active statement message used by the ReplicationServices
   * component.
   *
   * @param[in] Pointer to the message
   */
  void setStatementMessage(message::Statement *in_message)
  {
    statement_message= in_message;
  }

  /**
   * Sets the active Resultset message used by the Query Cache
   * plugin.
   *
   * @param[in] Pointer to the message
   */
  void setResultsetMessage(message::Resultset *in_message)
  {
    resultset= in_message;
  }
  /**
   * reset the active Resultset message used by the Query Cache
   * plugin.
   */

  void resetResultsetMessage()
  {
    resultset= NULL;
  }

  plugin::EventObserverList *getSessionObservers()
  {
    return session_event_observers;
  }

  void setSessionObservers(plugin::EventObserverList *observers)
  {
    session_event_observers= observers;
  }

  plugin::EventObserverList* getSchemaObservers(const std::string& schema);
  plugin::EventObserverList* setSchemaObservers(const std::string& schema, plugin::EventObserverList*);

public:
  void my_ok(ha_rows affected_rows= 0, ha_rows found_rows_arg= 0, uint64_t passed_id= 0, const char *message= NULL);
  void my_eof();
  void add_item_to_list(Item *item);
  void add_value_to_list(Item *value);
  void add_order_to_list(Item *item, bool asc);
  void add_group_to_list(Item *item, bool asc);

  void refresh_status();
  user_var_entry *getVariable(LEX_STRING &name, bool create_if_not_exists);
  user_var_entry *getVariable(const std::string  &name, bool create_if_not_exists);
  void setVariable(const std::string &name, const std::string &value);

  /**
   * Closes all tables used by the current substatement, or all tables
   * used by this thread if we are on the upper level.
   */
  void close_thread_tables();
  void close_old_data_files(bool morph_locks= false,
                            bool send_refresh= false);
  void close_data_files_and_morph_locks(const identifier::Table &identifier);

  /**
   * Prepares statement for reopening of tables and recalculation of set of
   * prelocked tables.
   *
   * @param Pointer to a pointer to a list of tables which we were trying to open and lock
   */
  void close_tables_for_reopen(TableList **tables);


  /**
   * Open all tables in list, locks them (all, including derived)
   *
   * @param Pointer to a list of tables for open & locking
   *
   * @retval
   *  false - ok
   * @retval
   *  true  - error
   *
   * @note
   *
   * The lock will automaticaly be freed by close_thread_tables()
   */
  bool openTablesLock(TableList*);
  Table *open_temporary_table(const identifier::Table &identifier, bool link_in_list= true);

  int open_tables_from_list(TableList **start, uint32_t *counter, uint32_t flags= 0);

  Table *openTableLock(TableList *table_list, thr_lock_type lock_type);
  Table *openTable(TableList *table_list, bool *refresh, uint32_t flags= 0);

  void unlink_open_table(Table *find);
  void drop_open_table(Table *table, const identifier::Table &identifier);
  void close_cached_table(Table *table);

  /* Create a lock in the cache */
  table::Placeholder& table_cache_insert_placeholder(const identifier::Table&);
  Table* lock_table_name_if_not_cached(const identifier::Table&);

  session::TableMessages &getMessageCache();

  /* Reopen operations */
  bool reopen_tables();
  bool close_cached_tables(TableList *tables, bool wait_for_refresh, bool wait_for_placeholders);

  void wait_for_condition(boost::mutex &mutex, boost::condition_variable_any &cond);
  int setup_conds(TableList *leaves, COND **conds);
  int lock_tables(TableList *tables, uint32_t count, bool *need_reopen);

  template <class T>
  T* getProperty(const std::string& name)
  {
    return static_cast<T*>(getProperty0(name));
  }

  template <class T>
  T setProperty(const std::string& name, T value)
  {
    setProperty0(name, value);
    return value;
  }

  /**
    Return the default storage engine

    @param getDefaultStorageEngine()

    @return
    pointer to plugin::StorageEngine
  */
  plugin::StorageEngine *getDefaultStorageEngine();
  void get_xid(DrizzleXid *xid); // Innodb only

  table::Singular& getInstanceTable();
  table::Singular& getInstanceTable(std::list<CreateField>&);

  void setUsage(bool arg)
  {
    use_usage= arg;
  }

  const rusage &getUsage()
  {
    return usage;
  }

  const catalog::Instance& catalog() const
  {
    return *_catalog;
  }

  catalog::Instance& catalog()
  {
    return *_catalog;
  }

  bool arg_of_last_insert_id_function; // Tells if LAST_INSERT_ID(#) was called for the current statement
private:
  drizzled::util::Storable* getProperty0(const std::string&);
  void setProperty0(const std::string&, drizzled::util::Storable*);

  bool resetUsage()
  {
    return not getrusage(RUSAGE_THREAD, &usage);
  }

  boost::shared_ptr<catalog::Instance> _catalog;

  /** Pointers to memory managed by the ReplicationServices component */
  message::Transaction *transaction_message;
  message::Statement *statement_message;
  /* Pointer to the current resultset of Select query */
  message::Resultset *resultset;
  plugin::EventObserverList *session_event_observers;

  uint64_t xa_id;
  const char *proc_info;
  bool abort_on_warning;
  bool concurrent_execute_allowed;
  bool tablespace_op; /**< This is true in DISCARD/IMPORT TABLESPACE */
  bool use_usage;
  rusage usage;
  identifier::user::mptr security_ctx;
  int32_t scoreboard_index;
  bool originating_server_uuid_set;
  std::string originating_server_uuid;
  uint64_t originating_commit_id;
  plugin::Client *client;
};

#define ESCAPE_CHARS "ntrb0ZN" // keep synchronous with READ_INFO::unescape

/* Bits in sql_command_flags */

enum sql_command_flag_bits
{
  CF_BIT_CHANGES_DATA,
  CF_BIT_HAS_ROW_COUNT,
  CF_BIT_STATUS_COMMAND,
  CF_BIT_SHOW_TABLE_COMMAND,
  CF_BIT_WRITE_LOGS_COMMAND,
  CF_BIT_SIZE
};

static const std::bitset<CF_BIT_SIZE> CF_CHANGES_DATA(1 << CF_BIT_CHANGES_DATA);
static const std::bitset<CF_BIT_SIZE> CF_HAS_ROW_COUNT(1 << CF_BIT_HAS_ROW_COUNT);
static const std::bitset<CF_BIT_SIZE> CF_STATUS_COMMAND(1 << CF_BIT_STATUS_COMMAND);
static const std::bitset<CF_BIT_SIZE> CF_SHOW_TABLE_COMMAND(1 << CF_BIT_SHOW_TABLE_COMMAND);
static const std::bitset<CF_BIT_SIZE> CF_WRITE_LOGS_COMMAND(1 << CF_BIT_WRITE_LOGS_COMMAND);

namespace display  
{
  const std::string &type(Session::global_read_lock_t);
  size_t max_string_length(Session::global_read_lock_t);
} /* namespace display */

} /* namespace drizzled */

