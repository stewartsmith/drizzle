/*
  Copyright (C) 2010 Stewart Smith

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* innobase_get_int_col_max_value() comes from ha_innodb.cc which is under
   the following license and Copyright */

/*****************************************************************************

Copyright (c) 2000, 2009, MySQL AB & Innobase Oy. All Rights Reserved.
Copyright (c) 2008, 2009 Google Inc.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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
/***********************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.
Copyright (c) 2009, Percona Inc.

Portions of this file contain modifications contributed and copyrighted
by Percona Inc.. Those modifications are
gratefully acknowledged and are described briefly in the InnoDB
documentation. The contributions by Percona Inc. are incorporated with
their permission, and subject to the conditions contained in the file
COPYING.Percona.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

***********************************************************************/


#include "config.h"
#include <drizzled/table.h>
#include <drizzled/error.h>
#include "drizzled/internal/my_pthread.h"
#include <drizzled/plugin/transactional_storage_engine.h>

#include <fcntl.h>

#include <string>
#include <boost/algorithm/string.hpp>
#include <boost/unordered_set.hpp>
#include <boost/foreach.hpp>
#include <map>
#include <fstream>
#include <drizzled/message/table.pb.h>
#include "drizzled/internal/m_string.h"

#include "drizzled/global_charset_info.h"

#include "haildb_datadict_dump_func.h"
#include "config_table_function.h"
#include "status_table_function.h"

#include <haildb.h>

#include "haildb_engine.h"

#include <drizzled/field.h>
#include "drizzled/field/timestamp.h" // needed for UPDATE NOW()
#include "drizzled/field/blob.h"
#include "drizzled/field/enum.h"
#include <drizzled/session.h>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <iostream>
#include <drizzled/charset.h>

namespace po= boost::program_options;
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace google;
using namespace drizzled;

int read_row_from_haildb(unsigned char* buf, ib_crsr_t cursor, ib_tpl_t tuple, Table* table, bool has_hidden_primary_key, uint64_t *hidden_pkey, drizzled::memory::Root **blobroot= NULL);
static void fill_ib_search_tpl_from_drizzle_key(ib_tpl_t search_tuple,
                                                const drizzled::KeyInfo *key_info,
                                                const unsigned char *key_ptr,
                                                uint32_t key_len);
static void store_key_value_from_haildb(KeyInfo *key_info, unsigned char* ref, int ref_len, const unsigned char *record);

#define HAILDB_EXT ".EID"

const char HAILDB_TABLE_DEFINITIONS_TABLE[]= "data_dictionary/haildb_table_definitions";
const string statement_savepoint_name("STATEMENT");

static boost::unordered_set<std::string> haildb_system_table_names;


static const char *HailDBCursor_exts[] = {
  NULL
};

class HailDBEngine : public drizzled::plugin::TransactionalStorageEngine
{
public:
  HailDBEngine(const string &name_arg)
   : drizzled::plugin::TransactionalStorageEngine(name_arg,
                                                  HTON_NULL_IN_KEY |
                                                  HTON_CAN_INDEX_BLOBS |
                                                  HTON_AUTO_PART_KEY |
                                                  HTON_PARTIAL_COLUMN_READ |
                                                  HTON_HAS_DOES_TRANSACTIONS)
  {
    table_definition_ext= HAILDB_EXT;
  }

  ~HailDBEngine();

  virtual Cursor *create(Table &table)
  {
    return new HailDBCursor(*this, table);
  }

  const char **bas_ext() const {
    return HailDBCursor_exts;
  }

  bool validateCreateTableOption(const std::string &key,
                                 const std::string &state);

  int doCreateTable(Session&,
                    Table& table_arg,
                    const drizzled::TableIdentifier &identifier,
                    drizzled::message::Table& proto);

  int doDropTable(Session&, const TableIdentifier &identifier);

  int doRenameTable(drizzled::Session&,
                    const drizzled::TableIdentifier&,
                    const drizzled::TableIdentifier&);

  int doGetTableDefinition(Session& session,
                           const TableIdentifier &identifier,
                           drizzled::message::Table &table_proto);

  bool doDoesTableExist(Session&, const TableIdentifier &identifier);

private:
  void getTableNamesInSchemaFromHailDB(const drizzled::SchemaIdentifier &schema,
                                       drizzled::plugin::TableNameList *set_of_names,
                                       drizzled::TableIdentifiers *identifiers);

public:
  void doGetTableIdentifiers(drizzled::CachedDirectory &,
                             const drizzled::SchemaIdentifier &schema,
                             drizzled::TableIdentifiers &identifiers);

  /* The following defines can be increased if necessary */
  uint32_t max_supported_keys()          const { return 1000; }
  uint32_t max_supported_key_length()    const { return 3500; }
  uint32_t max_supported_key_part_length() const { return 767; }

  uint32_t index_flags(enum  ha_key_alg) const
  {
    return (HA_READ_NEXT |
            HA_READ_PREV |
            HA_READ_RANGE |
            HA_READ_ORDER |
            HA_KEYREAD_ONLY);
  }
  virtual int doStartTransaction(Session *session,
                                 start_transaction_option_t options);
  virtual void doStartStatement(Session *session);
  virtual void doEndStatement(Session *session);

  virtual int doSetSavepoint(Session* session,
                                 drizzled::NamedSavepoint &savepoint);
  virtual int doRollbackToSavepoint(Session* session,
                                     drizzled::NamedSavepoint &savepoint);
  virtual int doReleaseSavepoint(Session* session,
                                     drizzled::NamedSavepoint &savepoint);
  virtual int doCommit(Session* session, bool all);
  virtual int doRollback(Session* session, bool all);

  typedef std::map<std::string, HailDBTableShare*> HailDBMap;
  HailDBMap haildb_open_tables;
  HailDBTableShare *findOpenTable(const std::string table_name);
  void addOpenTable(const std::string &table_name, HailDBTableShare *);
  void deleteOpenTable(const std::string &table_name);

  uint64_t getInitialAutoIncrementValue(HailDBCursor *cursor);
  uint64_t getHiddenPrimaryKeyInitialAutoIncrementValue(HailDBCursor *cursor);

};

static drizzled::plugin::StorageEngine *haildb_engine= NULL;


static ib_trx_t* get_trx(Session* session)
{
  return (ib_trx_t*) session->getEngineData(haildb_engine);
}

/* This is a superset of the map from innobase plugin.
   Unlike innobase plugin we don't act on errors here, we just
   map error codes. */
static int ib_err_t_to_drizzle_error(ib_err_t err)
{
  switch (err)
  {
  case DB_SUCCESS:
    return 0;

  case DB_ERROR:
  default:
    return -1;

  case DB_INTERRUPTED:
    return ER_QUERY_INTERRUPTED; // FIXME: is this correct?

  case DB_OUT_OF_MEMORY:
    return HA_ERR_OUT_OF_MEM;

  case DB_DUPLICATE_KEY:
    return HA_ERR_FOUND_DUPP_KEY;

  case DB_FOREIGN_DUPLICATE_KEY:
    return HA_ERR_FOREIGN_DUPLICATE_KEY;

  case DB_MISSING_HISTORY:
    return HA_ERR_TABLE_DEF_CHANGED;

  case DB_RECORD_NOT_FOUND:
    return HA_ERR_NO_ACTIVE_RECORD;

  case DB_DEADLOCK:
    return HA_ERR_LOCK_DEADLOCK;

  case DB_LOCK_WAIT_TIMEOUT:
    return HA_ERR_LOCK_WAIT_TIMEOUT;

  case DB_NO_REFERENCED_ROW:
    return HA_ERR_NO_REFERENCED_ROW;

  case DB_ROW_IS_REFERENCED:
    return HA_ERR_ROW_IS_REFERENCED;

  case DB_CANNOT_ADD_CONSTRAINT:
    return HA_ERR_CANNOT_ADD_FOREIGN;

  case DB_CANNOT_DROP_CONSTRAINT:
    return HA_ERR_ROW_IS_REFERENCED; /* misleading. should have new err code */

  case DB_COL_APPEARS_TWICE_IN_INDEX:
  case DB_CORRUPTION:
    return HA_ERR_CRASHED;

  case DB_MUST_GET_MORE_FILE_SPACE:
  case DB_OUT_OF_FILE_SPACE:
    return HA_ERR_RECORD_FILE_FULL;

  case DB_TABLE_IS_BEING_USED:
    return HA_ERR_WRONG_COMMAND;

  case DB_TABLE_NOT_FOUND:
    return HA_ERR_NO_SUCH_TABLE;

  case DB_TOO_BIG_RECORD:
    return HA_ERR_TO_BIG_ROW;

  case DB_NO_SAVEPOINT:
    return HA_ERR_NO_SAVEPOINT;

  case DB_LOCK_TABLE_FULL:
    return HA_ERR_LOCK_TABLE_FULL;

  case DB_PRIMARY_KEY_IS_NULL:
    return ER_PRIMARY_CANT_HAVE_NULL;

  case DB_TOO_MANY_CONCURRENT_TRXS:
    return HA_ERR_RECORD_FILE_FULL; /* need better error code */

  case DB_END_OF_INDEX:
    return HA_ERR_END_OF_FILE;

  case DB_UNSUPPORTED:
    return HA_ERR_UNSUPPORTED;
  }
}

static ib_trx_level_t tx_isolation_to_ib_trx_level(enum_tx_isolation level)
{
  switch(level)
  {
  case ISO_REPEATABLE_READ:
    return IB_TRX_REPEATABLE_READ;
  case ISO_READ_COMMITTED:
    return IB_TRX_READ_COMMITTED;
  case ISO_SERIALIZABLE:
    return IB_TRX_SERIALIZABLE;
  case ISO_READ_UNCOMMITTED:
    return IB_TRX_READ_UNCOMMITTED;
  }

  assert(0);
  return IB_TRX_REPEATABLE_READ;
}

int HailDBEngine::doStartTransaction(Session *session,
                                             start_transaction_option_t options)
{
  ib_trx_t *transaction;
  ib_trx_level_t isolation_level;

  (void)options;

  transaction= get_trx(session);
  isolation_level= tx_isolation_to_ib_trx_level((enum_tx_isolation)session_tx_isolation(session));
  *transaction= ib_trx_begin(isolation_level);

  return 0;
}

void HailDBEngine::doStartStatement(Session *session)
{
  if(*get_trx(session) == NULL)
    doStartTransaction(session, START_TRANS_NO_OPTIONS);

  ib_savepoint_take(*get_trx(session), statement_savepoint_name.c_str(),
                    statement_savepoint_name.length());
}

void HailDBEngine::doEndStatement(Session *)
{
}

int HailDBEngine::doSetSavepoint(Session* session,
                                         drizzled::NamedSavepoint &savepoint)
{
  ib_trx_t *transaction= get_trx(session);
  ib_savepoint_take(*transaction, savepoint.getName().c_str(),
                    savepoint.getName().length());
  return 0;
}

int HailDBEngine::doRollbackToSavepoint(Session* session,
                                                drizzled::NamedSavepoint &savepoint)
{
  ib_trx_t *transaction= get_trx(session);
  ib_err_t err;

  err= ib_savepoint_rollback(*transaction, savepoint.getName().c_str(),
                             savepoint.getName().length());

  return ib_err_t_to_drizzle_error(err);
}

int HailDBEngine::doReleaseSavepoint(Session* session,
                                             drizzled::NamedSavepoint &savepoint)
{
  ib_trx_t *transaction= get_trx(session);
  ib_err_t err;

  err= ib_savepoint_release(*transaction, savepoint.getName().c_str(),
                            savepoint.getName().length());
  if (err != DB_SUCCESS)
    return ib_err_t_to_drizzle_error(err);

  return 0;
}

int HailDBEngine::doCommit(Session* session, bool all)
{
  ib_err_t err;
  ib_trx_t *transaction= get_trx(session);

  if (all || (!session_test_options(session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
  {
    err= ib_trx_commit(*transaction);

    if (err != DB_SUCCESS)
      return ib_err_t_to_drizzle_error(err);

    *transaction= NULL;
  }

  return 0;
}

int HailDBEngine::doRollback(Session* session, bool all)
{
  ib_err_t err;
  ib_trx_t *transaction= get_trx(session);

  if (all || !session_test_options(session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
  {
    err= ib_trx_rollback(*transaction);

    if (err != DB_SUCCESS)
      return ib_err_t_to_drizzle_error(err);

    *transaction= NULL;
  }
  else
  {
    err= ib_savepoint_rollback(*transaction, statement_savepoint_name.c_str(),
                               statement_savepoint_name.length());
    if (err != DB_SUCCESS)
      return ib_err_t_to_drizzle_error(err);
  }

  return 0;
}

HailDBTableShare *HailDBEngine::findOpenTable(const string table_name)
{
  HailDBMap::iterator find_iter=
    haildb_open_tables.find(table_name);

  if (find_iter != haildb_open_tables.end())
    return (*find_iter).second;
  else
    return NULL;
}

void HailDBEngine::addOpenTable(const string &table_name, HailDBTableShare *share)
{
  haildb_open_tables[table_name]= share;
}

void HailDBEngine::deleteOpenTable(const string &table_name)
{
  haildb_open_tables.erase(table_name);
}

static pthread_mutex_t haildb_mutex= PTHREAD_MUTEX_INITIALIZER;

uint64_t HailDBCursor::getHiddenPrimaryKeyInitialAutoIncrementValue()
{
  uint64_t nr;
  ib_err_t err;
  ib_trx_t transaction= *get_trx(getTable()->in_use);
  ib_cursor_attach_trx(cursor, transaction);
  tuple= ib_clust_read_tuple_create(cursor);
  err= ib_cursor_last(cursor);
  assert(err == DB_SUCCESS || err == DB_END_OF_INDEX); // Probably a FIXME
  err= ib_cursor_read_row(cursor, tuple);
  if (err == DB_RECORD_NOT_FOUND)
    nr= 1;
  else
  {
    assert (err == DB_SUCCESS);
    err= ib_tuple_read_u64(tuple, getTable()->getShare()->fields, &nr);
    nr++;
  }
  ib_tuple_delete(tuple);
  err= ib_cursor_reset(cursor);
  assert(err == DB_SUCCESS);
  return nr;
}

uint64_t HailDBCursor::getInitialAutoIncrementValue()
{
  uint64_t nr;
  int error;

  (void) extra(HA_EXTRA_KEYREAD);
  getTable()->mark_columns_used_by_index_no_reset(getTable()->getShare()->next_number_index);
  doStartIndexScan(getTable()->getShare()->next_number_index, 1);
  if (getTable()->getShare()->next_number_keypart == 0)
  {						// Autoincrement at key-start
    error=index_last(getTable()->getUpdateRecord());
  }
  else
  {
    unsigned char key[MAX_KEY_LENGTH];
    key_copy(key, getTable()->getInsertRecord(),
             getTable()->key_info + getTable()->getShare()->next_number_index,
             getTable()->getShare()->next_number_key_offset);
    error= index_read_map(getTable()->getUpdateRecord(), key,
                          make_prev_keypart_map(getTable()->getShare()->next_number_keypart),
                          HA_READ_PREFIX_LAST);
  }

  if (error)
    nr=1;
  else
    nr= ((uint64_t) getTable()->found_next_number_field->
         val_int_offset(getTable()->getShare()->rec_buff_length)+1);
  doEndIndexScan();
  (void) extra(HA_EXTRA_NO_KEYREAD);

  if (getTable()->getShare()->getTableProto()->options().auto_increment_value() > nr)
    nr= getTable()->getShare()->getTableProto()->options().auto_increment_value();

  return nr;
}

HailDBTableShare::HailDBTableShare(const char* name, bool hidden_primary_key)
  : use_count(0), has_hidden_primary_key(hidden_primary_key)
{
  table_name.assign(name);
}

uint64_t HailDBEngine::getInitialAutoIncrementValue(HailDBCursor *cursor)
{
  doStartTransaction(current_session, START_TRANS_NO_OPTIONS);
  uint64_t initial_auto_increment_value= cursor->getInitialAutoIncrementValue();
  doCommit(current_session, true);

  return initial_auto_increment_value;
}

uint64_t HailDBEngine::getHiddenPrimaryKeyInitialAutoIncrementValue(HailDBCursor *cursor)
{
  doStartTransaction(current_session, START_TRANS_NO_OPTIONS);
  uint64_t initial_auto_increment_value= cursor->getHiddenPrimaryKeyInitialAutoIncrementValue();
  doCommit(current_session, true);

  return initial_auto_increment_value;
}

HailDBTableShare *HailDBCursor::get_share(const char *table_name, bool has_hidden_primary_key, int *rc)
{
  pthread_mutex_lock(&haildb_mutex);

  HailDBEngine *a_engine= static_cast<HailDBEngine *>(getEngine());
  share= a_engine->findOpenTable(table_name);

  if (!share)
  {
    share= new HailDBTableShare(table_name, has_hidden_primary_key);

    if (share == NULL)
    {
      pthread_mutex_unlock(&haildb_mutex);
      *rc= HA_ERR_OUT_OF_MEM;
      return(NULL);
    }

    if (getTable()->found_next_number_field)
    {
      share->auto_increment_value.fetch_and_store(
                                  a_engine->getInitialAutoIncrementValue(this));

    }

    if (has_hidden_primary_key)
    {
      uint64_t hidden_pkey= 0;
      hidden_pkey= a_engine->getHiddenPrimaryKeyInitialAutoIncrementValue(this);
      share->hidden_pkey_auto_increment_value.fetch_and_store(hidden_pkey);
    }

    a_engine->addOpenTable(share->table_name, share);
    thr_lock_init(&share->lock);
  }
  share->use_count++;

  pthread_mutex_unlock(&haildb_mutex);

  return(share);
}

int HailDBCursor::free_share()
{
  pthread_mutex_lock(&haildb_mutex);
  if (!--share->use_count)
  {
    HailDBEngine *a_engine= static_cast<HailDBEngine *>(getEngine());
    a_engine->deleteOpenTable(share->table_name);
    delete share;
  }
  pthread_mutex_unlock(&haildb_mutex);

  return 0;
}


THR_LOCK_DATA **HailDBCursor::store_lock(Session *session,
                                                 THR_LOCK_DATA **to,
                                                 thr_lock_type lock_type)
{
  /* Currently, we can get a transaction start by ::store_lock
     instead of beginTransaction, startStatement.

     See https://bugs.launchpad.net/drizzle/+bug/535528

     all stemming from the transactional engine interface needing
     a severe amount of immodium.
   */

  if(*get_trx(session) == NULL)
  {
    static_cast<HailDBEngine*>(getEngine())->
                    doStartTransaction(session, START_TRANS_NO_OPTIONS);
  }

  if (lock_type != TL_UNLOCK)
  {
    ib_savepoint_take(*get_trx(session), statement_savepoint_name.c_str(),
                      statement_savepoint_name.length());
  }

  /* the below is adapted from ha_innodb.cc */

  const uint32_t sql_command = session_sql_command(session);

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

    enum_tx_isolation isolation_level= session_tx_isolation(session);

    if (isolation_level != ISO_SERIALIZABLE
        && (lock_type == TL_READ || lock_type == TL_READ_NO_INSERT)
        && (sql_command == SQLCOM_INSERT_SELECT
      || sql_command == SQLCOM_UPDATE
      || sql_command == SQLCOM_CREATE_TABLE)) {

      /* If we either have innobase_locks_unsafe_for_binlog
      option set or this session is using READ COMMITTED
      isolation level and isolation level of the transaction
      is not set to serializable and MySQL is doing
      INSERT INTO...SELECT or UPDATE ... = (SELECT ...) or
      CREATE  ... SELECT... without FOR UPDATE or
      IN SHARE MODE in select, then we use consistent
      read for select. */

      ib_lock_mode= IB_LOCK_NONE;
    } else if (sql_command == SQLCOM_CHECKSUM) {
      /* Use consistent read for checksum table */

      ib_lock_mode= IB_LOCK_NONE;
    } else {
      ib_lock_mode= IB_LOCK_S;
    }

  } else if (lock_type != TL_IGNORE) {

    /* We set possible LOCK_X value in external_lock, not yet
    here even if this would be SELECT ... FOR UPDATE */
    ib_lock_mode= IB_LOCK_NONE;
  }

  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK) {

    /* If we are not doing a LOCK TABLE, DISCARD/IMPORT
    TABLESPACE or TRUNCATE TABLE then allow multiple
    writers. Note that ALTER TABLE uses a TL_WRITE_ALLOW_READ
    < TL_WRITE_CONCURRENT_INSERT.
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT
         && lock_type <= TL_WRITE)
        && !session_tablespace_op(session)
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

  return to;
}

void HailDBCursor::get_auto_increment(uint64_t, //offset,
                                              uint64_t, //increment,
                                              uint64_t, //nb_dis,
                                              uint64_t *first_value,
                                              uint64_t *nb_reserved_values)
{
fetch:
  *first_value= share->auto_increment_value.fetch_and_increment();
  if (*first_value == 0)
  {
    /* if it's zero, then we skip it... why? because of ass.
       set auto-inc to -1 and the sequence is:
       -1, 1.
       Zero is still "magic".
    */
    share->auto_increment_value.compare_and_swap(1, 0);
    goto fetch;
  }
  *nb_reserved_values= 1;
}

static const char* table_path_to_haildb_name(const char* name)
{
  size_t l= strlen(name);
  static string datadict_path("data_dictionary/");
  static string sys_prefix("data_dictionary/haildb_");
  static string sys_table_prefix("HAILDB_");

  if (strncmp(name, sys_prefix.c_str(), sys_prefix.length()) == 0)
  {
    string find_name(name+datadict_path.length());
    std::transform(find_name.begin(), find_name.end(), find_name.begin(), ::toupper);
    boost::unordered_set<string>::iterator iter= haildb_system_table_names.find(find_name);
    if (iter != haildb_system_table_names.end())
      return (*iter).c_str()+sys_table_prefix.length();
  }

  int slashes= 2;
  while(slashes>0 && l > 0)
  {
    l--;
    if (name[l] == '/')
      slashes--;
  }
  if (slashes==0)
    l++;

  return &name[l];
}

static void TableIdentifier_to_haildb_name(const TableIdentifier &identifier, std::string *str)
{
  str->assign(table_path_to_haildb_name(identifier.getPath().c_str()));
}

HailDBCursor::HailDBCursor(drizzled::plugin::StorageEngine &engine_arg,
                           Table &table_arg)
  :Cursor(engine_arg, table_arg),
   ib_lock_mode(IB_LOCK_NONE),
   write_can_replace(false),
   blobroot(NULL)
{ }

static unsigned int get_first_unique_index(drizzled::Table &table)
{
  for (uint32_t k= 0; k < table.getShare()->keys; k++)
  {
    if (table.key_info[k].flags & HA_NOSAME)
    {
      return k;
    }
  }

  return 0;
}

int HailDBCursor::open(const char *name, int, uint32_t)
{
  const char* haildb_table_name= table_path_to_haildb_name(name);
  ib_err_t err= ib_cursor_open_table(haildb_table_name, NULL, &cursor);
  bool has_hidden_primary_key= false;
  ib_id_t idx_id;

  if (err != DB_SUCCESS)
    return ib_err_t_to_drizzle_error(err);

  err= ib_index_get_id(haildb_table_name, "HIDDEN_PRIMARY", &idx_id);

  if (err == DB_SUCCESS)
    has_hidden_primary_key= true;

  int rc;
  share= get_share(name, has_hidden_primary_key, &rc);
  lock.init(&share->lock);


  if (getTable()->getShare()->getPrimaryKey() != MAX_KEY)
    ref_length= getTable()->key_info[getTable()->getShare()->getPrimaryKey()].key_length;
  else if (share->has_hidden_primary_key)
    ref_length= sizeof(uint64_t);
  else
  {
    unsigned int keynr= get_first_unique_index(*getTable());
    ref_length= getTable()->key_info[keynr].key_length;
  }

  in_table_scan= false;

  return(0);
}

int HailDBCursor::close(void)
{
  ib_err_t err= ib_cursor_close(cursor);
  if (err != DB_SUCCESS)
    return ib_err_t_to_drizzle_error(err);

  free_share();

  delete blobroot;
  blobroot= NULL;

  return 0;
}

int HailDBCursor::external_lock(Session* session, int lock_type)
{
  ib_cursor_stmt_begin(cursor);

  (void)session;

  if (lock_type == F_WRLCK)
  {
    /* SELECT ... FOR UPDATE or UPDATE TABLE */
    ib_lock_mode= IB_LOCK_X;
  }
  else
    ib_lock_mode= IB_LOCK_NONE;

  return 0;
}

static int create_table_add_field(ib_tbl_sch_t schema,
                                  const message::Table::Field &field,
                                  ib_err_t *err)
{
  ib_col_attr_t column_attr= IB_COL_NONE;

  if (field.has_constraints() && ! field.constraints().is_nullable())
    column_attr= IB_COL_NOT_NULL;

  switch (field.type())
  {
  case message::Table::Field::VARCHAR:
    *err= ib_table_schema_add_col(schema, field.name().c_str(), IB_VARCHAR,
                                  column_attr, 0,
                                  field.string_options().length());
    break;
  case message::Table::Field::INTEGER:
    *err= ib_table_schema_add_col(schema, field.name().c_str(), IB_INT,
                                  column_attr, 0, 4);
    break;
  case message::Table::Field::BIGINT:
    *err= ib_table_schema_add_col(schema, field.name().c_str(), IB_INT,
                                  column_attr, 0, 8);
    break;
  case message::Table::Field::DOUBLE:
  case message::Table::Field::DATETIME:
    *err= ib_table_schema_add_col(schema, field.name().c_str(), IB_DOUBLE,
                                  column_attr, 0, sizeof(double));
    break;
  case message::Table::Field::ENUM:
    *err= ib_table_schema_add_col(schema, field.name().c_str(), IB_INT,
                                  column_attr, 0, 4);
    break;
  case message::Table::Field::DATE:
    *err= ib_table_schema_add_col(schema, field.name().c_str(), IB_INT,
                                  column_attr, 0, 4);
    break;
  case message::Table::Field::TIMESTAMP:
    *err= ib_table_schema_add_col(schema, field.name().c_str(), IB_INT,
                                  column_attr, 0, 8);
    break;
  case message::Table::Field::BLOB:
    *err= ib_table_schema_add_col(schema, field.name().c_str(), IB_BLOB,
                                  column_attr, 0, 0);
    break;
  case message::Table::Field::DECIMAL:
    *err= ib_table_schema_add_col(schema, field.name().c_str(), IB_DECIMAL,
                                  column_attr, 0, 0);
    break;
  default:
    my_error(ER_CHECK_NOT_IMPLEMENTED, MYF(0), "Column Type");
    return(HA_ERR_UNSUPPORTED);
  }

  return 0;
}

static ib_err_t store_table_message(ib_trx_t transaction, const char* table_name, drizzled::message::Table& table_message)
{
  ib_crsr_t cursor;
  ib_tpl_t message_tuple;
  ib_err_t err;
  string serialized_message;

  err= ib_cursor_open_table(HAILDB_TABLE_DEFINITIONS_TABLE, transaction, &cursor);
  if (err != DB_SUCCESS)
    return err;

  message_tuple= ib_clust_read_tuple_create(cursor);

  err= ib_col_set_value(message_tuple, 0, table_name, strlen(table_name));
  if (err != DB_SUCCESS)
    goto cleanup;

  try {
    table_message.SerializeToString(&serialized_message);
  }
  catch (...)
  {
    goto cleanup;
  }

  err= ib_col_set_value(message_tuple, 1, serialized_message.c_str(),
                        serialized_message.length());
  if (err != DB_SUCCESS)
    goto cleanup;

  err= ib_cursor_insert_row(cursor, message_tuple);

cleanup:
  ib_tuple_delete(message_tuple);

  ib_err_t cleanup_err= ib_cursor_close(cursor);
  if (err == DB_SUCCESS)
    err= cleanup_err;

  return err;
}

bool HailDBEngine::validateCreateTableOption(const std::string &key,
                                                     const std::string &state)
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

static ib_tbl_fmt_t parse_ib_table_format(const std::string &value)
{
  if (boost::iequals(value, "REDUNDANT"))
    return IB_TBL_REDUNDANT;
  else if (boost::iequals(value, "COMPACT"))
    return IB_TBL_COMPACT;
  else if (boost::iequals(value, "DYNAMIC"))
    return IB_TBL_DYNAMIC;
  else if (boost::iequals(value, "COMPRESSED"))
    return IB_TBL_COMPRESSED;

  assert(false); /* You need to add possible table formats here */
  return IB_TBL_COMPACT;
}

int HailDBEngine::doCreateTable(Session &session,
                                        Table& table_obj,
                                        const drizzled::TableIdentifier &identifier,
                                        drizzled::message::Table& table_message)
{
  ib_tbl_sch_t haildb_table_schema= NULL;
//  ib_idx_sch_t haildb_pkey= NULL;
  ib_trx_t haildb_schema_transaction;
  ib_id_t haildb_table_id;
  ib_err_t haildb_err= DB_SUCCESS;
  string haildb_table_name;
  bool has_explicit_pkey= false;

  (void)table_obj;

  if (table_message.type() == message::Table::TEMPORARY)
  {
    ib_bool_t create_db_err= ib_database_create(GLOBAL_TEMPORARY_EXT);
    if (create_db_err != IB_TRUE)
      return -1;
  }

  TableIdentifier_to_haildb_name(identifier, &haildb_table_name);

  ib_tbl_fmt_t haildb_table_format= IB_TBL_COMPACT;

  const size_t num_engine_options= table_message.engine().options_size();
  for (size_t x= 0; x < num_engine_options; x++)
  {
    const message::Engine::Option &engine_option= table_message.engine().options(x);
    if (boost::iequals(engine_option.name(), "ROW_FORMAT"))
    {
      haildb_table_format= parse_ib_table_format(engine_option.state());
    }
  }

  haildb_err= ib_table_schema_create(haildb_table_name.c_str(),
                                     &haildb_table_schema,
                                     haildb_table_format, 0);

  if (haildb_err != DB_SUCCESS)
  {
    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot create table %s. HailDB Error %d (%s)\n"),
                        haildb_table_name.c_str(), haildb_err, ib_strerror(haildb_err));
    return ib_err_t_to_drizzle_error(haildb_err);
  }

  for (int colnr= 0; colnr < table_message.field_size() ; colnr++)
  {
    const message::Table::Field field = table_message.field(colnr);

    int field_err= create_table_add_field(haildb_table_schema, field,
                                          &haildb_err);

    if (haildb_err != DB_SUCCESS || field_err != 0)
      ib_table_schema_delete(haildb_table_schema); /* cleanup */

    if (haildb_err != DB_SUCCESS)
    {
      push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_CANT_CREATE_TABLE,
                          _("Cannot create field %s on table %s."
                            " HailDB Error %d (%s)\n"),
                          field.name().c_str(), haildb_table_name.c_str(),
                          haildb_err, ib_strerror(haildb_err));
      return ib_err_t_to_drizzle_error(haildb_err);
    }
    if (field_err != 0)
      return field_err;
  }

  bool has_primary= false;
  for (int indexnr= 0; indexnr < table_message.indexes_size() ; indexnr++)
  {
    message::Table::Index *index = table_message.mutable_indexes(indexnr);

    ib_idx_sch_t haildb_index;

    haildb_err= ib_table_schema_add_index(haildb_table_schema, index->name().c_str(),
                                   &haildb_index);
    if (haildb_err != DB_SUCCESS)
      goto schema_error;

    if (index->is_primary())
    {
      has_primary= true;
      haildb_err= ib_index_schema_set_clustered(haildb_index);
      has_explicit_pkey= true;
      if (haildb_err != DB_SUCCESS)
        goto schema_error;
    }

    if (index->is_unique())
    {
      haildb_err= ib_index_schema_set_unique(haildb_index);
      if (haildb_err != DB_SUCCESS)
        goto schema_error;
    }

    if (index->type() == message::Table::Index::UNKNOWN_INDEX)
      index->set_type(message::Table::Index::BTREE);

    for (int partnr= 0; partnr < index->index_part_size(); partnr++)
    {
      const message::Table::Index::IndexPart part= index->index_part(partnr);
      const message::Table::Field::FieldType part_type= table_message.field(part.fieldnr()).type();
      uint64_t compare_length= 0;

      if (part_type == message::Table::Field::BLOB
          || part_type == message::Table::Field::VARCHAR)
        compare_length= part.compare_length();

      haildb_err= ib_index_schema_add_col(haildb_index,
                            table_message.field(part.fieldnr()).name().c_str(),
                                          compare_length);
      if (haildb_err != DB_SUCCESS)
        goto schema_error;
    }

    if (! has_primary && index->is_unique())
    {
      haildb_err= ib_index_schema_set_clustered(haildb_index);
      has_explicit_pkey= true;
      if (haildb_err != DB_SUCCESS)
        goto schema_error;
    }

  }

  if (! has_explicit_pkey)
  {
    ib_idx_sch_t haildb_index;

    haildb_err= ib_table_schema_add_col(haildb_table_schema, "hidden_primary_key_col",
                                        IB_INT, IB_COL_NOT_NULL, 0, 8);

    haildb_err= ib_table_schema_add_index(haildb_table_schema, "HIDDEN_PRIMARY",
                                          &haildb_index);
    if (haildb_err != DB_SUCCESS)
      goto schema_error;

    haildb_err= ib_index_schema_set_clustered(haildb_index);
    if (haildb_err != DB_SUCCESS)
      goto schema_error;

    haildb_err= ib_index_schema_add_col(haildb_index, "hidden_primary_key_col", 0);
    if (haildb_err != DB_SUCCESS)
      goto schema_error;
  }

  haildb_schema_transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  haildb_err= ib_schema_lock_exclusive(haildb_schema_transaction);
  if (haildb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(haildb_schema_transaction);
    ib_table_schema_delete(haildb_table_schema);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot Lock HailDB Data Dictionary. HailDB Error %d (%s)\n"),
                        haildb_err, ib_strerror(haildb_err));

    assert (rollback_err == DB_SUCCESS);

    return HA_ERR_GENERIC;
  }

  haildb_err= ib_table_create(haildb_schema_transaction, haildb_table_schema,
                              &haildb_table_id);

  if (haildb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(haildb_schema_transaction);
    ib_table_schema_delete(haildb_table_schema);

    if (haildb_err == DB_TABLE_IS_BEING_USED)
      return EEXIST;

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot create table %s. HailDB Error %d (%s)\n"),
                        haildb_table_name.c_str(),
                        haildb_err, ib_strerror(haildb_err));

    assert (rollback_err == DB_SUCCESS);
    return HA_ERR_GENERIC;
  }

  if (table_message.type() == message::Table::TEMPORARY)
  {
    session.storeTableMessage(identifier, table_message);
    haildb_err= DB_SUCCESS;
  }
  else
    haildb_err= store_table_message(haildb_schema_transaction,
                                    haildb_table_name.c_str(),
                                    table_message);

  if (haildb_err == DB_SUCCESS)
    haildb_err= ib_trx_commit(haildb_schema_transaction);
  else
    haildb_err= ib_trx_rollback(haildb_schema_transaction);

schema_error:
  ib_table_schema_delete(haildb_table_schema);

  if (haildb_err != DB_SUCCESS)
  {
    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot create table %s. HailDB Error %d (%s)\n"),
                        haildb_table_name.c_str(),
                        haildb_err, ib_strerror(haildb_err));
    return ib_err_t_to_drizzle_error(haildb_err);
  }

  return 0;
}

static int delete_table_message_from_haildb(ib_trx_t transaction, const char* table_name)
{
  ib_crsr_t cursor;
  ib_tpl_t search_tuple;
  int res;
  ib_err_t err;

  err= ib_cursor_open_table(HAILDB_TABLE_DEFINITIONS_TABLE, transaction, &cursor);
  if (err != DB_SUCCESS)
    return err;

  search_tuple= ib_clust_search_tuple_create(cursor);

  err= ib_col_set_value(search_tuple, 0, table_name, strlen(table_name));
  if (err != DB_SUCCESS)
    goto rollback;

//  ib_cursor_set_match_mode(cursor, IB_EXACT_MATCH);

  err= ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
  if (err == DB_RECORD_NOT_FOUND || res != 0)
    goto rollback;

  err= ib_cursor_delete_row(cursor);
  assert (err == DB_SUCCESS);

rollback:
  ib_err_t rollback_err= ib_cursor_close(cursor);
  if (err == DB_SUCCESS)
    err= rollback_err;

  ib_tuple_delete(search_tuple);

  return err;
}

int HailDBEngine::doDropTable(Session &session,
                                      const TableIdentifier &identifier)
{
  ib_trx_t haildb_schema_transaction;
  ib_err_t haildb_err;
  string haildb_table_name;

  TableIdentifier_to_haildb_name(identifier, &haildb_table_name);

  haildb_schema_transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  haildb_err= ib_schema_lock_exclusive(haildb_schema_transaction);
  if (haildb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(haildb_schema_transaction);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot Lock HailDB Data Dictionary. HailDB Error %d (%s)\n"),
                        haildb_err, ib_strerror(haildb_err));

    assert (rollback_err == DB_SUCCESS);

    return HA_ERR_GENERIC;
  }

  if (identifier.getType() == message::Table::TEMPORARY)
  {
      session.removeTableMessage(identifier);
      delete_table_message_from_haildb(haildb_schema_transaction,
                                       haildb_table_name.c_str());
  }
  else
  {
    if (delete_table_message_from_haildb(haildb_schema_transaction, haildb_table_name.c_str()) != DB_SUCCESS)
    {
      ib_schema_unlock(haildb_schema_transaction);
      ib_err_t rollback_err= ib_trx_rollback(haildb_schema_transaction);
      assert(rollback_err == DB_SUCCESS);
      return HA_ERR_GENERIC;
    }
  }

  haildb_err= ib_table_drop(haildb_schema_transaction, haildb_table_name.c_str());

  if (haildb_err == DB_TABLE_NOT_FOUND)
  {
    haildb_err= ib_trx_rollback(haildb_schema_transaction);
    assert(haildb_err == DB_SUCCESS);
    return ENOENT;
  }
  else if (haildb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(haildb_schema_transaction);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot DROP table %s. HailDB Error %d (%s)\n"),
                        haildb_table_name.c_str(),
                        haildb_err, ib_strerror(haildb_err));

    assert(rollback_err == DB_SUCCESS);

    return HA_ERR_GENERIC;
  }

  haildb_err= ib_trx_commit(haildb_schema_transaction);
  if (haildb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(haildb_schema_transaction);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot DROP table %s. HailDB Error %d (%s)\n"),
                        haildb_table_name.c_str(),
                        haildb_err, ib_strerror(haildb_err));

    assert(rollback_err == DB_SUCCESS);
    return HA_ERR_GENERIC;
  }

  return 0;
}

static ib_err_t rename_table_message(ib_trx_t transaction, const TableIdentifier &from_identifier, const TableIdentifier &to_identifier)
{
  ib_crsr_t cursor;
  ib_tpl_t search_tuple;
  ib_tpl_t read_tuple;
  ib_tpl_t update_tuple;
  int res;
  ib_err_t err;
  ib_err_t rollback_err;
  const char *message;
  ib_ulint_t message_len;
  drizzled::message::Table table_message;
  string from_haildb_table_name;
  string to_haildb_table_name;
  const char *from;
  const char *to;
  string serialized_message;
  ib_col_meta_t col_meta;

  TableIdentifier_to_haildb_name(from_identifier, &from_haildb_table_name);
  TableIdentifier_to_haildb_name(to_identifier, &to_haildb_table_name);

  from= from_haildb_table_name.c_str();
  to= to_haildb_table_name.c_str();

  err= ib_cursor_open_table(HAILDB_TABLE_DEFINITIONS_TABLE, transaction, &cursor);
  if (err != DB_SUCCESS)
  {
    rollback_err= ib_trx_rollback(transaction);
    assert(rollback_err == DB_SUCCESS);
    return err;
  }

  search_tuple= ib_clust_search_tuple_create(cursor);
  read_tuple= ib_clust_read_tuple_create(cursor);

  err= ib_col_set_value(search_tuple, 0, from, strlen(from));
  if (err != DB_SUCCESS)
    goto rollback;

//  ib_cursor_set_match_mode(cursor, IB_EXACT_MATCH);

  err= ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
  if (err == DB_RECORD_NOT_FOUND || res != 0)
    goto rollback;

  err= ib_cursor_read_row(cursor, read_tuple);
  if (err == DB_RECORD_NOT_FOUND || res != 0)
    goto rollback;

  message= (const char*)ib_col_get_value(read_tuple, 1);
  message_len= ib_col_get_meta(read_tuple, 1, &col_meta);

  if (table_message.ParseFromArray(message, message_len) == false)
    goto rollback;

  table_message.set_name(to_identifier.getTableName());
  table_message.set_schema(to_identifier.getSchemaName());

  update_tuple= ib_clust_read_tuple_create(cursor);

  err= ib_tuple_copy(update_tuple, read_tuple);
  assert(err == DB_SUCCESS);

  err= ib_col_set_value(update_tuple, 0, to, strlen(to));

  try {
    table_message.SerializeToString(&serialized_message);
  }
  catch (...)
  {
    goto rollback;
  }

  err= ib_col_set_value(update_tuple, 1, serialized_message.c_str(),
                        serialized_message.length());

  err= ib_cursor_update_row(cursor, read_tuple, update_tuple);


  ib_tuple_delete(update_tuple);
  ib_tuple_delete(read_tuple);
  ib_tuple_delete(search_tuple);

  err= ib_cursor_close(cursor);

rollback:
  return err;
}

int HailDBEngine::doRenameTable(drizzled::Session &session,
                                        const drizzled::TableIdentifier &from,
                                        const drizzled::TableIdentifier &to)
{
  ib_trx_t haildb_schema_transaction;
  ib_err_t err;
  string from_haildb_table_name;
  string to_haildb_table_name;

  if (to.getType() == message::Table::TEMPORARY
      && from.getType() == message::Table::TEMPORARY)
  {
    session.renameTableMessage(from, to);
    return 0;
  }

  TableIdentifier_to_haildb_name(from, &from_haildb_table_name);
  TableIdentifier_to_haildb_name(to, &to_haildb_table_name);

  haildb_schema_transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  err= ib_schema_lock_exclusive(haildb_schema_transaction);
  if (err != DB_SUCCESS)
  {
    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot Lock HailDB Data Dictionary. HailDB Error %d (%s)\n"),
                        err, ib_strerror(err));

    goto rollback;
  }

  err= ib_table_rename(haildb_schema_transaction,
                       from_haildb_table_name.c_str(),
                       to_haildb_table_name.c_str());
  if (err != DB_SUCCESS)
    goto rollback;

  err= rename_table_message(haildb_schema_transaction, from, to);

  if (err != DB_SUCCESS)
    goto rollback;

  err= ib_trx_commit(haildb_schema_transaction);
  if (err != DB_SUCCESS)
    goto rollback;

  return 0;
rollback:
  ib_err_t rollback_err= ib_schema_unlock(haildb_schema_transaction);
  assert(rollback_err == DB_SUCCESS);
  rollback_err= ib_trx_rollback(haildb_schema_transaction);
  assert(rollback_err == DB_SUCCESS);
  return ib_err_t_to_drizzle_error(err);
}

void HailDBEngine::getTableNamesInSchemaFromHailDB(
                                 const drizzled::SchemaIdentifier &schema,
                                 drizzled::plugin::TableNameList *set_of_names,
                                 drizzled::TableIdentifiers *identifiers)
{
  ib_trx_t   transaction;
  ib_crsr_t  cursor;
  /*
    Why not use getPath()?
  */
  string search_string(schema.getSchemaName());

  boost::algorithm::to_lower(search_string);

  search_string.append("/");

  transaction = ib_trx_begin(IB_TRX_REPEATABLE_READ);
  ib_err_t haildb_err= ib_schema_lock_exclusive(transaction);
  assert(haildb_err == DB_SUCCESS); /* FIXME: doGetTableNames needs to be able to return error */

  if (search_string.compare("data_dictionary/") == 0)
  {
    if (set_of_names)
    {
      BOOST_FOREACH(std::string table_name, haildb_system_table_names)
      {
        set_of_names->insert(table_name);
      }
    }
    if (identifiers)
    {
      BOOST_FOREACH(std::string table_name, haildb_system_table_names)
      {
        identifiers->push_back(TableIdentifier(schema.getSchemaName(),
                                               table_name));
      }
    }
  }

  haildb_err= ib_cursor_open_table("SYS_TABLES", transaction, &cursor);
  assert(haildb_err == DB_SUCCESS); /* FIXME */

  ib_tpl_t read_tuple;
  ib_tpl_t search_tuple;

  read_tuple= ib_clust_read_tuple_create(cursor);
  search_tuple= ib_clust_search_tuple_create(cursor);

  haildb_err= ib_col_set_value(search_tuple, 0, search_string.c_str(),
                               search_string.length());
  assert (haildb_err == DB_SUCCESS); // FIXME

  int res;
  haildb_err = ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
  // fixme: check error above

  while (haildb_err == DB_SUCCESS)
  {
    haildb_err= ib_cursor_read_row(cursor, read_tuple);

    const char *table_name;
    int table_name_len;
    ib_col_meta_t column_metadata;

    table_name= (const char*)ib_col_get_value(read_tuple, 0);
    table_name_len=  ib_col_get_meta(read_tuple, 0, &column_metadata);

    if (search_string.compare(0, search_string.length(),
                              table_name, search_string.length()) == 0)
    {
      const char *just_table_name= strchr(table_name, '/');
      assert(just_table_name);
      just_table_name++; /* skip over '/' */
      if (set_of_names)
        set_of_names->insert(just_table_name);
      if (identifiers)
        identifiers->push_back(TableIdentifier(schema.getSchemaName(), just_table_name));
    }


    haildb_err= ib_cursor_next(cursor);
    read_tuple= ib_tuple_clear(read_tuple);
  }

  ib_tuple_delete(read_tuple);
  ib_tuple_delete(search_tuple);

  haildb_err= ib_cursor_close(cursor);
  assert(haildb_err == DB_SUCCESS); // FIXME

  haildb_err= ib_trx_commit(transaction);
  assert(haildb_err == DB_SUCCESS); // FIXME
}

void HailDBEngine::doGetTableIdentifiers(drizzled::CachedDirectory &,
                                                 const drizzled::SchemaIdentifier &schema,
                                                 drizzled::TableIdentifiers &identifiers)
{
  getTableNamesInSchemaFromHailDB(schema, NULL, &identifiers);
}

static int read_table_message_from_haildb(const char* table_name, drizzled::message::Table *table_message)
{
  ib_trx_t transaction;
  ib_tpl_t search_tuple;
  ib_tpl_t read_tuple;
  ib_crsr_t cursor;
  const char *message;
  ib_ulint_t message_len;
  ib_col_meta_t col_meta;
  int res;
  ib_err_t err;
  ib_err_t rollback_err;

  transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  err= ib_schema_lock_exclusive(transaction);
  if (err != DB_SUCCESS)
  {
    rollback_err= ib_trx_rollback(transaction);
    assert(rollback_err == DB_SUCCESS);
    return err;
  }

  err= ib_cursor_open_table(HAILDB_TABLE_DEFINITIONS_TABLE, transaction, &cursor);
  if (err != DB_SUCCESS)
  {
    rollback_err= ib_trx_rollback(transaction);
    assert(rollback_err == DB_SUCCESS);
    return err;
  }

  search_tuple= ib_clust_search_tuple_create(cursor);
  read_tuple= ib_clust_read_tuple_create(cursor);

  err= ib_col_set_value(search_tuple, 0, table_name, strlen(table_name));
  if (err != DB_SUCCESS)
    goto rollback;

//  ib_cursor_set_match_mode(cursor, IB_EXACT_MATCH);

  err= ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
  if (err == DB_RECORD_NOT_FOUND || res != 0)
    goto rollback;

  err= ib_cursor_read_row(cursor, read_tuple);
  if (err == DB_RECORD_NOT_FOUND || res != 0)
    goto rollback;

  message= (const char*)ib_col_get_value(read_tuple, 1);
  message_len= ib_col_get_meta(read_tuple, 1, &col_meta);

  if (table_message->ParseFromArray(message, message_len) == false)
    goto rollback;

  ib_tuple_delete(search_tuple);
  ib_tuple_delete(read_tuple);
  err= ib_cursor_close(cursor);
  if (err != DB_SUCCESS)
    goto rollback_close_err;
  err= ib_trx_commit(transaction);
  if (err != DB_SUCCESS)
    goto rollback_close_err;

  return 0;

rollback:
  ib_tuple_delete(search_tuple);
  ib_tuple_delete(read_tuple);
  rollback_err= ib_cursor_close(cursor);
  assert(rollback_err == DB_SUCCESS);
rollback_close_err:
  ib_schema_unlock(transaction);
  rollback_err= ib_trx_rollback(transaction);
  assert(rollback_err == DB_SUCCESS);

  if (strcmp(table_name, HAILDB_TABLE_DEFINITIONS_TABLE) == 0)
  {
    message::Engine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("haildb_table_definitions");
    table_message->set_schema("data_dictionary");
    table_message->set_type(message::Table::STANDARD);
    table_message->set_creation_timestamp(0);
    table_message->set_update_timestamp(0);

    message::Table::TableOptions *options= table_message->mutable_options();
    options->set_collation_id(my_charset_bin.number);
    options->set_collation(my_charset_bin.name);

    message::Table::Field *field= table_message->add_field();
    field->set_name("table_name");
    field->set_type(message::Table::Field::VARCHAR);
    message::Table::Field::StringFieldOptions *stropt= field->mutable_string_options();
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);

    field= table_message->add_field();
    field->set_name("message");
    field->set_type(message::Table::Field::BLOB);
    stropt= field->mutable_string_options();
    stropt->set_collation_id(my_charset_bin.number);
    stropt->set_collation(my_charset_bin.name);

    message::Table::Index *index= table_message->add_indexes();
    index->set_name("PRIMARY");
    index->set_is_primary(true);
    index->set_is_unique(true);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(IB_MAX_TABLE_NAME_LEN);
    message::Table::Index::IndexPart *part= index->add_index_part();
    part->set_fieldnr(0);
    part->set_compare_length(IB_MAX_TABLE_NAME_LEN);

    return 0;
  }

  return -1;
}

int HailDBEngine::doGetTableDefinition(Session &session,
                                               const TableIdentifier &identifier,
                                               drizzled::message::Table &table)
{
  ib_crsr_t haildb_cursor= NULL;
  string haildb_table_name;

  /* Check temporary tables!? */
  if (session.getTableMessage(identifier, table))
    return EEXIST;

  TableIdentifier_to_haildb_name(identifier, &haildb_table_name);

  if (ib_cursor_open_table(haildb_table_name.c_str(), NULL, &haildb_cursor) != DB_SUCCESS)
    return ENOENT;

  ib_err_t err= ib_cursor_close(haildb_cursor);

  assert (err == DB_SUCCESS);

  if (read_table_message_from_haildb(haildb_table_name.c_str(), &table) != 0)
  {
    if (get_haildb_system_table_message(haildb_table_name.c_str(), &table) == 0)
      return EEXIST;
  }

  return EEXIST;
}

bool HailDBEngine::doDoesTableExist(Session &,
                                    const TableIdentifier& identifier)
{
  ib_crsr_t haildb_cursor;
  string haildb_table_name;

  TableIdentifier_to_haildb_name(identifier, &haildb_table_name);

  boost::unordered_set<string>::iterator iter= haildb_system_table_names.find(identifier.getTableName());
  if (iter != haildb_system_table_names.end())
    return true;

  if (ib_cursor_open_table(haildb_table_name.c_str(), NULL, &haildb_cursor) != DB_SUCCESS)
    return false;

  ib_err_t err= ib_cursor_close(haildb_cursor);
  assert(err == DB_SUCCESS);

  return true;
}

const char *HailDBCursor::index_type(uint32_t)
{
  return("BTREE");
}

static ib_err_t write_row_to_haildb_tuple(Field **fields, ib_tpl_t tuple)
{
  int colnr= 0;
  ib_err_t err= DB_ERROR;

  for (Field **field= fields; *field; field++, colnr++)
  {
    if (! (**field).isWriteSet() && (**field).is_null())
      continue;

    if ((**field).is_null())
    {
      err= ib_col_set_value(tuple, colnr, NULL, IB_SQL_NULL);
      assert(err == DB_SUCCESS);
      continue;
    }

    if ((**field).type() == DRIZZLE_TYPE_VARCHAR)
    {
      /* To get around the length bytes (1 or 2) at (**field).ptr
         we can use Field_varstring::val_str to a String
         to get a pointer to the real string without copying it.
      */
      String str;
      (**field).setReadSet();
      (**field).val_str(&str);
      err= ib_col_set_value(tuple, colnr, str.ptr(), str.length());
    }
    else if ((**field).type() == DRIZZLE_TYPE_ENUM)
    {
      err= ib_tuple_write_u32(tuple, colnr, *((ib_u32_t*)(*field)->ptr));
    }
    else if ((**field).type() == DRIZZLE_TYPE_DATE)
    {
      (**field).setReadSet();
      err= ib_tuple_write_u32(tuple, colnr, (*field)->val_int());
    }
    else if ((**field).type() == DRIZZLE_TYPE_BLOB)
    {
      Field_blob *blob= reinterpret_cast<Field_blob*>(*field);
      unsigned char* blob_ptr;
      uint32_t blob_length= blob->get_length();
      blob->get_ptr(&blob_ptr);
      err= ib_col_set_value(tuple, colnr, blob_ptr, blob_length);
    }
    else
    {
      err= ib_col_set_value(tuple, colnr, (*field)->ptr, (*field)->data_length());
    }

    assert (err == DB_SUCCESS);
  }

  return err;
}

static uint64_t innobase_get_int_col_max_value(const Field* field)
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
    assert(false);
  }

  return(max_value);
}

int HailDBCursor::doInsertRecord(unsigned char *record)
{
  ib_err_t err;
  int ret= 0;

  ib_trx_t transaction= *get_trx(getTable()->in_use);

  tuple= ib_clust_read_tuple_create(cursor);

  ib_cursor_attach_trx(cursor, transaction);

  err= ib_cursor_first(cursor);
  if (current_session->lex->sql_command == SQLCOM_CREATE_TABLE
      && err == DB_MISSING_HISTORY)
  {
    /* See https://bugs.launchpad.net/drizzle/+bug/556978
     *
     * In CREATE SELECT, transaction is started in ::store_lock
     * at the start of the statement, before the table is created.
     * This means the table doesn't exist in our snapshot,
     * and we get a DB_MISSING_HISTORY error on ib_cursor_first().
     * The way to get around this is to here, restart the transaction
     * and continue.
     *
     * yuck.
     */

    HailDBEngine *storage_engine= static_cast<HailDBEngine*>(getEngine());
    err= ib_cursor_reset(cursor);
    storage_engine->doCommit(current_session, true);
    storage_engine->doStartTransaction(current_session, START_TRANS_NO_OPTIONS);
    transaction= *get_trx(getTable()->in_use);
    assert(err == DB_SUCCESS);
    ib_cursor_attach_trx(cursor, transaction);
    err= ib_cursor_first(cursor);
  }

  assert(err == DB_SUCCESS || err == DB_END_OF_INDEX);


  if (getTable()->next_number_field)
  {
    update_auto_increment();

    uint64_t temp_auto= getTable()->next_number_field->val_int();

    if (temp_auto <= innobase_get_int_col_max_value(getTable()->next_number_field))
    {
      while (true)
      {
        uint64_t fetched_auto= share->auto_increment_value;

        if (temp_auto >= fetched_auto)
        {
          uint64_t store_value= temp_auto+1;
          if (store_value == 0)
            store_value++;

          if (share->auto_increment_value.compare_and_swap(store_value, fetched_auto) == fetched_auto)
            break;
        }
        else
          break;
      }
    }

  }

  write_row_to_haildb_tuple(getTable()->getFields(), tuple);

  if (share->has_hidden_primary_key)
  {
    err= ib_tuple_write_u64(tuple, getTable()->getShare()->fields, share->hidden_pkey_auto_increment_value.fetch_and_increment());
  }

  err= ib_cursor_insert_row(cursor, tuple);

  if (err == DB_DUPLICATE_KEY)
  {
    if (write_can_replace)
    {
      store_key_value_from_haildb(getTable()->key_info + getTable()->getShare()->getPrimaryKey(),
                                  ref, ref_length, record);

      ib_tpl_t search_tuple= ib_clust_search_tuple_create(cursor);

      fill_ib_search_tpl_from_drizzle_key(search_tuple,
                                          getTable()->key_info + 0,
                                          ref, ref_length);

      int res;
      err= ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
      assert(err == DB_SUCCESS);
      ib_tuple_delete(search_tuple);

      tuple= ib_tuple_clear(tuple);
      err= ib_cursor_delete_row(cursor);

      err= ib_cursor_first(cursor);
      assert(err == DB_SUCCESS || err == DB_END_OF_INDEX);

      write_row_to_haildb_tuple(getTable()->getFields(), tuple);

      err= ib_cursor_insert_row(cursor, tuple);
      assert(err==DB_SUCCESS); // probably be nice and process errors
    }
    else
      ret= HA_ERR_FOUND_DUPP_KEY;
  }
  else if (err != DB_SUCCESS)
    ret= ib_err_t_to_drizzle_error(err);

  tuple= ib_tuple_clear(tuple);
  ib_tuple_delete(tuple);
  err= ib_cursor_reset(cursor);

  return ret;
}

int HailDBCursor::doUpdateRecord(const unsigned char *,
                                         unsigned char *)
{
  ib_tpl_t update_tuple;
  ib_err_t err;

  update_tuple= ib_clust_read_tuple_create(cursor);

  err= ib_tuple_copy(update_tuple, tuple);
  assert(err == DB_SUCCESS);

  write_row_to_haildb_tuple(getTable()->getFields(), update_tuple);

  err= ib_cursor_update_row(cursor, tuple, update_tuple);

  ib_tuple_delete(update_tuple);

  advance_cursor= true;

  if (err == DB_SUCCESS)
    return 0;
  else if (err == DB_DUPLICATE_KEY)
    return HA_ERR_FOUND_DUPP_KEY;
  else
    return -1;
}

int HailDBCursor::doDeleteRecord(const unsigned char *)
{
  ib_err_t err;

  err= ib_cursor_delete_row(cursor);
  if (err != DB_SUCCESS)
    return -1; // FIXME

  advance_cursor= true;
  return 0;
}

int HailDBCursor::delete_all_rows(void)
{
  /* I *think* ib_truncate is non-transactional....
     so only support TRUNCATE and not DELETE FROM t;
     (this is what ha_haildb does)
  */
  if (session_sql_command(getTable()->in_use) != SQLCOM_TRUNCATE)
    return HA_ERR_WRONG_COMMAND;

  ib_id_t id;
  ib_err_t err;

  ib_trx_t transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);

  ib_cursor_attach_trx(cursor, transaction);

  err= ib_schema_lock_exclusive(transaction);
  if (err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(transaction);

    push_warning_printf(getTable()->in_use, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot Lock HailDB Data Dictionary. HailDB Error %d (%s)\n"),
                        err, ib_strerror(err));

    assert (rollback_err == DB_SUCCESS);

    return HA_ERR_GENERIC;
  }

  share->auto_increment_value.fetch_and_store(1);

  err= ib_cursor_truncate(&cursor, &id);
  if (err != DB_SUCCESS)
    goto err;

  ib_schema_unlock(transaction);
  /* ib_cursor_truncate commits on success */

  err= ib_cursor_open_table_using_id(id, NULL, &cursor);
  if (err != DB_SUCCESS)
    goto err;

  return 0;

err:
  ib_schema_unlock(transaction);
  ib_err_t rollback_err= ib_trx_rollback(transaction);
  assert(rollback_err == DB_SUCCESS);
  return err;
}

int HailDBCursor::doStartTableScan(bool)
{
  ib_err_t err;
  ib_trx_t transaction;

  if (in_table_scan)
    doEndTableScan();
  in_table_scan= true;

  transaction= *get_trx(getTable()->in_use);

  assert(transaction != NULL);

  ib_cursor_attach_trx(cursor, transaction);

  err= ib_cursor_set_lock_mode(cursor, ib_lock_mode);
  assert(err == DB_SUCCESS); // FIXME

  tuple= ib_clust_read_tuple_create(cursor);

  err= ib_cursor_first(cursor);
  if (err != DB_SUCCESS && err != DB_END_OF_INDEX)
  {
    previous_error= ib_err_t_to_drizzle_error(err);
    err= ib_cursor_reset(cursor);
    return previous_error;
  }

  advance_cursor= false;

  previous_error= 0;
  return(0);
}

int read_row_from_haildb(unsigned char* buf, ib_crsr_t cursor, ib_tpl_t tuple, Table* table, bool has_hidden_primary_key, uint64_t *hidden_pkey, drizzled::memory::Root **blobroot)
{
  ib_err_t err;
  ptrdiff_t row_offset= buf - table->getInsertRecord();

  err= ib_cursor_read_row(cursor, tuple);

  if (err != DB_SUCCESS) // FIXME
    return HA_ERR_END_OF_FILE;

  int colnr= 0;

  /* We need the primary key for ::position() to work */
  if (table->getShare()->getPrimaryKey() != MAX_KEY)
    table->mark_columns_used_by_index_no_reset(table->getShare()->getPrimaryKey());

  for (Field **field= table->getFields() ; *field ; field++, colnr++)
  {
    if (! (**field).isReadSet())
      continue;

    (**field).move_field_offset(row_offset);

    (**field).setWriteSet();

    uint32_t length= ib_col_get_len(tuple, colnr);
    if (length == IB_SQL_NULL)
    {
      (**field).set_null();
      continue;
    }
    else
      (**field).set_notnull();

    if ((**field).type() == DRIZZLE_TYPE_VARCHAR)
    {
      (*field)->store((const char*)ib_col_get_value(tuple, colnr),
                      length,
                      &my_charset_bin);
    }
    else if ((**field).type() == DRIZZLE_TYPE_DATE)
    {
      uint32_t date_read;
      err= ib_tuple_read_u32(tuple, colnr, &date_read);
      (*field)->store(date_read);
    }
    else if ((**field).type() == DRIZZLE_TYPE_BLOB)
    {
      if (blobroot == NULL)
        (reinterpret_cast<Field_blob*>(*field))->set_ptr(length,
                                      (unsigned char*)ib_col_get_value(tuple,
                                                                       colnr));
      else
      {
        if (*blobroot == NULL)
        {
          *blobroot= new drizzled::memory::Root();
          (**blobroot).init_alloc_root();
        }

        unsigned char *blob_ptr= (unsigned char*)(**blobroot).alloc_root(length);
        memcpy(blob_ptr, ib_col_get_value(tuple, colnr), length);
        (reinterpret_cast<Field_blob*>(*field))->set_ptr(length, blob_ptr);
      }
    }
    else
    {
      ib_col_copy_value(tuple, colnr, (*field)->ptr, (*field)->data_length());
    }

    (**field).move_field_offset(-row_offset);

  }

  if (has_hidden_primary_key)
  {
    err= ib_tuple_read_u64(tuple, colnr, hidden_pkey);
  }

  return 0;
}

int HailDBCursor::rnd_next(unsigned char *buf)
{
  ib_err_t err;
  int ret;

  if (previous_error)
    return previous_error;

  if (advance_cursor)
    err= ib_cursor_next(cursor);

  tuple= ib_tuple_clear(tuple);
  ret= read_row_from_haildb(buf, cursor, tuple, getTable(),
                            share->has_hidden_primary_key,
                            &hidden_autoinc_pkey_position);

  advance_cursor= true;
  return ret;
}

int HailDBCursor::doEndTableScan()
{
  ib_err_t err;

  ib_tuple_delete(tuple);
  err= ib_cursor_reset(cursor);
  assert(err == DB_SUCCESS);
  in_table_scan= false;
  previous_error= 0;
  return 0;
}

int HailDBCursor::rnd_pos(unsigned char *buf, unsigned char *pos)
{
  ib_err_t err;
  int res;
  int ret= 0;
  ib_tpl_t search_tuple= ib_clust_search_tuple_create(cursor);

  if (share->has_hidden_primary_key)
  {
    err= ib_col_set_value(search_tuple, 0,
                          ((uint64_t*)(pos)), sizeof(uint64_t));
  }
  else
  {
    unsigned int keynr;
    if (getTable()->getShare()->getPrimaryKey() != MAX_KEY)
      keynr= getTable()->getShare()->getPrimaryKey();
    else
      keynr= get_first_unique_index(*getTable());

    fill_ib_search_tpl_from_drizzle_key(search_tuple,
                                        getTable()->key_info + keynr,
                                        pos, ref_length);
  }

  err= ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
  assert(err == DB_SUCCESS);

  assert(res==0);
  if (res != 0)
    ret= -1;

  ib_tuple_delete(search_tuple);

  tuple= ib_tuple_clear(tuple);

  if (ret == 0)
    ret= read_row_from_haildb(buf, cursor, tuple, getTable(),
                              share->has_hidden_primary_key,
                              &hidden_autoinc_pkey_position);

  advance_cursor= true;

  return(ret);
}

static void store_key_value_from_haildb(KeyInfo *key_info, unsigned char* ref, int ref_len, const unsigned char *record)
{
  KeyPartInfo* key_part= key_info->key_part;
  KeyPartInfo* end= key_info->key_part + key_info->key_parts;
  unsigned char* ref_start= ref;

  memset(ref, 0, ref_len);

  for (; key_part != end; key_part++)
  {
    char is_null= 0;

    if(key_part->null_bit)
    {
      *ref= is_null= record[key_part->null_offset] & key_part->null_bit;
      ref++;
    }

    Field *field= key_part->field;

    if (field->type() == DRIZZLE_TYPE_VARCHAR)
    {
      if (is_null)
      {
        ref+= key_part->length + 2; /* 2 bytes for length */
        continue;
      }

      String str;
      field->val_str(&str);

      *ref++= (char)(str.length() & 0x000000ff);
      *ref++= (char)((str.length()>>8) & 0x000000ff);

      memcpy(ref, str.ptr(), str.length());
      ref+= key_part->length;
    }
    // FIXME: blobs.
    else
    {
      if (is_null)
      {
        ref+= key_part->length;
        continue;
      }

      memcpy(ref, record+key_part->offset, key_part->length);
      ref+= key_part->length;
    }

  }

  assert(ref == ref_start + ref_len);
}

void HailDBCursor::position(const unsigned char *record)
{
  if (share->has_hidden_primary_key)
    *((uint64_t*) ref)= hidden_autoinc_pkey_position;
  else
  {
    unsigned int keynr;
    if (getTable()->getShare()->getPrimaryKey() != MAX_KEY)
      keynr= getTable()->getShare()->getPrimaryKey();
    else
      keynr= get_first_unique_index(*getTable());

    store_key_value_from_haildb(getTable()->key_info + keynr,
                                ref, ref_length, record);
  }

  return;
}

double HailDBCursor::scan_time()
{
  ib_table_stats_t table_stats;
  ib_err_t err;

  err= ib_get_table_statistics(cursor, &table_stats, sizeof(table_stats));

  /* Approximate I/O seeks for full table scan */
  return (double) (table_stats.stat_clustered_index_size / 16384);
}

int HailDBCursor::info(uint32_t flag)
{
  ib_table_stats_t table_stats;
  ib_err_t err;

  if (flag & HA_STATUS_VARIABLE)
  {
    err= ib_get_table_statistics(cursor, &table_stats, sizeof(table_stats));

    stats.records= table_stats.stat_n_rows;

    if (table_stats.stat_n_rows < 2)
      stats.records= 2;

    stats.deleted= 0;
    stats.data_file_length= table_stats.stat_clustered_index_size;
    stats.index_file_length= table_stats.stat_sum_of_other_index_sizes;

    stats.mean_rec_length= stats.data_file_length / stats.records;
  }

  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  return(0);
}

int HailDBCursor::doStartIndexScan(uint32_t keynr, bool)
{
  ib_trx_t transaction= *get_trx(getTable()->in_use);

  active_index= keynr;

  ib_cursor_attach_trx(cursor, transaction);

  if (active_index == 0 && ! share->has_hidden_primary_key)
  {
    tuple= ib_clust_read_tuple_create(cursor);
  }
  else
  {
    ib_err_t err;
    ib_id_t index_id;
    err= ib_index_get_id(table_path_to_haildb_name(getShare()->getPath()),
                         getShare()->getKeyInfo(keynr).name,
                         &index_id);
    if (err != DB_SUCCESS)
      return -1;

    err= ib_cursor_close(cursor);
    assert(err == DB_SUCCESS);
    err= ib_cursor_open_index_using_id(index_id, transaction, &cursor);

    if (err != DB_SUCCESS)
      return -1;

    tuple= ib_clust_read_tuple_create(cursor);
    ib_cursor_set_cluster_access(cursor);
  }

  ib_err_t err= ib_cursor_set_lock_mode(cursor, ib_lock_mode);
  assert(err == DB_SUCCESS);

  advance_cursor= false;
  return 0;
}

static ib_srch_mode_t ha_rkey_function_to_ib_srch_mode(drizzled::ha_rkey_function find_flag)
{
  switch (find_flag)
  {
  case HA_READ_KEY_EXACT:
    return IB_CUR_GE;
  case HA_READ_KEY_OR_NEXT:
    return IB_CUR_GE;
  case HA_READ_KEY_OR_PREV:
    return IB_CUR_LE;
  case HA_READ_AFTER_KEY:
    return IB_CUR_G;
  case HA_READ_BEFORE_KEY:
    return IB_CUR_L;
  case HA_READ_PREFIX:
    return IB_CUR_GE;
  case HA_READ_PREFIX_LAST:
    return IB_CUR_LE;
  case HA_READ_PREFIX_LAST_OR_PREV:
    return IB_CUR_LE;
  case HA_READ_MBR_CONTAIN:
  case HA_READ_MBR_INTERSECT:
  case HA_READ_MBR_WITHIN:
  case HA_READ_MBR_DISJOINT:
  case HA_READ_MBR_EQUAL:
    assert(false); /* these just exist in the enum, not used. */
  }

  assert(false);
  /* Must return or compiler complains about reaching end of function */
  return (ib_srch_mode_t)0;
}

static void fill_ib_search_tpl_from_drizzle_key(ib_tpl_t search_tuple,
                                                const drizzled::KeyInfo *key_info,
                                                const unsigned char *key_ptr,
                                                uint32_t key_len)
{
  KeyPartInfo *key_part= key_info->key_part;
  KeyPartInfo *end= key_part + key_info->key_parts;
  const unsigned char *buff= key_ptr;
  ib_err_t err;

  int fieldnr= 0;

  for(; key_part != end && buff < key_ptr + key_len; key_part++)
  {
    Field *field= key_part->field;
    bool is_null= false;

    if (key_part->null_bit)
    {
      is_null= *buff;
      if (is_null)
      {
        err= ib_col_set_value(search_tuple, fieldnr, NULL, IB_SQL_NULL);
        assert(err == DB_SUCCESS);
      }
      buff++;
    }

    if (field->type() == DRIZZLE_TYPE_VARCHAR)
    {
      if (is_null)
      {
        buff+= key_part->length + 2; /* 2 bytes length */
        continue;
      }

      int length= *buff + (*(buff + 1) << 8);
      buff+=2;
      err= ib_col_set_value(search_tuple, fieldnr, buff, length);
      assert(err == DB_SUCCESS);

      buff+= key_part->length;
    }
    else if (field->type() == DRIZZLE_TYPE_DATE)
    {
      uint32_t date_int= static_cast<uint32_t>(field->val_int());
      err= ib_col_set_value(search_tuple, fieldnr, &date_int, 4);
      buff+= key_part->length;
    }
    // FIXME: BLOBs
    else
    {
      if (is_null)
      {
        buff+= key_part->length;
        continue;
      }

      err= ib_col_set_value(search_tuple, fieldnr,
                            buff, key_part->length);
      assert(err == DB_SUCCESS);

      buff+= key_part->length;
    }

    fieldnr++;
  }

  assert(buff == key_ptr + key_len);
}

int HailDBCursor::haildb_index_read(unsigned char *buf,
                                            const unsigned char *key_ptr,
                                            uint32_t key_len,
                                            drizzled::ha_rkey_function find_flag,
                                            bool allocate_blobs)
{
  ib_tpl_t search_tuple;
  int res;
  ib_err_t err;
  int ret;
  ib_srch_mode_t search_mode;

  search_mode= ha_rkey_function_to_ib_srch_mode(find_flag);

  if (active_index == 0 && ! share->has_hidden_primary_key)
    search_tuple= ib_clust_search_tuple_create(cursor);
  else
    search_tuple= ib_sec_search_tuple_create(cursor);

  fill_ib_search_tpl_from_drizzle_key(search_tuple,
                                      getTable()->key_info + active_index,
                                      key_ptr, key_len);

  err= ib_cursor_moveto(cursor, search_tuple, search_mode, &res);
  ib_tuple_delete(search_tuple);

  if ((err == DB_RECORD_NOT_FOUND || err == DB_END_OF_INDEX))
  {
    getTable()->status= STATUS_NOT_FOUND;
    return HA_ERR_KEY_NOT_FOUND;
  }

  if (err != DB_SUCCESS)
  {
    return ib_err_t_to_drizzle_error(err);
  }

  tuple= ib_tuple_clear(tuple);
  ret= read_row_from_haildb(buf, cursor, tuple, getTable(),
                            share->has_hidden_primary_key,
                            &hidden_autoinc_pkey_position,
                            (allocate_blobs)? &blobroot : NULL);
  if (ret == 0)
    getTable()->status= 0;
  else
    getTable()->status= STATUS_NOT_FOUND;

  advance_cursor= true;

  return ret;
}

int HailDBCursor::index_read(unsigned char *buf,
                                     const unsigned char *key_ptr,
                                     uint32_t key_len,
                                     drizzled::ha_rkey_function find_flag)
{
  return haildb_index_read(buf, key_ptr, key_len, find_flag, false);
}

/* This is straight from cursor.cc, but it's private there :( */
uint32_t HailDBCursor::calculate_key_len(uint32_t key_position,
                                                 key_part_map keypart_map_arg)
{
  /* works only with key prefixes */
  assert(((keypart_map_arg + 1) & keypart_map_arg) == 0);

  KeyPartInfo *key_part_found= getTable()->getShare()->getKeyInfo(key_position).key_part;
  KeyPartInfo *end_key_part_found= key_part_found + getTable()->getShare()->getKeyInfo(key_position).key_parts;
  uint32_t length= 0;

  while (key_part_found < end_key_part_found && keypart_map_arg)
  {
    length+= key_part_found->store_length;
    keypart_map_arg >>= 1;
    key_part_found++;
  }
  return length;
}


int HailDBCursor::haildb_index_read_map(unsigned char * buf,
                                                const unsigned char *key,
                                                key_part_map keypart_map,
                                                enum ha_rkey_function find_flag,
                                                bool allocate_blobs)
{
  uint32_t key_len= calculate_key_len(active_index, keypart_map);
  return  haildb_index_read(buf, key, key_len, find_flag, allocate_blobs);
}

int HailDBCursor::index_read_idx_map(unsigned char * buf,
                                             uint32_t index,
                                             const unsigned char * key,
                                             key_part_map keypart_map,
                                             enum ha_rkey_function find_flag)
{
  int error, error1;
  error= doStartIndexScan(index, 0);
  if (!error)
  {
    error= haildb_index_read_map(buf, key, keypart_map, find_flag, true);
    error1= doEndIndexScan();
  }
  return error ?  error : error1;
}

int HailDBCursor::reset()
{
  if (blobroot)
    blobroot->free_root(MYF(0));

  return 0;
}

int HailDBCursor::index_next(unsigned char *buf)
{
  int ret= HA_ERR_END_OF_FILE;

  if (advance_cursor)
  {
    ib_err_t err= ib_cursor_next(cursor);
    if (err == DB_END_OF_INDEX)
      return HA_ERR_END_OF_FILE;
  }

  tuple= ib_tuple_clear(tuple);
  ret= read_row_from_haildb(buf, cursor, tuple, getTable(),
                            share->has_hidden_primary_key,
                            &hidden_autoinc_pkey_position);

  advance_cursor= true;
  return ret;
}

int HailDBCursor::doEndIndexScan()
{
  active_index= MAX_KEY;

  return doEndTableScan();
}

int HailDBCursor::index_prev(unsigned char *buf)
{
  int ret= HA_ERR_END_OF_FILE;
  ib_err_t err;

  if (advance_cursor)
  {
    err= ib_cursor_prev(cursor);
    if (err != DB_SUCCESS)
    {
      if (err == DB_END_OF_INDEX)
        return HA_ERR_END_OF_FILE;
      else
        return -1; // FIXME
    }
  }

  tuple= ib_tuple_clear(tuple);
  ret= read_row_from_haildb(buf, cursor, tuple, getTable(),
                            share->has_hidden_primary_key,
                            &hidden_autoinc_pkey_position);

  advance_cursor= true;

  return ret;
}


int HailDBCursor::index_first(unsigned char *buf)
{
  int ret= HA_ERR_END_OF_FILE;
  ib_err_t err;

  err= ib_cursor_first(cursor);
  if (err != DB_SUCCESS)
    return ib_err_t_to_drizzle_error(err);

  tuple= ib_tuple_clear(tuple);
  ret= read_row_from_haildb(buf, cursor, tuple, getTable(),
                            share->has_hidden_primary_key,
                            &hidden_autoinc_pkey_position);

  advance_cursor= true;

  return ret;
}


int HailDBCursor::index_last(unsigned char *buf)
{
  int ret= HA_ERR_END_OF_FILE;
  ib_err_t err;

  err= ib_cursor_last(cursor);
  if (err != DB_SUCCESS)
    return ib_err_t_to_drizzle_error(err);

  tuple= ib_tuple_clear(tuple);
  ret= read_row_from_haildb(buf, cursor, tuple, getTable(),
                            share->has_hidden_primary_key,
                            &hidden_autoinc_pkey_position);
  advance_cursor= true;

  return ret;
}

int HailDBCursor::extra(enum ha_extra_function operation)
{
  switch (operation)
  {
  case HA_EXTRA_FLUSH:
    if (blobroot)
      blobroot->free_root(MYF(0));
    break;
  case HA_EXTRA_WRITE_CAN_REPLACE:
    write_can_replace= true;
    break;
  case HA_EXTRA_WRITE_CANNOT_REPLACE:
    write_can_replace= false;
    break;
  default:
    break;
  }

  return 0;
}

static int create_table_message_table()
{
  ib_tbl_sch_t schema;
  ib_idx_sch_t index_schema;
  ib_trx_t transaction;
  ib_id_t table_id;
  ib_err_t err, rollback_err;
  ib_bool_t create_db_err;

  create_db_err= ib_database_create("data_dictionary");
  if (create_db_err != IB_TRUE)
    return -1;

  err= ib_table_schema_create(HAILDB_TABLE_DEFINITIONS_TABLE, &schema,
                              IB_TBL_COMPACT, 0);
  if (err != DB_SUCCESS)
    return err;

  err= ib_table_schema_add_col(schema, "table_name", IB_VARCHAR, IB_COL_NONE, 0,
                               IB_MAX_TABLE_NAME_LEN);
  if (err != DB_SUCCESS)
    goto free_err;

  err= ib_table_schema_add_col(schema, "message", IB_BLOB, IB_COL_NONE, 0, 0);
  if (err != DB_SUCCESS)
    goto free_err;

  err= ib_table_schema_add_index(schema, "PRIMARY_KEY", &index_schema);
  if (err != DB_SUCCESS)
    goto free_err;

  err= ib_index_schema_add_col(index_schema, "table_name", 0);
  if (err != DB_SUCCESS)
    goto free_err;
  err= ib_index_schema_set_clustered(index_schema);
  if (err != DB_SUCCESS)
    goto free_err;

  transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  err= ib_schema_lock_exclusive(transaction);
  if (err != DB_SUCCESS)
    goto rollback;

  err= ib_table_create(transaction, schema, &table_id);
  if (err != DB_SUCCESS)
    goto rollback;

  err= ib_trx_commit(transaction);
  if (err != DB_SUCCESS)
    goto rollback;

  ib_table_schema_delete(schema);

  return 0;
rollback:
  ib_schema_unlock(transaction);
  rollback_err= ib_trx_rollback(transaction);
  assert(rollback_err == DB_SUCCESS);
free_err:
  ib_table_schema_delete(schema);
  return err;
}

static char*  innobase_log_group_home_dir   = NULL;
static bool innobase_use_doublewrite= true;
static unsigned long innobase_fast_shutdown= 1;
static bool srv_file_per_table= false;
static bool innobase_adaptive_hash_index;
static bool srv_adaptive_flushing;
static bool innobase_print_verbose_log;
static bool innobase_rollback_on_timeout;
static bool innobase_create_status_file;
static bool srv_use_sys_malloc;
static char*  innobase_file_format_name   = const_cast<char *>("Barracuda");
static char*  innobase_unix_file_flush_method   = NULL;
static unsigned long srv_flush_log_at_trx_commit;
static unsigned long srv_max_buf_pool_modified_pct;
static unsigned long srv_max_purge_lag;
static unsigned long innobase_lru_old_blocks_pct;
static unsigned long innobase_lru_block_access_recency;
static unsigned long innobase_read_io_threads;
static unsigned long innobase_write_io_threads;
typedef constrained_check<unsigned int, 1000, 1> autoextend_constraint;
static autoextend_constraint srv_auto_extend_increment;
static unsigned long innobase_lock_wait_timeout;
static unsigned long srv_n_spin_wait_rounds;
typedef constrained_check<size_t, SIZE_MAX, 5242880, 1048576> buffer_pool_constraint;
static buffer_pool_constraint innobase_buffer_pool_size;
typedef constrained_check<size_t, SIZE_MAX, 512, 1024> additional_mem_pool_constraint;
static additional_mem_pool_constraint innobase_additional_mem_pool_size;
static bool  innobase_use_checksums= true;
typedef constrained_check<unsigned int, UINT_MAX, 100> io_capacity_constraint;
static io_capacity_constraint srv_io_capacity;

static long innobase_open_files;
static long innobase_force_recovery;
static long innobase_log_buffer_size;
static char  default_haildb_data_file_path[]= "ibdata1:10M:autoextend";
static char* haildb_data_file_path= NULL;

static int64_t haildb_log_file_size;
static int64_t haildb_log_files_in_group;

static int haildb_init(drizzled::module::Context &context)
{
  haildb_system_table_names.insert(std::string("HAILDB_SYS_TABLES"));
  haildb_system_table_names.insert(std::string("HAILDB_SYS_COLUMNS"));
  haildb_system_table_names.insert(std::string("HAILDB_SYS_INDEXES"));
  haildb_system_table_names.insert(std::string("HAILDB_SYS_FIELDS"));
  haildb_system_table_names.insert(std::string("HAILDB_SYS_FOREIGN"));
  haildb_system_table_names.insert(std::string("HAILDB_SYS_FOREIGN_COLS"));

  const module::option_map &vm= context.getOptions();

  /* Inverted Booleans */

  innobase_adaptive_hash_index= (vm.count("disable-adaptive-hash-index")) ? false : true;
  srv_adaptive_flushing= (vm.count("disable-adaptive-flushing")) ? false : true;
  innobase_use_checksums= (vm.count("disable-checksums")) ? false : true;
  innobase_use_doublewrite= (vm.count("disable-doublewrite")) ? false : true;
  innobase_print_verbose_log= (vm.count("disable-print-verbose-log")) ? false : true;
  srv_use_sys_malloc= (vm.count("use-internal-malloc")) ? false : true;

  if (vm.count("fast-shutdown"))
  {
    if (innobase_fast_shutdown > 2)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of fast-shutdown"));
      return 1;
    }
  }

  if (vm.count("flush-log-at-trx-commit"))
  {
    if (srv_flush_log_at_trx_commit > 2)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of flush-log-at-trx-commit"));
      return 1;
    }
  }

  if (vm.count("force-recovery"))
  {
    if (innobase_force_recovery > 6)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of force-recovery"));
      return 1;
    }
  }

  if (vm.count("log-file-size"))
  {
    if (haildb_log_file_size > INT64_MAX || haildb_log_file_size < 1*1024*1024L)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of log-file-size"));
      return 1;
    }
    haildb_log_file_size/= 1024*1024L;
    haildb_log_file_size*= 1024*1024L;
  }

  if (vm.count("log-files-in-group"))
  {
    if (haildb_log_files_in_group > 100 || haildb_log_files_in_group < 2)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of log-files-in-group"));
      return 1;
    }
  }

  if (vm.count("lock-wait-timeout"))
  {
    if (innobase_lock_wait_timeout > 1024*1024*1024 || innobase_lock_wait_timeout < 1)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of lock-wait-timeout"));
      return 1;
    }
  }

  if (vm.count("log-buffer-size"))
  {
    if (innobase_log_buffer_size > LONG_MAX || innobase_log_buffer_size < 256*1024L)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of log-buffer-size"));
      return 1;
    }
    innobase_log_buffer_size/= 1024;
    innobase_log_buffer_size*= 1024;
  }

  if (vm.count("lru-old-blocks-pct"))
  {
    if (innobase_lru_old_blocks_pct > 95 || innobase_lru_old_blocks_pct < 5)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of lru-old-blocks-pct"));
      return 1;
    }
  }

  if (vm.count("lru-block-access-recency"))
  {
    if (innobase_lru_block_access_recency > ULONG_MAX)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of lru-block-access-recency"));
      return 1;
    }
  }

  if (vm.count("max-dirty-pages-pct"))
  {
    if (srv_max_buf_pool_modified_pct > 99)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of max-dirty-pages-pct"));
      return 1;
    }
  }

  if (vm.count("max-purge-lag"))
  {
    if (srv_max_purge_lag > (unsigned long)~0L)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of max-purge-lag"));
      return 1;
    }
  }

  if (vm.count("open-files"))
  {
    if (innobase_open_files > LONG_MAX || innobase_open_files < 10L)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of open-files"));
      return 1;
    }
  }

  if (vm.count("read-io-threads"))
  {
    if (innobase_read_io_threads > 64 || innobase_read_io_threads < 1)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of read-io-threads"));
      return 1;
    }
  }

  if (vm.count("sync-spin-loops"))
  {
    if (srv_n_spin_wait_rounds > (unsigned long)~0L)
    {
      errmsg_printf(ERRMSG_LVL_ERROR, _("Invalid value of sync_spin_loops"));
      return 1;
    }
  }

  if (vm.count("file-format"))
  {
    innobase_file_format_name= const_cast<char *>(vm["file-format"].as<string>().c_str());
  }

  if (vm.count("log-group-home-dir"))
  {
    innobase_log_group_home_dir= const_cast<char *>(vm["log-group-home-dir"].as<string>().c_str());
  }

  if (vm.count("flush-method"))
  {
    innobase_unix_file_flush_method= const_cast<char *>(vm["flush-method"].as<string>().c_str());
  }

  if (vm.count("data-file-path"))
  {
    haildb_data_file_path= const_cast<char *>(vm["data-file-path"].as<string>().c_str());
  }

  ib_err_t err;

  err= ib_init();
  if (err != DB_SUCCESS)
    goto haildb_error;


  if (vm.count("data-home-dir"))
  {
    err= ib_cfg_set_text("data_home_dir", vm["data-home-dir"].as<string>().c_str());
    if (err != DB_SUCCESS)
      goto haildb_error;
  }

  if (innobase_log_group_home_dir)
  {
    err= ib_cfg_set_text("log_group_home_dir", innobase_log_group_home_dir);
    if (err != DB_SUCCESS)
      goto haildb_error;
  }

  if (haildb_data_file_path == NULL)
    haildb_data_file_path= default_haildb_data_file_path;

  if (innobase_print_verbose_log)
    err= ib_cfg_set_bool_on("print_verbose_log");
  else
    err= ib_cfg_set_bool_off("print_verbose_log");

  if (err != DB_SUCCESS)
    goto haildb_error;

  if (innobase_rollback_on_timeout)
    err= ib_cfg_set_bool_on("rollback_on_timeout");
  else
    err= ib_cfg_set_bool_off("rollback_on_timeout");

  if (err != DB_SUCCESS)
    goto haildb_error;

  if (innobase_use_doublewrite)
    err= ib_cfg_set_bool_on("doublewrite");
  else
    err= ib_cfg_set_bool_off("doublewrite");

  if (err != DB_SUCCESS)
    goto haildb_error;

  if (innobase_adaptive_hash_index)
    err= ib_cfg_set_bool_on("adaptive_hash_index");
  else
    err= ib_cfg_set_bool_off("adaptive_hash_index");

  if (err != DB_SUCCESS)
    goto haildb_error;

  if (srv_adaptive_flushing)
    err= ib_cfg_set_bool_on("adaptive_flushing");
  else
    err= ib_cfg_set_bool_off("adaptive_flushing");

  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("additional_mem_pool_size", static_cast<size_t>(innobase_additional_mem_pool_size));
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("autoextend_increment", static_cast<unsigned int>(srv_auto_extend_increment));
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("buffer_pool_size", static_cast<size_t>(innobase_buffer_pool_size));
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("io_capacity", static_cast<unsigned int>(srv_io_capacity));
  if (err != DB_SUCCESS)
    goto haildb_error;

  if (srv_file_per_table)
    err= ib_cfg_set_bool_on("file_per_table");
  else
    err= ib_cfg_set_bool_off("file_per_table");

  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("flush_log_at_trx_commit", srv_flush_log_at_trx_commit);
  if (err != DB_SUCCESS)
    goto haildb_error;

  if (innobase_unix_file_flush_method)
  {
    err= ib_cfg_set_text("flush_method", innobase_unix_file_flush_method);
    if (err != DB_SUCCESS)
      goto haildb_error;
  }

  err= ib_cfg_set_int("force_recovery", innobase_force_recovery);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_text("data_file_path", haildb_data_file_path);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("log_file_size", haildb_log_file_size);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("log_buffer_size", innobase_log_buffer_size);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("log_files_in_group", haildb_log_files_in_group);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("checksums", innobase_use_checksums);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("lock_wait_timeout", innobase_lock_wait_timeout);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("max_dirty_pages_pct", srv_max_buf_pool_modified_pct);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("max_purge_lag", srv_max_purge_lag);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("open_files", innobase_open_files);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("read_io_threads", innobase_read_io_threads);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("write_io_threads", innobase_write_io_threads);
  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_cfg_set_int("sync_spin_loops", srv_n_spin_wait_rounds);
  if (err != DB_SUCCESS)
    goto haildb_error;

  if (srv_use_sys_malloc)
    err= ib_cfg_set_bool_on("use_sys_malloc");
  else
    err= ib_cfg_set_bool_off("use_sys_malloc");

  if (err != DB_SUCCESS)
    goto haildb_error;

  err= ib_startup(innobase_file_format_name);
  if (err != DB_SUCCESS)
    goto haildb_error;

  create_table_message_table();

  haildb_engine= new HailDBEngine("InnoDB");
  context.add(haildb_engine);
  context.registerVariable(new sys_var_bool_ptr_readonly("adaptive_hash_index",
                                                         &innobase_adaptive_hash_index));
  context.registerVariable(new sys_var_bool_ptr_readonly("adaptive_flushing",
                                                         &srv_adaptive_flushing));
  context.registerVariable(new sys_var_constrained_value_readonly<size_t>("additional_mem_pool_size",innobase_additional_mem_pool_size));
  context.registerVariable(new sys_var_constrained_value_readonly<unsigned int>("autoextend_increment", srv_auto_extend_increment));
  context.registerVariable(new sys_var_constrained_value_readonly<size_t>("buffer_pool_size", innobase_buffer_pool_size));
  context.registerVariable(new sys_var_bool_ptr_readonly("checksums",
                                                         &innobase_use_checksums));
  context.registerVariable(new sys_var_bool_ptr_readonly("doublewrite",
                                                         &innobase_use_doublewrite));
  context.registerVariable(new sys_var_const_string_val("data_home_dir",
                                                vm.count("data-home-dir") ?  vm["data-home-dir"].as<string>() : ""));
  context.registerVariable(new sys_var_constrained_value_readonly<unsigned int>("io_capacity", srv_io_capacity));

  haildb_datadict_dump_func_initialize(context);
  config_table_function_initialize(context);
  status_table_function_initialize(context);

  return 0;
haildb_error:
  fprintf(stderr, _("Error starting HailDB %d (%s)\n"),
          err, ib_strerror(err));
  return -1;
}


HailDBEngine::~HailDBEngine()
{
  ib_err_t err;
  ib_shutdown_t shutdown_flag= IB_SHUTDOWN_NORMAL;

  if (innobase_fast_shutdown == 1)
    shutdown_flag= IB_SHUTDOWN_NO_IBUFMERGE_PURGE;
  else if (innobase_fast_shutdown == 2)
    shutdown_flag= IB_SHUTDOWN_NO_BUFPOOL_FLUSH;

  err= ib_shutdown(shutdown_flag);

  if (err != DB_SUCCESS)
  {
    fprintf(stderr,"Error %d shutting down HailDB!\n", err);
  }

}

static char haildb_file_format_name_storage[100];

static int haildb_file_format_name_validate(Session*, drizzle_sys_var*,
                                            void *save,
                                            drizzle_value *value)
{
  ib_err_t err;
  char buff[100];
  int len= sizeof(buff);
  const char *format= value->val_str(value, buff, &len);

  *static_cast<const char**>(save)= NULL;

  if (format == NULL)
    return 1;

  err= ib_cfg_set_text("file_format", format);

  if (err == DB_SUCCESS)
  {
    strncpy(haildb_file_format_name_storage, format, sizeof(haildb_file_format_name_storage));;
    haildb_file_format_name_storage[sizeof(haildb_file_format_name_storage)-1]= 0;

    *static_cast<const char**>(save)= haildb_file_format_name_storage;
    return 0;
  }
  else
    return 1;
}

static void haildb_file_format_name_update(Session*, drizzle_sys_var*,
                                           void *var_ptr,
                                           const void *save)

{
  const char* format;

  assert(var_ptr != NULL);
  assert(save != NULL);

  format= *static_cast<const char*const*>(save);

  /* Format is already set in validate */
  memmove(haildb_file_format_name_storage, format, sizeof(haildb_file_format_name_storage));;
  haildb_file_format_name_storage[sizeof(haildb_file_format_name_storage)-1]= 0;

  *static_cast<const char**>(var_ptr)= haildb_file_format_name_storage;
}

static void haildb_lru_old_blocks_pct_update(Session*, drizzle_sys_var*,
                                             void *,
                                             const void *save)

{
  unsigned long pct;

  pct= *static_cast<const unsigned long*>(save);

  ib_err_t err= ib_cfg_set_int("lru_old_blocks_pct", pct);
  if (err == DB_SUCCESS)
    innobase_lru_old_blocks_pct= pct;
}

static void haildb_lru_block_access_recency_update(Session*, drizzle_sys_var*,
                                                   void *,
                                                   const void *save)

{
  unsigned long ms;

  ms= *static_cast<const unsigned long*>(save);

  ib_err_t err= ib_cfg_set_int("lru_block_access_recency", ms);

  if (err == DB_SUCCESS)
    innobase_lru_block_access_recency= ms;
}

static void haildb_status_file_update(Session*, drizzle_sys_var*,
                                      void *,
                                      const void *save)

{
  bool status_file_enabled;
  ib_err_t err;

  status_file_enabled= *static_cast<const bool*>(save);


  if (status_file_enabled)
    err= ib_cfg_set_bool_on("status_file");
  else
    err= ib_cfg_set_bool_off("status_file");

  if (err == DB_SUCCESS)
    innobase_create_status_file= status_file_enabled;
}

static DRIZZLE_SYSVAR_ULONG(fast_shutdown, innobase_fast_shutdown,
  PLUGIN_VAR_OPCMDARG,
  "Speeds up the shutdown process of the HailDB storage engine. Possible "
  "values are 0, 1 (faster)"
  " or 2 (fastest - crash-like)"
  ".",
  NULL, NULL, 1, 0, 2, 0);

static DRIZZLE_SYSVAR_BOOL(file_per_table, srv_file_per_table,
  PLUGIN_VAR_NOCMDARG,
  "Stores each HailDB table to an .ibd file in the database dir.",
  NULL, NULL, false);

static DRIZZLE_SYSVAR_STR(file_format, innobase_file_format_name,
  PLUGIN_VAR_RQCMDARG,
  "File format to use for new tables in .ibd files.",
  haildb_file_format_name_validate,
  haildb_file_format_name_update, "Barracuda");

static DRIZZLE_SYSVAR_ULONG(flush_log_at_trx_commit, srv_flush_log_at_trx_commit,
  PLUGIN_VAR_OPCMDARG,
  "Set to 0 (write and flush once per second),"
  " 1 (write and flush at each commit)"
  " or 2 (write at commit, flush once per second).",
  NULL, NULL, 1, 0, 2, 0);

static DRIZZLE_SYSVAR_STR(flush_method, innobase_unix_file_flush_method,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "With which method to flush data.", NULL, NULL, NULL);

static DRIZZLE_SYSVAR_LONG(force_recovery, innobase_force_recovery,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Helps to save your data in case the disk image of the database becomes corrupt.",
  NULL, NULL, 0, 0, 6, 0);

static DRIZZLE_SYSVAR_STR(data_file_path, haildb_data_file_path,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Path to individual files and their sizes.",
  NULL, NULL, NULL);

static DRIZZLE_SYSVAR_STR(log_group_home_dir, innobase_log_group_home_dir,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Path to HailDB log files.", NULL, NULL, NULL);

static DRIZZLE_SYSVAR_LONGLONG(log_file_size, haildb_log_file_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Size of each log file in a log group.",
  NULL, NULL, 20*1024*1024L, 1*1024*1024L, INT64_MAX, 1024*1024L);

static DRIZZLE_SYSVAR_LONGLONG(log_files_in_group, haildb_log_files_in_group,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of log files in the log group. HailDB writes to the files in a circular fashion. Value 3 is recommended here.",
  NULL, NULL, 2, 2, 100, 0);

static DRIZZLE_SYSVAR_ULONG(lock_wait_timeout, innobase_lock_wait_timeout,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Timeout in seconds an HailDB transaction may wait for a lock before being rolled back. Values above 100000000 disable the timeout.",
  NULL, NULL, 5, 1, 1024 * 1024 * 1024, 0);

static DRIZZLE_SYSVAR_LONG(log_buffer_size, innobase_log_buffer_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "The size of the buffer which HailDB uses to write log to the log files on disk.",
  NULL, NULL, 8*1024*1024L, 256*1024L, LONG_MAX, 1024);

static DRIZZLE_SYSVAR_ULONG(lru_old_blocks_pct, innobase_lru_old_blocks_pct,
  PLUGIN_VAR_RQCMDARG,
  "Sets the point in the LRU list from where all pages are classified as "
  "old (Advanced users)",
  NULL,
  haildb_lru_old_blocks_pct_update, 37, 5, 95, 0);

static DRIZZLE_SYSVAR_ULONG(lru_block_access_recency, innobase_lru_block_access_recency,
  PLUGIN_VAR_RQCMDARG,
  "Milliseconds between accesses to a block at which it is made young. "
  "0=disabled (Advanced users)",
  NULL,
  haildb_lru_block_access_recency_update, 0, 0, ULONG_MAX, 0);


static DRIZZLE_SYSVAR_ULONG(max_dirty_pages_pct, srv_max_buf_pool_modified_pct,
  PLUGIN_VAR_RQCMDARG,
  "Percentage of dirty pages allowed in bufferpool.",
  NULL, NULL, 75, 0, 99, 0);

static DRIZZLE_SYSVAR_ULONG(max_purge_lag, srv_max_purge_lag,
  PLUGIN_VAR_RQCMDARG,
  "Desired maximum length of the purge queue (0 = no limit)",
  NULL, NULL, 0, 0, ~0L, 0);

static DRIZZLE_SYSVAR_BOOL(rollback_on_timeout, innobase_rollback_on_timeout,
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
  "Roll back the complete transaction on lock wait timeout, for 4.x compatibility (disabled by default)",
  NULL, NULL, false);

static DRIZZLE_SYSVAR_LONG(open_files, innobase_open_files,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "How many files at the maximum HailDB keeps open at the same time.",
  NULL, NULL, 300L, 10L, LONG_MAX, 0);

static DRIZZLE_SYSVAR_ULONG(read_io_threads, innobase_read_io_threads,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of background read I/O threads in HailDB.",
  NULL, NULL, 4, 1, 64, 0);

static DRIZZLE_SYSVAR_ULONG(write_io_threads, innobase_write_io_threads,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of background write I/O threads in HailDB.",
  NULL, NULL, 4, 1, 64, 0);

static DRIZZLE_SYSVAR_BOOL(print_verbose_log, innobase_print_verbose_log,
  PLUGIN_VAR_NOCMDARG,
  "Disable if you want to reduce the number of messages written to the log (default: enabled).",
  NULL, NULL, true);

static DRIZZLE_SYSVAR_BOOL(status_file, innobase_create_status_file,
  PLUGIN_VAR_OPCMDARG,
  "Enable SHOW HAILDB STATUS output in the log",
  NULL, haildb_status_file_update, false);

static DRIZZLE_SYSVAR_ULONG(sync_spin_loops, srv_n_spin_wait_rounds,
  PLUGIN_VAR_RQCMDARG,
  "Count of spin-loop rounds in HailDB mutexes (30 by default)",
  NULL, NULL, 30L, 0L, ~0L, 0);

static DRIZZLE_SYSVAR_BOOL(use_sys_malloc, srv_use_sys_malloc,
  PLUGIN_VAR_NOCMDARG | PLUGIN_VAR_READONLY,
  "Use OS memory allocator instead of HailDB's internal memory allocator",
  NULL, NULL, true);

static void init_options(drizzled::module::option_context &context)
{
  context("disable-adaptive-hash-index",
          N_("Disable HailDB adaptive hash index (enabled by default)."));
  context("disable-adaptive-flushing",
          N_("Do not attempt to flush dirty pages to avoid IO bursts at checkpoints."));
  context("additional-mem-pool-size",
          po::value<additional_mem_pool_constraint>(&innobase_additional_mem_pool_size)->default_value(8*1024*1024L),
          N_("Size of a memory pool HailDB uses to store data dictionary information and other internal data structures."));
  context("autoextend-increment",
          po::value<autoextend_constraint>(&srv_auto_extend_increment)->default_value(8),
          N_("Data file autoextend increment in megabytes"));
  context("buffer-pool-size",
          po::value<buffer_pool_constraint>(&innobase_buffer_pool_size)->default_value(128*1024*1024L),
          N_("The size of the memory buffer HailDB uses to cache data and indexes of its tables."));
  context("data-home-dir",
          po::value<string>(),
          N_("The common part for HailDB table spaces."));
  context("disable-checksums",
          N_("Disable HailDB checksums validation (enabled by default)."));
  context("disable-doublewrite",
          N_("Disable HailDB doublewrite buffer (enabled by default)."));
  context("io-capacity",
          po::value<io_capacity_constraint>(&srv_io_capacity)->default_value(200),
          N_("Number of IOPs the server can do. Tunes the background IO rate"));
  context("fast-shutdown",
          po::value<unsigned long>(&innobase_fast_shutdown)->default_value(1),
          N_("Speeds up the shutdown process of the HailDB storage engine. Possible values are 0, 1 (faster) or 2 (fastest - crash-like)."));
  context("file-per-table",
          po::value<bool>(&srv_file_per_table)->default_value(false)->zero_tokens(),
          N_("Stores each HailDB table to an .ibd file in the database dir."));
  context("file-format",
          po::value<string>(),
          N_("File format to use for new tables in .ibd files."));
  context("flush-log-at-trx-commit",
          po::value<unsigned long>(&srv_flush_log_at_trx_commit)->default_value(1),
          N_("Set to 0 (write and flush once per second),1 (write and flush at each commit) or 2 (write at commit, flush once per second)."));
  context("flush-method",
          po::value<string>(),
          N_("With which method to flush data."));
  context("force-recovery",
          po::value<long>(&innobase_force_recovery)->default_value(0),
          N_("Helps to save your data in case the disk image of the database becomes corrupt."));
  context("data-file-path",
          po::value<string>(),
          N_("Path to individual files and their sizes."));
  context("log-group-home-dir",
          po::value<string>(),
          N_("Path to individual files and their sizes."));
  context("log-group-home-dir",
          po::value<string>(),
          N_("Path to HailDB log files."));
  context("log-file-size",
          po::value<int64_t>(&haildb_log_file_size)->default_value(20*1024*1024L),
          N_("Size of each log file in a log group."));
  context("haildb-log-files-in-group",
          po::value<int64_t>(&haildb_log_files_in_group)->default_value(2),
          N_("Number of log files in the log group. HailDB writes to the files in a circular fashion. Value 3 is recommended here."));
  context("lock-wait-timeout",
          po::value<unsigned long>(&innobase_lock_wait_timeout)->default_value(5),
          N_("Timeout in seconds an HailDB transaction may wait for a lock before being rolled back. Values above 100000000 disable the timeout."));
  context("log-buffer-size",
        po::value<long>(&innobase_log_buffer_size)->default_value(8*1024*1024L),
        N_("The size of the buffer which HailDB uses to write log to the log files on disk."));
  context("lru-old-blocks-pct",
          po::value<unsigned long>(&innobase_lru_old_blocks_pct)->default_value(37),
          N_("Sets the point in the LRU list from where all pages are classified as old (Advanced users)"));
  context("lru-block-access-recency",
          po::value<unsigned long>(&innobase_lru_block_access_recency)->default_value(0),
          N_("Milliseconds between accesses to a block at which it is made young. 0=disabled (Advanced users)"));
  context("max-dirty-pages-pct",
          po::value<unsigned long>(&srv_max_buf_pool_modified_pct)->default_value(75),
          N_("Percentage of dirty pages allowed in bufferpool."));
  context("max-purge-lag",
          po::value<unsigned long>(&srv_max_purge_lag)->default_value(0),
          N_("Desired maximum length of the purge queue (0 = no limit)"));
  context("rollback-on-timeout",
          po::value<bool>(&innobase_rollback_on_timeout)->default_value(false)->zero_tokens(),
          N_("Roll back the complete transaction on lock wait timeout, for 4.x compatibility (disabled by default)"));
  context("open-files",
          po::value<long>(&innobase_open_files)->default_value(300),
          N_("How many files at the maximum HailDB keeps open at the same time."));
  context("read-io-threads",
          po::value<unsigned long>(&innobase_read_io_threads)->default_value(4),
          N_("Number of background read I/O threads in HailDB."));
  context("write-io-threads",
          po::value<unsigned long>(&innobase_write_io_threads)->default_value(4),
          N_("Number of background write I/O threads in HailDB."));
  context("disable-print-verbose-log",
          N_("Disable if you want to reduce the number of messages written to the log (default: enabled)."));
  context("status-file",
          po::value<bool>(&innobase_create_status_file)->default_value(false)->zero_tokens(),
          N_("Enable SHOW HAILDB STATUS output in the log"));
  context("sync-spin-loops",
          po::value<unsigned long>(&srv_n_spin_wait_rounds)->default_value(30L),
          N_("Count of spin-loop rounds in HailDB mutexes (30 by default)"));
  context("use-internal-malloc",
          N_("Use HailDB's internal memory allocator instead of the OS memory allocator"));
}

static drizzle_sys_var* innobase_system_variables[]= {
  DRIZZLE_SYSVAR(fast_shutdown),
  DRIZZLE_SYSVAR(file_per_table),
  DRIZZLE_SYSVAR(file_format),
  DRIZZLE_SYSVAR(flush_log_at_trx_commit),
  DRIZZLE_SYSVAR(flush_method),
  DRIZZLE_SYSVAR(force_recovery),
  DRIZZLE_SYSVAR(log_group_home_dir),
  DRIZZLE_SYSVAR(data_file_path),
  DRIZZLE_SYSVAR(lock_wait_timeout),
  DRIZZLE_SYSVAR(log_file_size),
  DRIZZLE_SYSVAR(log_files_in_group),
  DRIZZLE_SYSVAR(log_buffer_size),
  DRIZZLE_SYSVAR(lru_old_blocks_pct),
  DRIZZLE_SYSVAR(lru_block_access_recency),
  DRIZZLE_SYSVAR(max_dirty_pages_pct),
  DRIZZLE_SYSVAR(max_purge_lag),
  DRIZZLE_SYSVAR(open_files),
  DRIZZLE_SYSVAR(read_io_threads),
  DRIZZLE_SYSVAR(rollback_on_timeout),
  DRIZZLE_SYSVAR(write_io_threads),
  DRIZZLE_SYSVAR(print_verbose_log),
  DRIZZLE_SYSVAR(status_file),
  DRIZZLE_SYSVAR(sync_spin_loops),
  DRIZZLE_SYSVAR(use_sys_malloc),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "INNODB",
  "1.0",
  "Stewart Smith",
  "Transactional Storage Engine using the HailDB Library",
  PLUGIN_LICENSE_GPL,
  haildb_init,     /* Plugin Init */
  innobase_system_variables, /* system variables */
  init_options                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
