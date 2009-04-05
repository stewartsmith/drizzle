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


#ifndef DRIZZLED_SESSION_H
#define DRIZZLED_SESSION_H

/* Classes in mysql */

#include <drizzled/protocol.h>
#include <drizzled/sql_locale.h>
#include <drizzled/ha_trx_info.h>
#include <mysys/my_tree.h>
#include <drizzled/handler.h>
#include <drizzled/current_session.h>
#include <drizzled/sql_error.h>
#include <drizzled/query_arena.h>
#include <drizzled/file_exchange.h>
#include <drizzled/select_result_interceptor.h>
#include <drizzled/authentication.h>
#include <drizzled/db.h>
#include <drizzled/xid.h>

#include <netdb.h>
#include <string>
#include <bitset>

#define MIN_HANDSHAKE_SIZE      6

class Lex_input_stream;
class user_var_entry;
class Copy_field;
class Table_ident;

extern char internal_table_name[2];
extern char empty_c_string[1];
extern const char **errmesg;

#define TC_HEURISTIC_RECOVER_COMMIT   1
#define TC_HEURISTIC_RECOVER_ROLLBACK 2
extern uint32_t tc_heuristic_recover;

/*
  The COPY_INFO structure is used by INSERT/REPLACE code.
  The schema of the row counting by the INSERT/INSERT ... ON DUPLICATE KEY
  UPDATE code:
    If a row is inserted then the copied variable is incremented.
    If a row is updated by the INSERT ... ON DUPLICATE KEY UPDATE and the
      new data differs from the old one then the copied and the updated
      variables are incremented.
    The touched variable is incremented if a row was touched by the update part
      of the INSERT ... ON DUPLICATE KEY UPDATE no matter whether the row
      was actually changed or not.
*/
typedef struct st_copy_info {
  ha_rows records; /**< Number of processed records */
  ha_rows deleted; /**< Number of deleted records */
  ha_rows updated; /**< Number of updated records */
  ha_rows copied;  /**< Number of copied records */
  ha_rows error_count;
  ha_rows touched; /* Number of touched records */
  enum enum_duplicates handle_duplicates;
  int escape_char, last_errno;
  bool ignore;
  /* for INSERT ... UPDATE */
  List<Item> *update_fields;
  List<Item> *update_values;
  /* for VIEW ... WITH CHECK OPTION */
} COPY_INFO;


typedef struct drizzled_lock_st
{
  Table **table;
  uint32_t table_count,lock_count;
  THR_LOCK_DATA **locks;
} DRIZZLE_LOCK;


#include <drizzled/lex_column.h>

class select_result;
class Time_zone;

#define Session_SENTRY_MAGIC 0xfeedd1ff
#define Session_SENTRY_GONE  0xdeadbeef

#define Session_CHECK_SENTRY(session) assert(session->dbug_sentry == Session_SENTRY_MAGIC)

struct system_variables
{
  system_variables() {};
  /*
    How dynamically allocated system variables are handled:

    The global_system_variables and max_system_variables are "authoritative"
    They both should have the same 'version' and 'size'.
    When attempting to access a dynamic variable, if the session version
    is out of date, then the session version is updated and realloced if
    neccessary and bytes copied from global to make up for missing data.
  */
  ulong dynamic_variables_version;
  char* dynamic_variables_ptr;
  uint32_t dynamic_variables_head;  /* largest valid variable offset */
  uint32_t dynamic_variables_size;  /* how many bytes are in use */

  uint64_t myisam_max_extra_sort_file_size;
  uint64_t max_heap_table_size;
  uint64_t tmp_table_size;
  ha_rows select_limit;
  ha_rows max_join_size;
  uint64_t auto_increment_increment;
  uint64_t auto_increment_offset;
  uint64_t bulk_insert_buff_size;
  uint64_t join_buff_size;
  uint32_t max_allowed_packet;
  uint64_t max_error_count;
  uint64_t max_length_for_sort_data;
  size_t max_sort_length;
  uint64_t max_tmp_tables;
  uint64_t min_examined_row_limit;
  uint32_t myisam_stats_method;
  uint32_t net_buffer_length;
  uint32_t net_read_timeout;
  uint32_t net_retry_count;
  uint32_t net_wait_timeout;
  uint32_t net_write_timeout;
  bool optimizer_prune_level;
  uint32_t optimizer_search_depth;
  /*
    Controls use of Engine-MRR:
      0 - auto, based on cost
      1 - force MRR when the storage engine is capable of doing it
      2 - disable MRR.
  */
  uint32_t optimizer_use_mrr;
  /* A bitmap for switching optimizations on/off */
  uint32_t optimizer_switch;
  uint64_t preload_buff_size;
  uint32_t read_buff_size;
  uint32_t read_rnd_buff_size;
  uint32_t div_precincrement;
  size_t sortbuff_size;
  uint32_t thread_handling;
  uint32_t tx_isolation;
  uint32_t completion_type;
  /* Determines which non-standard SQL behaviour should be enabled */
  uint32_t sql_mode;
  uint64_t max_seeks_for_key;
  size_t range_alloc_block_size;
  uint32_t query_alloc_block_size;
  uint32_t query_prealloc_size;
  uint32_t trans_alloc_block_size;
  uint32_t trans_prealloc_size;
  bool log_warnings;
  uint64_t group_concat_max_len;
  /* TODO: change this to my_thread_id - but have to fix set_var first */
  uint64_t pseudo_thread_id;

  bool low_priority_updates;
  bool new_mode;
  /*
    compatibility option:
      - index usage hints (USE INDEX without a FOR clause) behave as in 5.0
  */
  bool old_mode;
  bool engine_condition_pushdown;
  bool keep_files_on_create;

  bool old_alter_table;

  plugin_ref table_plugin;

  /* Only charset part of these variables is sensible */
  const CHARSET_INFO  *character_set_filesystem;

  /* Both charset and collation parts of these variables are important */
  const CHARSET_INFO	*collation_server;
  const CHARSET_INFO	*collation_database;

  inline const CHARSET_INFO  *getCollation(void) 
  {
    return collation_database ? collation_database : collation_server;
  }

  /* Locale Support */
  MY_LOCALE *lc_time_names;

  Time_zone *time_zone;
};

extern struct system_variables global_system_variables;

#include "sql_lex.h"  /* only for SQLCOM_END */

/* per thread status variables */

typedef struct system_status_var
{
  uint64_t bytes_received;
  uint64_t bytes_sent;
  ulong com_other;
  ulong com_stat[(uint32_t) SQLCOM_END];
  ulong created_tmp_disk_tables;
  ulong created_tmp_tables;
  ulong ha_commit_count;
  ulong ha_delete_count;
  ulong ha_read_first_count;
  ulong ha_read_last_count;
  ulong ha_read_key_count;
  ulong ha_read_next_count;
  ulong ha_read_prev_count;
  ulong ha_read_rnd_count;
  ulong ha_read_rnd_next_count;
  ulong ha_rollback_count;
  ulong ha_update_count;
  ulong ha_write_count;
  ulong ha_prepare_count;
  ulong ha_savepoint_count;
  ulong ha_savepoint_rollback_count;

  /* KEY_CACHE parts. These are copies of the original */
  ulong key_blocks_changed;
  ulong key_blocks_used;
  ulong key_cache_r_requests;
  ulong key_cache_read;
  ulong key_cache_w_requests;
  ulong key_cache_write;
  /* END OF KEY_CACHE parts */

  ulong net_big_packet_count;
  ulong opened_tables;
  ulong opened_shares;
  ulong select_full_join_count;
  ulong select_full_range_join_count;
  ulong select_range_count;
  ulong select_range_check_count;
  ulong select_scan_count;
  ulong long_query_count;
  ulong filesort_merge_passes;
  ulong filesort_range_count;
  ulong filesort_rows;
  ulong filesort_scan_count;
  /*
    Number of statements sent from the client
  */
  ulong questions;

  /*
    IMPORTANT!
    SEE last_system_status_var DEFINITION BELOW.

    Below 'last_system_status_var' are all variables which doesn't make any
    sense to add to the /global/ status variable counter.
  */
  double last_query_cost;


} STATUS_VAR;

/*
  This is used for 'SHOW STATUS'. It must be updated to the last ulong
  variable in system_status_var which is makes sens to add to the global
  counter
*/

#define last_system_status_var questions

void mark_transaction_to_rollback(Session *session, bool all);

/**
  @class Statement
  @brief State of a single command executed against this connection.

  One connection can contain a lot of simultaneously running statements,
  some of which could be:
   - prepared, that is, contain placeholders,
  To perform some action with statement we reset Session part to the state  of
  that statement, do the action, and then save back modified state from Session
  to the statement. It will be changed in near future, and Statement will
  be used explicitly.
*/

class Statement: public ilink, public Query_arena
{
  Statement(const Statement &rhs);              /* not implemented: */
  Statement &operator=(const Statement &rhs);   /* non-copyable */
public:
  /*
    Uniquely identifies each statement object in thread scope; change during
    statement lifetime. FIXME: must be const
  */
   ulong id;

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
  enum enum_mark_columns mark_used_columns;

  LEX *lex;                                     // parse tree descriptor
  /*
    Points to the query associated with this statement. It's const, but
    we need to declare it char * because all table handlers are written
    in C and need to point to it.

    Note that (A) if we set query = NULL, we must at the same time set
    query_length = 0, and protect the whole operation with the
    LOCK_thread_count mutex. And (B) we are ONLY allowed to set query to a
    non-NULL value if its previous value is NULL. We do not need to protect
    operation (B) with any mutex. To avoid crashes in races, if we do not
    know that session->query cannot change at the moment, one should print
    session->query like this:
      (1) reserve the LOCK_thread_count mutex;
      (2) check if session->query is NULL;
      (3) if not NULL, then print at most session->query_length characters from
      it. We will see the query_length field as either 0, or the right value
      for it.
    Assuming that the write and read of an n-bit memory field in an n-bit
    computer is atomic, we can avoid races in the above way.
    This printing is needed at least in SHOW PROCESSLIST and SHOW INNODB
    STATUS.
  */
  char *query;
  uint32_t query_length;                          // current query length

  /**
    Name of the current (default) database.

    If there is the current (default) database, "db" contains its name. If
    there is no current (default) database, "db" is NULL and "db_length" is
    0. In other words, "db", "db_length" must either be NULL, or contain a
    valid database name.

    @note this attribute is set and alloced by the slave SQL thread (for
    the Session of that thread); that thread is (and must remain, for now) the
    only responsible for freeing this member.
  */

  char *db;
  uint32_t db_length;

public:

  /* This constructor is called for backup statements */
  Statement() {}

  Statement(LEX *lex_arg, MEM_ROOT *mem_root_arg, ulong id_arg);
  ~Statement() {}
};

struct st_savepoint {
  struct st_savepoint *prev;
  char                *name;
  uint32_t                 length;
  Ha_trx_info         *ha_list;
};

extern pthread_mutex_t LOCK_xid_cache;
extern HASH xid_cache;

#include <drizzled/security_context.h>
#include <drizzled/open_tables_state.h>

#include <drizzled/internal_error_handler.h> 
#include <drizzled/diagnostics_area.h> 

/**
  Storage engine specific thread local data.
*/
struct Ha_data
{
  /**
    Storage engine specific thread local data.
    Lifetime: one user connection.
  */
  void *ha_ptr;
  /**
    0: Life time: one statement within a transaction. If @@autocommit is
    on, also represents the entire transaction.
    @sa trans_register_ha()

    1: Life time: one transaction within a connection.
    If the storage engine does not participate in a transaction,
    this should not be used.
    @sa trans_register_ha()
  */
  Ha_trx_info ha_info[2];

  Ha_data() :ha_ptr(NULL) {}
};

class Session :public Statement, public Open_tables_state
{
public:
  /*
    Constant for Session::where initialization in the beginning of every query.

    It's needed because we do not save/restore Session::where normally during
    primary (non subselect) query execution.
  */
  static const char * const DEFAULT_WHERE;

  MEM_ROOT warn_root;			// For warnings and errors
  Protocol *protocol;			// Current protocol
  Protocol_text   protocol_text;	// Normal protocol
  char    compression;
  HASH    user_vars;			// hash for user variables
  String  packet;			// dynamic buffer for network I/O
  String  convert_buffer;               // buffer for charset conversions
  struct  system_variables variables;	// Changeable local variables
  struct  system_status_var status_var; // Per thread statistic vars
  struct  system_status_var *initial_status_var; /* used by show status */
  THR_LOCK_INFO lock_info;              // Locking info of this thread
  THR_LOCK_OWNER main_lock_id;          // To use for conventional queries
  THR_LOCK_OWNER *lock_id;              // If not main_lock_id, points to
                                        // the lock_id of a cursor.
  pthread_mutex_t LOCK_delete;		// Locked before session is deleted
  char process_list_info[PROCESS_LIST_WIDTH];
  /*
    A pointer to the stack frame of handle_one_connection(),
    which is called first in the thread for handling a client
  */
  char	  *thread_stack;

  /**
    Currently selected catalog.
  */
  char *catalog;

  /**
    @note
    Some members of Session (currently 'Statement::db',
    'catalog' and 'query')  are set and alloced by the slave SQL thread
    (for the Session of that thread); that thread is (and must remain, for now)
    the only responsible for freeing these 3 members. If you add members
    here, and you add code to set them in replication, don't forget to
    free_them_and_set_them_to_0 in replication properly. For details see
    the 'err:' label of the handle_slave_sql() in sql/slave.cc.

    @see handle_slave_sql
  */

  Security_context security_ctx;

  /*
    Points to info-string that we show in SHOW PROCESSLIST
    You are supposed to call Session_SET_PROC_INFO only if you have coded
    a time-consuming piece that MySQL can get stuck in for a long time.

    Set it using the  session_proc_info(Session *thread, const char *message)
    macro/function.
  */
  void        set_proc_info(const char *info) { proc_info= info; }
  const char* get_proc_info() const { return proc_info; }

  /*
    Used in error messages to tell user in what part of MySQL we found an
    error. E. g. when where= "having clause", if fix_fields() fails, user
    will know that the error was in having clause.
  */
  const char *where;

  double tmp_double_value;                    /* Used in set_var.cc */
  ulong client_capabilities;		/* What the client supports */
  ulong max_client_packet_length;

  /*
    One thread can hold up to one named user-level lock. This variable
    points to a lock object if the lock is present. See item_func.cc and
    chapter 'Miscellaneous functions', for functions GET_LOCK, RELEASE_LOCK.
  */
  uint32_t dbug_sentry; // watch out for memory corruption
  struct st_my_thread_var *mysys_var;
  /*
    Type of current query: COM_STMT_PREPARE, COM_QUERY, etc. Set from
    first byte of the packet in executeStatement()
  */
  enum enum_server_command command;
  uint32_t     server_id;
  uint32_t     file_id;			// for LOAD DATA INFILE
  /* remote (peer) port */
  uint16_t peer_port;
  time_t     start_time, user_time;
  uint64_t  connect_utime, thr_create_utime; // track down slow pthread_create
  uint64_t  start_utime, utime_after_lock;

  thr_lock_type update_lock_default;

  /*
    Both of the following container points in session will be converted to an API.
  */

  /* container for handler's private per-connection data */
  Ha_data ha_data[MAX_HA];

  /* container for replication data */
  void *replication_data;
  inline void setReplicationData (void *data) { replication_data= data; }
  inline void *getReplicationData () { return replication_data; }

  /* Place to store various things */
  void *session_marker;

  void set_server_id(uint32_t sid) { server_id = sid; }

public:

  struct st_transactions {
    SAVEPOINT *savepoints;
    Session_TRANS all;			// Trans since BEGIN WORK
    Session_TRANS stmt;			// Trans for current statement
    bool on;                            // see ha_enable_transaction()
    XID_STATE xid_state;

    /*
       Tables changed in transaction (that must be invalidated in query cache).
       List contain only transactional tables, that not invalidated in query
       cache (instead of full list of changed in transaction tables).
    */
    CHANGED_TableList* changed_tables;
    MEM_ROOT mem_root; // Transaction-life memory allocation pool
    void cleanup()
    {
      changed_tables= 0;
      savepoints= 0;
      free_root(&mem_root,MYF(MY_KEEP_PREALLOC));
    }
    st_transactions()
    {
      memset(this, 0, sizeof(*this));
      xid_state.xid.null();
      init_sql_alloc(&mem_root, ALLOC_ROOT_MIN_BLOCK_SIZE, 0);
    }
  } transaction;
  Field      *dup_field;
  sigset_t signals;

  /* Tells if LAST_INSERT_ID(#) was called for the current statement */
  bool arg_of_last_insert_id_function;
  /*
    ALL OVER THIS FILE, "insert_id" means "*automatically generated* value for
    insertion into an auto_increment column".
  */
  /*
    This is the first autogenerated insert id which was *successfully*
    inserted by the previous statement (exactly, if the previous statement
    didn't successfully insert an autogenerated insert id, then it's the one
    of the statement before, etc).
    It can also be set by SET LAST_INSERT_ID=# or SELECT LAST_INSERT_ID(#).
    It is returned by LAST_INSERT_ID().
  */
  uint64_t  first_successful_insert_id_in_prev_stmt;
  /*
    This is the first autogenerated insert id which was *successfully*
    inserted by the current statement. It is maintained only to set
    first_successful_insert_id_in_prev_stmt when statement ends.
  */
  uint64_t  first_successful_insert_id_in_cur_stmt;
  /*
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
  */
  /*
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
  Discrete_intervals_list auto_inc_intervals_in_cur_stmt_for_binlog;
  /* Used by replication and SET INSERT_ID */
  Discrete_intervals_list auto_inc_intervals_forced;
  /*
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
  inline uint64_t read_first_successful_insert_id_in_prev_stmt(void)
  {
    return first_successful_insert_id_in_prev_stmt;
  }
  /*
    Used by Intvar_log_event::do_apply_event() and by "SET INSERT_ID=#"
    (mysqlbinlog). We'll soon add a variant which can take many intervals in
    argument.
  */
  inline void force_one_auto_inc_interval(uint64_t next_id)
  {
    auto_inc_intervals_forced.empty(); // in case of multiple SET INSERT_ID
    auto_inc_intervals_forced.append(next_id, UINT64_MAX, 0);
  }

  uint64_t  limit_found_rows;
  uint64_t  options;           /* Bitmap of states */
  int64_t   row_count_func;    /* For the ROW_COUNT() function */
  ha_rows    cuted_fields;

  /*
    number of rows we actually sent to the client, including "synthetic"
    rows in ROLLUP etc.
  */
  ha_rows    sent_row_count;

  /*
    number of rows we read, sent or not, including in create_sort_index()
  */
  ha_rows    examined_row_count;

  /*
    The set of those tables whose fields are referenced in all subqueries
    of the query.
    TODO: possibly this it is incorrect to have used tables in Session because
    with more than one subquery, it is not clear what does the field mean.
  */
  table_map  used_tables;
  const CHARSET_INFO *db_charset;
  /*
    FIXME: this, and some other variables like 'count_cuted_fields'
    maybe should be statement/cursor local, that is, moved to Statement
    class. With current implementation warnings produced in each prepared
    statement/cursor settle here.
  */
  List	     <DRIZZLE_ERROR> warn_list;
  uint32_t   warn_count[(uint32_t) DRIZZLE_ERROR::WARN_LEVEL_END];
  uint32_t   total_warn_count;
  Diagnostics_area main_da;

  /*
    Id of current query. Statement can be reused to execute several queries
    query_id is global in context of the whole MySQL server.
    ID is automatically generated from mutex-protected counter.
    It's used in handler code for various purposes: to check which columns
    from table are necessary for this select, to check if it's necessary to
    update auto-updatable fields (like auto_increment and timestamp).
  */
  query_id_t query_id, warn_id;
  ulong      col_access;

#ifdef ERROR_INJECT_SUPPORT
  ulong      error_inject_value;
#endif
  /* Statement id is thread-wide. This counter is used to generate ids */
  ulong      statement_id_counter;
  ulong	     rand_saved_seed1, rand_saved_seed2;
  /*
    Row counter, mainly for errors and warnings. Not increased in
    create_sort_index(); may differ from examined_row_count.
  */
  ulong      row_count;
  pthread_t  real_id;                           /* For debugging */
  my_thread_id  thread_id;
  uint	     tmp_table, global_read_lock;
  uint	     server_status,open_options;
  uint32_t       select_number;             //number of select (used for EXPLAIN)
  /* variables.transaction_isolation is reset to this after each commit */
  enum_tx_isolation session_tx_isolation;
  enum_check_fields count_cuted_fields;

  enum killed_state
  {
    NOT_KILLED,
    KILL_BAD_DATA,
    KILL_CONNECTION,
    KILL_QUERY,
    KILLED_NO_VALUE      /* means neither of the states */
  };
  killed_state volatile killed;

  bool	     some_tables_deleted;
  bool       last_cuted_field;
  bool	     no_errors, password;
  /**
    Set to true if execution of the current compound statement
    can not continue. In particular, disables activation of
    CONTINUE or EXIT handlers of stored routines.
    Reset in the end of processing of the current user request, in
    @see mysql_reset_session_for_next_command().
  */
  bool is_fatal_error;
  /**
    Set by a storage engine to request the entire
    transaction (that possibly spans multiple engines) to
    rollback. Reset in ha_rollback.
  */
  bool       transaction_rollback_request;
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
  bool       is_fatal_sub_stmt_error;
  /* for IS NULL => = last_insert_id() fix in remove_eq_conds() */
  bool       substitute_null_with_insert_id;
  bool	     in_lock_tables;
  bool       cleanup_done;

  /**  is set if some thread specific value(s) used in a statement. */
  bool       thread_specific_used;
  bool	     charset_is_system_charset, charset_is_collation_connection;
  bool       charset_is_character_set_filesystem;
  bool	     abort_on_warning;
  bool 	     got_warning;       /* Set on call to push_warning() */
  bool	     no_warnings_for_error; /* no warnings on call to my_error() */
  /* set during loop of derived table processing */
  bool       derived_tables_processing;
  bool    tablespace_op;	/* This is true in DISCARD/IMPORT TABLESPACE */

  /* Used by the sys_var class to store temporary values */
  union
  {
    bool   bool_value;
    uint32_t  uint32_t_value;
    long      long_value;
    ulong     ulong_value;
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

  Session();
  ~Session();

  void init(void);
  /*
    Initialize memory roots necessary for query processing and (!)
    pre-allocate memory for it. We can't do that in Session constructor because
    there are use cases (acl_init, watcher threads,
    killing mysqld) where it's vital to not allocate excessive and not used
    memory. Note, that we still don't return error from init_for_queries():
    if preallocation fails, we should notice that at the first call to
    alloc_root.
  */
  void init_for_queries();
  void cleanup(void);
  void cleanup_after_query();
  bool store_globals();
  void awake(Session::killed_state state_to_set);
  /**
   * Pulls thread-specific variables into Session state.
   *
   * Returns true most times, or false if there was a problem
   * allocating resources for thread-specific storage.
   *
   * @TODO Kill this.  It's not necessary once my_thr_init() is bye bye.
   *
   */
  bool initGlobals();

  /**
   * Initializes the Session to handle queries.
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
  bool readAndStoreQuery(const char *in_packet, uint32_t in_packet_length);

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
  bool startTransaction();

  /**
   * Authenticates users, with error reporting.
   *
   * Returns true on success, or false on failure.
   */
  bool authenticate();

  /*
    For enter_cond() / exit_cond() to work the mutex must be got before
    enter_cond(); this mutex is then released by exit_cond().
    Usage must be: lock mutex; enter_cond(); your code; exit_cond().
  */
  inline const char* enter_cond(pthread_cond_t *cond, pthread_mutex_t* mutex, const char* msg)
  {
    const char* old_msg = get_proc_info();
    safe_mutex_assert_owner(mutex);
    mysys_var->current_mutex = mutex;
    mysys_var->current_cond = cond;
    this->set_proc_info(msg);
    return old_msg;
  }
  inline void exit_cond(const char* old_msg)
  {
    /*
      Putting the mutex unlock in exit_cond() ensures that
      mysys_var->current_mutex is always unlocked _before_ mysys_var->mutex is
      locked (if that would not be the case, you'll get a deadlock if someone
      does a Session::awake() on you).
    */
    pthread_mutex_unlock(mysys_var->current_mutex);
    pthread_mutex_lock(&mysys_var->mutex);
    mysys_var->current_mutex = 0;
    mysys_var->current_cond = 0;
    this->set_proc_info(old_msg);
    pthread_mutex_unlock(&mysys_var->mutex);
  }
  inline time_t query_start() { return start_time; }
  inline void set_time()
  {
    if (user_time)
    {
      start_time= user_time;
      start_utime= utime_after_lock= my_micro_time();
    }
    else
      start_utime= utime_after_lock= my_micro_time_and_time(&start_time);
  }
  inline void	set_current_time()    { start_time= time(NULL); }
  inline void	set_time(time_t t)
  {
    start_time= user_time= t;
    start_utime= utime_after_lock= my_micro_time();
  }
  void set_time_after_lock()  { utime_after_lock= my_micro_time(); }
  uint64_t current_utime()  { return my_micro_time(); }
  inline uint64_t found_rows(void)
  {
    return limit_found_rows;
  }
  /** Returns whether the session is currently inside a transaction */
  inline bool inTransaction()
  {
    return server_status & SERVER_STATUS_IN_TRANS;
  }
  inline bool fill_derived_tables()
  {
    return !lex->only_view_structure();
  }
  inline void* trans_alloc(unsigned int size)
  {
    return alloc_root(&transaction.mem_root,size);
  }

  LEX_STRING *make_lex_string(LEX_STRING *lex_str,
                              const char* str, uint32_t length,
                              bool allocate_lex_string);

  bool convert_string(LEX_STRING *to, const CHARSET_INFO * const to_cs,
		      const char *from, uint32_t from_length,
		      const CHARSET_INFO * const from_cs);

  bool convert_string(String *s, const CHARSET_INFO * const from_cs, const CHARSET_INFO * const to_cs);

  void add_changed_table(Table *table);
  void add_changed_table(const char *key, long key_length);
  CHANGED_TableList * changed_table_dup(const char *key, long key_length);
  int send_explain_fields(select_result *result);
  /**
    Clear the current error, if any.
    We do not clear is_fatal_error or is_fatal_sub_stmt_error since we
    assume this is never called if the fatal error is set.
    @todo: To silence an error, one should use Internal_error_handler
    mechanism. In future this function will be removed.
  */
  inline void clear_error()
  {
    if (main_da.is_error())
      main_da.reset_diagnostics_area();
    return;
  }

  /**
    Mark the current error as fatal. Warning: this does not
    set any error, it sets a property of the error, so must be
    followed or prefixed with my_error().
  */
  inline void fatal_error()
  {
    assert(main_da.is_error());
    is_fatal_error= 1;
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
  inline bool is_error() const { return main_da.is_error(); }
  inline const CHARSET_INFO *charset() { return default_charset_info; }
  void update_charset();

  void change_item_tree(Item **place, Item *new_value)
  {
    *place= new_value;
  }
  /*
    Cleanup statement parse state (parse tree, lex) and execution
    state after execution of a non-prepared SQL statement.
  */
  void end_statement();
  inline int killed_errno() const
  {
    killed_state killed_val; /* to cache the volatile 'killed' */
    return (killed_val= killed) != KILL_BAD_DATA ? killed_val : 0;
  }
  void send_kill_message() const;
  /* return true if we will abort query if we make a warning now */
  inline bool really_abort_on_warning()
  {
    return (abort_on_warning);
  }
  void set_status_var_init();
  void reset_n_backup_open_tables_state(Open_tables_state *backup);
  void restore_backup_open_tables_state(Open_tables_state *backup);

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

    @return Operation status
      @retval false Success
      @retval true  Out-of-memory error
  */
  bool set_db(const char *new_db, size_t new_db_len);

  /**
    Set the current database; use shallow copy of C-string.

    @param new_db     a pointer to the new database name.
    @param new_db_len length of the new database name.

    @note This operation just sets {db, db_length}. Switching the current
    database usually involves other actions, like switching other database
    attributes including security context. In the future, this operation
    will be made private and more convenient interface will be provided.
  */
  void reset_db(char *new_db, size_t new_db_len)
  {
    db= new_db;
    db_length= new_db_len;
  }
  /*
    Copy the current database to the argument. Use the current arena to
    allocate memory for a deep copy: current database may be freed after
    a statement is parsed but before it's executed.
  */
  bool copy_db_to(char **p_db, size_t *p_db_length);
  /* session_scheduler for events */
  void *scheduler;

public:
  /**
    Add an internal error handler to the thread execution context.
    @param handler the exception handler to add
  */
  void push_internal_handler(Internal_error_handler *handler);

  /**
    Handle an error condition.
    @param sql_errno the error number
    @param level the error level
    @return true if the error is handled
  */
  virtual bool handle_error(uint32_t sql_errno, const char *message,
                            DRIZZLE_ERROR::enum_warning_level level);

  /**
    Remove the error handler last pushed.
  */
  void pop_internal_handler();

  /**
    Reset object after executing commands.
  */
  void reset_for_next_command();

  /**
   * Disconnects the session from a client connection and
   * updates any status variables necessary.
   *
   * @param errcode	Error code to print to console
   * @param should_lock 1 if we have have to lock LOCK_thread_count
   *
   * @note  For the connection that is doing shutdown, this is called twice
   */
  void disconnect(uint32_t errcode, bool lock);
  void close_temporary_tables();

  /**
   * Check if user exists and the password supplied is correct.
   *
   * Returns true on success, and false on failure.
   *
   * @note Host, user and passwd may point to communication buffer.
   * Current implementation does not depend on that, but future changes
   * should be done with this in mind; 
   *
   * @param  Scrambled password received from client
   * @param  Length of scrambled password
   * @param  Database name to connect to, may be NULL
   */
  bool checkUser(const char *passwd, uint32_t passwd_len, const char *db);

private:
  const char *proc_info;

  /** The current internal error handler for this thread, or NULL. */
  Internal_error_handler *m_internal_handler;
  /**
    The lex to hold the parsed tree of conventional (non-prepared) queries.
    Whereas for prepared and stored procedure statements we use an own lex
    instance for each new query, for conventional statements we reuse
    the same lex. (@see mysql_parse for details).
  */
  LEX main_lex;
  /**
    This memory root is used for two purposes:
    - for conventional queries, to allocate structures stored in main_lex
    during parsing, and allocate runtime data (execution plan, etc.)
    during execution.
    - for prepared queries, only to allocate runtime data. The parsed
    tree itself is reused between executions and thus is stored elsewhere.
  */
  MEM_ROOT main_mem_root;

public:
  /** A short cut for session->main_da.set_ok_status(). */
  inline void my_ok(ha_rows affected_rows= 0, uint64_t passed_id= 0, const char *message= NULL)
  {
    main_da.set_ok_status(this, affected_rows, passed_id, message);
  }


  /** A short cut for session->main_da.set_eof_status(). */

  inline void my_eof()
  {
    main_da.set_eof_status(this);
  }

  /* Some inline functions for more speed */

  inline bool add_item_to_list(Item *item)
  {
    return lex->current_select->add_item_to_list(this, item);
  }

  inline bool add_value_to_list(Item *value)
  {
    return lex->value_list.push_back(value);
  }

  inline bool add_order_to_list(Item *item, bool asc)
  {
    return lex->current_select->add_order_to_list(this, item, asc);
  }

  inline bool add_group_to_list(Item *item, bool asc)
  {
    return lex->current_select->add_group_to_list(this, item, asc);
  }
  void refresh_status();
};

/*
  This is used to get result from a select
*/

class JOIN;


#define ESCAPE_CHARS "ntrb0ZN" // keep synchronous with READ_INFO::unescape

#include <drizzled/select_to_file.h>

#include <drizzled/select_export.h>

#include <drizzled/select_dump.h>

#include <drizzled/select_insert.h>

#include <drizzled/select_create.h>



#include <storage/myisam/myisam.h>

#include <drizzled/tmp_table_param.h>

#include <drizzled/select_union.h>

#include <drizzled/select_subselect.h>

#include <drizzled/select_singlerow_subselect.h>
#include <drizzled/select_max_min_finder_subselect.h>
#include <drizzled/select_exists_subselect.h>

/* Structs used when sorting */

typedef struct st_sort_field {
  Field *field;				/* Field to sort */
  Item	*item;				/* Item if not sorting fields */
  size_t length;			/* Length of sort field */
  uint32_t   suffix_length;                 /* Length suffix (0-4) */
  Item_result result_type;		/* Type of item */
  bool reverse;				/* if descending sort */
  bool need_strxnfrm;			/* If we have to use strxnfrm() */
} SORT_FIELD;


typedef struct st_sort_buffer {
  uint32_t index;					/* 0 or 1 */
  uint32_t sort_orders;
  uint32_t change_pos;				/* If sort-fields changed */
  char **buff;
  SORT_FIELD *sortorder;
} SORT_BUFFER;


#include <drizzled/table_ident.h>
#include <drizzled/user_var_entry.h>
#include <drizzled/unique.h>
#include <drizzled/multi_delete.h>
#include <drizzled/multi_update.h>
#include <drizzled/my_var.h>
#include <drizzled/select_dumpvar.h>

/* Bits in sql_command_flags */

enum sql_command_flag_bits {
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

/* Functions in sql_class.cc */

void add_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var);

void add_diff_to_status(STATUS_VAR *to_var, STATUS_VAR *from_var,
                        STATUS_VAR *dec_var);

#endif /* DRIZZLED_SESSION_H */
