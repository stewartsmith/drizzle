/*****************************************************************************

Copyright (C) 2000, 2010, MySQL AB & Innobase Oy. All Rights Reserved.
Copyright (C) 2008, 2009 Google Inc.
Copyright (C) 2009, Percona Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/* TODO list for the InnoDB Cursor in 5.0:
  - fix savepoint functions to use savepoint storage area
  - Find out what kind of problems the OS X case-insensitivity causes to
    table and database names; should we 'normalize' the names like we do
    in Windows?
*/

#include <config.h>

#include <limits.h>
#include <fcntl.h>

#include <drizzled/error.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/internal/my_sys.h>
#include <drizzled/plugin.h>
#include <drizzled/show.h>
#include <drizzled/data_home.h>
#include <drizzled/error.h>
#include <drizzled/field.h>
#include <drizzled/charset.h>
#include <drizzled/session.h>
#include <drizzled/current_session.h>
#include <drizzled/table.h>
#include <drizzled/field/blob.h>
#include <drizzled/field/varstring.h>
#include <drizzled/plugin/xa_storage_engine.h>
#include <drizzled/plugin/daemon.h>
#include <drizzled/memory/multi_malloc.h>
#include <drizzled/pthread_globals.h>
#include <drizzled/named_savepoint.h>
#include <drizzled/session/table_messages.h>
#include <drizzled/transaction_services.h>
#include <drizzled/message/statement_transform.h>
#include <drizzled/cached_directory.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/system_variables.h>
#include <drizzled/session/times.h>
#include <drizzled/session/transactions.h>
#include <drizzled/typelib.h>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/scoped_array.hpp>
#include <boost/filesystem.hpp>
#include <drizzled/module/option_map.h>
#include <iostream>

namespace po= boost::program_options;
namespace fs=boost::filesystem;
using namespace std;

/** @file ha_innodb.cc */

/* Include necessary InnoDB headers */
#include "univ.i"
#include "buf0lru.h"
#include "btr0sea.h"
#include "os0file.h"
#include "os0thread.h"
#include "srv0start.h"
#include "srv0srv.h"
#include "trx0roll.h"
#include "trx0trx.h"
#include "trx0sys.h"
#include "mtr0mtr.h"
#include "row0ins.h"
#include "row0mysql.h"
#include "row0sel.h"
#include "row0upd.h"
#include "log0log.h"
#include "lock0lock.h"
#include "dict0crea.h"
#include "create_replication.h"
#include "btr0cur.h"
#include "btr0btr.h"
#include "fsp0fsp.h"
#include "sync0sync.h"
#include "fil0fil.h"
#include "trx0xa.h"
#include "row0merge.h"
#include "thr0loc.h"
#include "dict0boot.h"
#include "ha_prototypes.h"
#include "ut0mem.h"
#include "ibuf0ibuf.h"

#include "ha_innodb.h"
#include "data_dictionary.h"
#include "replication_dictionary.h"
#include "internal_dictionary.h"
#include "handler0vars.h"

#include <iostream>
#include <sstream>
#include <string>

#include <plugin/innobase/handler/status_function.h>
#include <plugin/innobase/handler/replication_log.h>

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/text_format.h>

#include <boost/thread/mutex.hpp>

using namespace std;
using namespace drizzled;

/** to protect innobase_open_files */
static boost::mutex innobase_share_mutex;

/** to force correct commit order in binlog */
static ulong commit_threads = 0;
static boost::condition_variable commit_cond;
static boost::mutex commit_cond_m;
static bool innodb_inited = 0;

#define INSIDE_HA_INNOBASE_CC

/* In the Windows plugin, the return value of current_session is
undefined.  Map it to NULL. */
#if defined MYSQL_DYNAMIC_PLUGIN && defined __WIN__
# undef current_session
# define current_session NULL
# define EQ_CURRENT_SESSION(session) TRUE
#else /* MYSQL_DYNAMIC_PLUGIN && __WIN__ */
# define EQ_CURRENT_SESSION(session) ((session) == current_session)
#endif /* MYSQL_DYNAMIC_PLUGIN && __WIN__ */

static plugin::XaStorageEngine* innodb_engine_ptr= NULL;

typedef constrained_check<uint32_t, UINT32_MAX, 10> open_files_constraint;
static open_files_constraint innobase_open_files;
typedef constrained_check<uint32_t, 10, 1> mirrored_log_groups_constraint;
static mirrored_log_groups_constraint innobase_mirrored_log_groups;
typedef constrained_check<uint32_t, 100, 2> log_files_in_group_constraint;
static log_files_in_group_constraint innobase_log_files_in_group;
typedef constrained_check<uint32_t, 6, 0> force_recovery_constraint;
force_recovery_constraint innobase_force_recovery;
typedef constrained_check<size_t, SIZE_MAX, 256*1024, 1024> log_buffer_constraint;
static log_buffer_constraint innobase_log_buffer_size;
typedef constrained_check<size_t, SIZE_MAX, 512*1024, 1024> additional_mem_pool_constraint;
static additional_mem_pool_constraint innobase_additional_mem_pool_size;
typedef constrained_check<unsigned int, 1000, 1> autoextend_constraint;
static autoextend_constraint innodb_auto_extend_increment;
typedef constrained_check<size_t, SIZE_MAX, 33554432, 1048576> buffer_pool_constraint;
static buffer_pool_constraint innobase_buffer_pool_size;
typedef constrained_check<uint32_t, MAX_BUFFER_POOLS, 1> buffer_pool_instances_constraint;
static buffer_pool_instances_constraint innobase_buffer_pool_instances;
typedef constrained_check<uint32_t, UINT32_MAX, 100> io_capacity_constraint;
static io_capacity_constraint innodb_io_capacity;
typedef constrained_check<uint32_t, 5000, 1> purge_batch_constraint;
static purge_batch_constraint innodb_purge_batch_size;
typedef constrained_check<uint32_t, 1, 0> purge_threads_constraint;
static purge_threads_constraint innodb_n_purge_threads;
typedef constrained_check<uint32_t, 2, 0> trinary_constraint;
static trinary_constraint innodb_flush_log_at_trx_commit;
typedef constrained_check<unsigned int, 99, 0> max_dirty_pages_constraint;
static max_dirty_pages_constraint innodb_max_dirty_pages_pct;
static uint64_constraint innodb_max_purge_lag;
static uint64_nonzero_constraint innodb_stats_sample_pages;
typedef constrained_check<uint32_t, 64, 1> io_threads_constraint;
static io_threads_constraint innobase_read_io_threads;
static io_threads_constraint innobase_write_io_threads;

typedef constrained_check<uint32_t, 1000, 0> concurrency_constraint;
static concurrency_constraint innobase_commit_concurrency;
static concurrency_constraint innobase_thread_concurrency;
static uint32_nonzero_constraint innodb_concurrency_tickets;

typedef constrained_check<int64_t, INT64_MAX, 1024*1024, 1024*1024> log_file_constraint;
static log_file_constraint innobase_log_file_size;

static uint64_constraint innodb_replication_delay;

static uint32_constraint buffer_pool_restore_at_startup;

/** Percentage of the buffer pool to reserve for 'old' blocks.
Connected to buf_LRU_old_ratio. */
typedef constrained_check<uint32_t, 95, 5> old_blocks_constraint;
static old_blocks_constraint innobase_old_blocks_pct;

static uint32_constraint innodb_sync_spin_loops;
static uint32_constraint innodb_spin_wait_delay;
static uint32_constraint innodb_thread_sleep_delay;

typedef constrained_check<uint32_t, 64, 0> read_ahead_threshold_constraint;
static read_ahead_threshold_constraint innodb_read_ahead_threshold;

static uint64_constraint ibuf_max_size;

typedef constrained_check<uint32_t, 1, 0> binary_constraint;
static binary_constraint ibuf_active_contract;

typedef constrained_check<uint32_t, 999999999, 100> ibuf_accel_rate_constraint;
static ibuf_accel_rate_constraint ibuf_accel_rate;
static uint32_constraint checkpoint_age_target;
static binary_constraint flush_neighbor_pages;

/* The default values for the following char* start-up parameters
are determined in innobase_init below: */

std::string innobase_data_home_dir;
std::string innobase_data_file_path;
std::string innobase_log_group_home_dir;
static string innobase_file_format_name;
static string innobase_change_buffering;

static string read_ahead;
static string adaptive_flushing_method;

/* The highest file format being used in the database. The value can be
set by user, however, it will be adjusted to the newer file format if
a table of such format is created/opened. */
static string innobase_file_format_max;

/* Below we have boolean-valued start-up parameters, and their default
values */

static trinary_constraint innobase_fast_shutdown;

/* "innobase_file_format_check" decides whether we would continue
booting the server if the file format stamped on the system
table space exceeds the maximum file format supported
by the server. Can be set during server startup at command
line or configure file, and a read only variable after
server startup */

/* If a new file format is introduced, the file format
name needs to be updated accordingly. Please refer to
file_format_name_map[] defined in trx0sys.c for the next
file format name. */

static my_bool  innobase_file_format_check = TRUE;
static my_bool  innobase_use_doublewrite    = TRUE;
static my_bool  innobase_use_checksums      = TRUE;
static my_bool  innobase_rollback_on_timeout    = FALSE;
static my_bool  innobase_create_status_file   = FALSE;
static bool innobase_use_replication_log;
static bool support_xa;
static bool strict_mode;
typedef constrained_check<uint32_t, 1024*1024*1024, 1> lock_wait_constraint;
static lock_wait_constraint lock_wait_timeout;

static char*  internal_innobase_data_file_path  = NULL;

/* The following counter is used to convey information to InnoDB
about server activity: in selects it is not sensible to call
srv_active_wake_master_thread after each fetch or search, we only do
it every INNOBASE_WAKE_INTERVAL'th step. */

#define INNOBASE_WAKE_INTERVAL  32
static ulong  innobase_active_counter = 0;

static hash_table_t*  innobase_open_tables;

#ifdef __NETWARE__  /* some special cleanup for NetWare */
bool nw_panic = FALSE;
#endif

/** Allowed values of innodb_change_buffering */
static const char* innobase_change_buffering_values[IBUF_USE_COUNT] = {
  "none",   /* IBUF_USE_NONE */
  "inserts",	/* IBUF_USE_INSERT */
  "deletes",	/* IBUF_USE_DELETE_MARK */
  "changes",	/* IBUF_USE_INSERT_DELETE_MARK */
  "purges",	/* IBUF_USE_DELETE */
  "all"		/* IBUF_USE_ALL */
};

/** Allowed values of read_ahead */
static const char* read_ahead_names[] = {
  "none",       /* 0 */
  "random",
  "linear",
  "both",       /* 3 */
  /* For compatibility with the older Percona patch */
  "0",          /* 4 ("none" + 4) */
  "1",
  "2",
  "3",          /* 7 ("both" + 4) */
  NULL
};

static TYPELIB read_ahead_typelib = {
  array_elements(read_ahead_names) - 1, "read_ahead_typelib",
  read_ahead_names, NULL
};

/** Allowed values of adaptive_flushing_method */
static const char* adaptive_flushing_method_names[] = {
    "native",           /* 0 */
    "estimate",         /* 1 */
    "keep_average",     /* 2 */
    /* For compatibility with the older Percona patch */
    "0",                /* 3 ("native" + 3) */
    "1",                /* 4 ("estimate" + 3) */
    "2",                /* 5 ("keep_average" + 3) */
    NULL
};

static TYPELIB adaptive_flushing_method_typelib = {
  array_elements(adaptive_flushing_method_names) - 1,
  "adaptive_flushing_method_typelib",
  adaptive_flushing_method_names, NULL
};

/* "GEN_CLUST_INDEX" is the name reserved for Innodb default
system primary index. */
static const char innobase_index_reserve_name[]= "GEN_CLUST_INDEX";

/********************************************************************
Gives the file extension of an InnoDB single-table tablespace. */
static const char* ha_innobase_exts[] = {
  ".ibd",
  NULL
};

#define DEFAULT_FILE_EXTENSION ".dfe" // Deep Fried Elephant

static INNOBASE_SHARE *get_share(const char *table_name);
static void free_share(INNOBASE_SHARE *share);

class InnobaseEngine : public plugin::XaStorageEngine
{
public:
  explicit InnobaseEngine(string name_arg) :
    plugin::XaStorageEngine(name_arg,
                            HTON_NULL_IN_KEY |
                            HTON_CAN_INDEX_BLOBS |
                            HTON_PRIMARY_KEY_IN_READ_INDEX |
                            HTON_PARTIAL_COLUMN_READ |
                            HTON_TABLE_SCAN_ON_INDEX |
                            HTON_HAS_FOREIGN_KEYS |
                            HTON_HAS_DOES_TRANSACTIONS)
  {
    table_definition_ext= plugin::DEFAULT_DEFINITION_FILE_EXT;
    addAlias("INNOBASE");
  }

  virtual ~InnobaseEngine()
  {
    if (innodb_inited) {
      srv_fast_shutdown = (ulint) innobase_fast_shutdown;
      innodb_inited = 0;
      hash_table_free(innobase_open_tables);
      innobase_open_tables = NULL;
      if (innobase_shutdown_for_mysql() != DB_SUCCESS) {
        // Throw here?
      }
      srv_free_paths_and_sizes();
      free(internal_innobase_data_file_path);
    }
    
    /* These get strdup'd from vm variables */

  }

private:
  virtual int doStartTransaction(Session *session, start_transaction_option_t options);
  virtual void doStartStatement(Session *session);
  virtual void doEndStatement(Session *session);
public:
  virtual
  int
  close_connection(
/*======================*/
      /* out: 0 or error number */
  Session*  session); /* in: handle to the MySQL thread of the user
      whose resources should be free'd */

  virtual int doSetSavepoint(Session* session,
                                 drizzled::NamedSavepoint &savepoint);
  virtual int doRollbackToSavepoint(Session* session,
                                     drizzled::NamedSavepoint &savepoint);
  virtual int doReleaseSavepoint(Session* session,
                                     drizzled::NamedSavepoint &savepoint);
  virtual int doXaCommit(Session* session, bool all)
  {
    return doCommit(session, all); /* XA commit just does a SQL COMMIT */
  }
  virtual int doXaRollback(Session *session, bool all)
  {
    return doRollback(session, all); /* XA rollback just does a SQL ROLLBACK */
  }
  virtual uint64_t doGetCurrentTransactionId(Session *session);
  virtual uint64_t doGetNewTransactionId(Session *session);
  virtual int doCommit(Session* session, bool all);
  virtual int doRollback(Session* session, bool all);

  /***********************************************************************
  This function is used to prepare X/Open XA distributed transaction   */
  virtual
  int
  doXaPrepare(
  /*================*/
        /* out: 0 or error number */
    Session*  session,  /* in: handle to the MySQL thread of the user
        whose XA transaction should be prepared */
    bool  all); /* in: TRUE - commit transaction
        FALSE - the current SQL statement ended */
  /***********************************************************************
  This function is used to recover X/Open XA distributed transactions   */
  virtual
  int
  doXaRecover(
  /*================*/
          /* out: number of prepared transactions
          stored in xid_list */
    ::drizzled::XID*  xid_list, /* in/out: prepared transactions */
    size_t len);    /* in: number of slots in xid_list */
  /***********************************************************************
  This function is used to commit one X/Open XA distributed transaction
  which is in the prepared state */
  virtual
  int
  doXaCommitXid(
  /*===================*/
        /* out: 0 or error number */
    ::drizzled::XID*  xid); /* in: X/Open XA transaction identification */
  /***********************************************************************
  This function is used to rollback one X/Open XA distributed transaction
  which is in the prepared state */
  virtual
  int
  doXaRollbackXid(
  /*=====================*/
        /* out: 0 or error number */
    ::drizzled::XID *xid);  /* in: X/Open XA transaction identification */

  virtual Cursor *create(Table &table)
  {
    return new ha_innobase(*this, table);
  }

  /*********************************************************************
  Removes all tables in the named database inside InnoDB. */
  bool
  doDropSchema(
  /*===================*/
        /* out: error number */
    const identifier::Schema  &identifier); /* in: database path; inside InnoDB the name
        of the last directory in the path is used as
        the database name: for example, in 'mysql/data/test'
        the database name is 'test' */

  /********************************************************************
  Flushes InnoDB logs to disk and makes a checkpoint. Really, a commit flushes
  the logs, and the name of this function should be innobase_checkpoint. */
  virtual
  bool
  flush_logs();
  /*================*/
          /* out: TRUE if error */
  
  /****************************************************************************
  Implements the SHOW INNODB STATUS command. Sends the output of the InnoDB
  Monitor to the client. */
  virtual
  bool
  show_status(
  /*===============*/
    Session*  session,  /* in: the MySQL query thread of the caller */
    stat_print_fn *stat_print,
  enum ha_stat_type stat_type);

  virtual
  int
  doReleaseTemporaryLatches(
  /*===============================*/
        /* out: 0 */
  Session*    session); /* in: MySQL thread */


  const char** bas_ext() const {
  return(ha_innobase_exts);
  }

  UNIV_INTERN int doCreateTable(Session &session,
                                Table &form,
                                const identifier::Table &identifier,
                                const message::Table&);
  UNIV_INTERN int doRenameTable(Session&, const identifier::Table &from, const identifier::Table &to);
  UNIV_INTERN int doDropTable(Session &session, const identifier::Table &identifier);

  UNIV_INTERN virtual bool get_error_message(int error, String *buf) const;

  UNIV_INTERN uint32_t max_supported_keys() const;
  UNIV_INTERN uint32_t max_supported_key_length() const;
  UNIV_INTERN uint32_t max_supported_key_part_length() const;


  UNIV_INTERN uint32_t index_flags(enum  ha_key_alg) const
  {
    return (HA_READ_NEXT |
            HA_READ_PREV |
            HA_READ_ORDER |
            HA_READ_RANGE |
            HA_KEYREAD_ONLY);
  }

  int doGetTableDefinition(drizzled::Session& session,
                           const identifier::Table &identifier,
                           drizzled::message::Table &table_proto);

  bool doDoesTableExist(drizzled::Session& session, const identifier::Table &identifier);

  void doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                             const drizzled::identifier::Schema &schema_identifier,
                             drizzled::identifier::table::vector &set_of_identifiers);
  bool validateCreateTableOption(const std::string &key, const std::string &state);
  void dropTemporarySchema();

};


bool InnobaseEngine::validateCreateTableOption(const std::string &key, const std::string &state)
{
  if (boost::iequals(key, "ROW_FORMAT"))
  {
    if (boost::iequals(state, "COMPRESSED"))
      return true;

    if (boost::iequals(state, "COMPACT"))
      return true;

    if (boost::iequals(state, "DYNAMIC"))
      return true;

    if (boost::iequals(state, "REDUNDANT"))
      return true;
  }

  return false;
}

void InnobaseEngine::doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                                           const drizzled::identifier::Schema &schema_identifier,
                                           drizzled::identifier::table::vector &set_of_identifiers)
{
  CachedDirectory::Entries entries= directory.getEntries();

  std::string search_string(schema_identifier.getSchemaName());

  boost::algorithm::to_lower(search_string);

  if (search_string.compare("data_dictionary") == 0)
  {
    set_of_identifiers.push_back(identifier::Table(schema_identifier.getSchemaName(), "SYS_REPLICATION_LOG"));
  }

  for (CachedDirectory::Entries::iterator entry_iter= entries.begin(); 
       entry_iter != entries.end(); ++entry_iter)
  {
    CachedDirectory::Entry *entry= *entry_iter;
    const string *filename= &entry->filename;

    assert(filename->size());

    const char *ext= strchr(filename->c_str(), '.');

    if (ext == NULL || my_strcasecmp(system_charset_info, ext, DEFAULT_FILE_EXTENSION) ||
        (filename->compare(0, strlen(TMP_FILE_PREFIX), TMP_FILE_PREFIX) == 0))
    { }
    else
    {
      std::string path;
      path+= directory.getPath();
      path+= FN_LIBCHAR;
      path+= entry->filename;

      message::Table definition;
      if (StorageEngine::readTableFile(path, definition))
      {
        /* 
           Using schema_identifier here to stop unused warning, could use
           definition.schema() instead
        */
        identifier::Table identifier(schema_identifier.getSchemaName(), definition.name());
        set_of_identifiers.push_back(identifier);
      }
    }
  }
}

bool InnobaseEngine::doDoesTableExist(Session &session, const identifier::Table &identifier)
{
  string proto_path(identifier.getPath());
  proto_path.append(DEFAULT_FILE_EXTENSION);

  if (session.getMessageCache().doesTableMessageExist(identifier))
    return true;

  std::string search_string(identifier.getPath());
  boost::algorithm::to_lower(search_string);

  if (search_string.compare("data_dictionary/sys_replication_log") == 0)
    return true;

  if (access(proto_path.c_str(), F_OK))
  {
    return false;
  }

  return true;
}

int InnobaseEngine::doGetTableDefinition(Session &session,
                                         const identifier::Table &identifier,
                                         message::Table &table_proto)
{
  string proto_path(identifier.getPath());
  proto_path.append(DEFAULT_FILE_EXTENSION);

  // First we check the temporary tables.
  if (session.getMessageCache().getTableMessage(identifier, table_proto))
    return EEXIST;

  if (read_replication_log_table_message(identifier.getTableName().c_str(), &table_proto) == 0)
    return EEXIST;

  if (access(proto_path.c_str(), F_OK))
  {
    return errno;
  }

  if (StorageEngine::readTableFile(proto_path, table_proto))
    return EEXIST;

  return ENOENT;
}


/************************************************************//**
Validate the file format name and return its corresponding id.
@return valid file format id */
static
uint
innobase_file_format_name_lookup(
/*=============================*/
  const char* format_name);   /*!< in: pointer to file format
            name */
/************************************************************//**
Validate the file format check config parameters, as a side effect it
sets the srv_max_file_format_at_startup variable.
@return	the format_id if valid config value, otherwise, return -1 */
static
int
innobase_file_format_validate_and_set(
/*================================*/
  const char* format_max);    /*!< in: parameter value */

static const char innobase_engine_name[]= "InnoDB";


/*****************************************************************//**
Commits a transaction in an InnoDB database. */
static
void
innobase_commit_low(
/*================*/
  trx_t*  trx); /*!< in: transaction handle */

static drizzle_show_var innodb_status_variables[]= {
  {"buffer_pool_pages_data",
  (char*) &export_vars.innodb_buffer_pool_pages_data,   SHOW_LONG},
  {"buffer_pool_pages_dirty",
  (char*) &export_vars.innodb_buffer_pool_pages_dirty,    SHOW_LONG},
  {"buffer_pool_pages_flushed",
  (char*) &export_vars.innodb_buffer_pool_pages_flushed,  SHOW_LONG},
  {"buffer_pool_pages_free",
  (char*) &export_vars.innodb_buffer_pool_pages_free,   SHOW_LONG},
#ifdef UNIV_DEBUG
  {"buffer_pool_pages_latched",
  (char*) &export_vars.innodb_buffer_pool_pages_latched,  SHOW_LONG},
#endif /* UNIV_DEBUG */
  {"buffer_pool_pages_misc",
  (char*) &export_vars.innodb_buffer_pool_pages_misc,   SHOW_LONG},
  {"buffer_pool_pages_total",
  (char*) &export_vars.innodb_buffer_pool_pages_total,    SHOW_LONG},
  {"buffer_pool_read_ahead",
  (char*) &export_vars.innodb_buffer_pool_read_ahead, SHOW_LONG},
  {"buffer_pool_read_ahead_evicted",
  (char*) &export_vars.innodb_buffer_pool_read_ahead_evicted, SHOW_LONG},
  {"buffer_pool_read_requests",
  (char*) &export_vars.innodb_buffer_pool_read_requests,  SHOW_LONG},
  {"buffer_pool_reads",
  (char*) &export_vars.innodb_buffer_pool_reads,    SHOW_LONG},
  {"buffer_pool_wait_free",
  (char*) &export_vars.innodb_buffer_pool_wait_free,    SHOW_LONG},
  {"buffer_pool_write_requests",
  (char*) &export_vars.innodb_buffer_pool_write_requests, SHOW_LONG},
  {"data_fsyncs",
  (char*) &export_vars.innodb_data_fsyncs,      SHOW_LONG},
  {"data_pending_fsyncs",
  (char*) &export_vars.innodb_data_pending_fsyncs,    SHOW_LONG},
  {"data_pending_reads",
  (char*) &export_vars.innodb_data_pending_reads,   SHOW_LONG},
  {"data_pending_writes",
  (char*) &export_vars.innodb_data_pending_writes,    SHOW_LONG},
  {"data_read",
  (char*) &export_vars.innodb_data_read,      SHOW_LONG},
  {"data_reads",
  (char*) &export_vars.innodb_data_reads,     SHOW_LONG},
  {"data_writes",
  (char*) &export_vars.innodb_data_writes,      SHOW_LONG},
  {"data_written",
  (char*) &export_vars.innodb_data_written,     SHOW_LONG},
  {"dblwr_pages_written",
  (char*) &export_vars.innodb_dblwr_pages_written,    SHOW_LONG},
  {"dblwr_writes",
  (char*) &export_vars.innodb_dblwr_writes,     SHOW_LONG},
  {"have_atomic_builtins",
  (char*) &export_vars.innodb_have_atomic_builtins,   SHOW_BOOL},
  {"log_waits",
  (char*) &export_vars.innodb_log_waits,      SHOW_LONG},
  {"log_write_requests",
  (char*) &export_vars.innodb_log_write_requests,   SHOW_LONG},
  {"log_writes",
  (char*) &export_vars.innodb_log_writes,     SHOW_LONG},
  {"os_log_fsyncs",
  (char*) &export_vars.innodb_os_log_fsyncs,      SHOW_LONG},
  {"os_log_pending_fsyncs",
  (char*) &export_vars.innodb_os_log_pending_fsyncs,    SHOW_LONG},
  {"os_log_pending_writes",
  (char*) &export_vars.innodb_os_log_pending_writes,    SHOW_LONG},
  {"os_log_written",
  (char*) &export_vars.innodb_os_log_written,     SHOW_LONG},
  {"page_size",
  (char*) &export_vars.innodb_page_size,      SHOW_LONG},
  {"pages_created",
  (char*) &export_vars.innodb_pages_created,      SHOW_LONG},
  {"pages_read",
  (char*) &export_vars.innodb_pages_read,     SHOW_LONG},
  {"pages_written",
  (char*) &export_vars.innodb_pages_written,      SHOW_LONG},
  {"row_lock_current_waits",
  (char*) &export_vars.innodb_row_lock_current_waits,   SHOW_LONG},
  {"row_lock_time",
  (char*) &export_vars.innodb_row_lock_time,      SHOW_LONGLONG},
  {"row_lock_time_avg",
  (char*) &export_vars.innodb_row_lock_time_avg,    SHOW_LONG},
  {"row_lock_time_max",
  (char*) &export_vars.innodb_row_lock_time_max,    SHOW_LONG},
  {"row_lock_waits",
  (char*) &export_vars.innodb_row_lock_waits,     SHOW_LONG},
  {"rows_deleted",
  (char*) &export_vars.innodb_rows_deleted,     SHOW_LONG},
  {"rows_inserted",
  (char*) &export_vars.innodb_rows_inserted,      SHOW_LONG},
  {"rows_read",
  (char*) &export_vars.innodb_rows_read,      SHOW_LONG},
  {"rows_updated",
  (char*) &export_vars.innodb_rows_updated,     SHOW_LONG},
  {NULL, NULL, SHOW_LONG}
};

InnodbStatusTool::Generator::Generator(drizzled::Field **fields) :
  plugin::TableFunction::Generator(fields)
{ 
  srv_export_innodb_status();
  status_var_ptr= innodb_status_variables;
}

bool InnodbStatusTool::Generator::populate()
{
  if (status_var_ptr->name)
  {
    std::ostringstream oss;
    string return_value;
    const char *value= status_var_ptr->value;

    /* VARIABLE_NAME */
    push(status_var_ptr->name);

    switch (status_var_ptr->type)
    {
    case SHOW_LONG:
      oss << *(int64_t*) value;
      return_value= oss.str();
      break;
    case SHOW_LONGLONG:
      oss << *(int64_t*) value;
      return_value= oss.str();
      break;
    case SHOW_BOOL:
      return_value= *(bool*) value ? "ON" : "OFF";
      break;
    default:
      assert(0);
    }

    /* VARIABLE_VALUE */
    if (return_value.length())
      push(return_value);
    else 
      push(" ");

    status_var_ptr++;

    return true;
  }
  return false;
}

/* General functions */

/******************************************************************//**
Returns true if the thread is the replication thread on the slave
server. Used in srv_conc_enter_innodb() to determine if the thread
should be allowed to enter InnoDB - the replication thread is treated
differently than other threads. Also used in
srv_conc_force_exit_innodb().

DRIZZLE: Note, we didn't change this name to avoid more ifdef forking 
         in non-Cursor code.
@return true if session is the replication thread */
UNIV_INTERN
ibool
thd_is_replication_slave_thread(
/*============================*/
  drizzled::Session* ) /*!< in: thread handle (Session*) */
{
  return false;
}

/******************************************************************//**
Save some CPU by testing the value of srv_thread_concurrency in inline
functions. */
static inline
void
innodb_srv_conc_enter_innodb(
/*=========================*/
  trx_t*  trx)  /*!< in: transaction handle */
{
  if (UNIV_LIKELY(!srv_thread_concurrency)) {

    return;
  }

  srv_conc_enter_innodb(trx);
}

/******************************************************************//**
Save some CPU by testing the value of srv_thread_concurrency in inline
functions. */
static inline
void
innodb_srv_conc_exit_innodb(
/*========================*/
  trx_t*  trx)  /*!< in: transaction handle */
{
  if (UNIV_LIKELY(!trx->declared_to_be_inside_innodb)) {

    return;
  }

  srv_conc_exit_innodb(trx);
}

/******************************************************************//**
Releases possible search latch and InnoDB thread FIFO ticket. These should
be released at each SQL statement end, and also when mysqld passes the
control to the client. It does no harm to release these also in the middle
of an SQL statement. */
static inline
void
innobase_release_stat_resources(
/*============================*/
  trx_t*  trx)  /*!< in: transaction object */
{
  if (trx->has_search_latch) {
    trx_search_latch_release_if_reserved(trx);
  }

  if (trx->declared_to_be_inside_innodb) {
    /* Release our possible ticket in the FIFO */

    srv_conc_force_exit_innodb(trx);
  }
}

/******************************************************************//**
Returns true if the transaction this thread is processing has edited
non-transactional tables. Used by the deadlock detector when deciding
which transaction to rollback in case of a deadlock - we try to avoid
rolling back transactions that have edited non-transactional tables.

DRIZZLE: Note, we didn't change this name to avoid more ifdef forking 
         in non-Cursor code.
@return true if non-transactional tables have been edited */
UNIV_INTERN
ibool
thd_has_edited_nontrans_tables(
/*===========================*/
  drizzled::Session *session)  /*!< in: thread handle (Session*) */
{
  return((ibool)session->transaction.all.hasModifiedNonTransData());
}

/******************************************************************//**
Returns true if the thread is executing a SELECT statement.
@return true if session is executing SELECT */
UNIV_INTERN
ibool
thd_is_select(
/*==========*/
  const drizzled::Session *session)  /*!< in: thread handle (Session*) */
{
  return(session->getSqlCommand() == SQLCOM_SELECT);
}

/******************************************************************//**
Returns true if the thread supports XA,
global value of innodb_supports_xa if session is NULL.
@return true if session has XA support */
UNIV_INTERN
ibool
thd_supports_xa(
/*============*/
  drizzled::Session* )  /*!< in: thread handle (Session*), or NULL to query
        the global innodb_supports_xa */
{
  /* TODO: Add support here for per-session value */
  return(support_xa);
}

/******************************************************************//**
Returns the lock wait timeout for the current connection.
@return the lock wait timeout, in seconds */
UNIV_INTERN
ulong
thd_lock_wait_timeout(
/*==================*/
  drizzled::Session*)  /*!< in: thread handle (Session*), or NULL to query
      the global innodb_lock_wait_timeout */
{
  /* TODO: Add support here for per-session value */
  /* According to <drizzle/plugin.h>, passing session == NULL
  returns the global value of the session variable. */
  return((ulong)lock_wait_timeout.get());
}

/******************************************************************//**
Set the time waited for the lock for the current query. */
UNIV_INTERN
void
thd_set_lock_wait_time(
/*===================*/
	drizzled::Session*	in_session,	/*!< in: thread handle (THD*) */
	ulint	value)	/*!< in: time waited for the lock */
{
  if (in_session)
    in_session->times.utime_after_lock+= value;
}

/********************************************************************//**
Obtain the InnoDB transaction of a MySQL thread.
@return reference to transaction pointer */
static inline
trx_t*&
session_to_trx(
/*=======*/
  Session*  session)  /*!< in: Drizzle Session */
{
  return *(trx_t**) session->getEngineData(innodb_engine_ptr);
}


plugin::ReplicationReturnCode ReplicationLog::apply(Session &session,
                                                    const message::Transaction &message)
{
  char *data= new char[message.ByteSize()];

  message.SerializeToArray(data, message.ByteSize());

  trx_t *trx= session_to_trx(&session);

  uint64_t trx_id= message.transaction_context().transaction_id();
  uint32_t seg_id= message.segment_id();
  uint64_t end_timestamp= message.transaction_context().end_timestamp();
  bool is_end_segment= message.end_segment();
  trx->log_commit_id= TRUE;

  string server_uuid= session.getServerUUID();
  string originating_server_uuid= session.getOriginatingServerUUID();
  uint64_t originating_commit_id= session.getOriginatingCommitID();
  bool use_originating_server_uuid= session.isOriginatingServerUUIDSet();

  ulint error= insert_replication_message(data, message.ByteSize(), trx, trx_id,
               end_timestamp, is_end_segment, seg_id, server_uuid.c_str(),
               use_originating_server_uuid, originating_server_uuid.c_str(),
               originating_commit_id);

  (void)error;

  delete[] data;

  return plugin::SUCCESS;
}

/********************************************************************//**
Call this function when mysqld passes control to the client. That is to
avoid deadlocks on the adaptive hash S-latch possibly held by session. For more
documentation, see Cursor.cc.
@return 0 */
int
InnobaseEngine::doReleaseTemporaryLatches(
/*===============================*/
  Session*    session)  /*!< in: MySQL thread */
{
  trx_t*  trx;

  assert(this == innodb_engine_ptr);

  if (!innodb_inited) {

    return(0);
  }

  trx = session_to_trx(session);

  if (trx) {
    innobase_release_stat_resources(trx);
  }
  return(0);
}

/********************************************************************//**
Increments innobase_active_counter and every INNOBASE_WAKE_INTERVALth
time calls srv_active_wake_master_thread. This function should be used
when a single database operation may introduce a small need for
server utility activity, like checkpointing. */
static inline
void
innobase_active_small(void)
/*=======================*/
{
  innobase_active_counter++;

  if ((innobase_active_counter % INNOBASE_WAKE_INTERVAL) == 0) {
    srv_active_wake_master_thread();
  }
}

/********************************************************************//**
Converts an InnoDB error code to a MySQL error code and also tells to MySQL
about a possible transaction rollback inside InnoDB caused by a lock wait
timeout or a deadlock.
@return MySQL error code */
UNIV_INTERN
int
convert_error_code_to_mysql(
/*========================*/
  int   error,  /*!< in: InnoDB error code */
  ulint   flags,  /*!< in: InnoDB table flags, or 0 */
  Session*  session)/*!< in: user thread handle or NULL */
{
  switch (error) {
  case DB_SUCCESS:
    return(0);

  case DB_INTERRUPTED:
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
    /* fall through */

  case DB_FOREIGN_EXCEED_MAX_CASCADE:
    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        HA_ERR_ROW_IS_REFERENCED,
                        "InnoDB: Cannot delete/update "
                        "rows with cascading foreign key "
                        "constraints that exceed max "
                        "depth of %d. Please "
                        "drop extra constraints and try "
                        "again", DICT_FK_MAX_RECURSIVE_LOAD);
    /* fall through */

  case DB_ERROR:
  default:
    return(-1); /* unspecified error */

  case DB_DUPLICATE_KEY:
    /* Be cautious with returning this error, since
       mysql could re-enter the storage layer to get
       duplicated key info, the operation requires a
       valid table handle and/or transaction information,
       which might not always be available in the error
       handling stage. */
    return(HA_ERR_FOUND_DUPP_KEY);

  case DB_FOREIGN_DUPLICATE_KEY:
    return(HA_ERR_FOREIGN_DUPLICATE_KEY);

  case DB_MISSING_HISTORY:
    return(HA_ERR_TABLE_DEF_CHANGED);

  case DB_RECORD_NOT_FOUND:
    return(HA_ERR_NO_ACTIVE_RECORD);

  case DB_DEADLOCK:
    /* Since we rolled back the whole transaction, we must
    tell it also to MySQL so that MySQL knows to empty the
    cached binlog for this transaction */

    session->markTransactionForRollback(TRUE);

    return(HA_ERR_LOCK_DEADLOCK);

  case DB_LOCK_WAIT_TIMEOUT:
    /* Starting from 5.0.13, we let MySQL just roll back the
    latest SQL statement in a lock wait timeout. Previously, we
    rolled back the whole transaction. */

    session->markTransactionForRollback((bool)row_rollback_on_timeout);

    return(HA_ERR_LOCK_WAIT_TIMEOUT);

  case DB_NO_REFERENCED_ROW:
    return(HA_ERR_NO_REFERENCED_ROW);

  case DB_ROW_IS_REFERENCED:
    return(HA_ERR_ROW_IS_REFERENCED);

  case DB_CANNOT_ADD_CONSTRAINT:
  case DB_CHILD_NO_INDEX:
  case DB_PARENT_NO_INDEX:
    return(HA_ERR_CANNOT_ADD_FOREIGN);

  case DB_CANNOT_DROP_CONSTRAINT:

    return(HA_ERR_ROW_IS_REFERENCED); /* TODO: This is a bit
            misleading, a new MySQL error
            code should be introduced */

  case DB_COL_APPEARS_TWICE_IN_INDEX:
  case DB_CORRUPTION:
    return(HA_ERR_CRASHED);

  case DB_OUT_OF_FILE_SPACE:
    return(HA_ERR_RECORD_FILE_FULL);

  case DB_TABLE_IS_BEING_USED:
    return(HA_ERR_WRONG_COMMAND);

  case DB_TABLE_NOT_FOUND:
    return(HA_ERR_NO_SUCH_TABLE);

  case DB_TOO_BIG_RECORD:
    my_error(ER_TOO_BIG_ROWSIZE, MYF(0),
             page_get_free_space_of_empty(flags & DICT_TF_COMPACT) / 2);
    return(HA_ERR_TO_BIG_ROW);

  case DB_NO_SAVEPOINT:
    return(HA_ERR_NO_SAVEPOINT);

  case DB_LOCK_TABLE_FULL:
    /* Since we rolled back the whole transaction, we must
    tell it also to MySQL so that MySQL knows to empty the
    cached binlog for this transaction */

    session->markTransactionForRollback(TRUE);

    return(HA_ERR_LOCK_TABLE_FULL);

  case DB_PRIMARY_KEY_IS_NULL:
    return(ER_PRIMARY_CANT_HAVE_NULL);

  case DB_TOO_MANY_CONCURRENT_TRXS:

    /* Once MySQL add the appropriate code to errmsg.txt then
    we can get rid of this #ifdef. NOTE: The code checked by
    the #ifdef is the suggested name for the error condition
    and the actual error code name could very well be different.
    This will require some monitoring, ie. the status
    of this request on our part.*/

    /* New error code HA_ERR_TOO_MANY_CONCURRENT_TRXS is only
       available in 5.1.38 and later, but the plugin should still
       work with previous versions of MySQL.
       In Drizzle we seem to not have this yet.
    */
#ifdef HA_ERR_TOO_MANY_CONCURRENT_TRXS
    return(HA_ERR_TOO_MANY_CONCURRENT_TRXS);
#else /* HA_ERR_TOO_MANY_CONCURRENT_TRXS */
    return(HA_ERR_RECORD_FILE_FULL);
#endif /* HA_ERR_TOO_MANY_CONCURRENT_TRXS */
  case DB_UNSUPPORTED:
    return(HA_ERR_UNSUPPORTED);
  }
}


/*************************************************************//**
Prints info of a Session object (== user session thread) to the given file. */
UNIV_INTERN
void
innobase_mysql_print_thd(
/*=====================*/
  FILE* f,    /*!< in: output stream */
  drizzled::Session *in_session,  /*!< in: pointer to a Drizzle Session object */
  uint  )   /*!< in: max query length to print, or 0 to
           use the default max length */
{
  drizzled::identifier::user::ptr user_identifier(in_session->user());

  fprintf(f,
          "Drizzle thread %"PRIu64", query id %"PRIu64", %s, %s, %s ",
          static_cast<uint64_t>(in_session->getSessionId()),
          static_cast<uint64_t>(in_session->getQueryId()),
          getServerHostname().c_str(),
          user_identifier->address().c_str(),
          user_identifier->username().c_str()
  );
  fprintf(f, "\n%s", in_session->getQueryString()->c_str());
  putc('\n', f);
}

/******************************************************************//**
Get the variable length bounds of the given character set. */
UNIV_INTERN
void
innobase_get_cset_width(
/*====================*/
  ulint cset,   /*!< in: MySQL charset-collation code */
  ulint*  mbminlen, /*!< out: minimum length of a char (in bytes) */
  ulint*  mbmaxlen) /*!< out: maximum length of a char (in bytes) */
{
  charset_info_st* cs;
  ut_ad(cset < 256);
  ut_ad(mbminlen);
  ut_ad(mbmaxlen);

  cs = all_charsets[cset];
  if (cs) {
    *mbminlen = cs->mbminlen;
    *mbmaxlen = cs->mbmaxlen;
    ut_ad(*mbminlen < DATA_MBMAX);
    ut_ad(*mbmaxlen < DATA_MBMAX);
  } else {
    ut_a(cset == 0);
    *mbminlen = *mbmaxlen = 0;
  }
}

/******************************************************************//**
Converts an identifier to a table name. */
UNIV_INTERN
void
innobase_convert_from_table_id(
/*===========================*/
  const void*,      /*!< in: the 'from' character set */
  char*     to, /*!< out: converted identifier */
  const char*   from, /*!< in: identifier to convert */
  ulint     len)  /*!< in: length of 'to', in bytes */
{
  strncpy(to, from, len);
}

/******************************************************************//**
Converts an identifier to UTF-8. */
UNIV_INTERN
void
innobase_convert_from_id(
/*=====================*/
  const void*,      /*!< in: the 'from' character set */
  char*     to, /*!< out: converted identifier */
  const char*   from, /*!< in: identifier to convert */
  ulint     len)  /*!< in: length of 'to', in bytes */
{
  strncpy(to, from, len);
}

/******************************************************************//**
Compares NUL-terminated UTF-8 strings case insensitively.
@return 0 if a=b, <0 if a<b, >1 if a>b */
UNIV_INTERN
int
innobase_strcasecmp(
/*================*/
  const char* a,  /*!< in: first string to compare */
  const char* b)  /*!< in: second string to compare */
{
  return(my_strcasecmp(system_charset_info, a, b));
}

/******************************************************************//**
Makes all characters in a NUL-terminated UTF-8 string lower case. */
UNIV_INTERN
void
innobase_casedn_str(
/*================*/
  char* a)  /*!< in/out: string to put in lower case */
{
  my_casedn_str(system_charset_info, a);
}

UNIV_INTERN
bool
innobase_isspace(
  const void *cs,
  char char_to_test)
{
  return my_isspace(static_cast<const charset_info_st *>(cs), char_to_test);
}

#if defined (__WIN__) && defined (MYSQL_DYNAMIC_PLUGIN)
/*******************************************************************//**
Map an OS error to an errno value. The OS error number is stored in
_doserrno and the mapped value is stored in errno) */
void __cdecl
_dosmaperr(
  unsigned long); /*!< in: OS error value */

/*********************************************************************//**
Creates a temporary file.
@return temporary file descriptor, or < 0 on error */
UNIV_INTERN
int
innobase_mysql_tmpfile(void)
/*========================*/
{
  int fd;       /* handle of opened file */
  HANDLE  osfh;       /* OS handle of opened file */
  char* tmpdir;       /* point to the directory
            where to create file */
  TCHAR path_buf[MAX_PATH - 14];  /* buffer for tmp file path.
            The length cannot be longer
            than MAX_PATH - 14, or
            GetTempFileName will fail. */
  char  filename[MAX_PATH];   /* name of the tmpfile */
  DWORD fileaccess = GENERIC_READ /* OS file access */
           | GENERIC_WRITE
           | DELETE;
  DWORD fileshare = FILE_SHARE_READ /* OS file sharing mode */
          | FILE_SHARE_WRITE
          | FILE_SHARE_DELETE;
  DWORD filecreate = CREATE_ALWAYS; /* OS method of open/create */
  DWORD fileattrib =      /* OS file attribute flags */
           FILE_ATTRIBUTE_NORMAL
           | FILE_FLAG_DELETE_ON_CLOSE
           | FILE_ATTRIBUTE_TEMPORARY
           | FILE_FLAG_SEQUENTIAL_SCAN;

  tmpdir = my_tmpdir(&mysql_tmpdir_list);

  /* The tmpdir parameter can not be NULL for GetTempFileName. */
  if (!tmpdir) {
    uint  ret;

    /* Use GetTempPath to determine path for temporary files. */
    ret = GetTempPath(sizeof(path_buf), path_buf);
    if (ret > sizeof(path_buf) || (ret == 0)) {

      _dosmaperr(GetLastError()); /* map error */
      return(-1);
    }

    tmpdir = path_buf;
  }

  /* Use GetTempFileName to generate a unique filename. */
  if (!GetTempFileName(tmpdir, "ib", 0, filename)) {

    _dosmaperr(GetLastError()); /* map error */
    return(-1);
  }

  /* Open/Create the file. */
  osfh = CreateFile(filename, fileaccess, fileshare, NULL,
        filecreate, fileattrib, NULL);
  if (osfh == INVALID_HANDLE_VALUE) {

    /* open/create file failed! */
    _dosmaperr(GetLastError()); /* map error */
    return(-1);
  }

  do {
    /* Associates a CRT file descriptor with the OS file handle. */
    fd = _open_osfhandle((intptr_t) osfh, 0);
  } while (fd == -1 && errno == EINTR);

  if (fd == -1) {
    /* Open failed, close the file handle. */

    _dosmaperr(GetLastError()); /* map error */
    CloseHandle(osfh);    /* no need to check if
            CloseHandle fails */
  }

  return(fd);
}
#else
/*********************************************************************//**
Creates a temporary file.
@return temporary file descriptor, or < 0 on error */
UNIV_INTERN
int
innobase_mysql_tmpfile(void)
/*========================*/
{
  int fd2 = -1;
  int fd = ::drizzled::tmpfile("ib");
  if (fd >= 0) {
    /* Copy the file descriptor, so that the additional resources
    allocated by create_temp_file() can be freed by invoking
    internal::my_close().

    Because the file descriptor returned by this function
    will be passed to fdopen(), it will be closed by invoking
    fclose(), which in turn will invoke close() instead of
    internal::my_close(). */
    fd2 = dup(fd);
    if (fd2 < 0) {
      errno=errno;
      my_error(EE_OUT_OF_FILERESOURCES,
         MYF(ME_BELL+ME_WAITTANG),
         "ib*", errno);
    }
    internal::my_close(fd, MYF(MY_WME));
  }
  return(fd2);
}
#endif /* defined (__WIN__) && defined (MYSQL_DYNAMIC_PLUGIN) */


/*******************************************************************//**
Formats the raw data in "data" (in InnoDB on-disk format) that is of
type DATA_(CHAR|VARCHAR|DRIZZLE|VARDRIZZLE) using "charset_coll" and writes
the result to "buf". The result is converted to "system_charset_info".
Not more than "buf_size" bytes are written to "buf".
The result is always NUL-terminated (provided buf_size > 0) and the
number of bytes that were written to "buf" is returned (including the
terminating NUL).
@return number of bytes that were written */
UNIV_INTERN
ulint
innobase_raw_format(
/*================*/
  const char* data,   /*!< in: raw data */
  ulint   data_len, /*!< in: raw data length
          in bytes */
  ulint   ,   /*!< in: charset collation */
  char*   buf,    /*!< out: output buffer */
  ulint   buf_size) /*!< in: output buffer size
          in bytes */
{
  return(ut_str_sql_format(data, data_len, buf, buf_size));
}

/*********************************************************************//**
Compute the next autoinc value.

For MySQL replication the autoincrement values can be partitioned among
the nodes. The offset is the start or origin of the autoincrement value
for a particular node. For n nodes the increment will be n and the offset
will be in the interval [1, n]. The formula tries to allocate the next
value for a particular node.

Note: This function is also called with increment set to the number of
values we want to reserve for multi-value inserts e.g.,

  INSERT INTO T VALUES(), (), ();

innobase_next_autoinc() will be called with increment set to
to reserve 3 values for the multi-value INSERT above.
@return the next value */
static
uint64_t
innobase_next_autoinc(
/*==================*/
  uint64_t  current,  /*!< in: Current value */
  uint64_t  increment,  /*!< in: increment current by */
  uint64_t  offset,   /*!< in: AUTOINC offset */
  uint64_t  max_value)  /*!< in: max value for type */
{
  uint64_t  next_value;

  /* Should never be 0. */
  ut_a(increment > 0);

  /* According to MySQL documentation, if the offset is greater than
  the increment then the offset is ignored. */
  if (offset > increment) {
    offset = 0;
  }

  if (max_value <= current) {
    next_value = max_value;
  } else if (offset <= 1) {
    /* Offset 0 and 1 are the same, because there must be at
    least one node in the system. */
    if (max_value - current <= increment) {
      next_value = max_value;
    } else {
      next_value = current + increment;
    }
  } else if (max_value > current) {
    if (current > offset) {
      next_value = ((current - offset) / increment) + 1;
    } else {
      next_value = ((offset - current) / increment) + 1;
    }

    ut_a(increment > 0);
    ut_a(next_value > 0);

    /* Check for multiplication overflow. */
    if (increment > (max_value / next_value)) {

      next_value = max_value;
    } else {
      next_value *= increment;

      ut_a(max_value >= next_value);

      /* Check for overflow. */
      if (max_value - next_value <= offset) {
        next_value = max_value;
      } else {
        next_value += offset;
      }
    }
  } else {
    next_value = max_value;
  }

  ut_a(next_value <= max_value);

  return(next_value);
}

/*********************************************************************//**
Initializes some fields in an InnoDB transaction object. */
static
void
innobase_trx_init(
/*==============*/
  Session*  session,  /*!< in: user thread handle */
  trx_t*  trx)  /*!< in/out: InnoDB transaction handle */
{
  assert(session == trx->mysql_thd);

  trx->check_foreigns = !session_test_options(
    session, OPTION_NO_FOREIGN_KEY_CHECKS);

  trx->check_unique_secondary = !session_test_options(
    session, OPTION_RELAXED_UNIQUE_CHECKS);

  return;
}

/*********************************************************************//**
Allocates an InnoDB transaction for a MySQL Cursor object.
@return InnoDB transaction handle */
UNIV_INTERN
trx_t*
innobase_trx_allocate(
/*==================*/
  Session*  session)  /*!< in: user thread handle */
{
  trx_t*  trx;

  assert(session != NULL);
  assert(EQ_CURRENT_SESSION(session));

  trx = trx_allocate_for_mysql();

  trx->mysql_thd = session;

  innobase_trx_init(session, trx);

  return(trx);
}

/*********************************************************************//**
Gets the InnoDB transaction handle for a MySQL Cursor object, creates
an InnoDB transaction struct if the corresponding MySQL thread struct still
lacks one.
@return InnoDB transaction handle */
static
trx_t*
check_trx_exists(
/*=============*/
  Session*  session)  /*!< in: user thread handle */
{
  trx_t*& trx = session_to_trx(session);

  ut_ad(EQ_CURRENT_SESSION(session));

  if (trx == NULL) {
    trx = innobase_trx_allocate(session);
  } else if (UNIV_UNLIKELY(trx->magic_n != TRX_MAGIC_N)) {
    mem_analyze_corruption(trx);
    ut_error;
  }

  innobase_trx_init(session, trx);

  return(trx);
}


/*********************************************************************//**
Construct ha_innobase Cursor. */
UNIV_INTERN
ha_innobase::ha_innobase(plugin::StorageEngine &engine_arg,
                         Table &table_arg)
  :Cursor(engine_arg, table_arg),
  primary_key(0), /* needs initialization because index_flags() may be called 
                     before this is set to the real value. It's ok to have any 
                     value here because it doesn't matter if we return the
                     HA_DO_INDEX_COND_PUSHDOWN bit from those "early" calls */
  start_of_scan(0),
  num_write_row(0)
{}

/*********************************************************************//**
Destruct ha_innobase Cursor. */
UNIV_INTERN
ha_innobase::~ha_innobase()
{
}

/*********************************************************************//**
Updates the user_thd field in a handle and also allocates a new InnoDB
transaction handle if needed, and updates the transaction fields in the
prebuilt struct. */
UNIV_INTERN inline
void
ha_innobase::update_session(
/*====================*/
  Session*  session)  /*!< in: thd to use the handle */
{
  trx_t*    trx;

  assert(session);
  trx = check_trx_exists(session);

  if (prebuilt->trx != trx) {

    row_update_prebuilt_trx(prebuilt, trx);
  }

  user_session = session;
}

/*****************************************************************//**
Convert an SQL identifier to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return pointer to the end of buf */
static
char*
innobase_convert_identifier(
/*========================*/
  char*   buf,  /*!< out: buffer for converted identifier */
  ulint   buflen, /*!< in: length of buf, in bytes */
  const char* id, /*!< in: identifier to convert */
  ulint   idlen,  /*!< in: length of id, in bytes */
  drizzled::Session *session,/*!< in: MySQL connection thread, or NULL */
  ibool   file_id)/*!< in: TRUE=id is a table or database name;
        FALSE=id is an UTF-8 string */
{
  char nz[NAME_LEN + 1];
  const size_t nz2_size= NAME_LEN + 1 + srv_mysql50_table_name_prefix.size();
  boost::scoped_array<char> nz2(new char[nz2_size]);

  const char* s = id;
  int   q;

  if (file_id) {
    /* Decode the table name.  The filename_to_tablename()
    function expects a NUL-terminated string.  The input and
    output strings buffers must not be shared. */

    if (UNIV_UNLIKELY(idlen > (sizeof nz) - 1)) {
      idlen = (sizeof nz) - 1;
    }

    memcpy(nz, id, idlen);
    nz[idlen] = 0;

    s = nz2.get();
    idlen = identifier::Table::filename_to_tablename(nz, nz2.get(), nz2_size);
  }

  /* See if the identifier needs to be quoted. */
  if (UNIV_UNLIKELY(!session)) {
    q = '"';
  } else {
    q = get_quote_char_for_identifier();
  }

  if (q == EOF) {
    if (UNIV_UNLIKELY(idlen > buflen)) {
      idlen = buflen;
    }
    memcpy(buf, s, idlen);
    return(buf + idlen);
  }

  /* Quote the identifier. */
  if (buflen < 2) {
    return(buf);
  }

  *buf++ = q;
  buflen--;

  for (; idlen; idlen--) {
    int c = *s++;
    if (UNIV_UNLIKELY(c == q)) {
      if (UNIV_UNLIKELY(buflen < 3)) {
        break;
      }

      *buf++ = c;
      *buf++ = c;
      buflen -= 2;
    } else {
      if (UNIV_UNLIKELY(buflen < 2)) {
        break;
      }

      *buf++ = c;
      buflen--;
    }
  }

  *buf++ = q;
  return(buf);
}

/*****************************************************************//**
Convert a table or index name to the MySQL system_charset_info (UTF-8)
and quote it if needed.
@return pointer to the end of buf */
UNIV_INTERN
char*
innobase_convert_name(
/*==================*/
  char*   buf,  /*!< out: buffer for converted identifier */
  ulint   buflen, /*!< in: length of buf, in bytes */
  const char* id, /*!< in: identifier to convert */
  ulint   idlen,  /*!< in: length of id, in bytes */
  drizzled::Session *session,/*!< in: MySQL connection thread, or NULL */
  ibool   table_id)/*!< in: TRUE=id is a table or database name;
        FALSE=id is an index name */
{
  char*   s = buf;
  const char* bufend  = buf + buflen;

  if (table_id) {
    const char* slash = (const char*) memchr(id, '/', idlen);
    if (!slash) {

      goto no_db_name;
    }

    /* Print the database name and table name separately. */
    s = innobase_convert_identifier(s, bufend - s, id, slash - id,
            session, TRUE);
    if (UNIV_LIKELY(s < bufend)) {
      *s++ = '.';
      s = innobase_convert_identifier(s, bufend - s,
              slash + 1, idlen
              - (slash - id) - 1,
              session, TRUE);
    }
  } else if (UNIV_UNLIKELY(*id == TEMP_INDEX_PREFIX)) {
    /* Temporary index name (smart ALTER TABLE) */
    const char temp_index_suffix[]= "--temporary--";

    s = innobase_convert_identifier(buf, buflen, id + 1, idlen - 1,
            session, FALSE);
    if (s - buf + (sizeof temp_index_suffix - 1) < buflen) {
      memcpy(s, temp_index_suffix,
             sizeof temp_index_suffix - 1);
      s += sizeof temp_index_suffix - 1;
    }
  } else {
no_db_name:
    s = innobase_convert_identifier(buf, buflen, id, idlen,
            session, table_id);
  }

  return(s);

}

/**********************************************************************//**
Determines if the currently running transaction has been interrupted.
@return TRUE if interrupted */
UNIV_INTERN
ibool
trx_is_interrupted(
/*===============*/
  trx_t*  trx)  /*!< in: transaction */
{
  return(trx && trx->mysql_thd && trx->mysql_thd->getKilled());
}

/**********************************************************************//**
Determines if the currently running transaction is in strict mode.
@return	TRUE if strict */
UNIV_INTERN
ibool
trx_is_strict(
/*==========*/
	trx_t*	trx)	/*!< in: transaction */
{
	return(trx && trx->mysql_thd
	       && true);
}

/**************************************************************//**
Resets some fields of a prebuilt struct. The template is used in fast
retrieval of just those column values MySQL needs in its processing. */
static
void
reset_template(
/*===========*/
  row_prebuilt_t* prebuilt) /*!< in/out: prebuilt struct */
{
  prebuilt->keep_other_fields_on_keyread = 0;
  prebuilt->read_just_key = 0;
}

template<class T>
void align_value(T& value, size_t align_val= 1024)
{
  value= value - (value % align_val);
}

static void auto_extend_update(Session *, sql_var_t)
{
  srv_auto_extend_increment= innodb_auto_extend_increment.get();
}

static void io_capacity_update(Session *, sql_var_t)
{
  srv_io_capacity= innodb_io_capacity.get();
}

static void purge_batch_update(Session *, sql_var_t)
{
  srv_purge_batch_size= innodb_purge_batch_size.get();
}

static void purge_threads_update(Session *, sql_var_t)
{
  srv_n_purge_threads= innodb_n_purge_threads.get();
}

static void innodb_adaptive_hash_index_update(Session *, sql_var_t)
{
  if (btr_search_enabled)
  {
    btr_search_enable();
  } else {
    btr_search_disable();
  }
}

static void innodb_old_blocks_pct_update(Session *, sql_var_t)
{
  innobase_old_blocks_pct= buf_LRU_old_ratio_update(innobase_old_blocks_pct.get(), TRUE);
}

static void innodb_thread_concurrency_update(Session *, sql_var_t)
{
  srv_thread_concurrency= innobase_thread_concurrency.get();
}

static void innodb_sync_spin_loops_update(Session *, sql_var_t)
{
  srv_n_spin_wait_rounds= innodb_sync_spin_loops.get();
}

static void innodb_spin_wait_delay_update(Session *, sql_var_t)
{
  srv_spin_wait_delay= innodb_spin_wait_delay.get();
}

static void innodb_thread_sleep_delay_update(Session *, sql_var_t)
{
  srv_thread_sleep_delay= innodb_thread_sleep_delay.get();
}

static void innodb_read_ahead_threshold_update(Session *, sql_var_t)
{
  srv_read_ahead_threshold= innodb_read_ahead_threshold.get();
}

static void auto_lru_dump_update(Session *, sql_var_t)
{
  srv_auto_lru_dump= buffer_pool_restore_at_startup.get();
}

static void ibuf_active_contract_update(Session *, sql_var_t)
{
  srv_ibuf_active_contract= ibuf_active_contract.get();
}

static void ibuf_accel_rate_update(Session *, sql_var_t)
{
  srv_ibuf_accel_rate= ibuf_accel_rate.get();
}

static void checkpoint_age_target_update(Session *, sql_var_t)
{
  srv_checkpoint_age_target= checkpoint_age_target.get();
}

static void flush_neighbor_pages_update(Session *, sql_var_t)
{
  srv_flush_neighbor_pages= flush_neighbor_pages.get();
}

static int innodb_commit_concurrency_validate(Session *session, set_var *var)
{
   uint64_t new_value= var->getInteger();

   if ((innobase_commit_concurrency.get() == 0 && new_value != 0) ||
       (innobase_commit_concurrency.get() != 0 && new_value == 0))
   {
     push_warning_printf(session,
                         DRIZZLE_ERROR::WARN_LEVEL_WARN,
                         ER_WRONG_ARGUMENTS,
                         _("Once InnoDB is running, innodb_commit_concurrency "
                           "must not change between zero and nonzero."));
     return 1;
   }
   return 0;
}

/*************************************************************//**
Check if it is a valid file format. This function is registered as
a callback with MySQL.
@return 0 for valid file format */
static
int
innodb_file_format_name_validate(
/*=============================*/
  Session*      , /*!< in: thread handle */
  set_var *var)
{
  const char *file_format_input = var->value->str_value.ptr();
  if (file_format_input == NULL)
    return 1;

  if (file_format_input != NULL) {
    uint  format_id;

    format_id = innobase_file_format_name_lookup(
      file_format_input);

    if (format_id <= DICT_TF_FORMAT_MAX) {
      innobase_file_format_name =
        trx_sys_file_format_id_to_name(format_id);

      return(0);
    }
  }

  return(1);
}

/*************************************************************//**
Check if it is a valid value of innodb_change_buffering. This function is
registered as a callback with MySQL.
@return 0 for valid innodb_change_buffering */
static
int
innodb_change_buffering_validate(
/*=============================*/
  Session*      , /*!< in: thread handle */
  set_var *var)
{
  const char *change_buffering_input = var->value->str_value.ptr();

  if (change_buffering_input == NULL)
    return 1;

  ulint use;

  for (use = 0;
       use < UT_ARR_SIZE(innobase_change_buffering_values);
       ++use) {
    if (!innobase_strcasecmp(change_buffering_input,
                             innobase_change_buffering_values[use]))
    {
      ibuf_use= static_cast<ibuf_use_t>(use); 
      return 0;
    }
  }

  return 1;
}


/*************************************************************//**
Check if valid argument to innodb_file_format_max. This function
is registered as a callback with MySQL.
@return 0 for valid file format */
static
int
innodb_file_format_max_validate(
/*==============================*/
  Session*   session, /*!< in: thread handle */
  set_var *var)
{
  const char *file_format_input = var->value->str_value.ptr();
  if (file_format_input == NULL)
    return 1;

  if (file_format_input != NULL) {
    int format_id = innobase_file_format_validate_and_set(file_format_input);

    if (format_id > DICT_TF_FORMAT_MAX) {
      /* DEFAULT is "on", which is invalid at runtime. */
      return 1;
    }

    if (format_id >= 0) {
      innobase_file_format_max.assign(
                             trx_sys_file_format_id_to_name((uint)format_id));

      /* Update the max format id in the system tablespace. */
      const char *name_buff;

      if (trx_sys_file_format_max_set(format_id, &name_buff))
      {
        errmsg_printf(error::WARN,
                      " [Info] InnoDB: the file format in the system "
                      "tablespace is now set to %s.\n", name_buff);
        innobase_file_format_max= name_buff;
      }
      return(0);

    } else {
      push_warning_printf(session,
                          DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_WRONG_ARGUMENTS,
                          "InnoDB: invalid innodb_file_format_max "
                          "value; can be any format up to %s "
                          "or equivalent id of %d",
                          trx_sys_file_format_id_to_name(DICT_TF_FORMAT_MAX),
                          DICT_TF_FORMAT_MAX);
    }
  }

  return(1);
}

/*********************************************************************//**
Check if argument is a valid value for srv_read_ahead and set it.  This
function is registered as a callback with MySQL.
@return 0 for valid read_ahead value */
static
int
read_ahead_validate(
/*================*/
  Session*,             /*!< in: thread handle */
  set_var*    var)
{
  const char *read_ahead_input = var->value->str_value.ptr();
  int res = read_ahead_typelib.find_type(read_ahead_input, TYPELIB::e_none); // e_none is wrong

  if (res > 0) {
    srv_read_ahead = res - 1;
    return 0;
  }

  return 1;
}

/*********************************************************************//**
Check if argument is a valid value for srv_adaptive_flushing_method and
set it.  This function is registered as a callback with MySQL.
@return 0 for valid read_ahead value */
static
int
adaptive_flushing_method_validate(
/*==============================*/
  Session*,             /*!< in: thread handle */
  set_var*    var)
{
  const char *adaptive_flushing_method_input = var->value->str_value.ptr();
  int res = adaptive_flushing_method_typelib.find_type(adaptive_flushing_method_input, TYPELIB::e_none); // e_none is wrong

  if (res > 0) {
    srv_adaptive_flushing_method = res - 1;
    return 0;
  }
  return 1;
}


/*********************************************************************//**
Opens an InnoDB database.
@return 0 on success, error code on failure */
static
int
innobase_init(
/*==========*/
  module::Context &context) /*!< in: Drizzle Plugin Context */
{
  int   err;
  bool    ret;
  uint    format_id;
  InnobaseEngine *actuall_engine_ptr;
  const module::option_map &vm= context.getOptions();

  srv_auto_extend_increment= innodb_auto_extend_increment.get();
  srv_io_capacity= innodb_io_capacity.get();
  srv_purge_batch_size= innodb_purge_batch_size.get();
  srv_n_purge_threads= innodb_n_purge_threads.get();
  srv_flush_log_at_trx_commit= innodb_flush_log_at_trx_commit.get();
  srv_max_buf_pool_modified_pct= innodb_max_dirty_pages_pct.get();
  srv_max_purge_lag= innodb_max_purge_lag.get();
  srv_stats_sample_pages= innodb_stats_sample_pages.get();
  srv_n_free_tickets_to_enter= innodb_concurrency_tickets.get();
  srv_replication_delay= innodb_replication_delay.get();
  srv_thread_concurrency= innobase_thread_concurrency.get();
  srv_n_spin_wait_rounds= innodb_sync_spin_loops.get();
  srv_spin_wait_delay= innodb_spin_wait_delay.get();
  srv_thread_sleep_delay= innodb_thread_sleep_delay.get();
  srv_read_ahead_threshold= innodb_read_ahead_threshold.get();
  srv_auto_lru_dump= buffer_pool_restore_at_startup.get();
  srv_ibuf_max_size= ibuf_max_size.get();
  srv_ibuf_active_contract= ibuf_active_contract.get();
  srv_ibuf_accel_rate= ibuf_accel_rate.get();
  srv_checkpoint_age_target= checkpoint_age_target.get();
  srv_flush_neighbor_pages= flush_neighbor_pages.get();

  srv_read_ahead = read_ahead_typelib.find_type_or_exit(vm["read-ahead"].as<string>().c_str(),
                                                        "read_ahead_typelib") + 1;

  srv_adaptive_flushing_method = adaptive_flushing_method_typelib.find_type_or_exit(vm["adaptive-flushing-method"].as<string>().c_str(),
                                                                                    "adaptive_flushing_method_typelib") + 1;

  /* Inverted Booleans */

  innobase_use_checksums= not vm.count("disable-checksums");
  innobase_use_doublewrite= not vm.count("disable-doublewrite");
  srv_adaptive_flushing= not vm.count("disable-adaptive-flushing");
  srv_use_sys_malloc= not vm.count("use-internal-malloc");
  srv_use_native_aio= not vm.count("disable-native-aio");
  support_xa= not vm.count("disable-xa");
  btr_search_enabled= not vm.count("disable-adaptive-hash-index");

  /* Hafta do this here because we need to late-bind the default value */
  innobase_data_home_dir= vm.count("data-home-dir") ? vm["data-home-dir"].as<string>() : getDataHome().file_string();

  if (vm.count("data-file-path"))
  {
    innobase_data_file_path= vm["data-file-path"].as<string>();
  }


  innodb_engine_ptr= actuall_engine_ptr= new InnobaseEngine(innobase_engine_name);

  ut_a(DATA_MYSQL_TRUE_VARCHAR == (ulint)DRIZZLE_TYPE_VARCHAR);

#ifdef UNIV_DEBUG
  static const char test_filename[] = "-@";
  const size_t test_tablename_size= sizeof test_filename
    + srv_mysql50_table_name_prefix.size();
  boost::scoped_array test_tablename(new char[test_tablename_size]);
  if ((test_tablename_size) - 1
      != filename_to_tablename(test_filename, test_tablename.get(),
                               test_tablename_size)
      || strncmp(test_tablename.get(),
                 srv_mysql50_table_name_prefix.c_str(),
                 srv_mysql50_table_name_prefix.size())
      || strcmp(test_tablename.get()
                + srv_mysql50_table_name_prefix.size(),
                test_filename)) {
    errmsg_printf(error::ERROR, "tablename encoding has been changed");
    goto error;
  }
#endif /* UNIV_DEBUG */

  os_innodb_umask = (ulint)internal::my_umask;


  /* Set InnoDB initialization parameters according to the values
    read from MySQL .cnf file */

  /*--------------- Data files -------------------------*/

  /* The default dir for data files is the datadir of MySQL */

  srv_data_home = (char *)innobase_data_home_dir.c_str();

  /* Set default InnoDB data file size to 10 MB and let it be
    auto-extending. Thus users can use InnoDB in >= 4.0 without having
    to specify any startup options. */

  if (innobase_data_file_path.empty()) 
  {
    innobase_data_file_path= std::string("ibdata1:10M:autoextend");
  }

  /* Since InnoDB edits the argument in the next call, we make another
    copy of it: */

  internal_innobase_data_file_path = strdup(innobase_data_file_path.c_str());

  ret = (bool) srv_parse_data_file_paths_and_sizes(
                                                   internal_innobase_data_file_path);
  if (ret == FALSE) {
    errmsg_printf(error::ERROR, "InnoDB: syntax error in innodb_data_file_path");

mem_free_and_error:
    srv_free_paths_and_sizes();
    free(internal_innobase_data_file_path);
    goto error;
  }

  /* -------------- Log files ---------------------------*/

  /* The default dir for log files is the datadir of MySQL */

  if (vm.count("log-group-home-dir"))
  {
    innobase_log_group_home_dir= vm["log-group-home-dir"].as<string>();
  }
  else
  {
    innobase_log_group_home_dir= getDataHome().file_string();
  }

  ret = (bool)
    srv_parse_log_group_home_dirs((char *)innobase_log_group_home_dir.c_str());

  if (ret == FALSE || innobase_mirrored_log_groups.get() != 1) {
    errmsg_printf(error::ERROR, _("syntax error in innodb_log_group_home_dir, or a "
                                  "wrong number of mirrored log groups"));

    goto mem_free_and_error;
  }


  /* Validate the file format by animal name */
  if (vm.count("file-format"))
  {
    format_id = innobase_file_format_name_lookup(
                                                 vm["file-format"].as<string>().c_str());

    if (format_id > DICT_TF_FORMAT_MAX) {

      errmsg_printf(error::ERROR, "InnoDB: wrong innodb_file_format.");

      goto mem_free_and_error;
    }
  } else {
    /* Set it to the default file format id.*/
    format_id = 0;
  }

  srv_file_format = format_id;

  innobase_file_format_name =
    trx_sys_file_format_id_to_name(format_id);

  /* Check innobase_file_format_check variable */
  if (!innobase_file_format_check)
  {
    /* Set the value to disable checking. */
    srv_max_file_format_at_startup = DICT_TF_FORMAT_MAX + 1;
  } else {
    /* Set the value to the lowest supported format. */
    srv_max_file_format_at_startup = DICT_TF_FORMAT_MIN;
  }

  /* Did the user specify a format name that we support?
     As a side effect it will update the variable
     srv_max_file_format_at_startup */
  if (innobase_file_format_validate_and_set(innobase_file_format_max.c_str()) < 0)
  {
    errmsg_printf(error::ERROR, _("InnoDB: invalid innodb_file_format_max value: "
                                  "should be any value up to %s or its equivalent numeric id"),
                  trx_sys_file_format_id_to_name(DICT_TF_FORMAT_MAX));
    goto mem_free_and_error;
  }

  if (vm.count("change-buffering"))
  {
    ulint use;

    for (use = 0;
         use < UT_ARR_SIZE(innobase_change_buffering_values);
         use++) {
      if (!innobase_strcasecmp(
                               innobase_change_buffering.c_str(),
                               innobase_change_buffering_values[use])) {
        ibuf_use = static_cast<ibuf_use_t>(use);
        goto innobase_change_buffering_inited_ok;
      }
    }

    errmsg_printf(error::ERROR, "InnoDB: invalid value innodb_change_buffering=%s",
                  vm["change-buffering"].as<string>().c_str());
    goto mem_free_and_error;
  }

innobase_change_buffering_inited_ok:
  ut_a((ulint) ibuf_use < UT_ARR_SIZE(innobase_change_buffering_values));
  innobase_change_buffering = innobase_change_buffering_values[ibuf_use];

  /* --------------------------------------------------*/

  if (vm.count("flush-method") != 0)
  {
    srv_file_flush_method_str = (char *)vm["flush-method"].as<string>().c_str();
  }

  srv_n_log_groups = (ulint) innobase_mirrored_log_groups;
  srv_n_log_files = (ulint) innobase_log_files_in_group;
  srv_log_file_size = (ulint) innobase_log_file_size;

  srv_log_buffer_size = (ulint) innobase_log_buffer_size;

  srv_buf_pool_size = (ulint) innobase_buffer_pool_size;
  srv_buf_pool_instances = (ulint) innobase_buffer_pool_instances;

  srv_mem_pool_size = (ulint) innobase_additional_mem_pool_size;

  srv_n_read_io_threads = (ulint) innobase_read_io_threads;
  srv_n_write_io_threads = (ulint) innobase_write_io_threads;

  srv_read_ahead &= 3;
  srv_adaptive_flushing_method %= 3;

  srv_force_recovery = (ulint) innobase_force_recovery;

  srv_use_doublewrite_buf = (ibool) innobase_use_doublewrite;
  srv_use_checksums = (ibool) innobase_use_checksums;

#ifdef HAVE_LARGE_PAGES
  if ((os_use_large_pages = (ibool) my_use_large_pages))
    os_large_page_size = (ulint) opt_large_page_size;
#endif

  row_rollback_on_timeout = (ibool) innobase_rollback_on_timeout;

  srv_locks_unsafe_for_binlog = (ibool) TRUE;

  srv_max_n_open_files = (ulint) innobase_open_files;
  srv_innodb_status = (ibool) innobase_create_status_file;

  srv_print_verbose_log = true;

  /* Store the default charset-collation number of this MySQL
    installation */

  data_mysql_default_charset_coll = (ulint)default_charset_info->number;

  /* Since we in this module access directly the fields of a trx
    struct, and due to different headers and flags it might happen that
    mutex_t has a different size in this module and in InnoDB
    modules, we check at run time that the size is the same in
    these compilation modules. */

  err = innobase_start_or_create_for_mysql();

  if (err != DB_SUCCESS)
  {
    goto mem_free_and_error;
  }

  err = dict_create_sys_replication_log();

  if (err != DB_SUCCESS) {
    goto mem_free_and_error;
  }


  innobase_old_blocks_pct = buf_LRU_old_ratio_update(innobase_old_blocks_pct.get(),
                                                     TRUE);

  innobase_open_tables = hash_create(200);
  innodb_inited= 1;

  actuall_engine_ptr->dropTemporarySchema();

  context.add(new InnodbStatusTool);
  context.add(innodb_engine_ptr);
  context.add(new CmpTool(false));
  context.add(new CmpTool(true));
  context.add(new CmpmemTool(false));
  context.add(new CmpmemTool(true));
  context.add(new InnodbTrxTool("INNODB_TRX"));
  context.add(new InnodbTrxTool("INNODB_LOCKS"));
  context.add(new InnodbTrxTool("INNODB_LOCK_WAITS"));
  context.add(new InnodbSysTablesTool());
  context.add(new InnodbSysTableStatsTool());
  context.add(new InnodbSysIndexesTool());
  context.add(new InnodbSysColumnsTool());
  context.add(new InnodbSysFieldsTool());
  context.add(new InnodbSysForeignTool());
  context.add(new InnodbSysForeignColsTool());
  context.add(new InnodbInternalTables());
  context.add(new InnodbReplicationTable());

  if (innobase_use_replication_log)
  {
    ReplicationLog *replication_logger= new ReplicationLog();
    context.add(replication_logger);
    ReplicationLog::setup(replication_logger);
  }

  context.registerVariable(new sys_var_const_string_val("data-home-dir", innobase_data_home_dir));
  context.registerVariable(new sys_var_const_string_val("flush-method", 
                                                        vm.count("flush-method") ?  vm["flush-method"].as<string>() : ""));
  context.registerVariable(new sys_var_const_string_val("log-group-home-dir", innobase_log_group_home_dir));
  context.registerVariable(new sys_var_const_string_val("data-file-path", innobase_data_file_path));
  context.registerVariable(new sys_var_const_string_val("version", vm["version"].as<string>()));


  context.registerVariable(new sys_var_bool_ptr_readonly("replication_log", &innobase_use_replication_log));
  context.registerVariable(new sys_var_bool_ptr_readonly("checksums", &innobase_use_checksums));
  context.registerVariable(new sys_var_bool_ptr_readonly("doublewrite", &innobase_use_doublewrite));
  context.registerVariable(new sys_var_bool_ptr("file-per-table", &srv_file_per_table));
  context.registerVariable(new sys_var_bool_ptr_readonly("file-format-check", &innobase_file_format_check));
  context.registerVariable(new sys_var_bool_ptr("adaptive-flushing", &srv_adaptive_flushing));
  context.registerVariable(new sys_var_bool_ptr("status-file", &innobase_create_status_file));
  context.registerVariable(new sys_var_bool_ptr_readonly("use-sys-malloc", &srv_use_sys_malloc));
  context.registerVariable(new sys_var_bool_ptr_readonly("use-native-aio", &srv_use_native_aio));

  context.registerVariable(new sys_var_bool_ptr("support-xa", &support_xa));
  context.registerVariable(new sys_var_bool_ptr("strict_mode", &strict_mode));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("lock_wait_timeout", lock_wait_timeout));

  context.registerVariable(new sys_var_constrained_value_readonly<size_t>("additional_mem_pool_size",innobase_additional_mem_pool_size));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("autoextend_increment",
                                                                   innodb_auto_extend_increment,
                                                                   auto_extend_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("io_capacity",
                                                                   innodb_io_capacity,
                                                                   io_capacity_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("purge_batch_size",
                                                                   innodb_purge_batch_size,
                                                                   purge_batch_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("purge_threads",
                                                                   innodb_n_purge_threads,
                                                                   purge_threads_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("fast_shutdown", innobase_fast_shutdown));
  context.registerVariable(new sys_var_std_string("file_format",
                                                  innobase_file_format_name,
                                                  innodb_file_format_name_validate));
  context.registerVariable(new sys_var_std_string("change_buffering",
                                                  innobase_change_buffering,
                                                  innodb_change_buffering_validate));
  context.registerVariable(new sys_var_std_string("file_format_max",
                                                  innobase_file_format_max,
                                                  innodb_file_format_max_validate));
  context.registerVariable(new sys_var_constrained_value_readonly<size_t>("buffer_pool_size", innobase_buffer_pool_size));
  context.registerVariable(new sys_var_constrained_value_readonly<int64_t>("log_file_size", innobase_log_file_size));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("flush_log_at_trx_commit",
                                                  innodb_flush_log_at_trx_commit));
  context.registerVariable(new sys_var_constrained_value_readonly<unsigned int>("max_dirty_pages_pct",
                                                  innodb_max_dirty_pages_pct));
  context.registerVariable(new sys_var_constrained_value_readonly<uint64_t>("max_purge_lag", innodb_max_purge_lag));
  context.registerVariable(new sys_var_constrained_value_readonly<uint64_t>("stats_sample_pages", innodb_stats_sample_pages));
  context.registerVariable(new sys_var_bool_ptr("adaptive_hash_index", &btr_search_enabled, innodb_adaptive_hash_index_update));

  context.registerVariable(new sys_var_constrained_value<uint32_t>("commit_concurrency",
                                                                   innobase_commit_concurrency,
                                                                   innodb_commit_concurrency_validate));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("concurrency_tickets",
                                                                   innodb_concurrency_tickets));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("read_io_threads", innobase_read_io_threads));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("write_io_threads", innobase_write_io_threads));
  context.registerVariable(new sys_var_constrained_value_readonly<uint64_t>("replication_delay", innodb_replication_delay));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("force_recovery", innobase_force_recovery));
  context.registerVariable(new sys_var_constrained_value_readonly<size_t>("log_buffer_size", innobase_log_buffer_size));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("log_files_in_group", innobase_log_files_in_group));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("mirrored_log_groups", innobase_mirrored_log_groups));
  context.registerVariable(new sys_var_constrained_value_readonly<uint32_t>("open_files", innobase_open_files));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("old_blocks_pct",
                                                                   innobase_old_blocks_pct,
                                                                   innodb_old_blocks_pct_update));
  context.registerVariable(new sys_var_uint32_t_ptr("old_blocks_time", &buf_LRU_old_threshold_ms));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("sync_spin_loops", innodb_sync_spin_loops, innodb_sync_spin_loops_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("spin_wait_delay", innodb_spin_wait_delay, innodb_spin_wait_delay_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("thread_sleep_delay", innodb_thread_sleep_delay, innodb_thread_sleep_delay_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("thread_concurrency",
                                                                   innobase_thread_concurrency,
                                                                   innodb_thread_concurrency_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("read_ahead_threshold",
                                                                   innodb_read_ahead_threshold,
                                                                   innodb_read_ahead_threshold_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("auto_lru_dump",
                                                                   buffer_pool_restore_at_startup,
                                                                   auto_lru_dump_update));
  context.registerVariable(new sys_var_constrained_value_readonly<uint64_t>("ibuf_max_size",
                                                                            ibuf_max_size));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("ibuf_active_contract",
                                                                   ibuf_active_contract,
                                                                   ibuf_active_contract_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("ibuf_accel_rate",
                                                                   ibuf_accel_rate,
                                                                   ibuf_accel_rate_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("checkpoint_age_target",
                                                                   checkpoint_age_target,
                                                                   checkpoint_age_target_update));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("flush_neighbor_pages",
                                                                   flush_neighbor_pages,
                                                                   flush_neighbor_pages_update));
  context.registerVariable(new sys_var_std_string("read_ahead",
                                                  read_ahead,
                                                  read_ahead_validate));
  context.registerVariable(new sys_var_std_string("adaptive_flushing_method",
                                                  adaptive_flushing_method,
                                                  adaptive_flushing_method_validate));
  /* Get the current high water mark format. */
  innobase_file_format_max = trx_sys_file_format_max_get();
  btr_search_fully_disabled = (!btr_search_enabled);

  return(FALSE);

error:
  return(TRUE);
}


/****************************************************************//**
Flushes InnoDB logs to disk and makes a checkpoint. Really, a commit flushes
the logs, and the name of this function should be innobase_checkpoint.
@return TRUE if error */
bool
InnobaseEngine::flush_logs()
/*=====================*/
{
  bool  result = 0;

  assert(this == innodb_engine_ptr);

  log_buffer_flush_to_disk();

  return(result);
}

/*****************************************************************//**
Commits a transaction in an InnoDB database. */
static
void
innobase_commit_low(
/*================*/
  trx_t*  trx)  /*!< in: transaction handle */
{
  if (trx->conc_state == TRX_NOT_STARTED) {

    return;
  }

  trx_commit_for_mysql(trx);
}

/*****************************************************************//**
Creates an InnoDB transaction struct for the thd if it does not yet have one.
Starts a new InnoDB transaction if a transaction is not yet started. And
assigns a new snapshot for a consistent read if the transaction does not yet
have one.
@return 0 */
int
InnobaseEngine::doStartTransaction(
/*====================================*/
  Session*  session,  /*!< in: MySQL thread handle of the user for whom
                               the transaction should be committed */
  start_transaction_option_t options)
{
  assert(this == innodb_engine_ptr);

  /* Create a new trx struct for session, if it does not yet have one */
  trx_t *trx = check_trx_exists(session);

  /* This is just to play safe: release a possible FIFO ticket and
  search latch. Since we will reserve the kernel mutex, we have to
  release the search system latch first to obey the latching order. */
  innobase_release_stat_resources(trx);

  /* If the transaction is not started yet, start it */
  trx_start_if_not_started(trx);

  /* Assign a read view if the transaction does not have it yet */
  if (options == START_TRANS_OPT_WITH_CONS_SNAPSHOT)
    trx_assign_read_view(trx);

  return 0;
}

/*****************************************************************//**
Commits a transaction in an InnoDB database or marks an SQL statement
ended.
@return 0 */
int
InnobaseEngine::doCommit(
/*============*/
  Session*  session,  /*!< in: MySQL thread handle of the user for whom
      the transaction should be committed */
  bool  all)  /*!< in:  TRUE - commit transaction
        FALSE - the current SQL statement ended */
{
  trx_t*    trx;

  assert(this == innodb_engine_ptr);

  trx = check_trx_exists(session);

  /* Since we will reserve the kernel mutex, we have to release
  the search system latch first to obey the latching order. */

  if (trx->has_search_latch) {
    trx_search_latch_release_if_reserved(trx);
  }

  if (all)
  {
    /* We were instructed to commit the whole transaction, or
    this is an SQL statement end and autocommit is on */

    /* We need current binlog position for ibbackup to work.
    Note, the position is current because of
    prepare_commit_mutex */
    const uint32_t commit_concurrency= innobase_commit_concurrency.get();
    if (commit_concurrency)
    {
      do 
      {
        boost::mutex::scoped_lock scopedLock(commit_cond_m);
        commit_threads++;

        if (commit_threads <= commit_concurrency) 
          break;

        commit_threads--;
        commit_cond.wait(scopedLock);
      } while (1);
    }

    trx->mysql_log_file_name = NULL;
    trx->mysql_log_offset = 0;

    /* Don't do write + flush right now. For group commit
    to work we want to do the flush after releasing the
    prepare_commit_mutex. */
    trx->flush_log_later = TRUE;
    innobase_commit_low(trx);
    trx->flush_log_later = FALSE;

    if (commit_concurrency)
    {
      boost::mutex::scoped_lock scopedLock(commit_cond_m);
      commit_threads--;
      commit_cond.notify_one();
    }

    /* Now do a write + flush of logs. */
    trx_commit_complete_for_mysql(trx);

  } else {
    /* We just mark the SQL statement ended and do not do a
    transaction commit */

    /* If we had reserved the auto-inc lock for some
    table in this SQL statement we release it now */

    row_unlock_table_autoinc_for_mysql(trx);

    /* Store the current undo_no of the transaction so that we
    know where to roll back if we have to roll back the next
    SQL statement */

    trx_mark_sql_stat_end(trx);

    if (! session_test_options(session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
    {
      if (trx->conc_state != TRX_NOT_STARTED)
      {
        commit(session, TRUE);
      }
    }
  }

  trx->n_autoinc_rows = 0; /* Reset the number AUTO-INC rows required */

  if (trx->declared_to_be_inside_innodb) {
    /* Release our possible ticket in the FIFO */

    srv_conc_force_exit_innodb(trx);
  }

  /* Tell the InnoDB server that there might be work for utility
  threads: */
  srv_active_wake_master_thread();

  if (trx->isolation_level <= TRX_ISO_READ_COMMITTED &&
      trx->global_read_view)
  {
    /* At low transaction isolation levels we let
       each consistent read set its own snapshot */
    read_view_close_for_mysql(trx);
  }

  return(0);
}

/*****************************************************************//**
Rolls back a transaction or the latest SQL statement.
@return 0 or error number */
int
InnobaseEngine::doRollback(
/*==============*/
  Session*  session,/*!< in: handle to the MySQL thread of the user
      whose transaction should be rolled back */
  bool  all)  /*!< in:  TRUE - commit transaction
        FALSE - the current SQL statement ended */
{
  int error = 0;
  trx_t*  trx;

  assert(this == innodb_engine_ptr);

  trx = check_trx_exists(session);

  /* Release a possible FIFO ticket and search latch. Since we will
  reserve the kernel mutex, we have to release the search system latch
  first to obey the latching order. */

  innobase_release_stat_resources(trx);

  trx->n_autoinc_rows = 0;

  /* If we had reserved the auto-inc lock for some table (if
  we come here to roll back the latest SQL statement) we
  release it now before a possibly lengthy rollback */

  row_unlock_table_autoinc_for_mysql(trx);

  if (all)
  {
    error = trx_rollback_for_mysql(trx);
  } else {
    error = trx_rollback_last_sql_stat_for_mysql(trx);
  }

  if (trx->isolation_level <= TRX_ISO_READ_COMMITTED &&
      trx->global_read_view)
  {
    /* At low transaction isolation levels we let
       each consistent read set its own snapshot */
    read_view_close_for_mysql(trx);
  }

  return(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Rolls back a transaction
@return 0 or error number */
static
int
innobase_rollback_trx(
/*==================*/
  trx_t*  trx)  /*!< in: transaction */
{
  int error = 0;

  /* Release a possible FIFO ticket and search latch. Since we will
  reserve the kernel mutex, we have to release the search system latch
  first to obey the latching order. */

  innobase_release_stat_resources(trx);

  /* If we had reserved the auto-inc lock for some table (if
  we come here to roll back the latest SQL statement) we
  release it now before a possibly lengthy rollback */

  row_unlock_table_autoinc_for_mysql(trx);

  error = trx_rollback_for_mysql(trx);

  return(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Rolls back a transaction to a savepoint.
@return 0 if success, HA_ERR_NO_SAVEPOINT if no savepoint with the
given name */
int
InnobaseEngine::doRollbackToSavepoint(
/*===========================*/
  Session*  session,    /*!< in: handle to the MySQL thread of the user
        whose transaction should be rolled back */
  drizzled::NamedSavepoint &named_savepoint)  /*!< in: savepoint data */
{
  ib_int64_t  mysql_binlog_cache_pos;
  int   error = 0;
  trx_t*    trx;

  assert(this == innodb_engine_ptr);

  trx = check_trx_exists(session);

  /* Release a possible FIFO ticket and search latch. Since we will
  reserve the kernel mutex, we have to release the search system latch
  first to obey the latching order. */

  innobase_release_stat_resources(trx);

  error= (int)trx_rollback_to_savepoint_for_mysql(trx, named_savepoint.getName().c_str(),
                                                        &mysql_binlog_cache_pos);
  return(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Release transaction savepoint name.
@return 0 if success, HA_ERR_NO_SAVEPOINT if no savepoint with the
given name */
int
InnobaseEngine::doReleaseSavepoint(
/*=======================*/
  Session*  session,    /*!< in: handle to the MySQL thread of the user
        whose transaction should be rolled back */
  drizzled::NamedSavepoint &named_savepoint)  /*!< in: savepoint data */
{
  int   error = 0;
  trx_t*    trx;

  assert(this == innodb_engine_ptr);

  trx = check_trx_exists(session);

  error = (int) trx_release_savepoint_for_mysql(trx, named_savepoint.getName().c_str());

  return(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Sets a transaction savepoint.
@return always 0, that is, always succeeds */
int
InnobaseEngine::doSetSavepoint(
/*===============*/
  Session*  session,/*!< in: handle to the MySQL thread */
  drizzled::NamedSavepoint &named_savepoint)  /*!< in: savepoint data */
{
  int error = 0;
  trx_t*  trx;

  assert(this == innodb_engine_ptr);

  /*
    In the autocommit mode there is no sense to set a savepoint
    (unless we are in sub-statement), so SQL layer ensures that
    this method is never called in such situation.
  */

  trx = check_trx_exists(session);

  /* Release a possible FIFO ticket and search latch. Since we will
  reserve the kernel mutex, we have to release the search system latch
  first to obey the latching order. */

  innobase_release_stat_resources(trx);

  /* cannot happen outside of transaction */
  assert(trx->conc_state != TRX_NOT_STARTED);

  error = (int) trx_savepoint_for_mysql(trx, named_savepoint.getName().c_str(), (ib_int64_t)0);

  return(convert_error_code_to_mysql(error, 0, NULL));
}

/*****************************************************************//**
Frees a possible InnoDB trx object associated with the current Session.
@return 0 or error number */
int
InnobaseEngine::close_connection(
/*======================*/
  Session*  session)/*!< in: handle to the MySQL thread of the user
      whose resources should be free'd */
{
  trx_t*  trx;

  assert(this == innodb_engine_ptr);
  trx = session_to_trx(session);

  ut_a(trx);

  assert(session->getKilled() != Session::NOT_KILLED ||
         trx->conc_state == TRX_NOT_STARTED);

  /* Warn if rolling back some things... */
  if (session->getKilled() != Session::NOT_KILLED &&
      trx->conc_state != TRX_NOT_STARTED &&
      trx->undo_no > 0 &&
      global_system_variables.log_warnings)
  {
      errmsg_printf(error::WARN,
      "Drizzle is closing a connection during a KILL operation\n"
      "that has an active InnoDB transaction.  %llu row modifications will "
      "roll back.\n",
      (ullint) trx->undo_no);
  }

  innobase_rollback_trx(trx);

  thr_local_free(trx->mysql_thread_id);
  trx_free_for_mysql(trx);

  return(0);
}


/*************************************************************************//**
** InnoDB database tables
*****************************************************************************/

/****************************************************************//**
Returns the index type. */
UNIV_INTERN
const char*
ha_innobase::index_type(
/*====================*/
  uint)
        /*!< out: index type */
{
  return("BTREE");
}

/****************************************************************//**
Returns the maximum number of keys.
@return MAX_KEY */
UNIV_INTERN
uint
InnobaseEngine::max_supported_keys() const
/*===================================*/
{
  return(MAX_KEY);
}

/****************************************************************//**
Returns the maximum key length.
@return maximum supported key length, in bytes */
UNIV_INTERN
uint32_t
InnobaseEngine::max_supported_key_length() const
/*=========================================*/
{
  /* An InnoDB page must store >= 2 keys; a secondary key record
  must also contain the primary key value: max key length is
  therefore set to slightly less than 1 / 4 of page size which
  is 16 kB; but currently MySQL does not work with keys whose
  size is > MAX_KEY_LENGTH */
  return(3500);
}

/****************************************************************//**
Returns the key map of keys that are usable for scanning.
@return key_map_full */
UNIV_INTERN
const key_map*
ha_innobase::keys_to_use_for_scanning()
{
  return(&key_map_full);
}


/****************************************************************//**
Determines if the primary key is clustered index.
@return true */
UNIV_INTERN
bool
ha_innobase::primary_key_is_clustered()
{
  return(true);
}

/********************************************************************//**
Get the upper limit of the MySQL integral and floating-point type.
@return maximum allowed value for the field */
static
uint64_t
innobase_get_int_col_max_value(
/*===========================*/
	const Field*	field)	/*!< in: MySQL field */
{
	uint64_t	max_value = 0;

	switch(field->key_type()) {
	/* TINY */
	case HA_KEYTYPE_BINARY:
		max_value = 0xFFULL;
		break;
	/* LONG */
	case HA_KEYTYPE_ULONG_INT:
		max_value = 0xFFFFFFFFULL;
		break;
	case HA_KEYTYPE_LONG_INT:
		max_value = 0x7FFFFFFFULL;
		break;
	/* BIG */
	case HA_KEYTYPE_ULONGLONG:
		max_value = 0xFFFFFFFFFFFFFFFFULL;
		break;
	case HA_KEYTYPE_LONGLONG:
		max_value = 0x7FFFFFFFFFFFFFFFULL;
		break;
	case HA_KEYTYPE_DOUBLE:
		/* We use the maximum as per IEEE754-2008 standard, 2^53 */
		max_value = 0x20000000000000ULL;
		break;
	default:
		ut_error;
	}

	return(max_value);
}

/*******************************************************************//**
This function checks whether the index column information
is consistent between KEY info from mysql and that from innodb index.
@return TRUE if all column types match. */
static
ibool
innobase_match_index_columns(
/*=========================*/
	const KeyInfo*		key_info,	/*!< in: Index info
						from mysql */
	const dict_index_t*	index_info)	/*!< in: Index info
						from Innodb */
{
	const KeyPartInfo*	key_part;
	const KeyPartInfo*	key_end;
	const dict_field_t*	innodb_idx_fld;
	const dict_field_t*	innodb_idx_fld_end;

	/* Check whether user defined index column count matches */
	if (key_info->key_parts != index_info->n_user_defined_cols) {
		return(FALSE);
	}

	key_part = key_info->key_part;
	key_end = key_part + key_info->key_parts;
	innodb_idx_fld = index_info->fields;
	innodb_idx_fld_end = index_info->fields + index_info->n_fields;

	/* Check each index column's datatype. We do not check
	column name because there exists case that index
	column name got modified in mysql but such change does not
	propagate to InnoDB.
	One hidden assumption here is that the index column sequences
	are matched up between those in mysql and Innodb. */
	for (; key_part != key_end; ++key_part) {
		ulint	col_type;
		ibool	is_unsigned;
		ulint	mtype = innodb_idx_fld->col->mtype;

		/* Need to translate to InnoDB column type before
		comparison. */
		col_type = get_innobase_type_from_mysql_type(&is_unsigned,
							     key_part->field);

		/* Ignore Innodb specific system columns. */
		while (mtype == DATA_SYS) {
			innodb_idx_fld++;

			if (innodb_idx_fld >= innodb_idx_fld_end) {
				return(FALSE);
			}
		}

		if (col_type != mtype) {
			/* Column Type mismatches */
			return(FALSE);
		}

		innodb_idx_fld++;
	}

	return(TRUE);
}

/*******************************************************************//**
This function builds a translation table in INNOBASE_SHARE
structure for fast index location with mysql array number from its
table->key_info structure. This also provides the necessary translation
between the key order in mysql key_info and Innodb ib_table->indexes if
they are not fully matched with each other.
Note we do not have any mutex protecting the translation table
building based on the assumption that there is no concurrent
index creation/drop and DMLs that requires index lookup. All table
handle will be closed before the index creation/drop.
@return TRUE if index translation table built successfully */
static
ibool
innobase_build_index_translation(
/*=============================*/
	const Table*		table,	  /*!< in: table in MySQL data
					  dictionary */
	dict_table_t*		ib_table, /*!< in: table in Innodb data
					  dictionary */
	INNOBASE_SHARE*		share)	  /*!< in/out: share structure
					  where index translation table
					  will be constructed in. */
{
	ulint		mysql_num_index;
	ulint		ib_num_index;
	dict_index_t**	index_mapping;
	ibool		ret = TRUE;

        mutex_enter(&dict_sys->mutex);

	mysql_num_index = table->getShare()->keys;
	ib_num_index = UT_LIST_GET_LEN(ib_table->indexes);

	index_mapping = share->idx_trans_tbl.index_mapping;

	/* If there exists inconsistency between MySQL and InnoDB dictionary
	(metadata) information, the number of index defined in MySQL
	could exceed that in InnoDB, do not build index translation
	table in such case */
	if (UNIV_UNLIKELY(ib_num_index < mysql_num_index)) {
		ret = FALSE;
		goto func_exit;
	}

	/* If index entry count is non-zero, nothing has
	changed since last update, directly return TRUE */
	if (share->idx_trans_tbl.index_count) {
		/* Index entry count should still match mysql_num_index */
		ut_a(share->idx_trans_tbl.index_count == mysql_num_index);
		goto func_exit;
	}

	/* The number of index increased, rebuild the mapping table */
	if (mysql_num_index > share->idx_trans_tbl.array_size) {
		index_mapping = (dict_index_t**) realloc(index_mapping,
							mysql_num_index *
                                                         sizeof(*index_mapping));

		if (!index_mapping) {
			/* Report an error if index_mapping continues to be
			NULL and mysql_num_index is a non-zero value */
			errmsg_printf(error::ERROR, "InnoDB: fail to allocate memory for "
                                      "index translation table. Number of Index:%lu, array size:%lu",
					mysql_num_index,
					share->idx_trans_tbl.array_size);
			ret = FALSE;
			goto func_exit;
		}

		share->idx_trans_tbl.array_size = mysql_num_index;
	}

	/* For each index in the mysql key_info array, fetch its
	corresponding InnoDB index pointer into index_mapping
	array. */
	for (ulint count = 0; count < mysql_num_index; count++) {

		/* Fetch index pointers into index_mapping according to mysql
		index sequence */
		index_mapping[count] = dict_table_get_index_on_name(
			ib_table, table->key_info[count].name);

		if (!index_mapping[count]) {
			errmsg_printf(error::ERROR, "Cannot find index %s in InnoDB index dictionary.",
                                      table->key_info[count].name);
			ret = FALSE;
			goto func_exit;
		}

		/* Double check fetched index has the same
		column info as those in mysql key_info. */
		if (!innobase_match_index_columns(&table->key_info[count], index_mapping[count])) {
                  errmsg_printf(error::ERROR, "Found index %s whose column info does not match that of MySQL.",
                                table->key_info[count].name);
                  ret = FALSE;
                  goto func_exit;
		}
	}

	/* Successfully built the translation table */
	share->idx_trans_tbl.index_count = mysql_num_index;

func_exit:
	if (!ret) {
		/* Build translation table failed. */
		free(index_mapping);

		share->idx_trans_tbl.array_size = 0;
		share->idx_trans_tbl.index_count = 0;
		index_mapping = NULL;
	}

	share->idx_trans_tbl.index_mapping = index_mapping;

        mutex_exit(&dict_sys->mutex);

	return(ret);
}

/*******************************************************************//**
This function uses index translation table to quickly locate the
requested index structure.
Note we do not have mutex protection for the index translatoin table
access, it is based on the assumption that there is no concurrent
translation table rebuild (fter create/drop index) and DMLs that
require index lookup.
@return dict_index_t structure for requested index. NULL if
fail to locate the index structure. */
static
dict_index_t*
innobase_index_lookup(
/*==================*/
	INNOBASE_SHARE*	share,	/*!< in: share structure for index
				translation table. */
	uint		keynr)	/*!< in: index number for the requested
				index */
{
	if (!share->idx_trans_tbl.index_mapping
	    || keynr >= share->idx_trans_tbl.index_count) {
		return(NULL);
	}

	return(share->idx_trans_tbl.index_mapping[keynr]);
}

/********************************************************************//**
Set the autoinc column max value. This should only be called once from
ha_innobase::open(). Therefore there's no need for a covering lock. */
UNIV_INTERN
void
ha_innobase::innobase_initialize_autoinc()
/*======================================*/
{
  uint64_t  auto_inc;
  const Field*	field = getTable()->found_next_number_field;

  if (field != NULL) {
    auto_inc = innobase_get_int_col_max_value(field);
  } else {
    /* We have no idea what's been passed in to us as the
       autoinc column. We set it to the 0, effectively disabling
       updates to the table. */
    auto_inc = 0;

    ut_print_timestamp(stderr);
    errmsg_printf(error::ERROR, "InnoDB: Unable to determine the AUTOINC column name");
  }

  if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {
    /* If the recovery level is set so high that writes
       are disabled we force the AUTOINC counter to 0
       value effectively disabling writes to the table.
       Secondly, we avoid reading the table in case the read
       results in failure due to a corrupted table/index.

       We will not return an error to the client, so that the
       tables can be dumped with minimal hassle.  If an error
       were returned in this case, the first attempt to read
       the table would fail and subsequent SELECTs would succeed. */
    auto_inc = 0;
  } else if (field == NULL) {
    /* This is a far more serious error, best to avoid
       opening the table and return failure. */
    my_error(ER_AUTOINC_READ_FAILED, MYF(0));
  } else {
    dict_index_t*	index;
    const char*	col_name;
    uint64_t	read_auto_inc;
    ulint		err;

    update_session(getTable()->in_use);
    col_name = field->field_name;

    ut_a(prebuilt->trx == session_to_trx(user_session));

    index = innobase_get_index(getTable()->getShare()->next_number_index);

    /* Execute SELECT MAX(col_name) FROM TABLE; */
    err = row_search_max_autoinc(index, col_name, &read_auto_inc);

    switch (err) {
    case DB_SUCCESS: {
      uint64_t col_max_value;

      col_max_value = innobase_get_int_col_max_value(field);

      /* At the this stage we do not know the increment
         nor the offset, so use a default increment of 1. */

      auto_inc = innobase_next_autoinc(read_auto_inc, 1, 1, col_max_value);

      break;
    }
    case DB_RECORD_NOT_FOUND:
      ut_print_timestamp(stderr);
      errmsg_printf(error::ERROR, "InnoDB: MySQL and InnoDB data dictionaries are out of sync.\n"
                    "InnoDB: Unable to find the AUTOINC column %s in the InnoDB table %s.\n"
                    "InnoDB: We set the next AUTOINC column value to 0,\n"
                    "InnoDB: in effect disabling the AUTOINC next value generation.\n"
                    "InnoDB: You can either set the next AUTOINC value explicitly using ALTER TABLE\n"
                    "InnoDB: or fix the data dictionary by recreating the table.\n",
                    col_name, index->table->name);

      /* This will disable the AUTOINC generation. */
      auto_inc = 0;

      /* We want the open to succeed, so that the user can
         take corrective action. ie. reads should succeed but
         updates should fail. */
      err = DB_SUCCESS;
      break;
    default:
      /* row_search_max_autoinc() should only return
         one of DB_SUCCESS or DB_RECORD_NOT_FOUND. */
      ut_error;
    }
  }

  dict_table_autoinc_initialize(prebuilt->table, auto_inc);
}

/*****************************************************************//**
Creates and opens a handle to a table which already exists in an InnoDB
database.
@return 1 if error, 0 if success */
UNIV_INTERN
int
ha_innobase::doOpen(const identifier::Table &identifier,
                    int   mode,   /*!< in: not used */
                    uint    test_if_locked) /*!< in: not used */
{
  dict_table_t* ib_table;
  Session*    session;

  UT_NOT_USED(mode);
  UT_NOT_USED(test_if_locked);

  session= getTable()->in_use;

  /* Under some cases Drizzle seems to call this function while
  holding btr_search_latch. This breaks the latching order as
  we acquire dict_sys->mutex below and leads to a deadlock. */
  if (session != NULL) {
    getTransactionalEngine()->releaseTemporaryLatches(session);
  }

  user_session = NULL;

  std::string search_string(identifier.getSchemaName());
  boost::algorithm::to_lower(search_string);

  if (search_string.compare("data_dictionary") == 0)
  {
    std::string table_name(identifier.getTableName());
    boost::algorithm::to_upper(table_name);
    if (!(share=get_share(table_name.c_str())))
    {
      return 1;
    }
  }
  else
  {
    if (!(share=get_share(identifier.getKeyPath().c_str())))
    {
      return(1);
    }
  }

  /* Create buffers for packing the fields of a record. Why
  table->stored_rec_length did not work here? Obviously, because char
  fields when packed actually became 1 byte longer, when we also
  stored the string length as the first byte. */

  upd_and_key_val_buff_len =
        getTable()->getShare()->sizeStoredRecord()
        + getTable()->getShare()->max_key_length
        + MAX_REF_PARTS * 3;

  upd_buff.resize(upd_and_key_val_buff_len);

  if (upd_buff.size() < upd_and_key_val_buff_len)
  {
    free_share(share);
  }

  key_val_buff.resize(upd_and_key_val_buff_len);
  if (key_val_buff.size() < upd_and_key_val_buff_len)
  {
    return(1);
  }

  /* Get pointer to a table object in InnoDB dictionary cache */
  if (search_string.compare("data_dictionary") == 0)
  {
    std::string table_name(identifier.getTableName());
    boost::algorithm::to_upper(table_name);
    ib_table = dict_table_get(table_name.c_str(), TRUE);
  }
  else
  {
    ib_table = dict_table_get(identifier.getKeyPath().c_str(), TRUE);
  }
  
  if (NULL == ib_table) {
    errmsg_printf(error::ERROR, "Cannot find or open table %s from\n"
        "the internal data dictionary of InnoDB "
        "though the .frm file for the\n"
        "table exists. Maybe you have deleted and "
        "recreated InnoDB data\n"
        "files but have forgotten to delete the "
        "corresponding .frm files\n"
        "of InnoDB tables, or you have moved .frm "
        "files to another database?\n"
        "or, the table contains indexes that this "
        "version of the engine\n"
        "doesn't support.\n"
        "See " REFMAN "innodb-troubleshooting.html\n"
        "how you can resolve the problem.\n",
        identifier.getKeyPath().c_str());
    free_share(share);
    upd_buff.resize(0);
    key_val_buff.resize(0);
    errno = ENOENT;

    return(HA_ERR_NO_SUCH_TABLE);
  }

  if (ib_table->ibd_file_missing && ! session->doing_tablespace_operation()) {
    errmsg_printf(error::ERROR, "MySQL is trying to open a table handle but "
        "the .ibd file for\ntable %s does not exist.\n"
        "Have you deleted the .ibd file from the "
        "database directory under\nthe MySQL datadir, "
        "or have you used DISCARD TABLESPACE?\n"
        "See " REFMAN "innodb-troubleshooting.html\n"
        "how you can resolve the problem.\n",
        identifier.getKeyPath().c_str());
    free_share(share);
    upd_buff.resize(0);
    key_val_buff.resize(0);
    errno = ENOENT;

    dict_table_decrement_handle_count(ib_table, FALSE);
    return(HA_ERR_NO_SUCH_TABLE);
  }

  prebuilt = row_create_prebuilt(ib_table);

  prebuilt->mysql_row_len = getTable()->getShare()->sizeStoredRecord();
  prebuilt->default_rec = getTable()->getDefaultValues();
  ut_ad(prebuilt->default_rec);

  /* Looks like MySQL-3.23 sometimes has primary key number != 0 */

  primary_key = getTable()->getShare()->getPrimaryKey();
  key_used_on_scan = primary_key;

  if (!innobase_build_index_translation(getTable(), ib_table, share)) {
    errmsg_printf(error::ERROR, "Build InnoDB index translation table for"
                    " Table %s failed", identifier.getKeyPath().c_str());
  }

  /* Allocate a buffer for a 'row reference'. A row reference is
  a string of bytes of length ref_length which uniquely specifies
  a row in our table. Note that MySQL may also compare two row
  references for equality by doing a simple memcmp on the strings
  of length ref_length! */

  if (!row_table_got_default_clust_index(ib_table)) {

    prebuilt->clust_index_was_generated = FALSE;

    if (UNIV_UNLIKELY(primary_key >= MAX_KEY)) {
      errmsg_printf(error::ERROR, "Table %s has a primary key in "
                    "InnoDB data dictionary, but not "
                    "in MySQL!", identifier.getTableName().c_str());

      /* This mismatch could cause further problems
         if not attended, bring this to the user's attention
         by printing a warning in addition to log a message
         in the errorlog */
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_NO_SUCH_INDEX,
                          "InnoDB: Table %s has a "
                          "primary key in InnoDB data "
                          "dictionary, but not in "
                          "MySQL!", identifier.getTableName().c_str());

      /* If primary_key >= MAX_KEY, its (primary_key)
         value could be out of bound if continue to index
         into key_info[] array. Find InnoDB primary index,
         and assign its key_length to ref_length.
         In addition, since MySQL indexes are sorted starting
         with primary index, unique index etc., initialize
         ref_length to the first index key length in
         case we fail to find InnoDB cluster index.

         Please note, this will not resolve the primary
         index mismatch problem, other side effects are
         possible if users continue to use the table.
         However, we allow this table to be opened so
         that user can adopt necessary measures for the
         mismatch while still being accessible to the table
         date. */
      ref_length = getTable()->key_info[0].key_length;

      /* Find correspoinding cluster index
         key length in MySQL's key_info[] array */
      for (ulint i = 0; i < getTable()->getShare()->keys; i++) {
        dict_index_t*	index;
        index = innobase_get_index(i);
        if (dict_index_is_clust(index)) {
          ref_length =
            getTable()->key_info[i].key_length;
        }
      }
    } else {
      /* MySQL allocates the buffer for ref.
         key_info->key_length includes space for all key
         columns + one byte for each column that may be
         NULL. ref_length must be as exact as possible to
         save space, because all row reference buffers are
         allocated based on ref_length. */

      ref_length = getTable()->key_info[primary_key].key_length;
    }
  } else {
    if (primary_key != MAX_KEY) {
      errmsg_printf(error::ERROR,
                    "Table %s has no primary key in InnoDB data "
                    "dictionary, but has one in MySQL! If you "
                    "created the table with a MySQL version < "
                    "3.23.54 and did not define a primary key, "
                    "but defined a unique key with all non-NULL "
                    "columns, then MySQL internally treats that "
                    "key as the primary key. You can fix this "
                    "error by dump + DROP + CREATE + reimport "
                    "of the table.", identifier.getTableName().c_str());

      /* This mismatch could cause further problems
         if not attended, bring this to the user attention
         by printing a warning in addition to log a message
         in the errorlog */
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_NO_SUCH_INDEX,
                          "InnoDB: Table %s has no "
                          "primary key in InnoDB data "
                          "dictionary, but has one in "
                          "MySQL!", identifier.getTableName().c_str());
    }

    prebuilt->clust_index_was_generated = TRUE;

    ref_length = DATA_ROW_ID_LEN;

    /* If we automatically created the clustered index, then
    MySQL does not know about it, and MySQL must NOT be aware
    of the index used on scan, to make it avoid checking if we
    update the column of the index. That is why we assert below
    that key_used_on_scan is the undefined value MAX_KEY.
    The column is the row id in the automatical generation case,
    and it will never be updated anyway. */

    if (key_used_on_scan != MAX_KEY) {
      errmsg_printf(error::WARN, 
        "Table %s key_used_on_scan is %lu even "
        "though there is no primary key inside "
        "InnoDB.", identifier.getTableName().c_str(), (ulong) key_used_on_scan);
    }
  }

  /* Index block size in InnoDB: used by MySQL in query optimization */
  stats.block_size = 16 * 1024;

  /* Init table lock structure */
  lock.init(&share->lock);

  if (prebuilt->table) {
    /* We update the highest file format in the system table
    space, if this table has higher file format setting. */

    char changed_file_format_max[100];
    strcpy(changed_file_format_max, innobase_file_format_max.c_str());
    trx_sys_file_format_max_upgrade((const char **)&changed_file_format_max,
      dict_table_get_format(prebuilt->table));
    innobase_file_format_max= changed_file_format_max;
  }

  /* Only if the table has an AUTOINC column. */
  if (prebuilt->table != NULL && getTable()->found_next_number_field != NULL) {

    dict_table_autoinc_lock(prebuilt->table);

    /* Since a table can already be "open" in InnoDB's internal
    data dictionary, we only init the autoinc counter once, the
    first time the table is loaded. We can safely reuse the
    autoinc value from a previous Drizzle open. */
    if (dict_table_autoinc_read(prebuilt->table) == 0) {

      innobase_initialize_autoinc();
    }

    dict_table_autoinc_unlock(prebuilt->table);
  }

  info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

  return(0);
}

UNIV_INTERN
uint32_t
InnobaseEngine::max_supported_key_part_length() const
{
  return(DICT_MAX_INDEX_COL_LEN - 1);
}

/******************************************************************//**
Closes a handle to an InnoDB table.
@return 0 */
UNIV_INTERN
int
ha_innobase::close(void)
/*====================*/
{
  Session*  session;

  session= getTable()->in_use;
  if (session != NULL) {
    getTransactionalEngine()->releaseTemporaryLatches(session);
  }

  row_prebuilt_free(prebuilt, FALSE);

  upd_buff.clear();
  key_val_buff.clear();
  free_share(share);

  /* Tell InnoDB server that there might be work for
  utility threads: */

  srv_active_wake_master_thread();

  return(0);
}

/* The following accessor functions should really be inside MySQL code! */

/**************************************************************//**
Gets field offset for a field in a table.
@return offset */
static inline
uint
get_field_offset(
/*=============*/
  Table*  table,  /*!< in: MySQL table object */
  Field*  field)  /*!< in: MySQL field object */
{
  return((uint) (field->ptr - table->getInsertRecord()));
}

/**************************************************************//**
Checks if a field in a record is SQL NULL. Uses the record format
information in table to track the null bit in record.
@return 1 if NULL, 0 otherwise */
static inline
uint
field_in_record_is_null(
/*====================*/
  Table*  table,  /*!< in: MySQL table object */
  Field*  field,  /*!< in: MySQL field object */
  char* record) /*!< in: a row in MySQL format */
{
  int null_offset;

  if (!field->null_ptr) {

    return(0);
  }

  null_offset = (uint) ((char*) field->null_ptr
          - (char*) table->getInsertRecord());

  if (record[null_offset] & field->null_bit) {

    return(1);
  }

  return(0);
}

/**************************************************************//**
Sets a field in a record to SQL NULL. Uses the record format
information in table to track the null bit in record. */
static inline
void
set_field_in_record_to_null(
/*========================*/
  Table*  table,  /*!< in: MySQL table object */
  Field*  field,  /*!< in: MySQL field object */
  char* record) /*!< in: a row in MySQL format */
{
  int null_offset;

  null_offset = (uint) ((char*) field->null_ptr
          - (char*) table->getInsertRecord());

  record[null_offset] = record[null_offset] | field->null_bit;
}

/*************************************************************//**
InnoDB uses this function to compare two data fields for which the data type
is such that we must use MySQL code to compare them. NOTE that the prototype
of this function is in rem0cmp.c in InnoDB source code! If you change this
function, remember to update the prototype there!
@return 1, 0, -1, if a is greater, equal, less than b, respectively */
UNIV_INTERN int
innobase_mysql_cmp(
/*===============*/
  int   mysql_type, /*!< in: MySQL type */
  uint    charset_number, /*!< in: number of the charset */
  const unsigned char* a,   /*!< in: data field */
  unsigned int  a_length, /*!< in: data field length,
          not UNIV_SQL_NULL */
  const unsigned char* b,   /* in: data field */
  unsigned int  b_length);  /* in: data field length,
          not UNIV_SQL_NULL */

int
innobase_mysql_cmp(
/*===============*/
          /* out: 1, 0, -1, if a is greater, equal, less than b, respectively */
  int   mysql_type, /* in: MySQL type */
  uint    charset_number, /* in: number of the charset */
  const unsigned char* a,   /* in: data field */
  unsigned int  a_length, /* in: data field length, not UNIV_SQL_NULL */
  const unsigned char* b,   /* in: data field */
  unsigned int  b_length) /* in: data field length, not UNIV_SQL_NULL */
{
  const charset_info_st* charset;
  enum_field_types  mysql_tp;
  int     ret;

  assert(a_length != UNIV_SQL_NULL);
  assert(b_length != UNIV_SQL_NULL);

  mysql_tp = (enum_field_types) mysql_type;

  switch (mysql_tp) {

  case DRIZZLE_TYPE_BLOB:
  case DRIZZLE_TYPE_VARCHAR:
    /* Use the charset number to pick the right charset struct for
      the comparison. Since the MySQL function get_charset may be
      slow before Bar removes the mutex operation there, we first
      look at 2 common charsets directly. */

    if (charset_number == default_charset_info->number) {
      charset = default_charset_info;
    } else {
      charset = get_charset(charset_number);

      if (charset == NULL) {
        errmsg_printf(error::ERROR, "InnoDB needs charset %lu for doing "
                      "a comparison, but MySQL cannot "
                      "find that charset.",
                      (ulong) charset_number);
        ut_a(0);
      }
    }

    /* Starting from 4.1.3, we use strnncollsp() in comparisons of
      non-latin1_swedish_ci strings. NOTE that the collation order
      changes then: 'b\0\0...' is ordered BEFORE 'b  ...'. Users
      having indexes on such data need to rebuild their tables! */

    ret = charset->coll->strnncollsp(charset,
                                     a, a_length,
                                     b, b_length, 0);
    if (ret < 0) {
      return(-1);
    } else if (ret > 0) {
      return(1);
    } else {
      return(0);
    }
  default:
    ut_error;
  }

  return(0);
}

/**************************************************************//**
Converts a MySQL type to an InnoDB type. Note that this function returns
the 'mtype' of InnoDB. InnoDB differentiates between MySQL's old <= 4.1
VARCHAR and the new true VARCHAR in >= 5.0.3 by the 'prtype'.
@return DATA_BINARY, DATA_VARCHAR, ... */
UNIV_INTERN
ulint
get_innobase_type_from_mysql_type(
/*==============================*/
  ulint*    unsigned_flag,  /*!< out: DATA_UNSIGNED if an
          'unsigned type';
          at least ENUM and SET,
          and unsigned integer
          types are 'unsigned types' */
  const void* f)    /*!< in: MySQL Field */
{
  const class Field* field = reinterpret_cast<const class Field*>(f);

  /* The following asserts try to check that the MySQL type code fits in
  8 bits: this is used in ibuf and also when DATA_NOT_NULL is ORed to
  the type */

  assert((ulint)DRIZZLE_TYPE_DOUBLE < 256);

  if (field->flags & UNSIGNED_FLAG) {

    *unsigned_flag = DATA_UNSIGNED;
  } else {
    *unsigned_flag = 0;
  }

  if (field->real_type() == DRIZZLE_TYPE_ENUM)
  {
    /* MySQL has field->type() a string type for these, but the
    data is actually internally stored as an unsigned integer
    code! */

    *unsigned_flag = DATA_UNSIGNED; /* MySQL has its own unsigned
            flag set to zero, even though
            internally this is an unsigned
            integer type */
    return(DATA_INT);
  }

  switch (field->type()) {
    /* NOTE that we only allow string types in DATA_DRIZZLE and
    DATA_VARDRIZZLE */
  case DRIZZLE_TYPE_VARCHAR:    /* new >= 5.0.3 true VARCHAR */
    if (field->binary()) {
      return(DATA_BINARY);
    } else {
      return(DATA_VARMYSQL);
    }
  case DRIZZLE_TYPE_DECIMAL:
  case DRIZZLE_TYPE_MICROTIME:
    return(DATA_FIXBINARY);
  case DRIZZLE_TYPE_LONG:
  case DRIZZLE_TYPE_LONGLONG:
  case DRIZZLE_TYPE_DATETIME:
  case DRIZZLE_TYPE_TIME:
  case DRIZZLE_TYPE_DATE:
  case DRIZZLE_TYPE_TIMESTAMP:
  case DRIZZLE_TYPE_ENUM:
    return(DATA_INT);
  case DRIZZLE_TYPE_DOUBLE:
    return(DATA_DOUBLE);
  case DRIZZLE_TYPE_BLOB:
    return(DATA_BLOB);
  case DRIZZLE_TYPE_BOOLEAN:
  case DRIZZLE_TYPE_UUID:
    return(DATA_FIXBINARY);
  case DRIZZLE_TYPE_NULL:
    ut_error;
  }

  return(0);
}

/*******************************************************************//**
Writes an unsigned integer value < 64k to 2 bytes, in the little-endian
storage format. */
static inline
void
innobase_write_to_2_little_endian(
/*==============================*/
  byte* buf,  /*!< in: where to store */
  ulint val)  /*!< in: value to write, must be < 64k */
{
  ut_a(val < 256 * 256);

  buf[0] = (byte)(val & 0xFF);
  buf[1] = (byte)(val / 256);
}

/*******************************************************************//**
Reads an unsigned integer value < 64k from 2 bytes, in the little-endian
storage format.
@return value */
static inline
uint
innobase_read_from_2_little_endian(
/*===============================*/
  const unsigned char*  buf)  /*!< in: from where to read */
{
  return (uint) ((ulint)(buf[0]) + 256 * ((ulint)(buf[1])));
}

/*******************************************************************//**
Stores a key value for a row to a buffer.
@return key value length as stored in buff */
UNIV_INTERN
uint
ha_innobase::store_key_val_for_row(
/*===============================*/
  uint    keynr,  /*!< in: key number */
  char*   buff, /*!< in/out: buffer for the key value (in MySQL
        format) */
  uint    buff_len,/*!< in: buffer length */
  const unsigned char*  record)/*!< in: row in MySQL format */
{
  KeyInfo*    key_info  = &getTable()->key_info[keynr];
  KeyPartInfo*  key_part  = key_info->key_part;
  KeyPartInfo*  end   = key_part + key_info->key_parts;
  char*   buff_start  = buff;
  enum_field_types mysql_type;
  Field*    field;
  ibool   is_null;

  /* The format for storing a key field in MySQL is the following:

  1. If the column can be NULL, then in the first byte we put 1 if the
  field value is NULL, 0 otherwise.

  2. If the column is of a BLOB type (it must be a column prefix field
  in this case), then we put the length of the data in the field to the
  next 2 bytes, in the little-endian format. If the field is SQL NULL,
  then these 2 bytes are set to 0. Note that the length of data in the
  field is <= column prefix length.

  3. In a column prefix field, prefix_len next bytes are reserved for
  data. In a normal field the max field length next bytes are reserved
  for data. For a VARCHAR(n) the max field length is n. If the stored
  value is the SQL NULL then these data bytes are set to 0.

  4. We always use a 2 byte length for a true >= 5.0.3 VARCHAR. Note that
  in the MySQL row format, the length is stored in 1 or 2 bytes,
  depending on the maximum allowed length. But in the MySQL key value
  format, the length always takes 2 bytes.

  We have to zero-fill the buffer so that MySQL is able to use a
  simple memcmp to compare two key values to determine if they are
  equal. MySQL does this to compare contents of two 'ref' values. */

  bzero(buff, buff_len);

  for (; key_part != end; key_part++) {
    is_null = FALSE;

    if (key_part->null_bit) {
      if (record[key_part->null_offset]
            & key_part->null_bit) {
        *buff = 1;
        is_null = TRUE;
      } else {
        *buff = 0;
      }
      buff++;
    }

    field = key_part->field;
    mysql_type = field->type();

    if (mysql_type == DRIZZLE_TYPE_VARCHAR) {
            /* >= 5.0.3 true VARCHAR */
      ulint   lenlen;
      ulint   len;
      const byte* data;
      ulint   key_len;
      ulint   true_len;
      const charset_info_st* cs;
      int   error=0;

      key_len = key_part->length;

      if (is_null) {
        buff += key_len + 2;

        continue;
      }
      cs = field->charset();

      lenlen = (ulint)
        (((Field_varstring*)field)->pack_length_no_ptr());

      data = row_mysql_read_true_varchar(&len,
        (byte*) (record
        + (ulint)get_field_offset(getTable(), field)),
        lenlen);

      true_len = len;

      /* For multi byte character sets we need to calculate
      the true length of the key */

      if (len > 0 && cs->mbmaxlen > 1) {
        true_len = (ulint) cs->cset->well_formed_len(cs,
            (const char *) data,
            (const char *) data + len,
                                                (uint) (key_len /
                                                        cs->mbmaxlen),
            &error);
      }

      /* In a column prefix index, we may need to truncate
      the stored value: */

      if (true_len > key_len) {
        true_len = key_len;
      }

      /* The length in a key value is always stored in 2
      bytes */

      row_mysql_store_true_var_len((byte*)buff, true_len, 2);
      buff += 2;

      memcpy(buff, data, true_len);

      /* Note that we always reserve the maximum possible
      length of the true VARCHAR in the key value, though
      only len first bytes after the 2 length bytes contain
      actual data. The rest of the space was reset to zero
      in the bzero() call above. */

      buff += key_len;

    } else if (mysql_type == DRIZZLE_TYPE_BLOB) {

      const charset_info_st* cs;
      ulint   key_len;
      ulint   true_len;
      int   error=0;
      ulint   blob_len;
      const byte* blob_data;

      ut_a(key_part->key_part_flag & HA_PART_KEY_SEG);

      key_len = key_part->length;

      if (is_null) {
        buff += key_len + 2;

        continue;
      }

      cs = field->charset();

      blob_data = row_mysql_read_blob_ref(&blob_len,
        (byte*) (record
        + (ulint)get_field_offset(getTable(), field)),
          (ulint) field->pack_length());

      true_len = blob_len;

      ut_a(get_field_offset(getTable(), field)
        == key_part->offset);

      /* For multi byte character sets we need to calculate
      the true length of the key */

      if (blob_len > 0 && cs->mbmaxlen > 1) {
        true_len = (ulint) cs->cset->well_formed_len(cs,
                                                     (const char *) blob_data,
                                                     (const char *) blob_data
                                                     + blob_len,
                                                     (uint) (key_len /
                                                             cs->mbmaxlen),
                                                     &error);
      }

      /* All indexes on BLOB and TEXT are column prefix
      indexes, and we may need to truncate the data to be
      stored in the key value: */

      if (true_len > key_len) {
        true_len = key_len;
      }

      /* MySQL reserves 2 bytes for the length and the
      storage of the number is little-endian */

      innobase_write_to_2_little_endian(
          (byte*)buff, true_len);
      buff += 2;

      memcpy(buff, blob_data, true_len);

      /* Note that we always reserve the maximum possible
      length of the BLOB prefix in the key value. */

      buff += key_len;
    } else {
      /* Here we handle all other data types except the
      true VARCHAR, BLOB and TEXT. Note that the column
      value we store may be also in a column prefix
      index. */

      ulint     true_len;
      ulint     key_len;
      const unsigned char*    src_start;
      const charset_info_st* cs= field->charset();

      key_len = key_part->length;

      if (is_null) {
         buff += key_len;

         continue;
      }

      src_start = record + key_part->offset;
      true_len = key_len;

      /* Character set for the field is defined only
      to fields whose type is string and real field
      type is not enum or set. For these fields check
      if character set is multi byte. */

      memcpy(buff, src_start, true_len);
      buff += true_len;

      /* Pad the unused space with spaces. */

      if (true_len < key_len) {
        ulint	pad_len = key_len - true_len;
        ut_a(!(pad_len % cs->mbminlen));

        cs->cset->fill(cs, buff, pad_len,
                       0x20 /* space */);
        buff += pad_len;
      }
    }
  }

  ut_a(buff <= buff_start + buff_len);

  return((uint)(buff - buff_start));
}

/**************************************************************//**
Builds a 'template' to the prebuilt struct. The template is used in fast
retrieval of just those column values MySQL needs in its processing. */
static
void
build_template(
/*===========*/
  row_prebuilt_t* prebuilt, /*!< in/out: prebuilt struct */
  Session*  ,   /*!< in: current user thread, used
          only if templ_type is
          ROW_DRIZZLE_REC_FIELDS */
  Table*    table,    /*!< in: MySQL table */
  uint    templ_type) /*!< in: ROW_MYSQL_WHOLE_ROW or
          ROW_DRIZZLE_REC_FIELDS */
{
  dict_index_t* index;
  dict_index_t* clust_index;
  mysql_row_templ_t* templ;
  Field*    field;
  ulint   n_fields;
  ulint   n_requested_fields  = 0;
  ibool   fetch_all_in_key  = FALSE;
  ibool   fetch_primary_key_cols  = FALSE;
  ulint   i= 0;
  /* byte offset of the end of last requested column */
  ulint   mysql_prefix_len  = 0;

  if (prebuilt->select_lock_type == LOCK_X) {
    /* We always retrieve the whole clustered index record if we
    use exclusive row level locks, for example, if the read is
    done in an UPDATE statement. */

    templ_type = ROW_MYSQL_WHOLE_ROW;
  }

  if (templ_type == ROW_MYSQL_REC_FIELDS) {
    if (prebuilt->hint_need_to_fetch_extra_cols
      == ROW_RETRIEVE_ALL_COLS) {

      /* We know we must at least fetch all columns in the
      key, or all columns in the table */

      if (prebuilt->read_just_key) {
        /* MySQL has instructed us that it is enough
        to fetch the columns in the key; looks like
        MySQL can set this flag also when there is
        only a prefix of the column in the key: in
        that case we retrieve the whole column from
        the clustered index */

        fetch_all_in_key = TRUE;
      } else {
        templ_type = ROW_MYSQL_WHOLE_ROW;
      }
    } else if (prebuilt->hint_need_to_fetch_extra_cols
      == ROW_RETRIEVE_PRIMARY_KEY) {
      /* We must at least fetch all primary key cols. Note
         that if the clustered index was internally generated
         by InnoDB on the row id (no primary key was
         defined), then row_search_for_mysql() will always
         retrieve the row id to a special buffer in the
         prebuilt struct. */

      fetch_primary_key_cols = TRUE;
    }
  }

  clust_index = dict_table_get_first_index(prebuilt->table);

  if (templ_type == ROW_MYSQL_REC_FIELDS) {
    index = prebuilt->index;
  } else {
    index = clust_index;
  }

  if (index == clust_index) {
    prebuilt->need_to_access_clustered = TRUE;
  } else {
    prebuilt->need_to_access_clustered = FALSE;
    /* Below we check column by column if we need to access
    the clustered index */
  }

  n_fields = (ulint)table->getShare()->sizeFields(); /* number of columns */

  if (!prebuilt->mysql_template) {
    prebuilt->mysql_template = (mysql_row_templ_t*)
      mem_alloc(n_fields * sizeof(mysql_row_templ_t));
  }

  prebuilt->template_type = templ_type;
  prebuilt->null_bitmap_len = table->getShare()->null_bytes;

  prebuilt->templ_contains_blob = FALSE;

  /* Note that in InnoDB, i is the column number. MySQL calls columns
  'fields'. */
  for (i = 0; i < n_fields; i++)
  {
    const dict_col_t *col= &index->table->cols[i];
    templ = prebuilt->mysql_template + n_requested_fields;
    field = table->getField(i);

    if (UNIV_LIKELY(templ_type == ROW_MYSQL_REC_FIELDS)) {
      /* Decide which columns we should fetch
      and which we can skip. */
      register const ibool  index_contains_field =
        dict_index_contains_col_or_prefix(index, i);

      if (!index_contains_field && prebuilt->read_just_key) {
        /* If this is a 'key read', we do not need
        columns that are not in the key */

        goto skip_field;
      }

      if (index_contains_field && fetch_all_in_key) {
        /* This field is needed in the query */

        goto include_field;
      }

                        if (field->isReadSet() || field->isWriteSet())
        /* This field is needed in the query */
        goto include_field;

                        assert(table->isReadSet(i) == field->isReadSet());
                        assert(table->isWriteSet(i) == field->isWriteSet());

      if (fetch_primary_key_cols
        && dict_table_col_in_clustered_key(
          index->table, i)) {
        /* This field is needed in the query */

        goto include_field;
      }

      /* This field is not needed in the query, skip it */

      goto skip_field;
    }
include_field:
    n_requested_fields++;

    templ->col_no = i;
    templ->clust_rec_field_no = dict_col_get_clust_pos(col, clust_index);
    ut_ad(templ->clust_rec_field_no != ULINT_UNDEFINED);

    if (index == clust_index) {
      templ->rec_field_no = templ->clust_rec_field_no;
    } else {
      templ->rec_field_no = dict_index_get_nth_col_pos(
                index, i);
      if (templ->rec_field_no == ULINT_UNDEFINED) {
        prebuilt->need_to_access_clustered = TRUE;
      }
    }

    if (field->null_ptr) {
      templ->mysql_null_byte_offset =
        (ulint) ((char*) field->null_ptr
          - (char*) table->getInsertRecord());

      templ->mysql_null_bit_mask = (ulint) field->null_bit;
    } else {
      templ->mysql_null_bit_mask = 0;
    }

    templ->mysql_col_offset = (ulint)
          get_field_offset(table, field);

    templ->mysql_col_len = (ulint) field->pack_length();
    if (mysql_prefix_len < templ->mysql_col_offset
        + templ->mysql_col_len) {
      mysql_prefix_len = templ->mysql_col_offset
        + templ->mysql_col_len;
    }
    templ->type = col->mtype;
    templ->mysql_type = (ulint)field->type();

    if (templ->mysql_type == DATA_MYSQL_TRUE_VARCHAR) {
      templ->mysql_length_bytes = (ulint)
        (((Field_varstring*)field)->pack_length_no_ptr());
    }

    templ->charset = dtype_get_charset_coll(col->prtype);
    templ->mbminlen = dict_col_get_mbminlen(col);
    templ->mbmaxlen = dict_col_get_mbmaxlen(col);
    templ->is_unsigned = col->prtype & DATA_UNSIGNED;
    if (templ->type == DATA_BLOB) {
      prebuilt->templ_contains_blob = TRUE;
    }
skip_field:
    ;
  }

  prebuilt->n_template = n_requested_fields;
  prebuilt->mysql_prefix_len = mysql_prefix_len;

  if (index != clust_index && prebuilt->need_to_access_clustered) {
    /* Change rec_field_no's to correspond to the clustered index
    record */
    for (i = 0; i < n_requested_fields; i++) {
      templ = prebuilt->mysql_template + i;

      templ->rec_field_no = templ->clust_rec_field_no;
    }
  }
}

/********************************************************************//**
This special handling is really to overcome the limitations of MySQL's
binlogging. We need to eliminate the non-determinism that will arise in
INSERT ... SELECT type of statements, since MySQL binlog only stores the
min value of the autoinc interval. Once that is fixed we can get rid of
the special lock handling.
@return DB_SUCCESS if all OK else error code */
UNIV_INTERN
ulint
ha_innobase::innobase_lock_autoinc(void)
/*====================================*/
{
  ulint   error = DB_SUCCESS;

  dict_table_autoinc_lock(prebuilt->table);

  return(ulong(error));
}

/********************************************************************//**
Reset the autoinc value in the table.
@return DB_SUCCESS if all went well else error code */
UNIV_INTERN
ulint
ha_innobase::innobase_reset_autoinc(
/*================================*/
  uint64_t  autoinc)  /*!< in: value to store */
{
  dict_table_autoinc_lock(prebuilt->table);
  dict_table_autoinc_initialize(prebuilt->table, autoinc);
  dict_table_autoinc_unlock(prebuilt->table);

  return(ulong(DB_SUCCESS));
}

/********************************************************************//**
Store the autoinc value in the table. The autoinc value is only set if
it's greater than the existing autoinc value in the table.
@return DB_SUCCESS if all went well else error code */
UNIV_INTERN
ulint
ha_innobase::innobase_set_max_autoinc(
/*==================================*/
  uint64_t  auto_inc) /*!< in: value to store */
{
  dict_table_autoinc_lock(prebuilt->table);
  dict_table_autoinc_update_if_greater(prebuilt->table, auto_inc);
  dict_table_autoinc_unlock(prebuilt->table);

  return(ulong(DB_SUCCESS));
}

/********************************************************************//**
Stores a row in an InnoDB database, to the table specified in this
handle.
@return error code */
UNIV_INTERN
int
ha_innobase::doInsertRecord(
/*===================*/
  unsigned char*  record) /*!< in: a row in MySQL format */
{
  ulint   error = 0;
        int             error_result= 0;
  ibool   auto_inc_used= FALSE;
  ulint   sql_command;
  trx_t*    trx = session_to_trx(user_session);

  if (prebuilt->trx != trx) {
    errmsg_printf(error::ERROR, "The transaction object for the table handle is at "
        "%p, but for the current thread it is at %p",
        (const void*) prebuilt->trx, (const void*) trx);

    fputs("InnoDB: Dump of 200 bytes around prebuilt: ", stderr);
    ut_print_buf(stderr, ((const byte*)prebuilt) - 100, 200);
    fputs("\n"
      "InnoDB: Dump of 200 bytes around ha_data: ",
      stderr);
    ut_print_buf(stderr, ((const byte*) trx) - 100, 200);
    putc('\n', stderr);
    ut_error;
  }

  sql_command = user_session->getSqlCommand();

  if ((sql_command == SQLCOM_ALTER_TABLE
       || sql_command == SQLCOM_CREATE_INDEX
       || sql_command == SQLCOM_DROP_INDEX)
      && num_write_row >= 10000) {
    /* ALTER TABLE is COMMITted at every 10000 copied rows.
    The IX table lock for the original table has to be re-issued.
    As this method will be called on a temporary table where the
    contents of the original table is being copied to, it is
    a bit tricky to determine the source table.  The cursor
    position in the source table need not be adjusted after the
    intermediate COMMIT, since writes by other transactions are
    being blocked by a MySQL table lock TL_WRITE_ALLOW_READ. */

    dict_table_t* src_table;
    enum lock_mode  mode;

    num_write_row = 0;

    /* Commit the transaction.  This will release the table
    locks, so they have to be acquired again. */

    /* Altering an InnoDB table */
    /* Get the source table. */
    src_table = lock_get_src_table(
        prebuilt->trx, prebuilt->table, &mode);
    if (!src_table) {
no_commit:
      /* Unknown situation: do not commit */
      /*
      ut_print_timestamp(stderr);
      fprintf(stderr,
        "  InnoDB: ALTER TABLE is holding lock"
        " on %lu tables!\n",
        prebuilt->trx->mysql_n_tables_locked);
      */
      ;
    } else if (src_table == prebuilt->table) {
      /* Source table is not in InnoDB format:
      no need to re-acquire locks on it. */

      /* Altering to InnoDB format */
      getTransactionalEngine()->commit(user_session, 1);
      /* We will need an IX lock on the destination table. */
      prebuilt->sql_stat_start = TRUE;
    } else {
      /* Ensure that there are no other table locks than
      LOCK_IX and LOCK_AUTO_INC on the destination table. */

      if (!lock_is_table_exclusive(prebuilt->table,
              prebuilt->trx)) {
        goto no_commit;
      }

      /* Commit the transaction.  This will release the table
      locks, so they have to be acquired again. */
      getTransactionalEngine()->commit(user_session, 1);
      /* Re-acquire the table lock on the source table. */
      row_lock_table_for_mysql(prebuilt, src_table, mode);
      /* We will need an IX lock on the destination table. */
      prebuilt->sql_stat_start = TRUE;
    }
  }

  num_write_row++;

  /* This is the case where the table has an auto-increment column */
  if (getTable()->next_number_field && record == getTable()->getInsertRecord()) {

    /* Reset the error code before calling
    innobase_get_auto_increment(). */
    prebuilt->autoinc_error = DB_SUCCESS;

    if ((error = update_auto_increment())) {
      /* We don't want to mask autoinc overflow errors. */

      /* Handle the case where the AUTOINC sub-system
         failed during initialization. */
      if (prebuilt->autoinc_error == DB_UNSUPPORTED) {
        error_result = ER_AUTOINC_READ_FAILED;
        /* Set the error message to report too. */
        my_error(ER_AUTOINC_READ_FAILED, MYF(0));
        goto func_exit;
      } else if (prebuilt->autoinc_error != DB_SUCCESS) {
        error = (int) prebuilt->autoinc_error;

        goto report_error;
      }

      /* MySQL errors are passed straight back. */
      error_result = (int) error;
      goto func_exit;
    }

    auto_inc_used = TRUE;
  }

  if (prebuilt->mysql_template == NULL
      || prebuilt->template_type != ROW_MYSQL_WHOLE_ROW) {

    /* Build the template used in converting quickly between
    the two database formats */

    build_template(prebuilt, NULL, getTable(), ROW_MYSQL_WHOLE_ROW);
  }

  innodb_srv_conc_enter_innodb(prebuilt->trx);

  error = row_insert_for_mysql((byte*) record, prebuilt);

  user_session->setXaId(trx->id);

  /* Handle duplicate key errors */
  if (auto_inc_used) {
    ulint   err;
    uint64_t  auto_inc;
    uint64_t  col_max_value;

    /* Note the number of rows processed for this statement, used
    by get_auto_increment() to determine the number of AUTO-INC
    values to reserve. This is only useful for a mult-value INSERT
    and is a statement level counter.*/
    if (trx->n_autoinc_rows > 0) {
      --trx->n_autoinc_rows;
    }

    /* We need the upper limit of the col type to check for
    whether we update the table autoinc counter or not. */
    col_max_value = innobase_get_int_col_max_value(
      getTable()->next_number_field); 
    /* Get the value that MySQL attempted to store in the table.*/
    auto_inc = getTable()->next_number_field->val_int();

    switch (error) {
    case DB_DUPLICATE_KEY:

      /* A REPLACE command and LOAD DATA INFILE REPLACE
      handle a duplicate key error themselves, but we
      must update the autoinc counter if we are performing
      those statements. */

      switch (sql_command) {
      case SQLCOM_LOAD:
        if ((trx->duplicates
            & (TRX_DUP_IGNORE | TRX_DUP_REPLACE))) {

          goto set_max_autoinc;
        }
        break;

      case SQLCOM_REPLACE:
      case SQLCOM_INSERT_SELECT:
      case SQLCOM_REPLACE_SELECT:
        goto set_max_autoinc;

      default:
        break;
      }

      break;

    case DB_SUCCESS:
      /* If the actual value inserted is greater than
      the upper limit of the interval, then we try and
      update the table upper limit. Note: last_value
      will be 0 if get_auto_increment() was not called.*/

      if (auto_inc >= prebuilt->autoinc_last_value) {
set_max_autoinc:
        /* This should filter out the negative
           values set explicitly by the user. */
        if (auto_inc <= col_max_value) {
          ut_a(prebuilt->autoinc_increment > 0);

          uint64_t	need;
          uint64_t	offset;

          offset = prebuilt->autoinc_offset;
          need = prebuilt->autoinc_increment;

          auto_inc = innobase_next_autoinc(
                                           auto_inc,
                                           need, offset, col_max_value);

          err = innobase_set_max_autoinc(
                                         auto_inc);

          if (err != DB_SUCCESS) {
            error = err;
          }
        }
      }
      break;
    }
  }

  innodb_srv_conc_exit_innodb(prebuilt->trx);

report_error:
  error_result = convert_error_code_to_mysql((int) error,
               prebuilt->table->flags,
               user_session);

func_exit:
  innobase_active_small();

  return(error_result);
}

/**********************************************************************//**
Checks which fields have changed in a row and stores information
of them to an update vector.
@return error number or 0 */
static
int
calc_row_difference(
/*================*/
  upd_t*    uvect,    /*!< in/out: update vector */
  unsigned char*    old_row,  /*!< in: old row in MySQL format */
  unsigned char*    new_row,  /*!< in: new row in MySQL format */
  Table* table,   /*!< in: table in MySQL data
          dictionary */
  unsigned char*  upd_buff, /*!< in: buffer to use */
  ulint   buff_len, /*!< in: buffer length */
  row_prebuilt_t* prebuilt, /*!< in: InnoDB prebuilt struct */
  Session*  )   /*!< in: user thread */
{
  unsigned char*    original_upd_buff = upd_buff;
  enum_field_types field_mysql_type;
  uint    n_fields;
  ulint   o_len;
  ulint   n_len;
  ulint   col_pack_len;
  const byte* new_mysql_row_col;
  const byte* o_ptr;
  const byte* n_ptr;
  byte*   buf;
  upd_field_t*  ufield;
  ulint   col_type;
  ulint   n_changed = 0;
  dfield_t  dfield;
  dict_index_t* clust_index;
  uint    i= 0;

  n_fields = table->getShare()->sizeFields();
  clust_index = dict_table_get_first_index(prebuilt->table);

  /* We use upd_buff to convert changed fields */
  buf = (byte*) upd_buff;

  for (i = 0; i < n_fields; i++) {
    Field *field= table->getField(i);

    o_ptr = (const byte*) old_row + get_field_offset(table, field);
    n_ptr = (const byte*) new_row + get_field_offset(table, field);

    /* Use new_mysql_row_col and col_pack_len save the values */

    new_mysql_row_col = n_ptr;
    col_pack_len = field->pack_length();

    o_len = col_pack_len;
    n_len = col_pack_len;

    /* We use o_ptr and n_ptr to dig up the actual data for
    comparison. */

    field_mysql_type = field->type();

    col_type = prebuilt->table->cols[i].mtype;

    switch (col_type) {

    case DATA_BLOB:
      o_ptr = row_mysql_read_blob_ref(&o_len, o_ptr, o_len);
      n_ptr = row_mysql_read_blob_ref(&n_len, n_ptr, n_len);

      break;

    case DATA_VARCHAR:
    case DATA_BINARY:
    case DATA_VARMYSQL:
      if (field_mysql_type == DRIZZLE_TYPE_VARCHAR) {
        /* This is a >= 5.0.3 type true VARCHAR where
        the real payload data length is stored in
        1 or 2 bytes */

        o_ptr = row_mysql_read_true_varchar(
          &o_len, o_ptr,
          (ulint)
          (((Field_varstring*)field)->pack_length_no_ptr()));

        n_ptr = row_mysql_read_true_varchar(
          &n_len, n_ptr,
          (ulint)
          (((Field_varstring*)field)->pack_length_no_ptr()));
      }

      break;
    default:
      ;
    }

    if (field->null_ptr) {
      if (field_in_record_is_null(table, field,
              (char*) old_row)) {
        o_len = UNIV_SQL_NULL;
      }

      if (field_in_record_is_null(table, field,
              (char*) new_row)) {
        n_len = UNIV_SQL_NULL;
      }
    }

    if (o_len != n_len || (o_len != UNIV_SQL_NULL &&
          0 != memcmp(o_ptr, n_ptr, o_len))) {
      /* The field has changed */

      ufield = uvect->fields + n_changed;

      /* Let us use a dummy dfield to make the conversion
      from the MySQL column format to the InnoDB format */

      dict_col_copy_type(prebuilt->table->cols + i,
                 &dfield.type);

      if (n_len != UNIV_SQL_NULL) {
        buf = row_mysql_store_col_in_innobase_format(
          &dfield,
          (byte*)buf,
          TRUE,
          new_mysql_row_col,
          col_pack_len,
          dict_table_is_comp(prebuilt->table));
        dfield_copy_data(&ufield->new_val, &dfield);
      } else {
        dfield_set_null(&ufield->new_val);
      }

      ufield->exp = NULL;
      ufield->orig_len = 0;
      ufield->field_no = dict_col_get_clust_pos(
        &prebuilt->table->cols[i], clust_index);
      n_changed++;
    }
  }

  uvect->n_fields = n_changed;
  uvect->info_bits = 0;

  ut_a(buf <= (byte*)original_upd_buff + buff_len);

  return(0);
}

/**********************************************************************//**
Updates a row given as a parameter to a new value. Note that we are given
whole rows, not just the fields which are updated: this incurs some
overhead for CPU when we check which fields are actually updated.
TODO: currently InnoDB does not prevent the 'Halloween problem':
in a searched update a single row can get updated several times
if its index columns are updated!
@return error number or 0 */
UNIV_INTERN
int
ha_innobase::doUpdateRecord(
/*====================*/
  const unsigned char*  old_row,/*!< in: old row in MySQL format */
  unsigned char*    new_row)/*!< in: new row in MySQL format */
{
  upd_t*    uvect;
  int   error = 0;
  trx_t*    trx = session_to_trx(user_session);

  ut_a(prebuilt->trx == trx);

  if (prebuilt->upd_node) {
    uvect = prebuilt->upd_node->update;
  } else {
    uvect = row_get_prebuilt_update_vector(prebuilt);
  }

  /* Build an update vector from the modified fields in the rows
  (uses upd_buff of the handle) */

  calc_row_difference(uvect, (unsigned char*) old_row, new_row, getTable(),
      &upd_buff[0], (ulint)upd_and_key_val_buff_len,
      prebuilt, user_session);

  /* This is not a delete */
  prebuilt->upd_node->is_delete = FALSE;

  ut_a(prebuilt->template_type == ROW_MYSQL_WHOLE_ROW);

  if (getTable()->found_next_number_field)
  {
    uint64_t  auto_inc;
    uint64_t  col_max_value;

    auto_inc = getTable()->found_next_number_field->val_int();

    /* We need the upper limit of the col type to check for
    whether we update the table autoinc counter or not. */
    col_max_value = innobase_get_int_col_max_value(
      getTable()->found_next_number_field);

    uint64_t current_autoinc;
    ulint autoinc_error= innobase_get_autoinc(&current_autoinc);
    if (autoinc_error == DB_SUCCESS
        && auto_inc <= col_max_value && auto_inc != 0
        && auto_inc >= current_autoinc)
    {

      uint64_t  need;
      uint64_t  offset;

      offset = prebuilt->autoinc_offset;
      need = prebuilt->autoinc_increment;

      auto_inc = innobase_next_autoinc(
        auto_inc, need, offset, col_max_value);

      dict_table_autoinc_update_if_greater(prebuilt->table, auto_inc);
    }

    dict_table_autoinc_unlock(prebuilt->table);
  }

  innodb_srv_conc_enter_innodb(trx);

  error = row_update_for_mysql((byte*) old_row, prebuilt);

  user_session->setXaId(trx->id);

  /* We need to do some special AUTOINC handling for the following case:

  INSERT INTO t (c1,c2) VALUES(x,y) ON DUPLICATE KEY UPDATE ...

  We need to use the AUTOINC counter that was actually used by
  MySQL in the UPDATE statement, which can be different from the
  value used in the INSERT statement.*/

  if (error == DB_SUCCESS
      && getTable()->next_number_field
      && new_row == getTable()->getInsertRecord()
      && user_session->getSqlCommand() == SQLCOM_INSERT
      && (trx->duplicates & (TRX_DUP_IGNORE | TRX_DUP_REPLACE))
    == TRX_DUP_IGNORE)  {

    uint64_t  auto_inc;
    uint64_t  col_max_value;

    auto_inc = getTable()->next_number_field->val_int();

    /* We need the upper limit of the col type to check for
    whether we update the table autoinc counter or not. */
    col_max_value = innobase_get_int_col_max_value(
      getTable()->next_number_field);

    if (auto_inc <= col_max_value && auto_inc != 0) {

      uint64_t  need;
      uint64_t  offset;

      offset = prebuilt->autoinc_offset;
      need = prebuilt->autoinc_increment;

      auto_inc = innobase_next_autoinc(
        auto_inc, need, offset, col_max_value);

      error = innobase_set_max_autoinc(auto_inc);
    }
  }

  innodb_srv_conc_exit_innodb(trx);

  error = convert_error_code_to_mysql(error,
              prebuilt->table->flags,
                                            user_session);

  if (error == 0 /* success */
      && uvect->n_fields == 0 /* no columns were updated */) {

    /* This is the same as success, but instructs
    MySQL that the row is not really updated and it
    should not increase the count of updated rows.
    This is fix for http://bugs.mysql.com/29157 */
    error = HA_ERR_RECORD_IS_THE_SAME;
  }

  /* Tell InnoDB server that there might be work for
  utility threads: */

  innobase_active_small();

  return(error);
}

/**********************************************************************//**
Deletes a row given as the parameter.
@return error number or 0 */
UNIV_INTERN
int
ha_innobase::doDeleteRecord(
/*====================*/
  const unsigned char*  record) /*!< in: a row in MySQL format */
{
  int   error = 0;
  trx_t*    trx = session_to_trx(user_session);

  ut_a(prebuilt->trx == trx);

  if (!prebuilt->upd_node) {
    row_get_prebuilt_update_vector(prebuilt);
  }

  /* This is a delete */

  prebuilt->upd_node->is_delete = TRUE;

  innodb_srv_conc_enter_innodb(trx);

  error = row_update_for_mysql((byte*) record, prebuilt);

  user_session->setXaId(trx->id);

  innodb_srv_conc_exit_innodb(trx);

  error = convert_error_code_to_mysql(
    error, prebuilt->table->flags, user_session);

  /* Tell the InnoDB server that there might be work for
  utility threads: */

  innobase_active_small();

  return(error);
}

/**********************************************************************//**
Removes a new lock set on a row, if it was not read optimistically. This can
be called after a row has been read in the processing of an UPDATE or a DELETE
query, if the option innodb_locks_unsafe_for_binlog is set. */
UNIV_INTERN
void
ha_innobase::unlock_row(void)
/*=========================*/
{
  /* Consistent read does not take any locks, thus there is
  nothing to unlock. */

  if (prebuilt->select_lock_type == LOCK_NONE) {
    return;
  }

  switch (prebuilt->row_read_type) {
  case ROW_READ_WITH_LOCKS:
    if (!srv_locks_unsafe_for_binlog
        && prebuilt->trx->isolation_level
        > TRX_ISO_READ_COMMITTED) {
      break;
    }
    /* fall through */
  case ROW_READ_TRY_SEMI_CONSISTENT:
    row_unlock_for_mysql(prebuilt, FALSE);
    break;
  case ROW_READ_DID_SEMI_CONSISTENT:
    prebuilt->row_read_type = ROW_READ_TRY_SEMI_CONSISTENT;
    break;
  }

  return;
}

/* See Cursor.h and row0mysql.h for docs on this function. */
UNIV_INTERN
bool
ha_innobase::was_semi_consistent_read(void)
/*=======================================*/
{
  return(prebuilt->row_read_type == ROW_READ_DID_SEMI_CONSISTENT);
}

/* See Cursor.h and row0mysql.h for docs on this function. */
UNIV_INTERN
void
ha_innobase::try_semi_consistent_read(bool yes)
/*===========================================*/
{
  ut_a(prebuilt->trx == session_to_trx(getTable()->in_use));

  /* Row read type is set to semi consistent read if this was
  requested by the MySQL and either innodb_locks_unsafe_for_binlog
  option is used or this session is using READ COMMITTED isolation
  level. */

  if (yes
      && (srv_locks_unsafe_for_binlog
    || prebuilt->trx->isolation_level <= TRX_ISO_READ_COMMITTED)) {
    prebuilt->row_read_type = ROW_READ_TRY_SEMI_CONSISTENT;
  } else {
    prebuilt->row_read_type = ROW_READ_WITH_LOCKS;
  }
}

/******************************************************************//**
Initializes a handle to use an index.
@return 0 or error number */
UNIV_INTERN
int
ha_innobase::doStartIndexScan(
/*====================*/
  uint  keynr,  /*!< in: key (index) number */
  bool )    /*!< in: 1 if result MUST be sorted according to index */
{
  return(change_active_index(keynr));
}

/******************************************************************//**
Currently does nothing.
@return 0 */
UNIV_INTERN
int
ha_innobase::doEndIndexScan(void)
/*========================*/
{
  int error = 0;
  active_index=MAX_KEY;
  return(error);
}

/*********************************************************************//**
Converts a search mode flag understood by MySQL to a flag understood
by InnoDB. */
static inline
ulint
convert_search_mode_to_innobase(
/*============================*/
  enum ha_rkey_function find_flag)
{
  switch (find_flag) {
  case HA_READ_KEY_EXACT:
    /* this does not require the index to be UNIQUE */
    return(PAGE_CUR_GE);
  case HA_READ_KEY_OR_NEXT:
    return(PAGE_CUR_GE);
  case HA_READ_KEY_OR_PREV:
    return(PAGE_CUR_LE);
  case HA_READ_AFTER_KEY: 
    return(PAGE_CUR_G);
  case HA_READ_BEFORE_KEY:
    return(PAGE_CUR_L);
  case HA_READ_PREFIX:
    return(PAGE_CUR_GE);
  case HA_READ_PREFIX_LAST:
    return(PAGE_CUR_LE);
  case HA_READ_PREFIX_LAST_OR_PREV:
    return(PAGE_CUR_LE);
    /* In MySQL-4.0 HA_READ_PREFIX and HA_READ_PREFIX_LAST always
    pass a complete-field prefix of a key value as the search
    tuple. I.e., it is not allowed that the last field would
    just contain n first bytes of the full field value.
    MySQL uses a 'padding' trick to convert LIKE 'abc%'
    type queries so that it can use as a search tuple
    a complete-field-prefix of a key value. Thus, the InnoDB
    search mode PAGE_CUR_LE_OR_EXTENDS is never used.
    TODO: when/if MySQL starts to use also partial-field
    prefixes, we have to deal with stripping of spaces
    and comparison of non-latin1 char type fields in
    innobase_mysql_cmp() to get PAGE_CUR_LE_OR_EXTENDS to
    work correctly. */
  case HA_READ_MBR_CONTAIN:
  case HA_READ_MBR_INTERSECT:
  case HA_READ_MBR_WITHIN:
  case HA_READ_MBR_DISJOINT:
  case HA_READ_MBR_EQUAL:
    return(PAGE_CUR_UNSUPP);
  /* do not use "default:" in order to produce a gcc warning:
  enumeration value '...' not handled in switch
  (if -Wswitch or -Wall is used) */
  }

  my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "this functionality");

  return(PAGE_CUR_UNSUPP);
}

/*
   BACKGROUND INFO: HOW A SELECT SQL QUERY IS EXECUTED
   ---------------------------------------------------
The following does not cover all the details, but explains how we determine
the start of a new SQL statement, and what is associated with it.

For each table in the database the MySQL interpreter may have several
table handle instances in use, also in a single SQL query. For each table
handle instance there is an InnoDB  'prebuilt' struct which contains most
of the InnoDB data associated with this table handle instance.

  A) if the user has not explicitly set any MySQL table level locks:

  1) Drizzle calls StorageEngine::doStartStatement(), indicating to
     InnoDB that a new SQL statement has begun.

  2a) For each InnoDB-managed table in the SELECT, Drizzle calls ::external_lock
     to set an 'intention' table level lock on the table of the Cursor instance.
     There we set prebuilt->sql_stat_start = TRUE. The flag sql_stat_start should 
     be set true if we are taking this table handle instance to use in a new SQL
     statement issued by the user.

  2b) If prebuilt->sql_stat_start == TRUE we 'pre-compile' the MySQL search
instructions to prebuilt->template of the table handle instance in
::index_read. The template is used to save CPU time in large joins.

  3) In row_search_for_mysql, if prebuilt->sql_stat_start is true, we
allocate a new consistent read view for the trx if it does not yet have one,
or in the case of a locking read, set an InnoDB 'intention' table level
lock on the table.

  4) We do the SELECT. MySQL may repeatedly call ::index_read for the
same table handle instance, if it is a join.

5) When the SELECT ends, the Drizzle kernel calls doEndStatement()

 (a) we execute a COMMIT there if the autocommit is on. The Drizzle interpreter 
     does NOT execute autocommit for pure read transactions, though it should.
     That is why we must execute the COMMIT in ::doEndStatement().
 (b) we also release possible 'SQL statement level resources' InnoDB may
     have for this SQL statement.

  @todo

  Remove need for InnoDB to call autocommit for read-only trx

  @todo Check the below is still valid (I don't think it is...)

  B) If the user has explicitly set MySQL table level locks, then MySQL
does NOT call ::external_lock at the start of the statement. To determine
when we are at the start of a new SQL statement we at the start of
::index_read also compare the query id to the latest query id where the
table handle instance was used. If it has changed, we know we are at the
start of a new SQL statement. Since the query id can theoretically
overwrap, we use this test only as a secondary way of determining the
start of a new SQL statement. */


/**********************************************************************//**
Positions an index cursor to the index specified in the handle. Fetches the
row if any.
@return 0, HA_ERR_KEY_NOT_FOUND, or error number */
UNIV_INTERN
int
ha_innobase::index_read(
/*====================*/
  unsigned char*    buf,  /*!< in/out: buffer for the returned
          row */
  const unsigned char*  key_ptr,/*!< in: key value; if this is NULL
          we position the cursor at the
          start or end of index; this can
          also contain an InnoDB row id, in
          which case key_len is the InnoDB
          row id length; the key value can
          also be a prefix of a full key value,
          and the last column can be a prefix
          of a full column */
  uint      key_len,/*!< in: key value length */
  enum ha_rkey_function find_flag)/*!< in: search flags from my_base.h */
{
  ulint   mode;
  dict_index_t* index;
  ulint   match_mode  = 0;
  int   error;
  ulint   ret;

  ut_a(prebuilt->trx == session_to_trx(user_session));

  ha_statistic_increment(&system_status_var::ha_read_key_count);

  index = prebuilt->index;

  if (UNIV_UNLIKELY(index == NULL)) {
    prebuilt->index_usable = FALSE;
    return(HA_ERR_CRASHED);
  }

  if (UNIV_UNLIKELY(!prebuilt->index_usable)) {
    return(HA_ERR_TABLE_DEF_CHANGED);
  }

  /* Note that if the index for which the search template is built is not
  necessarily prebuilt->index, but can also be the clustered index */

  if (prebuilt->sql_stat_start) {
    build_template(prebuilt, user_session, getTable(),
             ROW_MYSQL_REC_FIELDS);
  }

  if (key_ptr) {
    /* Convert the search key value to InnoDB format into
    prebuilt->search_tuple */

    row_sel_convert_mysql_key_to_innobase(
      prebuilt->search_tuple,
      (byte*) &key_val_buff[0],
      (ulint)upd_and_key_val_buff_len,
      index,
      (byte*) key_ptr,
      (ulint) key_len,
      prebuilt->trx);
  } else {
    /* We position the cursor to the last or the first entry
    in the index */

    dtuple_set_n_fields(prebuilt->search_tuple, 0);
  }

  mode = convert_search_mode_to_innobase(find_flag);

  match_mode = 0;

  if (find_flag == HA_READ_KEY_EXACT) {

    match_mode = ROW_SEL_EXACT;

  } else if (find_flag == HA_READ_PREFIX
       || find_flag == HA_READ_PREFIX_LAST) {

    match_mode = ROW_SEL_EXACT_PREFIX;
  }

  last_match_mode = (uint) match_mode;

  if (mode != PAGE_CUR_UNSUPP) {

    innodb_srv_conc_enter_innodb(prebuilt->trx);

    ret = row_search_for_mysql((byte*) buf, mode, prebuilt,
             match_mode, 0);

    innodb_srv_conc_exit_innodb(prebuilt->trx);
  } else {

    ret = DB_UNSUPPORTED;
  }

  switch (ret) {
  case DB_SUCCESS:
    error = 0;
    getTable()->status = 0;
    break;
  case DB_RECORD_NOT_FOUND:
    error = HA_ERR_KEY_NOT_FOUND;
    getTable()->status = STATUS_NOT_FOUND;
    break;
  case DB_END_OF_INDEX:
    error = HA_ERR_KEY_NOT_FOUND;
    getTable()->status = STATUS_NOT_FOUND;
    break;
  default:
    error = convert_error_code_to_mysql((int) ret,
                prebuilt->table->flags,
                user_session);
    getTable()->status = STATUS_NOT_FOUND;
    break;
  }

  return(error);
}

/*******************************************************************//**
The following functions works like index_read, but it find the last
row with the current key value or prefix.
@return 0, HA_ERR_KEY_NOT_FOUND, or an error code */
UNIV_INTERN
int
ha_innobase::index_read_last(
/*=========================*/
  unsigned char*  buf,  /*!< out: fetched row */
  const unsigned char*  key_ptr,/*!< in: key value, or a prefix of a full
        key value */
  uint    key_len)/*!< in: length of the key val or prefix
        in bytes */
{
  return(index_read(buf, key_ptr, key_len, HA_READ_PREFIX_LAST));
}

/********************************************************************//**
Get the index for a handle. Does not change active index.
@return NULL or index instance. */
UNIV_INTERN
dict_index_t*
ha_innobase::innobase_get_index(
/*============================*/
  uint    keynr)  /*!< in: use this index; MAX_KEY means always
        clustered index, even if it was internally
        generated by InnoDB */
{
  dict_index_t* index = 0;

  ha_statistic_increment(&system_status_var::ha_read_key_count);

  if (keynr != MAX_KEY && getTable()->getShare()->sizeKeys() > 0) 
  {
    KeyInfo *key = getTable()->key_info + keynr;
    index = innobase_index_lookup(share, keynr);

    if (index) {
      ut_a(ut_strcmp(index->name, key->name) == 0);
    } else {
      /* Can't find index with keynr in the translation
         table. Only print message if the index translation
         table exists */
      if (share->idx_trans_tbl.index_mapping) {
        errmsg_printf(error::ERROR,
                      "InnoDB could not find "
                      "index %s key no %u for "
                      "table %s through its "
                      "index translation table",
                      key ? key->name : "NULL",
                      keynr,
                      prebuilt->table->name);
      }

      index = dict_table_get_index_on_name(prebuilt->table,
                                           key->name);
    }
  } else {
    index = dict_table_get_first_index(prebuilt->table);
  }

  if (!index) {
    errmsg_printf(error::ERROR, 
      "Innodb could not find key n:o %u with name %s "
      "from dict cache for table %s",
      keynr, getTable()->getShare()->getTableMessage()->indexes(keynr).name().c_str(),
      prebuilt->table->name);
  }

  return(index);
}

/********************************************************************//**
Changes the active index of a handle.
@return 0 or error code */
UNIV_INTERN
int
ha_innobase::change_active_index(
/*=============================*/
  uint  keynr)  /*!< in: use this index; MAX_KEY means always clustered
      index, even if it was internally generated by
      InnoDB */
{
  ut_ad(user_session == table->in_use);
  ut_a(prebuilt->trx == session_to_trx(user_session));

  active_index = keynr;

  prebuilt->index = innobase_get_index(keynr);

  if (UNIV_UNLIKELY(!prebuilt->index)) {
    errmsg_printf(error::WARN, "InnoDB: change_active_index(%u) failed",
          keynr);
    prebuilt->index_usable = FALSE;
    return(1);
  }

  prebuilt->index_usable = row_merge_is_index_usable(prebuilt->trx,
                 prebuilt->index);

  if (UNIV_UNLIKELY(!prebuilt->index_usable)) {
    push_warning_printf(user_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        HA_ERR_TABLE_DEF_CHANGED,
                        "InnoDB: insufficient history for index %u",
                        keynr);
    /* The caller seems to ignore this.  Thus, we must check
    this again in row_search_for_mysql(). */
    return(2);
  }

  ut_a(prebuilt->search_tuple != 0);

  dtuple_set_n_fields(prebuilt->search_tuple, prebuilt->index->n_fields);

  dict_index_copy_types(prebuilt->search_tuple, prebuilt->index,
      prebuilt->index->n_fields);

  /* MySQL changes the active index for a handle also during some
  queries, for example SELECT MAX(a), SUM(a) first retrieves the MAX()
  and then calculates the sum. Previously we played safe and used
  the flag ROW_MYSQL_WHOLE_ROW below, but that caused unnecessary
  copying. Starting from MySQL-4.1 we use a more efficient flag here. */

  build_template(prebuilt, user_session, getTable(), ROW_MYSQL_REC_FIELDS);

  return(0);
}

/**********************************************************************//**
Positions an index cursor to the index specified in keynr. Fetches the
row if any.
??? This is only used to read whole keys ???
@return error number or 0 */
UNIV_INTERN
int
ha_innobase::index_read_idx(
/*========================*/
  unsigned char*  buf,    /*!< in/out: buffer for the returned
          row */
  uint    keynr,    /*!< in: use this index */
  const unsigned char*  key,  /*!< in: key value; if this is NULL
          we position the cursor at the
          start or end of index */
  uint    key_len,  /*!< in: key value length */
  enum ha_rkey_function find_flag)/*!< in: search flags from my_base.h */
{
  if (change_active_index(keynr)) {

    return(1);
  }

  return(index_read(buf, key, key_len, find_flag));
}

/***********************************************************************//**
Reads the next or previous row from a cursor, which must have previously been
positioned using index_read.
@return 0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::general_fetch(
/*=======================*/
  unsigned char*  buf,  /*!< in/out: buffer for next row in MySQL
        format */
  uint  direction,  /*!< in: ROW_SEL_NEXT or ROW_SEL_PREV */
  uint  match_mode) /*!< in: 0, ROW_SEL_EXACT, or
        ROW_SEL_EXACT_PREFIX */
{
  ulint   ret;
  int   error = 0;

  ut_a(prebuilt->trx == session_to_trx(user_session));

  innodb_srv_conc_enter_innodb(prebuilt->trx);

  ret = row_search_for_mysql(
    (byte*)buf, 0, prebuilt, match_mode, direction);

  innodb_srv_conc_exit_innodb(prebuilt->trx);

  switch (ret) {
  case DB_SUCCESS:
    error = 0;
    getTable()->status = 0;
    break;
  case DB_RECORD_NOT_FOUND:
    error = HA_ERR_END_OF_FILE;
    getTable()->status = STATUS_NOT_FOUND;
    break;
  case DB_END_OF_INDEX:
    error = HA_ERR_END_OF_FILE;
    getTable()->status = STATUS_NOT_FOUND;
    break;
  default:
    error = convert_error_code_to_mysql(
      (int) ret, prebuilt->table->flags, user_session);
    getTable()->status = STATUS_NOT_FOUND;
    break;
  }

  return(error);
}

/***********************************************************************//**
Reads the next row from a cursor, which must have previously been
positioned using index_read.
@return 0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::index_next(
/*====================*/
  unsigned char*  buf)  /*!< in/out: buffer for next row in MySQL
        format */
{
  ha_statistic_increment(&system_status_var::ha_read_next_count);

  return(general_fetch(buf, ROW_SEL_NEXT, 0));
}

/*******************************************************************//**
Reads the next row matching to the key value given as the parameter.
@return 0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::index_next_same(
/*=========================*/
  unsigned char*    buf,  /*!< in/out: buffer for the row */
  const unsigned char*  , /*!< in: key value */
  uint    ) /*!< in: key value length */
{
  ha_statistic_increment(&system_status_var::ha_read_next_count);

  return(general_fetch(buf, ROW_SEL_NEXT, last_match_mode));
}

/***********************************************************************//**
Reads the previous row from a cursor, which must have previously been
positioned using index_read.
@return 0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::index_prev(
/*====================*/
  unsigned char*  buf)  /*!< in/out: buffer for previous row in MySQL format */
{
  ha_statistic_increment(&system_status_var::ha_read_prev_count);

  return(general_fetch(buf, ROW_SEL_PREV, 0));
}

/********************************************************************//**
Positions a cursor on the first record in an index and reads the
corresponding row to buf.
@return 0, HA_ERR_END_OF_FILE, or error code */
UNIV_INTERN
int
ha_innobase::index_first(
/*=====================*/
  unsigned char*  buf)  /*!< in/out: buffer for the row */
{
  int error;

  ha_statistic_increment(&system_status_var::ha_read_first_count);

  error = index_read(buf, NULL, 0, HA_READ_AFTER_KEY);

  /* MySQL does not seem to allow this to return HA_ERR_KEY_NOT_FOUND */

  if (error == HA_ERR_KEY_NOT_FOUND) {
    error = HA_ERR_END_OF_FILE;
  }

  return(error);
}

/********************************************************************//**
Positions a cursor on the last record in an index and reads the
corresponding row to buf.
@return 0, HA_ERR_END_OF_FILE, or error code */
UNIV_INTERN
int
ha_innobase::index_last(
/*====================*/
  unsigned char*  buf)  /*!< in/out: buffer for the row */
{
  int error;

  ha_statistic_increment(&system_status_var::ha_read_last_count);

  error = index_read(buf, NULL, 0, HA_READ_BEFORE_KEY);

  /* MySQL does not seem to allow this to return HA_ERR_KEY_NOT_FOUND */

  if (error == HA_ERR_KEY_NOT_FOUND) {
    error = HA_ERR_END_OF_FILE;
  }

  return(error);
}

/****************************************************************//**
Initialize a table scan.
@return 0 or error number */
UNIV_INTERN
int
ha_innobase::doStartTableScan(
/*==================*/
  bool  scan) /*!< in: TRUE if table/index scan FALSE otherwise */
{
  int err;

  /* Store the active index value so that we can restore the original
  value after a scan */

  if (prebuilt->clust_index_was_generated) {
    err = change_active_index(MAX_KEY);
  } else {
    err = change_active_index(primary_key);
  }

  /* Don't use semi-consistent read in random row reads (by position).
  This means we must disable semi_consistent_read if scan is false */

  if (!scan) {
    try_semi_consistent_read(0);
  }

  start_of_scan = 1;

  return(err);
}

/*****************************************************************//**
Ends a table scan.
@return 0 or error number */
UNIV_INTERN
int
ha_innobase::doEndTableScan(void)
/*======================*/
{
  return(doEndIndexScan());
}

/*****************************************************************//**
Reads the next row in a table scan (also used to read the FIRST row
in a table scan).
@return 0, HA_ERR_END_OF_FILE, or error number */
UNIV_INTERN
int
ha_innobase::rnd_next(
/*==================*/
  unsigned char*  buf)  /*!< in/out: returns the row in this buffer,
      in MySQL format */
{
  int error;

  ha_statistic_increment(&system_status_var::ha_read_rnd_next_count);

  if (start_of_scan) {
    error = index_first(buf);

    if (error == HA_ERR_KEY_NOT_FOUND) {
      error = HA_ERR_END_OF_FILE;
    }

    start_of_scan = 0;
  } else {
    error = general_fetch(buf, ROW_SEL_NEXT, 0);
  }

  return(error);
}

/**********************************************************************//**
Fetches a row from the table based on a row reference.
@return 0, HA_ERR_KEY_NOT_FOUND, or error code */
UNIV_INTERN
int
ha_innobase::rnd_pos(
/*=================*/
  unsigned char*  buf,  /*!< in/out: buffer for the row */
  unsigned char*  pos)  /*!< in: primary key value of the row in the
      MySQL format, or the row id if the clustered
      index was internally generated by InnoDB; the
      length of data in pos has to be ref_length */
{
  int   error;
  uint    keynr = active_index;

  ha_statistic_increment(&system_status_var::ha_read_rnd_count);

  ut_a(prebuilt->trx == session_to_trx(getTable()->in_use));

  if (prebuilt->clust_index_was_generated) {
    /* No primary key was defined for the table and we
    generated the clustered index from the row id: the
    row reference is the row id, not any key value
    that MySQL knows of */

    error = change_active_index(MAX_KEY);
  } else {
    error = change_active_index(primary_key);
  }

  if (error) {
    return(error);
  }

  /* Note that we assume the length of the row reference is fixed
  for the table, and it is == ref_length */

  error = index_read(buf, pos, ref_length, HA_READ_KEY_EXACT);

  if (error) {
  }

  change_active_index(keynr);

  return(error);
}

/*********************************************************************//**
Stores a reference to the current row to 'ref' field of the handle. Note
that in the case where we have generated the clustered index for the
table, the function parameter is illogical: we MUST ASSUME that 'record'
is the current 'position' of the handle, because if row ref is actually
the row id internally generated in InnoDB, then 'record' does not contain
it. We just guess that the row id must be for the record where the handle
was positioned the last time. */
UNIV_INTERN
void
ha_innobase::position(
/*==================*/
  const unsigned char*  record) /*!< in: row in MySQL format */
{
  uint    len;

  ut_a(prebuilt->trx == session_to_trx(getTable()->in_use));

  if (prebuilt->clust_index_was_generated) {
    /* No primary key was defined for the table and we
    generated the clustered index from row id: the
    row reference will be the row id, not any key value
    that MySQL knows of */

    len = DATA_ROW_ID_LEN;

    memcpy(ref, prebuilt->row_id, len);
  } else {
    len = store_key_val_for_row(primary_key, (char*)ref,
               ref_length, record);
  }

  /* We assume that the 'ref' value len is always fixed for the same
  table. */

  if (len != ref_length) {
    errmsg_printf(error::ERROR, "Stored ref len is %lu, but table ref len is %lu",
        (ulong) len, (ulong) ref_length);
  }
}


/*****************************************************************//**
Creates a table definition to an InnoDB database. */
static
int
create_table_def(
/*=============*/
  trx_t*    trx,    /*!< in: InnoDB transaction handle */
  Table*    form,   /*!< in: information on table
          columns and indexes */
  const char* table_name, /*!< in: table name */
  const char* path_of_temp_table,/*!< in: if this is a table explicitly
          created by the user with the
          TEMPORARY keyword, then this
          parameter is the dir path where the
          table should be placed if we create
          an .ibd file for it (no .ibd extension
          in the path, though); otherwise this
          is NULL */
  ulint   flags)    /*!< in: table flags */
{
  Field*    field;
  dict_table_t* table;
  ulint   n_cols;
  int   error;
  ulint   col_type;
  ulint   col_len;
  ulint   nulls_allowed;
  ulint   unsigned_type;
  ulint   binary_type;
  ulint   long_true_varchar;
  ulint   charset_no;
  ulint   i;

  n_cols = form->getShare()->sizeFields();

  /* We pass 0 as the space id, and determine at a lower level the space
  id where to store the table */

  table = dict_mem_table_create(table_name, 0, n_cols, flags);

  if (path_of_temp_table) {
    table->dir_path_of_temp_table =
      mem_heap_strdup(table->heap, path_of_temp_table);
  }

  for (i = 0; i < n_cols; i++) {
    field = form->getField(i);

    col_type = get_innobase_type_from_mysql_type(&unsigned_type,
                  field);

    if (!col_type) {
      push_warning_printf(
                          trx->mysql_thd,
                          DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_CANT_CREATE_TABLE,
                          "Error creating table '%s' with "
                          "column '%s'. Please check its "
                          "column type and try to re-create "
                          "the table with an appropriate "
                          "column type.",
                          table->name, (char*) field->field_name);
      goto err_col;
    }

    if (field->null_ptr) {
      nulls_allowed = 0;
    } else {
      nulls_allowed = DATA_NOT_NULL;
    }

    if (field->binary()) {
      binary_type = DATA_BINARY_TYPE;
    } else {
      binary_type = 0;
    }

    charset_no = 0;

    if (dtype_is_string_type(col_type)) {

      charset_no = (ulint)field->charset()->number;

      if (UNIV_UNLIKELY(charset_no >= 256)) {
        /* in data0type.h we assume that the
        number fits in one byte in prtype */
        push_warning_printf(
          trx->mysql_thd,
          DRIZZLE_ERROR::WARN_LEVEL_ERROR,
          ER_CANT_CREATE_TABLE,
          "In InnoDB, charset-collation codes"
          " must be below 256."
          " Unsupported code %lu.",
          (ulong) charset_no);
        return(ER_CANT_CREATE_TABLE);
      }
    }

    ut_a(field->type() < 256); /* we assume in dtype_form_prtype()
             that this fits in one byte */
    col_len = field->pack_length();

    /* The MySQL pack length contains 1 or 2 bytes length field
    for a true VARCHAR. Let us subtract that, so that the InnoDB
    column length in the InnoDB data dictionary is the real
    maximum byte length of the actual data. */

    long_true_varchar = 0;

    if (field->type() == DRIZZLE_TYPE_VARCHAR) {
      col_len -= ((Field_varstring*)field)->pack_length_no_ptr();

      if (((Field_varstring*)field)->pack_length_no_ptr() == 2) {
        long_true_varchar = DATA_LONG_TRUE_VARCHAR;
      }
    }

    /* First check whether the column to be added has a
       system reserved name. */
    if (dict_col_name_is_reserved(field->field_name)){
      my_error(ER_WRONG_COLUMN_NAME, MYF(0), field->field_name);

  err_col:
      dict_mem_table_free(table);
      trx_commit_for_mysql(trx);

      error = DB_ERROR;
      goto error_ret;
    }

    dict_mem_table_add_col(table, table->heap,
      (char*) field->field_name,
      col_type,
      dtype_form_prtype(
        (ulint)field->type()
        | nulls_allowed | unsigned_type
        | binary_type | long_true_varchar,
        charset_no),
      col_len);
  }

  error = row_create_table_for_mysql(table, trx);

	if (error == DB_DUPLICATE_KEY) {
		char buf[100];
		char* buf_end = innobase_convert_identifier(
			buf, sizeof buf - 1, table_name, strlen(table_name),
			trx->mysql_thd, TRUE);

		*buf_end = '\0';
		my_error(ER_TABLE_EXISTS_ERROR, MYF(0), buf);
	}

error_ret:
  error = convert_error_code_to_mysql(error, flags, NULL);

  return(error);
}

/*****************************************************************//**
Creates an index in an InnoDB database. */
static
int
create_index(
/*=========*/
  trx_t*    trx,    /*!< in: InnoDB transaction handle */
  Table*    form,   /*!< in: information on table
          columns and indexes */
  ulint   flags,    /*!< in: InnoDB table flags */
  const char* table_name, /*!< in: table name */
  uint    key_num)  /*!< in: index number */
{
  Field*    field;
  dict_index_t* index;
  int   error;
  ulint   n_fields;
  KeyInfo*    key;
  KeyPartInfo*  key_part;
  ulint   ind_type;
  ulint   col_type;
  ulint   prefix_len;
  ulint   is_unsigned;
  ulint   i;
  ulint   j;
  ulint*    field_lengths;

  key = &form->key_info[key_num];

  n_fields = key->key_parts;

  /* Assert that "GEN_CLUST_INDEX" cannot be used as non-primary index */
  ut_a(innobase_strcasecmp(key->name, innobase_index_reserve_name) != 0);

  ind_type = 0;

  if (key_num == form->getShare()->getPrimaryKey()) {
    ind_type = ind_type | DICT_CLUSTERED;
  }

  if (key->flags & HA_NOSAME ) {
    ind_type = ind_type | DICT_UNIQUE;
  }

  /* We pass 0 as the space id, and determine at a lower level the space
  id where to store the table */

  index = dict_mem_index_create(table_name, key->name, 0,
              ind_type, n_fields);

  field_lengths = (ulint*) malloc(sizeof(ulint) * n_fields);

  for (i = 0; i < n_fields; i++) {
    key_part = key->key_part + i;

    /* (The flag HA_PART_KEY_SEG denotes in MySQL a column prefix
    field in an index: we only store a specified number of first
    bytes of the column to the index field.) The flag does not
    seem to be properly set by MySQL. Let us fall back on testing
    the length of the key part versus the column. */

    field = NULL;
    for (j = 0; j < form->getShare()->sizeFields(); j++)
    {

      field = form->getField(j);

      if (0 == innobase_strcasecmp(
          field->field_name,
          key_part->field->field_name)) {
        /* Found the corresponding column */

        break;
      }
    }

    ut_a(j < form->getShare()->sizeFields());

    col_type = get_innobase_type_from_mysql_type(
          &is_unsigned, key_part->field);

    if (DATA_BLOB == col_type
      || (key_part->length < field->pack_length()
        && field->type() != DRIZZLE_TYPE_VARCHAR)
      || (field->type() == DRIZZLE_TYPE_VARCHAR
        && key_part->length < field->pack_length()
        - ((Field_varstring*)field)->pack_length_no_ptr())) {

      prefix_len = key_part->length;

      if (col_type == DATA_INT
        || col_type == DATA_FLOAT
        || col_type == DATA_DOUBLE
        || col_type == DATA_DECIMAL) {
        errmsg_printf(error::ERROR, 
          "MySQL is trying to create a column "
          "prefix index field, on an "
          "inappropriate data type. Table "
          "name %s, column name %s.",
          table_name,
          key_part->field->field_name);

        prefix_len = 0;
      }
    } else {
      prefix_len = 0;
    }

    field_lengths[i] = key_part->length;

    dict_mem_index_add_field(index,
      (char*) key_part->field->field_name, prefix_len);
  }

  /* Even though we've defined max_supported_key_part_length, we
  still do our own checking using field_lengths to be absolutely
  sure we don't create too long indexes. */
  error = row_create_index_for_mysql(index, trx, field_lengths);

  error = convert_error_code_to_mysql(error, flags, NULL);

  free(field_lengths);

  return(error);
}

/*****************************************************************//**
Creates an index to an InnoDB table when the user has defined no
primary index. */
static
int
create_clustered_index_when_no_primary(
/*===================================*/
  trx_t*    trx,    /*!< in: InnoDB transaction handle */
  ulint   flags,    /*!< in: InnoDB table flags */
  const char* table_name) /*!< in: table name */
{
  dict_index_t* index;
  int   error;

  /* We pass 0 as the space id, and determine at a lower level the space
  id where to store the table */

  index = dict_mem_index_create(table_name,
                                innobase_index_reserve_name,
                                0, DICT_CLUSTERED, 0);

  error = row_create_index_for_mysql(index, trx, NULL);

  error = convert_error_code_to_mysql(error, flags, NULL);

  return(error);
}

/*****************************************************************//**
Validates the create options. We may build on this function
in future. For now, it checks two specifiers:
KEY_BLOCK_SIZE and ROW_FORMAT
If innodb_strict_mode is not set then this function is a no-op
@return TRUE if valid. */
#if 0
static
ibool
create_options_are_valid(
/*=====================*/
  Session*  session,  /*!< in: connection thread. */
  Table&    form,   /*!< in: information on table
          columns and indexes */
        message::Table& create_proto)
{
  ibool kbs_specified = FALSE;
  ibool ret   = TRUE;


  ut_ad(session != NULL);

  /* If innodb_strict_mode is not set don't do any validation. */
  if (!(SessionVAR(session, strict_mode))) {
    return(TRUE);
  }

  /* Now check for ROW_FORMAT specifier. */
  return(ret);
}
#endif

/*********************************************************************
Creates a new table to an InnoDB database. */
UNIV_INTERN
int
InnobaseEngine::doCreateTable(
			      /*================*/
			      Session         &session, /*!< in: Session */
			      Table&    form,   /*!< in: information on table columns and indexes */
			      const identifier::Table &identifier,
			      const message::Table& create_proto)
{
  int   error;
  dict_table_t* innobase_table;
  trx_t*    parent_trx;
  trx_t*    trx;
  int   primary_key_no;
  uint    i;
  ib_int64_t  auto_inc_value;
  ulint   iflags;
  /* Cache the value of innodb_file_format, in case it is
    modified by another thread while the table is being created. */
  const ulint file_format = srv_file_format;
  bool lex_identified_temp_table= (create_proto.type() == message::Table::TEMPORARY);
  const char* stmt;
  size_t stmt_len;

  std::string search_string(identifier.getSchemaName());
  boost::algorithm::to_lower(search_string);

  if (search_string.compare("data_dictionary") == 0)
  {
    return HA_WRONG_CREATE_OPTION;
  }

  if (form.getShare()->sizeFields() > 1000) {
    /* The limit probably should be REC_MAX_N_FIELDS - 3 = 1020,
      but we play safe here */

    return(HA_ERR_TO_BIG_ROW);
  }

  /* Get the transaction associated with the current session, or create one
    if not yet created */

  parent_trx = check_trx_exists(&session);

  /* In case MySQL calls this in the middle of a SELECT query, release
    possible adaptive hash latch to avoid deadlocks of threads */

  trx_search_latch_release_if_reserved(parent_trx);

  trx = innobase_trx_allocate(&session);

  srv_lower_case_table_names = TRUE;

  /* Latch the InnoDB data dictionary exclusively so that no deadlocks
    or lock waits can happen in it during a table create operation.
    Drop table etc. do this latching in row0mysql.c. */

  row_mysql_lock_data_dictionary(trx);

  /* Create the table definition in InnoDB */

  iflags = 0;

#if 0 // Since we validate the options before this stage, we no longer need to do this.
  /* Validate create options if innodb_strict_mode is set. */
  if (! create_options_are_valid(&session, form, create_proto)) {
    error = ER_ILLEGAL_HA_CREATE_OPTION;
    goto cleanup;
  }
#endif

  // We assume compact format by default
  iflags= DICT_TF_COMPACT;

  size_t num_engine_options= create_proto.engine().options_size();
  for (size_t x= 0; x < num_engine_options; ++x)
  {
    if (boost::iequals(create_proto.engine().options(x).name(), "ROW_FORMAT"))
    {
      if (boost::iequals(create_proto.engine().options(x).state(), "COMPRESSED"))
      {
        iflags= DICT_TF_FORMAT_ZIP;
      }
      else if (boost::iequals(create_proto.engine().options(x).state(), "COMPACT"))
      {
        iflags= DICT_TF_FORMAT_ZIP;
      }
      else if (boost::iequals(create_proto.engine().options(x).state(), "DYNAMIC"))
      {
        iflags= DICT_TF_COMPACT;
      }
      else if (boost::iequals(create_proto.engine().options(x).state(), "REDUNDANT"))
      {
        iflags= DICT_TF_COMPACT;
      }
    }
    else
    {
      assert(0); // This should never happen since we have already validated the options.
    }
  }

  if (iflags == DICT_TF_FORMAT_ZIP)
  {
    /* 
      ROW_FORMAT=COMPRESSED without KEY_BLOCK_SIZE implies half the maximum KEY_BLOCK_SIZE.
      @todo implement KEY_BLOCK_SIZE
    */
    iflags= (DICT_TF_ZSSIZE_MAX - 1)
      << DICT_TF_ZSSIZE_SHIFT
      | DICT_TF_COMPACT
      | DICT_TF_FORMAT_ZIP
      << DICT_TF_FORMAT_SHIFT;
#if DICT_TF_ZSSIZE_MAX < 1
# error "DICT_TF_ZSSIZE_MAX < 1"
#endif

    if (strict_mode)
    {
      if (! srv_file_per_table)
      {
        push_warning_printf(
                            &session,
                            DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            "InnoDB: ROW_FORMAT=COMPRESSED requires innodb_file_per_table.");
      } 
      else if (file_format < DICT_TF_FORMAT_ZIP) 
      {
        push_warning_printf(
                            &session,
                            DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_ILLEGAL_HA_CREATE_OPTION,
                            "InnoDB: ROW_FORMAT=compressed requires innodb_file_format > Antelope.");
      }
    }
  }

  /* Look for a primary key */

  primary_key_no= (form.getShare()->hasPrimaryKey() ?
                   (int) form.getShare()->getPrimaryKey() :
                   -1);

  /* Our function innobase_get_mysql_key_number_for_index assumes
    the primary key is always number 0, if it exists */

  assert(primary_key_no == -1 || primary_key_no == 0);

  /* Check for name conflicts (with reserved name) for
     any user indices to be created. */
  if (innobase_index_name_is_reserved(trx, form.key_info,
                                      form.getShare()->keys)) {
    error = -1;
    goto cleanup;
  }

  if (lex_identified_temp_table)
    iflags |= DICT_TF2_TEMPORARY << DICT_TF2_SHIFT;

  error= create_table_def(trx, &form, identifier.getKeyPath().c_str(),
                          lex_identified_temp_table ? identifier.getKeyPath().c_str() : NULL,
                          iflags);

  session.setXaId(trx->id);

  if (error) {
    goto cleanup;
  }

  /* Create the keys */

  if (form.getShare()->sizeKeys() == 0 || primary_key_no == -1) {
    /* Create an index which is used as the clustered index;
      order the rows by their row id which is internally generated
      by InnoDB */

    error = create_clustered_index_when_no_primary(trx, iflags, identifier.getKeyPath().c_str());
    if (error) {
      goto cleanup;
    }
  }

  if (primary_key_no != -1) {
    /* In InnoDB the clustered index must always be created first */
    if ((error = create_index(trx, &form, iflags, identifier.getKeyPath().c_str(),
                              (uint) primary_key_no))) {
      goto cleanup;
    }
  }

  for (i = 0; i < form.getShare()->sizeKeys(); i++) {
    if (i != (uint) primary_key_no) {

      if ((error = create_index(trx, &form, iflags, identifier.getKeyPath().c_str(),
                                i))) {
        goto cleanup;
      }
    }
  }

  stmt= session.getQueryStringCopy(stmt_len);

  if (stmt) {
    string generated_create_table;
    const char *query= stmt;

    if (session.getSqlCommand() == SQLCOM_CREATE_TABLE)
    {
      message::transformTableDefinitionToSql(create_proto,
                                             generated_create_table,
                                             message::DRIZZLE, true);
      query= generated_create_table.c_str();
    }

    error = row_table_add_foreign_constraints(trx,
                                              query, strlen(query),
                                              identifier.getKeyPath().c_str(),
                                              lex_identified_temp_table);
    switch (error) {

    case DB_PARENT_NO_INDEX:
      push_warning_printf(
                          &session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          HA_ERR_CANNOT_ADD_FOREIGN,
                          "Create table '%s' with foreign key constraint"
                          " failed. There is no index in the referenced"
                          " table where the referenced columns appear"
                          " as the first columns.\n", identifier.getKeyPath().c_str());
      break;

    case DB_CHILD_NO_INDEX:
      push_warning_printf(
                          &session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          HA_ERR_CANNOT_ADD_FOREIGN,
                          "Create table '%s' with foreign key constraint"
                          " failed. There is no index in the referencing"
                          " table where referencing columns appear"
                          " as the first columns.\n", identifier.getKeyPath().c_str());
      break;
    }

    error = convert_error_code_to_mysql(error, iflags, NULL);

    if (error) {
      goto cleanup;
    }
  }

  innobase_commit_low(trx);

  row_mysql_unlock_data_dictionary(trx);

  /* Flush the log to reduce probability that the .frm files and
    the InnoDB data dictionary get out-of-sync if the user runs
    with innodb_flush_log_at_trx_commit = 0 */

  log_buffer_flush_to_disk();

  innobase_table = dict_table_get(identifier.getKeyPath().c_str(), FALSE);

  assert(innobase_table != 0);

  if (innobase_table) {
    /* We update the highest file format in the system table
      space, if this table has higher file format setting. */

    char changed_file_format_max[100];
    strcpy(changed_file_format_max, innobase_file_format_max.c_str());
    trx_sys_file_format_max_upgrade((const char **)&changed_file_format_max,
      dict_table_get_format(innobase_table));
    innobase_file_format_max= changed_file_format_max;
  }

  /* Note: We can't call update_session() as prebuilt will not be
    setup at this stage and so we use session. */

  /* We need to copy the AUTOINC value from the old table if
    this is an ALTER TABLE or CREATE INDEX because CREATE INDEX
    does a table copy too. */

  if ((create_proto.options().has_auto_increment_value()
       || session.getSqlCommand() == SQLCOM_ALTER_TABLE
       || session.getSqlCommand() == SQLCOM_CREATE_INDEX)
      && create_proto.options().auto_increment_value() != 0) {

    /* Query was one of :
       CREATE TABLE ...AUTO_INCREMENT = x; or
       ALTER TABLE...AUTO_INCREMENT = x;   or
       CREATE INDEX x on t(...);
       Find out a table definition from the dictionary and get
       the current value of the auto increment field. Set a new
       value to the auto increment field if the value is greater
       than the maximum value in the column. */

    auto_inc_value = create_proto.options().auto_increment_value();

    dict_table_autoinc_lock(innobase_table);
    dict_table_autoinc_initialize(innobase_table, auto_inc_value);
    dict_table_autoinc_unlock(innobase_table);
  }

  /* Tell the InnoDB server that there might be work for
    utility threads: */

  srv_active_wake_master_thread();

  trx_free_for_mysql(trx);

  if (lex_identified_temp_table)
  {
    session.getMessageCache().storeTableMessage(identifier, create_proto);
  }
  else
  {
    StorageEngine::writeDefinitionFromPath(identifier, create_proto);
  }

  return(0);

cleanup:
  innobase_commit_low(trx);

  row_mysql_unlock_data_dictionary(trx);

  trx_free_for_mysql(trx);

  return(error);
}

/*****************************************************************//**
Discards or imports an InnoDB tablespace.
@return 0 == success, -1 == error */
UNIV_INTERN
int
ha_innobase::discard_or_import_tablespace(
/*======================================*/
  my_bool discard)  /*!< in: TRUE if discard, else import */
{
  dict_table_t* dict_table;
  trx_t*    trx;
  int   err;

  ut_a(prebuilt->trx);
  ut_a(prebuilt->trx->magic_n == TRX_MAGIC_N);
  ut_a(prebuilt->trx == session_to_trx(getTable()->in_use));

  dict_table = prebuilt->table;
  trx = prebuilt->trx;

  if (discard) {
    err = row_discard_tablespace_for_mysql(dict_table->name, trx);
  } else {
    err = row_import_tablespace_for_mysql(dict_table->name, trx);
  }

  err = convert_error_code_to_mysql(err, dict_table->flags, NULL);

  return(err);
}

/*****************************************************************//**
Deletes all rows of an InnoDB table.
@return error number */
UNIV_INTERN
int
ha_innobase::delete_all_rows(void)
/*==============================*/
{
  int   error;

  /* Get the transaction associated with the current session, or create one
  if not yet created, and update prebuilt->trx */

  update_session(getTable()->in_use);

  if (user_session->getSqlCommand() != SQLCOM_TRUNCATE) {
  fallback:
    /* We only handle TRUNCATE TABLE t as a special case.
    DELETE FROM t will have to use ha_innobase::doDeleteRecord(),
    because DELETE is transactional while TRUNCATE is not. */
    return(errno=HA_ERR_WRONG_COMMAND);
  }

  /* Truncate the table in InnoDB */

  error = row_truncate_table_for_mysql(prebuilt->table, prebuilt->trx);
  if (error == DB_ERROR) {
    /* Cannot truncate; resort to ha_innobase::doDeleteRecord() */
    goto fallback;
  }

  error = convert_error_code_to_mysql(error, prebuilt->table->flags,
              NULL);

  return(error);
}

/*****************************************************************//**
Drops a table from an InnoDB database. Before calling this function,
MySQL calls innobase_commit to commit the transaction of the current user.
Then the current user cannot have locks set on the table. Drop table
operation inside InnoDB will remove all locks any user has on the table
inside InnoDB.
@return error number */
UNIV_INTERN
int
InnobaseEngine::doDropTable(
/*======================*/
        Session &session,
        const identifier::Table &identifier)
{
  int error;
  trx_t*  parent_trx;
  trx_t*  trx;

  ut_a(identifier.getPath().length() < 1000);

  std::string search_string(identifier.getSchemaName());
  boost::algorithm::to_lower(search_string);

  if (search_string.compare("data_dictionary") == 0)
  {
    return HA_ERR_TABLE_READONLY;
  }

  /* Get the transaction associated with the current session, or create one
    if not yet created */

  parent_trx = check_trx_exists(&session);

  /* In case MySQL calls this in the middle of a SELECT query, release
    possible adaptive hash latch to avoid deadlocks of threads */

  trx_search_latch_release_if_reserved(parent_trx);

  trx = innobase_trx_allocate(&session);

  srv_lower_case_table_names = TRUE;

  /* Drop the table in InnoDB */

  error = row_drop_table_for_mysql(identifier.getKeyPath().c_str(), trx,
                                   session.getSqlCommand()
                                   == SQLCOM_DROP_DB);

  session.setXaId(trx->id);

  /* Flush the log to reduce probability that the .frm files and
    the InnoDB data dictionary get out-of-sync if the user runs
    with innodb_flush_log_at_trx_commit = 0 */

  log_buffer_flush_to_disk();

  /* Tell the InnoDB server that there might be work for
    utility threads: */

  srv_active_wake_master_thread();

  innobase_commit_low(trx);

  trx_free_for_mysql(trx);

  if (error != ENOENT)
    error = convert_error_code_to_mysql(error, 0, NULL);

  if (error == 0 || error == ENOENT)
  {
    if (identifier.getType() == message::Table::TEMPORARY)
    {
      session.getMessageCache().removeTableMessage(identifier);
      ulint sql_command = session.getSqlCommand();

      // If this was the final removal to an alter table then we will need
      // to remove the .dfe that was left behind.
      if ((sql_command == SQLCOM_ALTER_TABLE
       || sql_command == SQLCOM_CREATE_INDEX
       || sql_command == SQLCOM_DROP_INDEX))
      {
        string path(identifier.getPath());

        path.append(DEFAULT_FILE_EXTENSION);

        (void)internal::my_delete(path.c_str(), MYF(0));
      }
    }
    else
    {
      string path(identifier.getPath());

      path.append(DEFAULT_FILE_EXTENSION);

      (void)internal::my_delete(path.c_str(), MYF(0));
    }
  }

  return(error);
}

/*****************************************************************//**
Removes all tables in the named database inside InnoDB. */
bool
InnobaseEngine::doDropSchema(
/*===================*/
                             const identifier::Schema &identifier)
    /*!< in: database path; inside InnoDB the name
      of the last directory in the path is used as
      the database name: for example, in 'mysql/data/test'
      the database name is 'test' */
{
  trx_t*  trx;
  int error;
  string schema_path(identifier.getPath());
  Session*  session   = current_session;

  /* Get the transaction associated with the current session, or create one
    if not yet created */

  assert(this == innodb_engine_ptr);

  /* In the Windows plugin, session = current_session is always NULL */
  if (session) {
    trx_t*  parent_trx = check_trx_exists(session);

    /* In case Drizzle calls this in the middle of a SELECT
      query, release possible adaptive hash latch to avoid
      deadlocks of threads */

    trx_search_latch_release_if_reserved(parent_trx);
  }

  schema_path.append("/");
  trx = innobase_trx_allocate(session);
  error = row_drop_database_for_mysql(schema_path.c_str(), trx);

  /* Flush the log to reduce probability that the .frm files and
    the InnoDB data dictionary get out-of-sync if the user runs
    with innodb_flush_log_at_trx_commit = 0 */

  log_buffer_flush_to_disk();

  /* Tell the InnoDB server that there might be work for
    utility threads: */

  srv_active_wake_master_thread();

  innobase_commit_low(trx);
  trx_free_for_mysql(trx);

  if (error) {
    // What do we do here?
  }

  return false; // We are just a listener since we lack control over DDL, so we give no positive acknowledgement. 
}

void InnobaseEngine::dropTemporarySchema()
{
  identifier::Schema schema_identifier(GLOBAL_TEMPORARY_EXT);
  trx_t*  trx= NULL;
  string schema_path(GLOBAL_TEMPORARY_EXT);

  schema_path.append("/");

  trx = trx_allocate_for_mysql();

  trx->mysql_thd = NULL;

  trx->check_foreigns = false;
  trx->check_unique_secondary = false;

  (void)row_drop_database_for_mysql(schema_path.c_str(), trx);

  /* Flush the log to reduce probability that the .frm files and
    the InnoDB data dictionary get out-of-sync if the user runs
    with innodb_flush_log_at_trx_commit = 0 */

  log_buffer_flush_to_disk();

  /* Tell the InnoDB server that there might be work for
    utility threads: */

  srv_active_wake_master_thread();

  innobase_commit_low(trx);
  trx_free_for_mysql(trx);
}
/*********************************************************************//**
Renames an InnoDB table.
@return 0 or error code */
static
int
innobase_rename_table(
/*==================*/
  trx_t*    trx,  /*!< in: transaction */
  const identifier::Table &from,
  const identifier::Table &to,
  ibool   lock_and_commit)
        /*!< in: TRUE=lock data dictionary and commit */
{
  int error;

  srv_lower_case_table_names = TRUE;

  /* Serialize data dictionary operations with dictionary mutex:
  no deadlocks can occur then in these operations */

  if (lock_and_commit) {
    row_mysql_lock_data_dictionary(trx);
  }

  error = row_rename_table_for_mysql(from.getKeyPath().c_str(), to.getKeyPath().c_str(), trx, lock_and_commit);

  if (error != DB_SUCCESS) {
    FILE* ef = dict_foreign_err_file;

    fputs("InnoDB: Renaming table ", ef);
    ut_print_name(ef, trx, TRUE, from.getKeyPath().c_str());
    fputs(" to ", ef);
    ut_print_name(ef, trx, TRUE, to.getKeyPath().c_str());
    fputs(" failed!\n", ef);
  }

  if (lock_and_commit) {
    row_mysql_unlock_data_dictionary(trx);

    /* Flush the log to reduce probability that the .frm
    files and the InnoDB data dictionary get out-of-sync
    if the user runs with innodb_flush_log_at_trx_commit = 0 */

    log_buffer_flush_to_disk();
  }

  return error;
}
/*********************************************************************//**
Renames an InnoDB table.
@return 0 or error code */
UNIV_INTERN int InnobaseEngine::doRenameTable(Session &session, const identifier::Table &from, const identifier::Table &to)
{
  // A temp table alter table/rename is a shallow rename and only the
  // definition needs to be updated.
  if (to.getType() == message::Table::TEMPORARY && from.getType() == message::Table::TEMPORARY)
  {
    session.getMessageCache().renameTableMessage(from, to);
    return 0;
  }

  trx_t*  trx;
  int error;
  trx_t*  parent_trx;

  /* Get the transaction associated with the current session, or create one
    if not yet created */

  parent_trx = check_trx_exists(&session);

  /* In case MySQL calls this in the middle of a SELECT query, release
    possible adaptive hash latch to avoid deadlocks of threads */

  trx_search_latch_release_if_reserved(parent_trx);

  trx = innobase_trx_allocate(&session);

  error = innobase_rename_table(trx, from, to, TRUE);

  session.setXaId(trx->id);

  /* Tell the InnoDB server that there might be work for
    utility threads: */

  srv_active_wake_master_thread();

  innobase_commit_low(trx);
  trx_free_for_mysql(trx);

  /* Add a special case to handle the Duplicated Key error
     and return DB_ERROR instead.
     This is to avoid a possible SIGSEGV error from mysql error
     handling code. Currently, mysql handles the Duplicated Key
     error by re-entering the storage layer and getting dup key
     info by calling get_dup_key(). This operation requires a valid
     table handle ('row_prebuilt_t' structure) which could no
     longer be available in the error handling stage. The suggested
     solution is to report a 'table exists' error message (since
     the dup key error here is due to an existing table whose name
     is the one we are trying to rename to) and return the generic
     error code. */
  if (error == (int) DB_DUPLICATE_KEY) {
    my_error(ER_TABLE_EXISTS_ERROR, to);
    error = DB_ERROR;
  }

  error = convert_error_code_to_mysql(error, 0, NULL);

  if (not error)
  {
    // If this fails, we are in trouble
    plugin::StorageEngine::renameDefinitionFromPath(to, from);
  }

  return(error);
}

/*********************************************************************//**
Estimates the number of index records in a range.
@return estimated number of rows */
UNIV_INTERN
ha_rows
ha_innobase::records_in_range(
/*==========================*/
  uint      keynr,    /*!< in: index number */
  key_range   *min_key, /*!< in: start key value of the
               range, may also be 0 */
  key_range   *max_key) /*!< in: range end key val, may
               also be 0 */
{
  KeyInfo*    key;
  dict_index_t* index;
  unsigned char*    key_val_buff2 = (unsigned char*) malloc(
              getTable()->getShare()->sizeStoredRecord()
          + getTable()->getShare()->max_key_length + 100);
  ulint   buff2_len = getTable()->getShare()->sizeStoredRecord()
          + getTable()->getShare()->max_key_length + 100;
  dtuple_t* range_start;
  dtuple_t* range_end;
  ib_int64_t  n_rows;
  ulint   mode1;
  ulint   mode2;
  mem_heap_t* heap;

  ut_a(prebuilt->trx == session_to_trx(getTable()->in_use));

  prebuilt->trx->op_info = (char*)"estimating records in index range";

  /* In case MySQL calls this in the middle of a SELECT query, release
  possible adaptive hash latch to avoid deadlocks of threads */

  trx_search_latch_release_if_reserved(prebuilt->trx);

  active_index = keynr;

  key = &getTable()->key_info[active_index];

  index = innobase_get_index(keynr);

  /* There exists possibility of not being able to find requested
     index due to inconsistency between MySQL and InoDB dictionary info.
     Necessary message should have been printed in innobase_get_index() */
  if (UNIV_UNLIKELY(!index)) {
    n_rows = HA_POS_ERROR;
    goto func_exit;
  }

  if (UNIV_UNLIKELY(!row_merge_is_index_usable(prebuilt->trx, index))) {
    n_rows = HA_ERR_TABLE_DEF_CHANGED;
    goto func_exit;
  }

  heap = mem_heap_create(2 * (key->key_parts * sizeof(dfield_t)
            + sizeof(dtuple_t)));

  range_start = dtuple_create(heap, key->key_parts);
  dict_index_copy_types(range_start, index, key->key_parts);

  range_end = dtuple_create(heap, key->key_parts);
  dict_index_copy_types(range_end, index, key->key_parts);

  row_sel_convert_mysql_key_to_innobase(
        range_start, (byte*) &key_val_buff[0],
        (ulint)upd_and_key_val_buff_len,
        index,
        (byte*) (min_key ? min_key->key :
           (const unsigned char*) 0),
        (ulint) (min_key ? min_key->length : 0),
        prebuilt->trx);

  row_sel_convert_mysql_key_to_innobase(
        range_end, (byte*) key_val_buff2,
        buff2_len, index,
        (byte*) (max_key ? max_key->key :
           (const unsigned char*) 0),
        (ulint) (max_key ? max_key->length : 0),
        prebuilt->trx);

  mode1 = convert_search_mode_to_innobase(min_key ? min_key->flag :
            HA_READ_KEY_EXACT);
  mode2 = convert_search_mode_to_innobase(max_key ? max_key->flag :
            HA_READ_KEY_EXACT);

  if (mode1 != PAGE_CUR_UNSUPP && mode2 != PAGE_CUR_UNSUPP) {

    n_rows = btr_estimate_n_rows_in_range(index, range_start,
                  mode1, range_end,
                  mode2);
  } else {

    n_rows = HA_POS_ERROR;
  }

  mem_heap_free(heap);

func_exit:
  free(key_val_buff2);

  prebuilt->trx->op_info = (char*)"";

  /* The MySQL optimizer seems to believe an estimate of 0 rows is
  always accurate and may return the result 'Empty set' based on that.
  The accuracy is not guaranteed, and even if it were, for a locking
  read we should anyway perform the search to set the next-key lock.
  Add 1 to the value to make sure MySQL does not make the assumption! */

  if (n_rows == 0) {
    n_rows = 1;
  }

  return((ha_rows) n_rows);
}

/*********************************************************************//**
Gives an UPPER BOUND to the number of rows in a table. This is used in
filesort.cc.
@return upper bound of rows */
UNIV_INTERN
ha_rows
ha_innobase::estimate_rows_upper_bound(void)
/*======================================*/
{
  dict_index_t* index;
  uint64_t  estimate;
  uint64_t  local_data_file_length;
  ulint stat_n_leaf_pages;

  /* We do not know if MySQL can call this function before calling
  external_lock(). To be safe, update the session of the current table
  handle. */

  update_session(getTable()->in_use);

  prebuilt->trx->op_info = (char*)
         "calculating upper bound for table rows";

  /* In case MySQL calls this in the middle of a SELECT query, release
  possible adaptive hash latch to avoid deadlocks of threads */

  trx_search_latch_release_if_reserved(prebuilt->trx);

  index = dict_table_get_first_index(prebuilt->table);

  stat_n_leaf_pages = index->stat_n_leaf_pages;

  ut_a(stat_n_leaf_pages > 0);

  local_data_file_length =
    ((uint64_t) stat_n_leaf_pages) * UNIV_PAGE_SIZE;


  /* Calculate a minimum length for a clustered index record and from
  that an upper bound for the number of rows. Since we only calculate
  new statistics in row0mysql.c when a table has grown by a threshold
  factor, we must add a safety factor 2 in front of the formula below. */

  estimate = 2 * local_data_file_length /
           dict_index_calc_min_rec_len(index);

  prebuilt->trx->op_info = (char*)"";

  return((ha_rows) estimate);
}

/*********************************************************************//**
How many seeks it will take to read through the table. This is to be
comparable to the number returned by records_in_range so that we can
decide if we should scan the table or use keys.
@return estimated time measured in disk seeks */
UNIV_INTERN
double
ha_innobase::scan_time()
/*====================*/
{
  /* Since MySQL seems to favor table scans too much over index
  searches, we pretend that a sequential read takes the same time
  as a random disk read, that is, we do not divide the following
  by 10, which would be physically realistic. */

  return((double) (prebuilt->table->stat_clustered_index_size));
}

/******************************************************************//**
Calculate the time it takes to read a set of ranges through an index
This enables us to optimise reads for clustered indexes.
@return estimated time measured in disk seeks */
UNIV_INTERN
double
ha_innobase::read_time(
/*===================*/
  uint  index,  /*!< in: key number */
  uint  ranges, /*!< in: how many ranges */
  ha_rows rows) /*!< in: estimated number of rows in the ranges */
{
  ha_rows total_rows;
  double  time_for_scan;

  if (index != getTable()->getShare()->getPrimaryKey()) {
    /* Not clustered */
    return(Cursor::read_time(index, ranges, rows));
  }

  if (rows <= 2) {

    return((double) rows);
  }

  /* Assume that the read time is proportional to the scan time for all
  rows + at most one seek per range. */

  time_for_scan = scan_time();

  if ((total_rows = estimate_rows_upper_bound()) < rows) {

    return(time_for_scan);
  }

  return(ranges + (double) rows / (double) total_rows * time_for_scan);
}

/*********************************************************************//**
Calculates the key number used inside MySQL for an Innobase index. We will
first check the "index translation table" for a match of the index to get
the index number. If there does not exist an "index translation table",
or not able to find the index in the translation table, then we will fall back
to the traditional way of looping through dict_index_t list to find a
match. In this case, we have to take into account if we generated a
default clustered index for the table
@return the key number used inside MySQL */
static
unsigned int
innobase_get_mysql_key_number_for_index(
/*====================================*/
	INNOBASE_SHARE*		share,	/*!< in: share structure for index
					translation table. */
	const drizzled::Table*	table,	/*!< in: table in MySQL data
					dictionary */
	dict_table_t*		ib_table,/*!< in: table in Innodb data
					dictionary */
        const dict_index_t*     index)	/*!< in: index */
{
	const dict_index_t*	ind;
	unsigned int		i;

	ut_ad(index);
	ut_ad(ib_table);
	ut_ad(table);
	ut_ad(share);

	/* If index does not belong to the table of share structure. Search
	index->table instead */
	if (index->table != ib_table) {
		i = 0;
		ind = dict_table_get_first_index(index->table);

		while (index != ind) {
			ind = dict_table_get_next_index(ind);
			i++;
		}

		if (row_table_got_default_clust_index(index->table)) {
			ut_a(i > 0);
			i--;
		}

		return(i);
	}

	/* If index does not belong to the table of share structure. Search
	index->table instead */
	if (index->table != ib_table) {
		i = 0;
		ind = dict_table_get_first_index(index->table);

		while (index != ind) {
			ind = dict_table_get_next_index(ind);
			i++;
		}

		if (row_table_got_default_clust_index(index->table)) {
			ut_a(i > 0);
			i--;
		}

		return(i);
	}

	/* If index translation table exists, we will first check
	the index through index translation table for a match. */
        if (share->idx_trans_tbl.index_mapping) {
		for (i = 0; i < share->idx_trans_tbl.index_count; i++) {
			if (share->idx_trans_tbl.index_mapping[i] == index) {
				return(i);
			}
		}

		/* Print an error message if we cannot find the index
		** in the "index translation table". */
		errmsg_printf(error::ERROR,
                              "Cannot find index %s in InnoDB index "
				"translation table.", index->name);
	}

	/* If we do not have an "index translation table", or not able
	to find the index in the translation table, we'll directly find
	matching index in the dict_index_t list */
	for (i = 0; i < table->getShare()->keys; i++) {
		ind = dict_table_get_index_on_name(
			ib_table, table->key_info[i].name);

        	if (index == ind) {
			return(i);
		}
        }

		errmsg_printf(error::ERROR,
                              "Cannot find matching index number for index %s "
                              "in InnoDB index list.", index->name);

        return(0);
}
/*********************************************************************//**
Returns statistics information of the table to the MySQL interpreter,
in various fields of the handle object. */
UNIV_INTERN
int
ha_innobase::info(
/*==============*/
  uint flag)  /*!< in: what information MySQL requests */
{
  dict_table_t* ib_table;
  dict_index_t* index;
  ha_rows   rec_per_key;
  ib_int64_t  n_rows;
  os_file_stat_t  stat_info;

  /* If we are forcing recovery at a high level, we will suppress
  statistics calculation on tables, because that may crash the
  server if an index is badly corrupted. */

  /* We do not know if MySQL can call this function before calling
  external_lock(). To be safe, update the session of the current table
  handle. */

  update_session(getTable()->in_use);

  /* In case MySQL calls this in the middle of a SELECT query, release
  possible adaptive hash latch to avoid deadlocks of threads */

  prebuilt->trx->op_info = (char*)"returning various info to MySQL";

  trx_search_latch_release_if_reserved(prebuilt->trx);

  ib_table = prebuilt->table;

  if (flag & HA_STATUS_TIME) {
    /* In Analyze we call with this flag: update
       then statistics so that they are up-to-date */

    prebuilt->trx->op_info = "updating table statistics";

    dict_update_statistics(ib_table,
                           FALSE /* update even if stats
                                    are initialized */);


    prebuilt->trx->op_info = "returning various info to MySQL";

    fs::path get_status_path(getDataHomeCatalog());
    get_status_path /= ib_table->name;
    fs::change_extension(get_status_path, "dfe");

    /* Note that we do not know the access time of the table,
    nor the CHECK TABLE time, nor the UPDATE or INSERT time. */

    if (os_file_get_status(get_status_path.file_string().c_str(), &stat_info)) {
      stats.create_time = (ulong) stat_info.ctime;
    }
  }

  if (flag & HA_STATUS_VARIABLE) {

    dict_table_stats_lock(ib_table, RW_S_LATCH);

    n_rows = ib_table->stat_n_rows;

    /* Because we do not protect stat_n_rows by any mutex in a
    delete, it is theoretically possible that the value can be
    smaller than zero! TODO: fix this race.

    The MySQL optimizer seems to assume in a left join that n_rows
    is an accurate estimate if it is zero. Of course, it is not,
    since we do not have any locks on the rows yet at this phase.
    Since SHOW TABLE STATUS seems to call this function with the
    HA_STATUS_TIME flag set, while the left join optimizer does not
    set that flag, we add one to a zero value if the flag is not
    set. That way SHOW TABLE STATUS will show the best estimate,
    while the optimizer never sees the table empty. */

    if (n_rows < 0) {
      n_rows = 0;
    }

    if (n_rows == 0 && !(flag & HA_STATUS_TIME)) {
      n_rows++;
    }

    /* Fix bug#40386: Not flushing query cache after truncate.
    n_rows can not be 0 unless the table is empty, set to 1
    instead. The original problem of bug#29507 is actually
    fixed in the server code. */
    if (user_session->getSqlCommand() == SQLCOM_TRUNCATE) {

      n_rows = 1;

      /* We need to reset the prebuilt value too, otherwise
      checks for values greater than the last value written
      to the table will fail and the autoinc counter will
      not be updated. This will force doInsertRecord() into
      attempting an update of the table's AUTOINC counter. */

      prebuilt->autoinc_last_value = 0;
    }

    stats.records = (ha_rows)n_rows;
    stats.deleted = 0;
    stats.data_file_length = ((uint64_t)
        ib_table->stat_clustered_index_size)
          * UNIV_PAGE_SIZE;
    stats.index_file_length = ((uint64_t)
        ib_table->stat_sum_of_other_index_sizes)
          * UNIV_PAGE_SIZE;

    dict_table_stats_unlock(ib_table, RW_S_LATCH);

    /* Since fsp_get_available_space_in_free_extents() is
    acquiring latches inside InnoDB, we do not call it if we
    are asked by MySQL to avoid locking. Another reason to
    avoid the call is that it uses quite a lot of CPU.
    See Bug#38185. */
    if (flag & HA_STATUS_NO_LOCK) {
      /* We do not update delete_length if no
         locking is requested so the "old" value can
         remain. delete_length is initialized to 0 in
         the ha_statistics' constructor. */
    } else if (UNIV_UNLIKELY
               (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE)) {
      /* Avoid accessing the tablespace if
         innodb_crash_recovery is set to a high value. */
      stats.delete_length = 0;
    } else {
      ullint	avail_space;

      avail_space = fsp_get_available_space_in_free_extents(ib_table->space);

      if (avail_space == ULLINT_UNDEFINED) {
        Session*  session;

        session= getTable()->in_use;
        assert(session);

        push_warning_printf(
          session,
          DRIZZLE_ERROR::WARN_LEVEL_WARN,
          ER_CANT_GET_STAT,
          "InnoDB: Trying to get the free "
          "space for table %s but its "
          "tablespace has been discarded or "
          "the .ibd file is missing. Setting "
          "the free space to zero.",
          ib_table->name);

        stats.delete_length = 0;
      } else {
        stats.delete_length = avail_space * 1024;
      }
    }

    stats.check_time = 0;

    if (stats.records == 0) {
      stats.mean_rec_length = 0;
    } else {
      stats.mean_rec_length = (ulong) (stats.data_file_length / stats.records);
    }
  }

  if (flag & HA_STATUS_CONST) {
    ulong i;
    /* Verify the number of index in InnoDB and MySQL
       matches up. If prebuilt->clust_index_was_generated
       holds, InnoDB defines GEN_CLUST_INDEX internally */
    ulint	num_innodb_index = UT_LIST_GET_LEN(ib_table->indexes) - prebuilt->clust_index_was_generated;

    if (getTable()->getShare()->keys != num_innodb_index) {
      errmsg_printf(error::ERROR, "Table %s contains %lu "
                      "indexes inside InnoDB, which "
                      "is different from the number of "
                      "indexes %u defined in the MySQL ",
                      ib_table->name, num_innodb_index,
                      getTable()->getShare()->keys);
    }

    dict_table_stats_lock(ib_table, RW_S_LATCH);

    for (i = 0; i < getTable()->getShare()->sizeKeys(); i++) {
      ulong j;
      /* We could get index quickly through internal
         index mapping with the index translation table.
         The identity of index (match up index name with
         that of table->key_info[i]) is already verified in
         innobase_get_index().  */
      index = innobase_get_index(i);

      if (index == NULL) {
        errmsg_printf(error::ERROR, "Table %s contains fewer "
            "indexes inside InnoDB than "
            "are defined in the MySQL "
            ".frm file. Have you mixed up "
            ".frm files from different "
            "installations? See "
            REFMAN
            "innodb-troubleshooting.html\n",
            ib_table->name);
        break;
      }

      for (j = 0; j < getTable()->key_info[i].key_parts; j++) {

        if (j + 1 > index->n_uniq) {
          errmsg_printf(error::ERROR, 
"Index %s of %s has %lu columns unique inside InnoDB, but MySQL is asking "
"statistics for %lu columns. Have you mixed up .frm files from different "
"installations? "
"See " REFMAN "innodb-troubleshooting.html\n",
              index->name,
              ib_table->name,
              (unsigned long)
              index->n_uniq, j + 1);
          break;
        }

        if (index->stat_n_diff_key_vals[j + 1] == 0) {

          rec_per_key = stats.records;
        } else {
          rec_per_key = (ha_rows)(stats.records /
           index->stat_n_diff_key_vals[j + 1]);
        }

        /* Since MySQL seems to favor table scans
        too much over index searches, we pretend
        index selectivity is 2 times better than
        our estimate: */

        rec_per_key = rec_per_key / 2;

        if (rec_per_key == 0) {
          rec_per_key = 1;
        }

        getTable()->key_info[i].rec_per_key[j]=
          rec_per_key >= ~(ulong) 0 ? ~(ulong) 0 :
          (ulong) rec_per_key;
      }
    }

    dict_table_stats_unlock(ib_table, RW_S_LATCH);
  }

  if (srv_force_recovery >= SRV_FORCE_NO_IBUF_MERGE) {
    goto func_exit;
  }

  if (flag & HA_STATUS_ERRKEY) {
    const dict_index_t* err_index;

    ut_a(prebuilt->trx);
    ut_a(prebuilt->trx->magic_n == TRX_MAGIC_N);

    err_index = trx_get_error_info(prebuilt->trx);

    if (err_index) {
      errkey = (unsigned int)
        innobase_get_mysql_key_number_for_index(share, getTable(), ib_table,
                                                err_index);
    } else {
      errkey = (unsigned int) prebuilt->trx->error_key_num;
    }
  }

  if ((flag & HA_STATUS_AUTO) && getTable()->found_next_number_field) {
    stats.auto_increment_value = innobase_peek_autoinc();
  }

func_exit:
  prebuilt->trx->op_info = (char*)"";

  return(0);
}

/**********************************************************************//**
Updates index cardinalities of the table, based on 8 random dives into
each index tree. This does NOT calculate exact statistics on the table.
@return returns always 0 (success) */
UNIV_INTERN
int
ha_innobase::analyze(
/*=================*/
  Session*)   /*!< in: connection thread handle */
{
  /* Simply call ::info() with all the flags */
  info(HA_STATUS_TIME | HA_STATUS_CONST | HA_STATUS_VARIABLE);

  return(0);
}

/*******************************************************************//**
Tries to check that an InnoDB table is not corrupted. If corruption is
noticed, prints to stderr information about it. In case of corruption
may also assert a failure and crash the server.
@return HA_ADMIN_CORRUPT or HA_ADMIN_OK */
UNIV_INTERN
int
ha_innobase::check(
/*===============*/
  Session*  session)  /*!< in: user thread handle */
{
  dict_index_t*	index;
  ulint		n_rows;
  ulint		n_rows_in_table	= ULINT_UNDEFINED;
  ibool		is_ok		= TRUE;
  ulint		old_isolation_level;

  assert(session == getTable()->in_use);
  ut_a(prebuilt->trx);
  ut_a(prebuilt->trx->magic_n == TRX_MAGIC_N);
  ut_a(prebuilt->trx == session_to_trx(session));

  if (prebuilt->mysql_template == NULL) {
    /* Build the template; we will use a dummy template
    in index scans done in checking */

    build_template(prebuilt, NULL, getTable(), ROW_MYSQL_WHOLE_ROW);
  }

  if (prebuilt->table->ibd_file_missing) {
        errmsg_printf(error::ERROR, "InnoDB: Error:\n"
                    "InnoDB: MySQL is trying to use a table handle"
                    " but the .ibd file for\n"
                    "InnoDB: table %s does not exist.\n"
                    "InnoDB: Have you deleted the .ibd file"
                    " from the database directory under\n"
                    "InnoDB: the MySQL datadir, or have you"
                    " used DISCARD TABLESPACE?\n"
                    "InnoDB: Please refer to\n"
                    "InnoDB: " REFMAN "innodb-troubleshooting.html\n"
                    "InnoDB: how you can resolve the problem.\n",
                    prebuilt->table->name);
    return(HA_ADMIN_CORRUPT);
  }

  prebuilt->trx->op_info = "checking table";

  old_isolation_level = prebuilt->trx->isolation_level;

  /* We must run the index record counts at an isolation level
     >= READ COMMITTED, because a dirty read can see a wrong number
     of records in some index; to play safe, we use always
     REPEATABLE READ here */

  prebuilt->trx->isolation_level = TRX_ISO_REPEATABLE_READ;

  /* Enlarge the fatal lock wait timeout during CHECK TABLE. */
  mutex_enter(&kernel_mutex);
  srv_fatal_semaphore_wait_threshold += 7200; /* 2 hours */
  mutex_exit(&kernel_mutex);

  for (index = dict_table_get_first_index(prebuilt->table);
       index != NULL;
       index = dict_table_get_next_index(index)) {
#if 0
    fputs("Validating index ", stderr);
    ut_print_name(stderr, trx, FALSE, index->name);
    putc('\n', stderr);
#endif

    if (!btr_validate_index(index, prebuilt->trx)) {
      is_ok = FALSE;
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_NOT_KEYFILE,
                          "InnoDB: The B-tree of"
                          " index '%-.200s' is corrupted.",
                          index->name);
      continue;
    }

    /* Instead of invoking change_active_index(), set up
       a dummy template for non-locking reads, disabling
       access to the clustered index. */
    prebuilt->index = index;

    prebuilt->index_usable = row_merge_is_index_usable(
			prebuilt->trx, prebuilt->index);

    if (UNIV_UNLIKELY(!prebuilt->index_usable)) {
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          HA_ERR_TABLE_DEF_CHANGED,
                          "InnoDB: Insufficient history for"
                          " index '%-.200s'",
                          index->name);
      continue;
    }

    prebuilt->sql_stat_start = TRUE;
    prebuilt->template_type = ROW_MYSQL_DUMMY_TEMPLATE;
    prebuilt->n_template = 0;
    prebuilt->need_to_access_clustered = FALSE;

    dtuple_set_n_fields(prebuilt->search_tuple, 0);

    prebuilt->select_lock_type = LOCK_NONE;

    if (!row_check_index_for_mysql(prebuilt, index, &n_rows)) {
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_NOT_KEYFILE,
                          "InnoDB: The B-tree of"
                          " index '%-.200s' is corrupted.",
                          index->name);
      is_ok = FALSE;
    }

    if (user_session->getKilled()) {
      break;
    }

#if 0
    fprintf(stderr, "%lu entries in index %s\n", n_rows,
            index->name);
#endif

    if (index == dict_table_get_first_index(prebuilt->table)) {
      n_rows_in_table = n_rows;
    } else if (n_rows != n_rows_in_table) {
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_NOT_KEYFILE,
                          "InnoDB: Index '%-.200s'"
                          " contains %lu entries,"
                          " should be %lu.",
                          index->name,
                          (ulong) n_rows,
                          (ulong) n_rows_in_table);
      is_ok = FALSE;
    }
  }

  /* Restore the original isolation level */
  prebuilt->trx->isolation_level = old_isolation_level;

  /* We validate also the whole adaptive hash index for all tables
     at every CHECK TABLE */

  if (!btr_search_validate()) {
    push_warning(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                 ER_NOT_KEYFILE,
                 "InnoDB: The adaptive hash index is corrupted.");
    is_ok = FALSE;
  }

  /* Restore the fatal lock wait timeout after CHECK TABLE. */
  mutex_enter(&kernel_mutex);
  srv_fatal_semaphore_wait_threshold -= 7200; /* 2 hours */
  mutex_exit(&kernel_mutex);

  prebuilt->trx->op_info = "";
  if (user_session->getKilled()) {
    my_error(ER_QUERY_INTERRUPTED, MYF(0));
  }

  return(is_ok ? HA_ADMIN_OK : HA_ADMIN_CORRUPT);
}

/*************************************************************//**
Adds information about free space in the InnoDB tablespace to a table comment
which is printed out when a user calls SHOW TABLE STATUS. Adds also info on
foreign keys.
@return table comment + InnoDB free space + info on foreign keys */
UNIV_INTERN
char*
ha_innobase::update_table_comment(
/*==============================*/
  const char* comment)/*!< in: table comment defined by user */
{
  uint  length = (uint) strlen(comment);
  char* str;
  long  flen;

  /* We do not know if MySQL can call this function before calling
  external_lock(). To be safe, update the session of the current table
  handle. */

  if (length > 64000 - 3) {
    return((char*)comment); /* string too long */
  }

  update_session(getTable()->in_use);

  prebuilt->trx->op_info = (char*)"returning table comment";

  /* In case MySQL calls this in the middle of a SELECT query, release
  possible adaptive hash latch to avoid deadlocks of threads */

  trx_search_latch_release_if_reserved(prebuilt->trx);
  str = NULL;

  /* output the data to a temporary file */

  mutex_enter(&srv_dict_tmpfile_mutex);
  rewind(srv_dict_tmpfile);

  fprintf(srv_dict_tmpfile, "InnoDB free: %llu kB",
    fsp_get_available_space_in_free_extents(
      prebuilt->table->space));

  dict_print_info_on_foreign_keys(FALSE, srv_dict_tmpfile,
        prebuilt->trx, prebuilt->table);
  flen = ftell(srv_dict_tmpfile);
  if (flen < 0) {
    flen = 0;
  } else if (length + flen + 3 > 64000) {
    flen = 64000 - 3 - length;
  }

  /* allocate buffer for the full string, and
  read the contents of the temporary file */

  str = (char*) malloc(length + flen + 3);

  if (str) {
    char* pos = str + length;
    if (length) {
      memcpy(str, comment, length);
      *pos++ = ';';
      *pos++ = ' ';
    }
    rewind(srv_dict_tmpfile);
    flen = (uint) fread(pos, 1, flen, srv_dict_tmpfile);
    pos[flen] = 0;
  }

  mutex_exit(&srv_dict_tmpfile_mutex);

  prebuilt->trx->op_info = (char*)"";

  return(str ? str : (char*) comment);
}

/*******************************************************************//**
Gets the foreign key create info for a table stored in InnoDB.
@return own: character string in the form which can be inserted to the
CREATE TABLE statement, MUST be freed with
ha_innobase::free_foreign_key_create_info */
UNIV_INTERN
char*
ha_innobase::get_foreign_key_create_info(void)
/*==========================================*/
{
  char* str = 0;
  long  flen;

  ut_a(prebuilt != NULL);

  /* We do not know if MySQL can call this function before calling
  external_lock(). To be safe, update the session of the current table
  handle. */

  update_session(getTable()->in_use);

  prebuilt->trx->op_info = (char*)"getting info on foreign keys";

  /* In case MySQL calls this in the middle of a SELECT query,
  release possible adaptive hash latch to avoid
  deadlocks of threads */

  trx_search_latch_release_if_reserved(prebuilt->trx);

  mutex_enter(&srv_dict_tmpfile_mutex);
  rewind(srv_dict_tmpfile);

  /* output the data to a temporary file */
  dict_print_info_on_foreign_keys(TRUE, srv_dict_tmpfile,
        prebuilt->trx, prebuilt->table);
  prebuilt->trx->op_info = (char*)"";

  flen = ftell(srv_dict_tmpfile);
  if (flen < 0) {
    flen = 0;
  }

  /* allocate buffer for the string, and
  read the contents of the temporary file */

  str = (char*) malloc(flen + 1);

  if (str) {
    rewind(srv_dict_tmpfile);
    flen = (uint) fread(str, 1, flen, srv_dict_tmpfile);
    str[flen] = 0;
  }

  mutex_exit(&srv_dict_tmpfile_mutex);

  return(str);
}


UNIV_INTERN
int
ha_innobase::get_foreign_key_list(Session *session, List<ForeignKeyInfo> *f_key_list)
{
  dict_foreign_t* foreign;

  ut_a(prebuilt != NULL);
  update_session(getTable()->in_use);
  prebuilt->trx->op_info = (char*)"getting list of foreign keys";
  trx_search_latch_release_if_reserved(prebuilt->trx);
  mutex_enter(&(dict_sys->mutex));
  foreign = UT_LIST_GET_FIRST(prebuilt->table->foreign_list);

  while (foreign != NULL) {

    uint i;
    LEX_STRING *name = 0;
    uint ulen;
    char uname[NAME_LEN + 1];           /* Unencoded name */
    char db_name[NAME_LEN + 1];
    const char *tmp_buff;

    /** Foreign id **/
    tmp_buff = foreign->id;
    i = 0;
    while (tmp_buff[i] != '/')
      i++;
    tmp_buff += i + 1;
    LEX_STRING *tmp_foreign_id = session->make_lex_string(NULL, tmp_buff, strlen(tmp_buff), true);

    /* Database name */
    tmp_buff = foreign->referenced_table_name;

    i= 0;
    while (tmp_buff[i] != '/')
    {
      db_name[i]= tmp_buff[i];
      i++;
    }
    db_name[i] = 0;
    ulen= identifier::Table::filename_to_tablename(db_name, uname, sizeof(uname));
    LEX_STRING *tmp_referenced_db = session->make_lex_string(NULL, uname, ulen, true);

    /* Table name */
    tmp_buff += i + 1;
    ulen= identifier::Table::filename_to_tablename(tmp_buff, uname, sizeof(uname));
    LEX_STRING *tmp_referenced_table = session->make_lex_string(NULL, uname, ulen, true);

    /** Foreign Fields **/
    List<LEX_STRING> tmp_foreign_fields;
    List<LEX_STRING> tmp_referenced_fields;
    for (i= 0;;) {
      tmp_buff= foreign->foreign_col_names[i];
      name = session->make_lex_string(name, tmp_buff, strlen(tmp_buff), true);
      tmp_foreign_fields.push_back(name);
      tmp_buff= foreign->referenced_col_names[i];
      name = session->make_lex_string(name, tmp_buff, strlen(tmp_buff), true);
      tmp_referenced_fields.push_back(name);
      if (++i >= foreign->n_fields)
        break;
    }

    ulong length;
    if (foreign->type & DICT_FOREIGN_ON_DELETE_CASCADE)
    {
      length=7;
      tmp_buff= "CASCADE";
    }
    else if (foreign->type & DICT_FOREIGN_ON_DELETE_SET_NULL)
    {
      length=8;
      tmp_buff= "SET NULL";
    }
    else if (foreign->type & DICT_FOREIGN_ON_DELETE_NO_ACTION)
    {
      length=9;
      tmp_buff= "NO ACTION";
    }
    else
    {
      length=8;
      tmp_buff= "RESTRICT";
    }
    LEX_STRING *tmp_delete_method = session->make_lex_string(NULL, tmp_buff, length, true);


    if (foreign->type & DICT_FOREIGN_ON_UPDATE_CASCADE)
    {
      length=7;
      tmp_buff= "CASCADE";
    }
    else if (foreign->type & DICT_FOREIGN_ON_UPDATE_SET_NULL)
    {
      length=8;
      tmp_buff= "SET NULL";
    }
    else if (foreign->type & DICT_FOREIGN_ON_UPDATE_NO_ACTION)
    {
      length=9;
      tmp_buff= "NO ACTION";
    }
    else
    {
      length=8;
      tmp_buff= "RESTRICT";
    }
    LEX_STRING *tmp_update_method = session->make_lex_string(NULL, tmp_buff, length, true);

    LEX_STRING *tmp_referenced_key_name = NULL;

    if (foreign->referenced_index &&
        foreign->referenced_index->name)
    {
      tmp_referenced_key_name = session->make_lex_string(NULL,
                                                         foreign->referenced_index->name, strlen(foreign->referenced_index->name), true);
    }

    ForeignKeyInfo f_key_info(
                              tmp_foreign_id, tmp_referenced_db, tmp_referenced_table,
                              tmp_update_method, tmp_delete_method, tmp_referenced_key_name,
                              tmp_foreign_fields, tmp_referenced_fields);

    ForeignKeyInfo *pf_key_info = (ForeignKeyInfo*)session->mem.memdup(&f_key_info, sizeof(ForeignKeyInfo));
    f_key_list->push_back(pf_key_info);
    foreign = UT_LIST_GET_NEXT(foreign_list, foreign);
  }
  mutex_exit(&(dict_sys->mutex));
  prebuilt->trx->op_info = (char*)"";

  return(0);
}

/*****************************************************************//**
Checks if ALTER TABLE may change the storage engine of the table.
Changing storage engines is not allowed for tables for which there
are foreign key constraints (parent or child tables).
@return TRUE if can switch engines */
UNIV_INTERN
bool
ha_innobase::can_switch_engines(void)
/*=================================*/
{
  bool  can_switch;

  ut_a(prebuilt->trx == session_to_trx(getTable()->in_use));

  prebuilt->trx->op_info =
      "determining if there are foreign key constraints";
  row_mysql_lock_data_dictionary(prebuilt->trx);

  can_switch = !UT_LIST_GET_FIRST(prebuilt->table->referenced_list)
      && !UT_LIST_GET_FIRST(prebuilt->table->foreign_list);

  row_mysql_unlock_data_dictionary(prebuilt->trx);
  prebuilt->trx->op_info = "";

  return(can_switch);
}

/*******************************************************************//**
Checks if a table is referenced by a foreign key. The MySQL manual states that
a REPLACE is either equivalent to an INSERT, or DELETE(s) + INSERT. Only a
delete is then allowed internally to resolve a duplicate key conflict in
REPLACE, not an update.
@return > 0 if referenced by a FOREIGN KEY */
UNIV_INTERN
uint
ha_innobase::referenced_by_foreign_key(void)
/*========================================*/
{
  if (dict_table_is_referenced_by_foreign_key(prebuilt->table)) {

    return(1);
  }

  return(0);
}

/*******************************************************************//**
Frees the foreign key create info for a table stored in InnoDB, if it is
non-NULL. */
UNIV_INTERN
void
ha_innobase::free_foreign_key_create_info(
/*======================================*/
  char* str)  /*!< in, own: create info string to free */
{
  free(str);
}

/*******************************************************************//**
Tells something additional to the Cursor about how to do things.
@return 0 or error number */
UNIV_INTERN
int
ha_innobase::extra(
/*===============*/
  enum ha_extra_function operation)
         /*!< in: HA_EXTRA_FLUSH or some other flag */
{
  /* Warning: since it is not sure that MySQL calls external_lock
  before calling this function, the trx field in prebuilt can be
  obsolete! */

  switch (operation) {
    case HA_EXTRA_FLUSH:
      if (prebuilt->blob_heap) {
        row_mysql_prebuilt_free_blob_heap(prebuilt);
      }
      break;
    case HA_EXTRA_RESET_STATE:
      reset_template(prebuilt);
      break;
    case HA_EXTRA_NO_KEYREAD:
      prebuilt->read_just_key = 0;
      break;
    case HA_EXTRA_KEYREAD:
      prebuilt->read_just_key = 1;
      break;
    case HA_EXTRA_KEYREAD_PRESERVE_FIELDS:
      prebuilt->keep_other_fields_on_keyread = 1;
      break;

      /* IMPORTANT: prebuilt->trx can be obsolete in
      this method, because it is not sure that MySQL
      calls external_lock before this method with the
      parameters below.  We must not invoke update_session()
      either, because the calling threads may change.
      CAREFUL HERE, OR MEMORY CORRUPTION MAY OCCUR! */
    case HA_EXTRA_IGNORE_DUP_KEY:
      session_to_trx(getTable()->in_use)->duplicates |= TRX_DUP_IGNORE;
      break;
    case HA_EXTRA_WRITE_CAN_REPLACE:
      session_to_trx(getTable()->in_use)->duplicates |= TRX_DUP_REPLACE;
      break;
    case HA_EXTRA_WRITE_CANNOT_REPLACE:
      session_to_trx(getTable()->in_use)->duplicates &= ~TRX_DUP_REPLACE;
      break;
    case HA_EXTRA_NO_IGNORE_DUP_KEY:
      session_to_trx(getTable()->in_use)->duplicates &=
        ~(TRX_DUP_IGNORE | TRX_DUP_REPLACE);
      break;
    default:/* Do nothing */
      ;
  }

  return(0);
}

UNIV_INTERN
int
ha_innobase::reset()
{
  if (prebuilt->blob_heap) {
    row_mysql_prebuilt_free_blob_heap(prebuilt);
  }

  reset_template(prebuilt);

  /* TODO: This should really be reset in reset_template() but for now
  it's safer to do it explicitly here. */

  /* This is a statement level counter. */
  prebuilt->autoinc_last_value = 0;

  return(0);
}

/******************************************************************//**
Maps a MySQL trx isolation level code to the InnoDB isolation level code
@return InnoDB isolation level */
static inline
ulint
innobase_map_isolation_level(
/*=========================*/
  enum_tx_isolation iso)  /*!< in: MySQL isolation level code */
{
  switch(iso) {
    case ISO_REPEATABLE_READ: return(TRX_ISO_REPEATABLE_READ);
    case ISO_READ_COMMITTED: return(TRX_ISO_READ_COMMITTED);
    case ISO_SERIALIZABLE: return(TRX_ISO_SERIALIZABLE);
    case ISO_READ_UNCOMMITTED: return(TRX_ISO_READ_UNCOMMITTED);
    default: ut_a(0); return(0);
  }
}

/******************************************************************//**
As MySQL will execute an external lock for every new table it uses when it
starts to process an SQL statement.  We can use this function to store the pointer to
the Session in the handle.
@return 0 */
UNIV_INTERN
int
ha_innobase::external_lock(
/*=======================*/
  Session*  session,  /*!< in: handle to the user thread */
  int lock_type)  /*!< in: lock type */
{
  update_session(session);

  trx_t *trx= prebuilt->trx;

  prebuilt->sql_stat_start = TRUE;
  prebuilt->hint_need_to_fetch_extra_cols = 0;

  reset_template(prebuilt);

  if (lock_type == F_WRLCK) {

    /* If this is a SELECT, then it is in UPDATE TABLE ...
    or SELECT ... FOR UPDATE */
    prebuilt->select_lock_type = LOCK_X;
    prebuilt->stored_select_lock_type = LOCK_X;
  }

  if (lock_type != F_UNLCK) {
    /* MySQL is setting a new table lock */

    if (trx->isolation_level == TRX_ISO_SERIALIZABLE
      && prebuilt->select_lock_type == LOCK_NONE
      && session_test_options(session,
        OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {

      /* To get serializable execution, we let InnoDB
      conceptually add 'LOCK IN SHARE MODE' to all SELECTs
      which otherwise would have been consistent reads. An
      exception is consistent reads in the AUTOCOMMIT=1 mode:
      we know that they are read-only transactions, and they
      can be serialized also if performed as consistent
      reads. */

      prebuilt->select_lock_type = LOCK_S;
      prebuilt->stored_select_lock_type = LOCK_S;
    }

    /* Starting from 4.1.9, no InnoDB table lock is taken in LOCK
    TABLES if AUTOCOMMIT=1. It does not make much sense to acquire
    an InnoDB table lock if it is released immediately at the end
    of LOCK TABLES, and InnoDB's table locks in that case cause
    VERY easily deadlocks.

    We do not set InnoDB table locks if user has not explicitly
    requested a table lock. Note that session_in_lock_tables(session)
    can hold in some cases, e.g., at the start of a stored
    procedure call (SQLCOM_CALL). */

    if (prebuilt->select_lock_type != LOCK_NONE) {
      trx->mysql_n_tables_locked++;
    }

    prebuilt->mysql_has_locked = TRUE;

    return(0);
  }

  /* MySQL is releasing a table lock */
  prebuilt->mysql_has_locked = FALSE;
  trx->mysql_n_tables_locked= 0;

  return(0);
}

/************************************************************************//**
Implements the SHOW INNODB STATUS command. Sends the output of the InnoDB
Monitor to the client. */
static
bool
innodb_show_status(
/*===============*/
  plugin::StorageEngine*  engine, /*!< in: the innodb StorageEngine */
  Session*  session,/*!< in: the MySQL query thread of the caller */
  stat_print_fn *stat_print)
{
  trx_t*      trx;
  static const char truncated_msg[] = "... truncated...\n";
  const long    MAX_STATUS_SIZE = 1048576;
  ulint     trx_list_start = ULINT_UNDEFINED;
  ulint     trx_list_end = ULINT_UNDEFINED;

  assert(engine == innodb_engine_ptr);

  trx = check_trx_exists(session);

  innobase_release_stat_resources(trx);

  /* We let the InnoDB Monitor to output at most MAX_STATUS_SIZE
  bytes of text. */

  long  flen, usable_len;
  char* str;

  mutex_enter(&srv_monitor_file_mutex);
  rewind(srv_monitor_file);
  srv_printf_innodb_monitor(srv_monitor_file, FALSE,
        &trx_list_start, &trx_list_end);
  flen = ftell(srv_monitor_file);
  os_file_set_eof(srv_monitor_file);

  if (flen < 0) {
    flen = 0;
  }

  if (flen > MAX_STATUS_SIZE) {
    usable_len = MAX_STATUS_SIZE;
    srv_truncated_status_writes++;
  } else {
    usable_len = flen;
  }

  /* allocate buffer for the string, and
  read the contents of the temporary file */

  if (!(str = (char*) malloc(usable_len + 1))) {
    mutex_exit(&srv_monitor_file_mutex);
    return(TRUE);
  }

  rewind(srv_monitor_file);
  if (flen < MAX_STATUS_SIZE) {
    /* Display the entire output. */
    flen = (long) fread(str, 1, flen, srv_monitor_file);
  } else if (trx_list_end < (ulint) flen
      && trx_list_start < trx_list_end
      && trx_list_start + (flen - trx_list_end)
      < MAX_STATUS_SIZE - sizeof truncated_msg - 1) {
    /* Omit the beginning of the list of active transactions. */
    long len = (long) fread(str, 1, trx_list_start, srv_monitor_file);
    memcpy(str + len, truncated_msg, sizeof truncated_msg - 1);
    len += sizeof truncated_msg - 1;
    usable_len = (MAX_STATUS_SIZE - 1) - len;
    fseek(srv_monitor_file, flen - usable_len, SEEK_SET);
    len += (long) fread(str + len, 1, usable_len, srv_monitor_file);
    flen = len;
  } else {
    /* Omit the end of the output. */
    flen = (long) fread(str, 1, MAX_STATUS_SIZE - 1, srv_monitor_file);
  }

  mutex_exit(&srv_monitor_file_mutex);

  stat_print(session, innobase_engine_name, strlen(innobase_engine_name),
             STRING_WITH_LEN(""), str, flen);

  free(str);

  return(FALSE);
}

/************************************************************************//**
Implements the SHOW MUTEX STATUS command.
@return true on failure false on success*/
static
bool
innodb_mutex_show_status(
/*=====================*/
  plugin::StorageEngine*  engine,   /*!< in: the innodb StorageEngine */
  Session*  session,  /*!< in: the MySQL query thread of the
          caller */
  stat_print_fn*  stat_print)	/*!< in: function for printing
					statistics */
{
  char buf1[IO_SIZE], buf2[IO_SIZE];
  mutex_t*  mutex;
  rw_lock_t*  lock;
  ulint		block_mutex_oswait_count = 0;
  ulint		block_lock_oswait_count = 0;
  mutex_t*	block_mutex = NULL;
  rw_lock_t*	block_lock = NULL;
#ifdef UNIV_DEBUG
  ulint   rw_lock_count= 0;
  ulint   rw_lock_count_spin_loop= 0;
  ulint   rw_lock_count_spin_rounds= 0;
  ulint   rw_lock_count_os_wait= 0;
  ulint   rw_lock_count_os_yield= 0;
  uint64_t rw_lock_wait_time= 0;
#endif /* UNIV_DEBUG */
  uint    engine_name_len= strlen(innobase_engine_name), buf1len, buf2len;
  assert(engine == innodb_engine_ptr);

  mutex_enter(&mutex_list_mutex);

  for (mutex = UT_LIST_GET_FIRST(mutex_list); mutex != NULL;
       mutex = UT_LIST_GET_NEXT(list, mutex)) {
    if (mutex->count_os_wait == 0) {
      continue;
    }


    if (buf_pool_is_block_mutex(mutex)) {
      block_mutex = mutex;
      block_mutex_oswait_count += mutex->count_os_wait;
      continue;
    }
#ifdef UNIV_DEBUG
    if (mutex->mutex_type != 1) {
      if (mutex->count_using > 0) {
        buf1len= my_snprintf(buf1, sizeof(buf1),
          "%s:%s",
          mutex->cmutex_name, mutex->cfile_name);
        buf2len= my_snprintf(buf2, sizeof(buf2),
          "count=%lu, spin_waits=%lu,"
          " spin_rounds=%lu, "
          "os_waits=%lu, os_yields=%lu,"
          " os_wait_times=%lu",
          mutex->count_using,
          mutex->count_spin_loop,
          mutex->count_spin_rounds,
          mutex->count_os_wait,
          mutex->count_os_yield,
          (ulong) (mutex->lspent_time/1000));

        if (stat_print(session, innobase_engine_name,
            engine_name_len, buf1, buf1len,
            buf2, buf2len)) {
          mutex_exit(&mutex_list_mutex);
          return(1);
        }
      }
    } else {
      rw_lock_count += mutex->count_using;
      rw_lock_count_spin_loop += mutex->count_spin_loop;
      rw_lock_count_spin_rounds += mutex->count_spin_rounds;
      rw_lock_count_os_wait += mutex->count_os_wait;
      rw_lock_count_os_yield += mutex->count_os_yield;
      rw_lock_wait_time += mutex->lspent_time;
    }
#else /* UNIV_DEBUG */
    buf1len= snprintf(buf1, sizeof(buf1), "%s:%lu",
          mutex->cfile_name, (ulong) mutex->cline);
    buf2len= snprintf(buf2, sizeof(buf2), "os_waits=%lu",
                      (ulong) mutex->count_os_wait);

    if (stat_print(session, innobase_engine_name,
             engine_name_len, buf1, buf1len,
             buf2, buf2len)) {
      mutex_exit(&mutex_list_mutex);
      return(1);
    }
#endif /* UNIV_DEBUG */
  }

  if (block_mutex) {
    buf1len = snprintf(buf1, sizeof buf1,
                       "combined %s:%lu",
                       block_mutex->cfile_name,
                       (ulong) block_mutex->cline);
    buf2len = snprintf(buf2, sizeof buf2,
                       "os_waits=%lu",
                       (ulong) block_mutex_oswait_count);

    if (stat_print(session, innobase_engine_name,
                   strlen(innobase_engine_name), buf1, buf1len,
                   buf2, buf2len)) {
      mutex_exit(&mutex_list_mutex);
      return(1);
    }
  }

  mutex_exit(&mutex_list_mutex);

  mutex_enter(&rw_lock_list_mutex);

  for (lock = UT_LIST_GET_FIRST(rw_lock_list); lock != NULL;
       lock = UT_LIST_GET_NEXT(list, lock)) {
    if (lock->count_os_wait == 0) {
      continue;
    }

    if (buf_pool_is_block_lock(lock)) {
      block_lock = lock;
      block_lock_oswait_count += lock->count_os_wait;
      continue;
    }

    buf1len = snprintf(buf1, sizeof buf1, "%s:%lu",
                       lock->cfile_name, (ulong) lock->cline);
    buf2len = snprintf(buf2, sizeof buf2, "os_waits=%lu",
                       (ulong) lock->count_os_wait);

    if (stat_print(session, innobase_engine_name,
                   strlen(innobase_engine_name), buf1, buf1len,
                   buf2, buf2len)) {
      mutex_exit(&rw_lock_list_mutex);
      return(1);
    }
  }

  if (block_lock) {
    buf1len = snprintf(buf1, sizeof buf1,
                       "combined %s:%lu",
                       block_lock->cfile_name,
                       (ulong) block_lock->cline);
    buf2len = snprintf(buf2, sizeof buf2,
                       "os_waits=%lu",
                       (ulong) block_lock_oswait_count);

    if (stat_print(session, innobase_engine_name,
                   strlen(innobase_engine_name), buf1, buf1len,
                   buf2, buf2len)) {
      mutex_exit(&rw_lock_list_mutex);
      return(1);
    }
  }

  mutex_exit(&rw_lock_list_mutex);

#ifdef UNIV_DEBUG
  buf2len = snprintf(buf2, sizeof buf2,
                     "count=%lu, spin_waits=%lu, spin_rounds=%lu, "
                     "os_waits=%lu, os_yields=%lu, os_wait_times=%lu",
                     (ulong) rw_lock_count,
                     (ulong) rw_lock_count_spin_loop,
                     (ulong) rw_lock_count_spin_rounds,
                     (ulong) rw_lock_count_os_wait,
                     (ulong) rw_lock_count_os_yield,
                     (ulong) (rw_lock_wait_time / 1000));

  if (stat_print(session, innobase_engine_name, engine_name_len,
      STRING_WITH_LEN("rw_lock_mutexes"), buf2, buf2len)) {
    return(1);
  }
#endif /* UNIV_DEBUG */

  return(FALSE);
}

bool InnobaseEngine::show_status(Session* session, 
                                 stat_print_fn* stat_print,
                                 enum ha_stat_type stat_type)
{
  assert(this == innodb_engine_ptr);

  switch (stat_type) {
  case HA_ENGINE_STATUS:
    return innodb_show_status(this, session, stat_print);
  case HA_ENGINE_MUTEX:
    return innodb_mutex_show_status(this, session, stat_print);
  default:
    return(FALSE);
  }
}

/************************************************************************//**
 Handling the shared INNOBASE_SHARE structure that is needed to provide table
 locking.
****************************************************************************/

static INNOBASE_SHARE* get_share(const char* table_name)
{
  INNOBASE_SHARE *share;
  boost::mutex::scoped_lock scopedLock(innobase_share_mutex);

  ulint fold = ut_fold_string(table_name);

  HASH_SEARCH(table_name_hash, innobase_open_tables, fold,
        INNOBASE_SHARE*, share,
        ut_ad(share->use_count > 0),
        !strcmp(share->table_name, table_name));

  if (!share) {
    /* TODO: invoke HASH_MIGRATE if innobase_open_tables
    grows too big */

    share= new INNOBASE_SHARE(table_name);

    HASH_INSERT(INNOBASE_SHARE, table_name_hash,
          innobase_open_tables, fold, share);

    thr_lock_init(&share->lock);

    /* Index translation table initialization */
    share->idx_trans_tbl.index_mapping = NULL;
    share->idx_trans_tbl.index_count = 0;
    share->idx_trans_tbl.array_size = 0;
  }

  share->use_count++;

  return(share);
}

static void free_share(INNOBASE_SHARE* share)
{
  boost::mutex::scoped_lock scopedLock(innobase_share_mutex);

#ifdef UNIV_DEBUG
  INNOBASE_SHARE* share2;
  ulint fold = ut_fold_string(share->table_name);

  HASH_SEARCH(table_name_hash, innobase_open_tables, fold,
        INNOBASE_SHARE*, share2,
        ut_ad(share->use_count > 0),
        !strcmp(share->table_name, share2->table_name));

  ut_a(share2 == share);
#endif /* UNIV_DEBUG */

  if (!--share->use_count) {
    ulint fold = ut_fold_string(share->table_name);

    HASH_DELETE(INNOBASE_SHARE, table_name_hash,
          innobase_open_tables, fold, share);
    share->lock.deinit();

    /* Free any memory from index translation table */
    free(share->idx_trans_tbl.index_mapping);

    delete share;

    /* TODO: invoke HASH_MIGRATE if innobase_open_tables
    shrinks too much */
  }
}

/*****************************************************************//**
Converts a MySQL table lock stored in the 'lock' field of the handle to
a proper type before storing pointer to the lock into an array of pointers.
MySQL also calls this if it wants to reset some table locks to a not-locked
state during the processing of an SQL query. An example is that during a
SELECT the read lock is released early on the 'const' tables where we only
fetch one row. MySQL does not call this when it releases all locks at the
end of an SQL statement.
@return pointer to the next element in the 'to' array */
UNIV_INTERN
THR_LOCK_DATA**
ha_innobase::store_lock(
/*====================*/
  Session*    session,  /*!< in: user thread handle */
  THR_LOCK_DATA**   to,   /*!< in: pointer to an array
            of pointers to lock structs;
            pointer to the 'lock' field
            of current handle is stored
            next to this array */
  enum thr_lock_type  lock_type)  /*!< in: lock type to store in
            'lock'; this may also be
            TL_IGNORE */
{
  trx_t*    trx;

  /* Note that trx in this function is NOT necessarily prebuilt->trx
  because we call update_session() later, in ::external_lock()! Failure to
  understand this caused a serious memory corruption bug in 5.1.11. */

  trx = check_trx_exists(session);

  assert(EQ_CURRENT_SESSION(session));
  const uint32_t sql_command = session->getSqlCommand();

  if (sql_command == SQLCOM_DROP_TABLE) {

    /* MySQL calls this function in DROP Table though this table
    handle may belong to another session that is running a query.
    Let us in that case skip any changes to the prebuilt struct. */ 

  } else if (lock_type == TL_READ_WITH_SHARED_LOCKS
       || lock_type == TL_READ_NO_INSERT
       || (lock_type != TL_IGNORE
           && sql_command != SQLCOM_SELECT)) {

    /* The OR cases above are in this order:
    1) MySQL is doing LOCK TABLES ... READ LOCAL, or we
    are processing a stored procedure or function, or
    2) (we do not know when TL_READ_HIGH_PRIORITY is used), or
    3) this is a SELECT ... IN SHARE MODE, or
    4) we are doing a complex SQL statement like
    INSERT INTO ... SELECT ... and the logical logging (MySQL
    binlog) requires the use of a locking read, or
    MySQL is doing LOCK TABLES ... READ.
    5) we let InnoDB do locking reads for all SQL statements that
    are not simple SELECTs; note that select_lock_type in this
    case may get strengthened in ::external_lock() to LOCK_X.
    Note that we MUST use a locking read in all data modifying
    SQL statements, because otherwise the execution would not be
    serializable, and also the results from the update could be
    unexpected if an obsolete consistent read view would be
    used. */

    ulint isolation_level;

    isolation_level = trx->isolation_level;

    if ((srv_locks_unsafe_for_binlog
         || isolation_level <= TRX_ISO_READ_COMMITTED)
        && isolation_level != TRX_ISO_SERIALIZABLE
        && (lock_type == TL_READ || lock_type == TL_READ_NO_INSERT)
        && (sql_command == SQLCOM_INSERT_SELECT
            || sql_command == SQLCOM_REPLACE_SELECT
            || sql_command == SQLCOM_UPDATE
            || sql_command == SQLCOM_CREATE_TABLE
            || sql_command == SQLCOM_SET_OPTION)) {

      /* If we either have innobase_locks_unsafe_for_binlog
      option set or this session is using READ COMMITTED
      isolation level and isolation level of the transaction
      is not set to serializable and MySQL is doing
      INSERT INTO...SELECT or REPLACE INTO...SELECT
      or UPDATE ... = (SELECT ...) or CREATE  ...
      SELECT... or SET ... = (SELECT ...) without
      FOR UPDATE or IN SHARE MODE in select,
      then we use consistent read for select. */

      prebuilt->select_lock_type = LOCK_NONE;
      prebuilt->stored_select_lock_type = LOCK_NONE;
    } else if (sql_command == SQLCOM_CHECKSUM) {
      /* Use consistent read for checksum table */

      prebuilt->select_lock_type = LOCK_NONE;
      prebuilt->stored_select_lock_type = LOCK_NONE;
    } else {
      prebuilt->select_lock_type = LOCK_S;
      prebuilt->stored_select_lock_type = LOCK_S;
    }

  } else if (lock_type != TL_IGNORE) {

    /* We set possible LOCK_X value in external_lock, not yet
    here even if this would be SELECT ... FOR UPDATE */

    prebuilt->select_lock_type = LOCK_NONE;
    prebuilt->stored_select_lock_type = LOCK_NONE;
  }

  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {

    /* If we are not doing a LOCK TABLE, DISCARD/IMPORT
    TABLESPACE or TRUNCATE TABLE then allow multiple
    writers. Note that ALTER TABLE uses a TL_WRITE_ALLOW_READ
    < TL_WRITE_CONCURRENT_INSERT.
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT
         && lock_type <= TL_WRITE)
        && ! session->doing_tablespace_operation()
        && sql_command != SQLCOM_TRUNCATE
        && sql_command != SQLCOM_CREATE_TABLE) {

      lock_type = TL_WRITE_ALLOW_WRITE;
    }

    /* In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
    MySQL would use the lock TL_READ_NO_INSERT on t2, and that
    would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
    to t2. Convert the lock to a normal read lock to allow
    concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT) {

      lock_type = TL_READ;
    }

    lock.type = lock_type;
  }

  *to++= &lock;

  return(to);
}

/*********************************************************************//**
Read the next autoinc value. Acquire the relevant locks before reading
the AUTOINC value. If SUCCESS then the table AUTOINC mutex will be locked
on return and all relevant locks acquired.
@return DB_SUCCESS or error code */
UNIV_INTERN
ulint
ha_innobase::innobase_get_autoinc(
/*==============================*/
  uint64_t* value)    /*!< out: autoinc value */
{
  *value = 0;

  dict_table_autoinc_lock(prebuilt->table);
  prebuilt->autoinc_error= DB_SUCCESS;
  /* Determine the first value of the interval */
  *value = dict_table_autoinc_read(prebuilt->table);

  /* It should have been initialized during open. */
  if (*value == 0) {
    prebuilt->autoinc_error = DB_UNSUPPORTED;
    dict_table_autoinc_unlock(prebuilt->table);
  }

  return(DB_SUCCESS);
}

/*******************************************************************//**
This function reads the global auto-inc counter. It doesn't use the
AUTOINC lock even if the lock mode is set to TRADITIONAL.
@return the autoinc value */
UNIV_INTERN
uint64_t
ha_innobase::innobase_peek_autoinc(void)
/*====================================*/
{
  uint64_t  auto_inc;
  dict_table_t* innodb_table;

  ut_a(prebuilt != NULL);
  ut_a(prebuilt->table != NULL);

  innodb_table = prebuilt->table;

  dict_table_autoinc_lock(innodb_table);

  auto_inc = dict_table_autoinc_read(innodb_table);

  if (auto_inc == 0) {
    ut_print_timestamp(stderr);
    errmsg_printf(error::ERROR, "  InnoDB: AUTOINC next value generation is disabled for '%s'\n", innodb_table->name);
  }

  dict_table_autoinc_unlock(innodb_table);

  return(auto_inc);
}

/*********************************************************************//**
This function initializes the auto-inc counter if it has not been
initialized yet. This function does not change the value of the auto-inc
counter if it already has been initialized. Returns the value of the
auto-inc counter in *first_value, and UINT64_T_MAX in *nb_reserved_values (as
we have a table-level lock). offset, increment, nb_desired_values are ignored.
*first_value is set to -1 if error (deadlock or lock wait timeout) */
UNIV_INTERN
void
ha_innobase::get_auto_increment(
/*============================*/
        uint64_t  offset,              /*!< in: table autoinc offset */
        uint64_t  increment,           /*!< in: table autoinc increment */
        uint64_t  nb_desired_values,   /*!< in: number of values reqd */
        uint64_t  *first_value,        /*!< out: the autoinc value */
        uint64_t  *nb_reserved_values) /*!< out: count of reserved values */
{
  trx_t*    trx;
  ulint   error;
  uint64_t  autoinc = 0;

  /* Prepare prebuilt->trx in the table handle */
  update_session(getTable()->in_use);

  error = innobase_get_autoinc(&autoinc);

  if (error != DB_SUCCESS) {
    *first_value = (~(uint64_t) 0);
    return;
  }

  /* This is a hack, since nb_desired_values seems to be accurate only
  for the first call to get_auto_increment() for multi-row INSERT and
  meaningless for other statements e.g, LOAD etc. Subsequent calls to
  this method for the same statement results in different values which
  don't make sense. Therefore we store the value the first time we are
  called and count down from that as rows are written (see doInsertRecord()).
  */

  trx = prebuilt->trx;

  /* Note: We can't rely on *first_value since some MySQL engines,
  in particular the partition engine, don't initialize it to 0 when
  invoking this method. So we are not sure if it's guaranteed to
  be 0 or not. */

  /* We need the upper limit of the col type to check for
     whether we update the table autoinc counter or not. */
  uint64_t col_max_value = innobase_get_int_col_max_value(getTable()->next_number_field);

  /* Called for the first time ? */
  if (trx->n_autoinc_rows == 0) {

    trx->n_autoinc_rows = (ulint) nb_desired_values;

    /* It's possible for nb_desired_values to be 0:
    e.g., INSERT INTO T1(C) SELECT C FROM T2; */
    if (nb_desired_values == 0) {

      trx->n_autoinc_rows = 1;
    }

    set_if_bigger(*first_value, autoinc);
  /* Not in the middle of a mult-row INSERT. */
  } else if (prebuilt->autoinc_last_value == 0) {
    set_if_bigger(*first_value, autoinc);
    /* Check for -ve values. */
  } else if (*first_value > col_max_value && trx->n_autoinc_rows > 0) {
    /* Set to next logical value. */
    ut_a(autoinc > trx->n_autoinc_rows);
    *first_value = (autoinc - trx->n_autoinc_rows) - 1;
  }

  *nb_reserved_values = trx->n_autoinc_rows;

  /* This all current style autoinc. */
  {
    uint64_t  need;
    uint64_t  current;
    uint64_t  next_value;

    current = *first_value > col_max_value ? autoinc : *first_value;
    need = *nb_reserved_values * increment;

    /* Compute the last value in the interval */
    next_value = innobase_next_autoinc(current, need, offset, col_max_value);

    prebuilt->autoinc_last_value = next_value;

    if (prebuilt->autoinc_last_value < *first_value) {
      *first_value = (~(unsigned long long) 0);
    } else {
      /* Update the table autoinc variable */
      dict_table_autoinc_update_if_greater(
        prebuilt->table, prebuilt->autoinc_last_value);
    }
  }

  /* The increment to be used to increase the AUTOINC value, we use
  this in doInsertRecord() and doUpdateRecord() to increase the autoinc counter
  for columns that are filled by the user. We need the offset and
  the increment. */
  prebuilt->autoinc_offset = offset;
  prebuilt->autoinc_increment = increment;

  dict_table_autoinc_unlock(prebuilt->table);
}

/*******************************************************************//**
Reset the auto-increment counter to the given value, i.e. the next row
inserted will get the given value. This is called e.g. after TRUNCATE
is emulated by doing a 'DELETE FROM t'. HA_ERR_WRONG_COMMAND is
returned by storage engines that don't support this operation.
@return 0 or error code */
UNIV_INTERN
int
ha_innobase::reset_auto_increment(
/*==============================*/
  uint64_t  value)    /*!< in: new value for table autoinc */
{
  int error;

  update_session(getTable()->in_use);

  error = row_lock_table_autoinc_for_mysql(prebuilt);

  if (error != DB_SUCCESS) {
    error = convert_error_code_to_mysql(error,
                prebuilt->table->flags,
                user_session);

    return(error);
  }

  /* The next value can never be 0. */
  if (value == 0) {
    value = 1;
  }

  innobase_reset_autoinc(value);

  return 0;
}

/* See comment in Cursor.cc */
UNIV_INTERN
bool
InnobaseEngine::get_error_message(int, String *buf) const
{
  trx_t*  trx = check_trx_exists(current_session);

  buf->copy(trx->detailed_error, (uint) strlen(trx->detailed_error),
    system_charset_info);

  return(FALSE);
}

/*******************************************************************//**
Compares two 'refs'. A 'ref' is the (internal) primary key value of the row.
If there is no explicitly declared non-null unique key or a primary key, then
InnoDB internally uses the row id as the primary key.
@return < 0 if ref1 < ref2, 0 if equal, else > 0 */
UNIV_INTERN
int
ha_innobase::cmp_ref(
/*=================*/
  const unsigned char*  ref1, /*!< in: an (internal) primary key value in the
        MySQL key value format */
  const unsigned char*  ref2) /*!< in: an (internal) primary key value in the
        MySQL key value format */
{
  enum_field_types mysql_type;
  Field*    field;
  KeyPartInfo*  key_part;
  KeyPartInfo*  key_part_end;
  uint    len1;
  uint    len2;
  int   result;

  if (prebuilt->clust_index_was_generated) {
    /* The 'ref' is an InnoDB row id */

    return(memcmp(ref1, ref2, DATA_ROW_ID_LEN));
  }

  /* Do a type-aware comparison of primary key fields. PK fields
  are always NOT NULL, so no checks for NULL are performed. */

  key_part = getTable()->key_info[getTable()->getShare()->getPrimaryKey()].key_part;

  key_part_end = key_part
      + getTable()->key_info[getTable()->getShare()->getPrimaryKey()].key_parts;

  for (; key_part != key_part_end; ++key_part) {
    field = key_part->field;
    mysql_type = field->type();

    if (mysql_type == DRIZZLE_TYPE_BLOB) {

      /* In the MySQL key value format, a column prefix of
      a BLOB is preceded by a 2-byte length field */

      len1 = innobase_read_from_2_little_endian(ref1);
      len2 = innobase_read_from_2_little_endian(ref2);

      ref1 += 2;
      ref2 += 2;
      result = ((Field_blob*)field)->cmp( ref1, len1,
                                                            ref2, len2);
    } else {
      result = field->key_cmp(ref1, ref2);
    }

    if (result) {

      return(result);
    }

    ref1 += key_part->store_length;
    ref2 += key_part->store_length;
  }

  return(0);
}

/**********************************************************************
This function is used to find the storage length in bytes of the first n
characters for prefix indexes using a multibyte character set. The function
finds charset information and returns length of prefix_len characters in the
index field in bytes.
@return number of bytes occupied by the first n characters */

ulint
innobase_get_at_most_n_mbchars(
/*===========================*/
  ulint charset_id, /*!< in: character set id */
  ulint prefix_len, /*!< in: prefix length in bytes of the index
        (this has to be divided by mbmaxlen to get the
        number of CHARACTERS n in the prefix) */
  ulint data_len,   /*!< in: length of the string in bytes */
  const char* str)  /*!< in: character string */
{
  ulint char_length;    /*!< character length in bytes */
  ulint n_chars;      /*!< number of characters in prefix */
  const charset_info_st* charset;  /*!< charset used in the field */

  charset = get_charset((uint) charset_id);

  ut_ad(charset);
  ut_ad(charset->mbmaxlen);

  /* Calculate how many characters at most the prefix index contains */

  n_chars = prefix_len / charset->mbmaxlen;

  /* If the charset is multi-byte, then we must find the length of the
  first at most n chars in the string. If the string contains less
  characters than n, then we return the length to the end of the last
  character. */

  if (charset->mbmaxlen > 1) {
    /* my_charpos() returns the byte length of the first n_chars
    characters, or a value bigger than the length of str, if
    there were not enough full characters in str.

    Why does the code below work:
    Suppose that we are looking for n UTF-8 characters.

    1) If the string is long enough, then the prefix contains at
    least n complete UTF-8 characters + maybe some extra
    characters + an incomplete UTF-8 character. No problem in
    this case. The function returns the pointer to the
    end of the nth character.

    2) If the string is not long enough, then the string contains
    the complete value of a column, that is, only complete UTF-8
    characters, and we can store in the column prefix index the
    whole string. */

    char_length = my_charpos(charset, str,
            str + data_len, (int) n_chars);
    if (char_length > data_len) {
      char_length = data_len;
    }
  } else {
    if (data_len < prefix_len) {
      char_length = data_len;
    } else {
      char_length = prefix_len;
    }
  }

  return(char_length);
}
/**
 * We will also use this function to communicate
 * to InnoDB that a new SQL statement has started and that we must store a
 * savepoint to our transaction handle, so that we are able to roll back
 * the SQL statement in case of an error.
 */
void
InnobaseEngine::doStartStatement(
  Session *session) /*!< in: handle to the Drizzle session */
{
  /* 
   * Create the InnoDB transaction structure
   * for the session
   */
  trx_t *trx= check_trx_exists(session);

  /* "reset" the error message for the transaction */
  trx->detailed_error[0]= '\0';

  /* Set the isolation level of the transaction. */
  trx->isolation_level= innobase_map_isolation_level(session->getTxIsolation());
}

void
InnobaseEngine::doEndStatement(
  Session *session)
{
  trx_t *trx= check_trx_exists(session);

  /* Release a possible FIFO ticket and search latch. Since we
  may reserve the kernel mutex, we have to release the search
  system latch first to obey the latching order. */

  innobase_release_stat_resources(trx);

}

/*******************************************************************//**
This function is used to prepare an X/Open XA distributed transaction.
@return 0 or error number */
int
InnobaseEngine::doXaPrepare(
/*================*/
  Session*  session,/*!< in: handle to the MySQL thread of
        the user whose XA transaction should
        be prepared */
  bool    all)  /*!< in: TRUE - commit transaction
        FALSE - the current SQL statement
        ended */
{
  int error = 0;
  trx_t* trx = check_trx_exists(session);

  assert(this == innodb_engine_ptr);

  /* we use support_xa value as it was seen at transaction start
  time, not the current session variable value. Any possible changes
  to the session variable take effect only in the next transaction */
  if (!trx->support_xa) {

    return(0);
  }

  session->get_xid(reinterpret_cast<DrizzleXid*>(&trx->xid));

  /* Release a possible FIFO ticket and search latch. Since we will
  reserve the kernel mutex, we have to release the search system latch
  first to obey the latching order. */

  innobase_release_stat_resources(trx);

  if (all
    || (!session_test_options(session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) {

    /* We were instructed to prepare the whole transaction, or
    this is an SQL statement end and autocommit is on */

    ut_ad(trx->conc_state != TRX_NOT_STARTED);

    error = (int) trx_prepare_for_mysql(trx);
  } else {
    /* We just mark the SQL statement ended and do not do a
    transaction prepare */

    /* If we had reserved the auto-inc lock for some
    table in this SQL statement we release it now */

    row_unlock_table_autoinc_for_mysql(trx);

    /* Store the current undo_no of the transaction so that we
    know where to roll back if we have to roll back the next
    SQL statement */

    trx_mark_sql_stat_end(trx);
  }

  /* Tell the InnoDB server that there might be work for utility
  threads: */

  srv_active_wake_master_thread();

  return(error);
}

uint64_t InnobaseEngine::doGetCurrentTransactionId(Session *session)
{
  trx_t *trx= session_to_trx(session);
  return (trx->id);
}

uint64_t InnobaseEngine::doGetNewTransactionId(Session *session)
{
  trx_t*& trx = session_to_trx(session);

  if (trx == NULL)
  {
    trx = innobase_trx_allocate(session);

    innobase_trx_init(session, trx);
  }

  mutex_enter(&kernel_mutex);
  trx->id= trx_sys_get_new_trx_id();
  mutex_exit(&kernel_mutex);

  uint64_t transaction_id= trx->id;

  return transaction_id;
}

/*******************************************************************//**
This function is used to recover X/Open XA distributed transactions.
@return number of prepared transactions stored in xid_list */
int
InnobaseEngine::doXaRecover(
/*================*/
  ::drizzled::XID*  xid_list,/*!< in/out: prepared transactions */
  size_t len) /*!< in: number of slots in xid_list */
{
  assert(this == innodb_engine_ptr);

  if (len == 0 || xid_list == NULL) {

    return(0);
  }

  return(trx_recover_for_mysql((::XID *)xid_list, len));
}

/*******************************************************************//**
This function is used to commit one X/Open XA distributed transaction
which is in the prepared state
@return 0 or error number */
int
InnobaseEngine::doXaCommitXid(
/*===================*/
  ::drizzled::XID*  xid)  /*!< in: X/Open XA transaction identification */
{
  trx_t*  trx;

  assert(this == innodb_engine_ptr);

  trx = trx_get_trx_by_xid((::XID *)xid);

  if (trx) {
    innobase_commit_low(trx);

    return(XA_OK);
  } else {
    return(XAER_NOTA);
  }
}

/*******************************************************************//**
This function is used to rollback one X/Open XA distributed transaction
which is in the prepared state
@return 0 or error number */
int
InnobaseEngine::doXaRollbackXid(
/*=====================*/
  ::drizzled::XID*    xid)  /*!< in: X/Open XA transaction
        identification */
{
  trx_t*  trx;

  assert(this == innodb_engine_ptr);

  trx = trx_get_trx_by_xid((::XID *)xid);

  if (trx) {
    return(innobase_rollback_trx(trx));
  } else {
    return(XAER_NOTA);
  }
}


/************************************************************//**
Validate the file format name and return its corresponding id.
@return valid file format id */
static
uint
innobase_file_format_name_lookup(
/*=============================*/
  const char* format_name)  /*!< in: pointer to file format name */
{
  char* endp;
  uint  format_id;

  ut_a(format_name != NULL);

  /* The format name can contain the format id itself instead of
  the name and we check for that. */
  format_id = (uint) strtoul(format_name, &endp, 10);

  /* Check for valid parse. */
  if (*endp == '\0' && *format_name != '\0') {

    if (format_id <= DICT_TF_FORMAT_MAX) {

      return(format_id);
    }
  } else {

    for (format_id = 0; format_id <= DICT_TF_FORMAT_MAX;
         format_id++) {
      const char* name;

      name = trx_sys_file_format_id_to_name(format_id);

      if (!innobase_strcasecmp(format_name, name)) {

        return(format_id);
      }
    }
  }

  return(DICT_TF_FORMAT_MAX + 1);
}

/************************************************************//**
Validate the file format check config parameters, as a side effect it
sets the srv_max_file_format_at_startup variable.
@return the format_id if valid config value, otherwise, return -1 */
static
int
innobase_file_format_validate_and_set(
/*================================*/
  const char* format_max) /*!< in: parameter value */
{
  uint    format_id;

  format_id = innobase_file_format_name_lookup(format_max);

  if (format_id < DICT_TF_FORMAT_MAX + 1) {
    srv_max_file_format_at_startup = format_id;
    return((int) format_id);
  } else {
    return(-1);
  }
}



static void init_options(drizzled::module::option_context &context)
{
  context("disable-checksums",
          "Disable InnoDB checksums validation.");
  context("data-home-dir",
          po::value<string>(),
          "The common part for InnoDB table spaces.");
  context("disable-doublewrite",
          "Disable InnoDB doublewrite buffer.");
  context("io-capacity",
          po::value<io_capacity_constraint>(&innodb_io_capacity)->default_value(200),
          "Number of IOPs the server can do. Tunes the background IO rate");
  context("fast-shutdown",
          po::value<trinary_constraint>(&innobase_fast_shutdown)->default_value(1), 
          "Speeds up the shutdown process of the InnoDB storage engine. Possible values are 0, 1 (faster) or 2 (fastest - crash-like).");
  context("purge-batch-size",
          po::value<purge_batch_constraint>(&innodb_purge_batch_size)->default_value(20),
          "Number of UNDO logs to purge in one batch from the history list. "
          "Default is 20.");
  context("purge-threads",
          po::value<purge_threads_constraint>(&innodb_n_purge_threads)->default_value(1),
          "Purge threads can be either 0 or 1. Default is 1.");
  context("file-per-table",
          po::value<bool>(&srv_file_per_table)->default_value(false)->zero_tokens(),
           "Stores each InnoDB table to an .ibd file in the database dir.");
  context("file-format-max",
          po::value<string>(&innobase_file_format_max)->default_value("Antelope"),
          "The highest file format in the tablespace.");
  context("file-format-check",
          po::value<bool>(&innobase_file_format_check)->default_value(true)->zero_tokens(),
          "Whether to perform system file format check.");
  context("file-format",
          po::value<string>(&innobase_file_format_name)->default_value("Antelope"),
          "File format to use for new tables in .ibd files.");
  context("flush-log-at-trx-commit",
          po::value<trinary_constraint>(&innodb_flush_log_at_trx_commit)->default_value(1),
          "Set to 0 (write and flush once per second), 1 (write and flush at each commit) or 2 (write at commit, flush once per second).");
  context("flush-method",
          po::value<string>(),
          "With which method to flush data.");
  context("log-group-home-dir",
          po::value<string>(),
          "Path to InnoDB log files.");
  context("max-dirty-pages-pct",
          po::value<max_dirty_pages_constraint>(&innodb_max_dirty_pages_pct)->default_value(75),
          "Percentage of dirty pages allowed in bufferpool.");
  context("disable-adaptive-flushing",
          "Do not attempt flushing dirty pages to avoid IO bursts at checkpoints.");
  context("max-purge-lag",
          po::value<uint64_constraint>(&innodb_max_purge_lag)->default_value(0),
          "Desired maximum length of the purge queue (0 = no limit)");
  context("status-file",
          po::value<bool>(&innobase_create_status_file)->default_value(false)->zero_tokens(),
          "Enable SHOW INNODB STATUS output in the innodb_status.<pid> file");
  context("disable-stats-on-metadata",
          "Disable statistics gathering for metadata commands such as SHOW TABLE STATUS (on by default)");
  context("stats-sample-pages",
          po::value<uint64_nonzero_constraint>(&innodb_stats_sample_pages)->default_value(8),
          "The number of index pages to sample when calculating statistics (default 8)");
  context("disable-adaptive-hash-index",
          "Enable InnoDB adaptive hash index (enabled by default)");
  context("replication-delay",
          po::value<uint64_constraint>(&innodb_replication_delay)->default_value(0),
          "Replication thread delay (ms) on the slave server if innodb_thread_concurrency is reached (0 by default)");
  context("additional-mem-pool-size",
          po::value<additional_mem_pool_constraint>(&innobase_additional_mem_pool_size)->default_value(8*1024*1024L),
          "Size of a memory pool InnoDB uses to store data dictionary information and other internal data structures.");
  context("autoextend-increment",
          po::value<autoextend_constraint>(&innodb_auto_extend_increment)->default_value(64L),
          "Data file autoextend increment in megabytes");
  context("buffer-pool-size",
          po::value<buffer_pool_constraint>(&innobase_buffer_pool_size)->default_value(128*1024*1024L),
          "The size of the memory buffer InnoDB uses to cache data and indexes of its tables.");
  context("buffer-pool-instances",
          po::value<buffer_pool_instances_constraint>(&innobase_buffer_pool_instances)->default_value(1),
          "Number of buffer pool instances, set to higher value on high-end machines to increase scalability");

  context("commit-concurrency",
          po::value<concurrency_constraint>(&innobase_commit_concurrency)->default_value(0),
          "Helps in performance tuning in heavily concurrent environments.");
  context("concurrency-tickets",
          po::value<uint32_nonzero_constraint>(&innodb_concurrency_tickets)->default_value(500L),
          "Number of times a thread is allowed to enter InnoDB within the same SQL query after it has once got the ticket");
  context("read-io-threads",
          po::value<io_threads_constraint>(&innobase_read_io_threads)->default_value(4),
          "Number of background read I/O threads in InnoDB.");
  context("write-io-threads",
          po::value<io_threads_constraint>(&innobase_write_io_threads)->default_value(4),
          "Number of background write I/O threads in InnoDB.");
  context("force-recovery",
          po::value<force_recovery_constraint>(&innobase_force_recovery)->default_value(0),
          "Helps to save your data in case the disk image of the database becomes corrupt.");
  context("log-buffer-size",
          po::value<log_buffer_constraint>(&innobase_log_buffer_size)->default_value(8*1024*1024L),
          "The size of the buffer which InnoDB uses to write log to the log files on disk.");
  context("log-file-size",
          po::value<log_file_constraint>(&innobase_log_file_size)->default_value(20*1024*1024L),
          "The size of the buffer which InnoDB uses to write log to the log files on disk.");
  context("log-files-in-group",
          po::value<log_files_in_group_constraint>(&innobase_log_files_in_group)->default_value(2),
          "Number of log files in the log group. InnoDB writes to the files in a circular fashion.");
  context("mirrored-log-groups",
          po::value<mirrored_log_groups_constraint>(&innobase_mirrored_log_groups)->default_value(1),
          "Number of identical copies of log groups we keep for the database. Currently this should be set to 1.");
  context("open-files",
          po::value<open_files_constraint>(&innobase_open_files)->default_value(300L),
          "How many files at the maximum InnoDB keeps open at the same time.");
  context("sync-spin-loops",
          po::value<uint32_constraint>(&innodb_sync_spin_loops)->default_value(30L),
          "Count of spin-loop rounds in InnoDB mutexes (30 by default)");
  context("spin-wait-delay",
          po::value<uint32_constraint>(&innodb_spin_wait_delay)->default_value(6L),
          "Maximum delay between polling for a spin lock (6 by default)");
  context("thread-concurrency",
          po::value<concurrency_constraint>(&innobase_thread_concurrency)->default_value(0),
          "Helps in performance tuning in heavily concurrent environments. Sets the maximum number of threads allowed inside InnoDB. Value 0 will disable the thread throttling.");
  context("thread-sleep-delay",
          po::value<uint32_constraint>(&innodb_thread_sleep_delay)->default_value(10000L),
          "Time of innodb thread sleeping before joining InnoDB queue (usec). Value 0 disable a sleep");
  context("data-file-path",
          po::value<string>(),
          "Path to individual files and their sizes.");
  context("version",
          po::value<string>()->default_value(INNODB_VERSION_STR),
          "InnoDB version");
  context("use-internal-malloc",
          "Use InnoDB's internal memory allocator instal of the OS memory allocator.");
  context("disable-native-aio",
          _("Do not use Native AIO library for IO, even if available"));
  context("change-buffering",
          po::value<string>(&innobase_change_buffering),
          "Buffer changes to reduce random access: OFF, ON, inserting, deleting, changing, or purging.");
  context("read-ahead-threshold",
          po::value<read_ahead_threshold_constraint>(&innodb_read_ahead_threshold)->default_value(56),
          "Number of pages that must be accessed sequentially for InnoDB to trigger a readahead.");
  context("auto-lru-dump",
	  po::value<uint32_constraint>(&buffer_pool_restore_at_startup)->default_value(0),
	  "Time in seconds between automatic buffer pool dumps. "
	  "0 (the default) disables automatic dumps.");
  context("ibuf-max-size",
          po::value<uint64_constraint>(&ibuf_max_size)->default_value(UINT64_MAX),
          "The maximum size of the insert buffer (in bytes).");
  context("ibuf-active-contract",
          po::value<binary_constraint>(&ibuf_active_contract)->default_value(1),
          "Enable/Disable active_contract of insert buffer. 0:disable 1:enable");
  context("ibuf-accel-rate",
          po::value<ibuf_accel_rate_constraint>(&ibuf_accel_rate)->default_value(100),
          "Tunes amount of insert buffer processing of background, in addition to innodb_io_capacity. (in percentage)");
  context("checkpoint-age-target",
          po::value<uint32_constraint>(&checkpoint_age_target)->default_value(0),
          "Control soft limit of checkpoint age. (0 : not control)");
  context("flush-neighbor-pages",
          po::value<binary_constraint>(&flush_neighbor_pages)->default_value(1),
          "Enable/Disable flushing also neighbor pages. 0:disable 1:enable");
  context("read-ahead",
          po::value<string>(&read_ahead)->default_value("linear"),
          "Control read ahead activity (none, random, [linear], both). [from 1.0.5: random read ahead is ignored]");
  context("adaptive-flushing-method",
          po::value<string>(&adaptive_flushing_method)->default_value("estimate"),
          "Choose method of innodb_adaptive_flushing. (native, [estimate], keep_average)");
  context("disable-xa",
          "Disable InnoDB support for the XA two-phase commit");
  context("disable-table-locks",
          "Disable InnoDB locking in LOCK TABLES");
  context("strict-mode",
          po::value<bool>(&strict_mode)->default_value(false)->zero_tokens(),
          "Use strict mode when evaluating create options.");
  context("replication-log",
          po::value<bool>(&innobase_use_replication_log)->default_value(false)->zero_tokens(),
          _("Enable internal replication log."));
  context("lock-wait-timeout",
          po::value<lock_wait_constraint>(&lock_wait_timeout)->default_value(50),
          _("Timeout in seconds an InnoDB transaction may wait for a lock before being rolled back. Values above 100000000 disable the timeout."));
  context("old-blocks-pct",
          po::value<old_blocks_constraint>(&innobase_old_blocks_pct)->default_value(100 * 3 / 8),
          _("Percentage of the buffer pool to reserve for 'old' blocks."));
  context("old-blocks-time",
          po::value<uint32_t>(&buf_LRU_old_threshold_ms)->default_value(0),
          _("ove blocks to the 'new' end of the buffer pool if the first access"
            " was at least this many milliseconds ago."
            " The timeout is disabled if 0 (the default)."));
}



DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  innobase_engine_name,
  INNODB_VERSION_STR,
  "Innobase Oy",
  "Supports transactions, row-level locking, and foreign keys",
  PLUGIN_LICENSE_GPL,
  innobase_init, /* Plugin Init */
  NULL, /* depends */
  init_options /* reserved */
}
DRIZZLE_DECLARE_PLUGIN_END;

int ha_innobase::read_range_first(const key_range *start_key,
          const key_range *end_key,
          bool eq_range_arg,
          bool sorted)
{
  int res;
  //if (!eq_range_arg)
    //in_range_read= TRUE;
  res= Cursor::read_range_first(start_key, end_key, eq_range_arg, sorted);
  //if (res)
  //  in_range_read= FALSE;
  return res;
}


int ha_innobase::read_range_next()
{
  int res= Cursor::read_range_next();
  //if (res)
  //  in_range_read= FALSE;
  return res;
}

/***********************************************************************
This function checks each index name for a table against reserved
system default primary index name 'GEN_CLUST_INDEX'. If a name matches,
this function pushes an warning message to the client, and returns true. */
UNIV_INTERN
bool
innobase_index_name_is_reserved(
/*============================*/
					/* out: true if an index name
					matches the reserved name */
	const trx_t*	trx,		/* in: InnoDB transaction handle */
	const KeyInfo*	key_info,	/* in: Indexes to be created */
	ulint		num_of_keys)	/* in: Number of indexes to
					be created. */
{
  const KeyInfo*	key;
  uint		key_num;	/* index number */

  for (key_num = 0; key_num < num_of_keys; key_num++) {
    key = &key_info[key_num];

    if (innobase_strcasecmp(key->name,
                            innobase_index_reserve_name) == 0) {
      /* Push warning to drizzle */
      push_warning_printf(trx->mysql_thd,
                          DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_WRONG_NAME_FOR_INDEX,
                          "Cannot Create Index with name "
                          "'%s'. The name is reserved "
                          "for the system default primary "
                          "index.",
                          innobase_index_reserve_name);

      my_error(ER_WRONG_NAME_FOR_INDEX, MYF(0),
               innobase_index_reserve_name);

      return(true);
    }
  }

  return(false);
}

#ifdef UNIV_COMPILE_TEST_FUNCS

typedef struct innobase_convert_name_test_struct {
  char*   buf;
  ulint   buflen;
  const char* id;
  ulint   idlen;
  drizzled::Session *session;
  ibool   file_id;

  const char* expected;
} innobase_convert_name_test_t;

void
test_innobase_convert_name()
{
  char  buf[1024];
  ulint i;

  innobase_convert_name_test_t test_input[] = {
    {buf, sizeof(buf), "abcd", 4, NULL, TRUE, "\"abcd\""},
    {buf, 7, "abcd", 4, NULL, TRUE, "\"abcd\""},
    {buf, 6, "abcd", 4, NULL, TRUE, "\"abcd\""},
    {buf, 5, "abcd", 4, NULL, TRUE, "\"abc\""},
    {buf, 4, "abcd", 4, NULL, TRUE, "\"ab\""},

    {buf, sizeof(buf), "ab@0060cd", 9, NULL, TRUE, "\"ab`cd\""},
    {buf, 9, "ab@0060cd", 9, NULL, TRUE, "\"ab`cd\""},
    {buf, 8, "ab@0060cd", 9, NULL, TRUE, "\"ab`cd\""},
    {buf, 7, "ab@0060cd", 9, NULL, TRUE, "\"ab`cd\""},
    {buf, 6, "ab@0060cd", 9, NULL, TRUE, "\"ab`c\""},
    {buf, 5, "ab@0060cd", 9, NULL, TRUE, "\"ab`\""},
    {buf, 4, "ab@0060cd", 9, NULL, TRUE, "\"ab\""},

    {buf, sizeof(buf), "ab\"cd", 5, NULL, TRUE,
      "\"#mysql50#ab\"\"cd\""},
    {buf, 17, "ab\"cd", 5, NULL, TRUE,
      "\"#mysql50#ab\"\"cd\""},
    {buf, 16, "ab\"cd", 5, NULL, TRUE,
      "\"#mysql50#ab\"\"c\""},
    {buf, 15, "ab\"cd", 5, NULL, TRUE,
      "\"#mysql50#ab\"\"\""},
    {buf, 14, "ab\"cd", 5, NULL, TRUE,
      "\"#mysql50#ab\""},
    {buf, 13, "ab\"cd", 5, NULL, TRUE,
      "\"#mysql50#ab\""},
    {buf, 12, "ab\"cd", 5, NULL, TRUE,
      "\"#mysql50#a\""},
    {buf, 11, "ab\"cd", 5, NULL, TRUE,
      "\"#mysql50#\""},
    {buf, 10, "ab\"cd", 5, NULL, TRUE,
      "\"#mysql50\""},

    {buf, sizeof(buf), "ab/cd", 5, NULL, TRUE, "\"ab\".\"cd\""},
    {buf, 9, "ab/cd", 5, NULL, TRUE, "\"ab\".\"cd\""},
    {buf, 8, "ab/cd", 5, NULL, TRUE, "\"ab\".\"c\""},
    {buf, 7, "ab/cd", 5, NULL, TRUE, "\"ab\".\"\""},
    {buf, 6, "ab/cd", 5, NULL, TRUE, "\"ab\"."},
    {buf, 5, "ab/cd", 5, NULL, TRUE, "\"ab\"."},
    {buf, 4, "ab/cd", 5, NULL, TRUE, "\"ab\""},
    {buf, 3, "ab/cd", 5, NULL, TRUE, "\"a\""},
    {buf, 2, "ab/cd", 5, NULL, TRUE, "\"\""},
    /* XXX probably "" is a better result in this case
    {buf, 1, "ab/cd", 5, NULL, TRUE, "."},
    */
    {buf, 0, "ab/cd", 5, NULL, TRUE, ""},
  };

  for (i = 0; i < sizeof(test_input) / sizeof(test_input[0]); i++) {

    char* end;
    ibool ok = TRUE;
    size_t  res_len;

    fprintf(stderr, "TESTING %lu, %s, %lu, %s\n",
      test_input[i].buflen,
      test_input[i].id,
      test_input[i].idlen,
      test_input[i].expected);

    end = innobase_convert_name(
      test_input[i].buf,
      test_input[i].buflen,
      test_input[i].id,
      test_input[i].idlen,
      test_input[i].session,
      test_input[i].file_id);

    res_len = (size_t) (end - test_input[i].buf);

    if (res_len != strlen(test_input[i].expected)) {

      fprintf(stderr, "unexpected len of the result: %u, "
        "expected: %u\n", (unsigned) res_len,
        (unsigned) strlen(test_input[i].expected));
      ok = FALSE;
    }

    if (memcmp(test_input[i].buf,
         test_input[i].expected,
         strlen(test_input[i].expected)) != 0
        || !ok) {

      fprintf(stderr, "unexpected result: %.*s, "
        "expected: %s\n", (int) res_len,
        test_input[i].buf,
        test_input[i].expected);
      ok = FALSE;
    }

    if (ok) {
      fprintf(stderr, "OK: res: %.*s\n\n", (int) res_len,
        buf);
    } else {
      fprintf(stderr, "FAILED\n\n");
      return;
    }
  }
}

#endif /* UNIV_COMPILE_TEST_FUNCS */
