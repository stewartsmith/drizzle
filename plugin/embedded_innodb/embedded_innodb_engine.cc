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

#include "config.h"
#include <drizzled/table.h>
#include <drizzled/error.h>
#include "drizzled/internal/my_pthread.h"
#include <drizzled/plugin/transactional_storage_engine.h>

#include <fcntl.h>

#include <string>
#include <map>
#include <fstream>
#include <drizzled/message/table.pb.h>
#include "drizzled/internal/m_string.h"

#include "drizzled/global_charset_info.h"

#include "libinnodb_version_func.h"
#include "libinnodb_datadict_dump_func.h"

#include "embedded_innodb-1.0/innodb.h"

#include "embedded_innodb_engine.h"

#include <drizzled/field.h>
#include <drizzled/session.h>

using namespace std;
using namespace google;
using namespace drizzled;

int read_row_from_innodb(ib_crsr_t cursor, ib_tpl_t tuple, Table* table);

#define EMBEDDED_INNODB_EXT ".EID"

const char INNODB_TABLE_DEFINITIONS_TABLE[]= "data_dictionary/innodb_table_definitions";
const string statement_savepoint_name("STATEMENT");


static const char *EmbeddedInnoDBCursor_exts[] = {
  NULL
};

class EmbeddedInnoDBEngine : public drizzled::plugin::TransactionalStorageEngine
{
public:
  EmbeddedInnoDBEngine(const string &name_arg)
   : drizzled::plugin::TransactionalStorageEngine(name_arg,
                                                  HTON_NULL_IN_KEY |
                                                  HTON_CAN_INDEX_BLOBS |
                                                  HTON_AUTO_PART_KEY |
                                                  HTON_HAS_DOES_TRANSACTIONS)
  {
    table_definition_ext= EMBEDDED_INNODB_EXT;
  }

  ~EmbeddedInnoDBEngine();

  virtual Cursor *create(TableShare &table,
                         drizzled::memory::Root *mem_root)
  {
    return new (mem_root) EmbeddedInnoDBCursor(*this, table);
  }

  const char **bas_ext() const {
    return EmbeddedInnoDBCursor_exts;
  }

  int doCreateTable(Session&,
                    Table& table_arg,
                    drizzled::TableIdentifier &identifier,
                    drizzled::message::Table& proto);

  int doDropTable(Session&, TableIdentifier &identifier);

  int doRenameTable(drizzled::Session&,
                    drizzled::TableIdentifier&,
                    drizzled::TableIdentifier&);

  int doGetTableDefinition(Session& session,
                           TableIdentifier &identifier,
                           drizzled::message::Table &table_proto);

  bool doDoesTableExist(Session&, TableIdentifier &identifier);

  void doGetTableNames(drizzled::CachedDirectory &,
                       drizzled::SchemaIdentifier &schema,
                       drizzled::plugin::TableNameList &set_of_names);

  /* The following defines can be increased if necessary */
  uint32_t max_supported_keys()          const { return 64; }
  uint32_t max_supported_key_length()    const { return 1000; }
  uint32_t max_supported_key_part_length() const { return 1000; }

  uint32_t index_flags(enum  ha_key_alg) const
  {
    return (HA_READ_NEXT |
            HA_READ_PREV |
            HA_READ_RANGE |
            HA_READ_ORDER |
            HA_KEYREAD_ONLY);
  }
private:
  virtual int doStartTransaction(Session *session,
                                 start_transaction_option_t options);
  virtual void doStartStatement(Session *session);
  virtual void doEndStatement(Session *session);

public:
  virtual int doSetSavepoint(Session* session,
                                 drizzled::NamedSavepoint &savepoint);
  virtual int doRollbackToSavepoint(Session* session,
                                     drizzled::NamedSavepoint &savepoint);
  virtual int doReleaseSavepoint(Session* session,
                                     drizzled::NamedSavepoint &savepoint);
  virtual int doCommit(Session* session, bool all);
  virtual int doRollback(Session* session, bool all);

  typedef std::map<std::string, EmbeddedInnoDBTableShare*> EmbeddedInnoDBMap;
  EmbeddedInnoDBMap embedded_innodb_open_tables;
  EmbeddedInnoDBTableShare *findOpenTable(const std::string table_name);
  void addOpenTable(const std::string &table_name, EmbeddedInnoDBTableShare *);
  void deleteOpenTable(const std::string &table_name);

};

static drizzled::plugin::StorageEngine *embedded_innodb_engine= NULL;


static ib_trx_t* get_trx(Session* session)
{
  return (ib_trx_t*) session->getEngineData(embedded_innodb_engine);
}

int EmbeddedInnoDBEngine::doStartTransaction(Session *session,
                                             start_transaction_option_t options)
{
  ib_trx_t *transaction;

  (void)options;

  transaction= get_trx(session);
  *transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);

  return 0;
}

void EmbeddedInnoDBEngine::doStartStatement(Session *session)
{
  if(*get_trx(session) == NULL)
    doStartTransaction(session, START_TRANS_NO_OPTIONS);

  ib_savepoint_take(*get_trx(session), statement_savepoint_name.c_str(),
                    statement_savepoint_name.length());
}

void EmbeddedInnoDBEngine::doEndStatement(Session *session)
{
  doCommit(session, false);
}

int EmbeddedInnoDBEngine::doSetSavepoint(Session* session,
                                         drizzled::NamedSavepoint &savepoint)
{
  ib_trx_t *transaction= get_trx(session);
  ib_savepoint_take(*transaction, savepoint.getName().c_str(),
                    savepoint.getName().length());
  return 0;
}

int EmbeddedInnoDBEngine::doRollbackToSavepoint(Session* session,
                                                drizzled::NamedSavepoint &savepoint)
{
  ib_trx_t *transaction= get_trx(session);
  ib_err_t err;

  err= ib_savepoint_rollback(*transaction, savepoint.getName().c_str(),
                             savepoint.getName().length());
  if (err != DB_SUCCESS)
    return -1;

  return 0;
}

int EmbeddedInnoDBEngine::doReleaseSavepoint(Session* session,
                                             drizzled::NamedSavepoint &savepoint)
{
  ib_trx_t *transaction= get_trx(session);
  ib_err_t err;

  err= ib_savepoint_release(*transaction, savepoint.getName().c_str(),
                            savepoint.getName().length());
  if (err != DB_SUCCESS)
    return -1;

  return 0;
}

int EmbeddedInnoDBEngine::doCommit(Session* session, bool all)
{
  ib_err_t err;
  ib_trx_t *transaction= get_trx(session);

  if (all || (!session_test_options(session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)))
  {
    err= ib_trx_commit(*transaction);

    if (err != DB_SUCCESS)
      return -1;

    *transaction= NULL;
  }

  return 0;
}

int EmbeddedInnoDBEngine::doRollback(Session* session, bool all)
{
  ib_err_t err;
  ib_trx_t *transaction= get_trx(session);

  if (all || !session_test_options(session, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
  {
    err= ib_trx_rollback(*transaction);

    if (err != DB_SUCCESS)
      return -1;

    *transaction= NULL;
  }
  else
  {
    err= ib_savepoint_rollback(*transaction, statement_savepoint_name.c_str(),
                               statement_savepoint_name.length());
    if (err != DB_SUCCESS)
      return -1;
  }

  return 0;
}

EmbeddedInnoDBTableShare *EmbeddedInnoDBEngine::findOpenTable(const string table_name)
{
  EmbeddedInnoDBMap::iterator find_iter=
    embedded_innodb_open_tables.find(table_name);

  if (find_iter != embedded_innodb_open_tables.end())
    return (*find_iter).second;
  else
    return NULL;
}

void EmbeddedInnoDBEngine::addOpenTable(const string &table_name, EmbeddedInnoDBTableShare *share)
{
  embedded_innodb_open_tables[table_name]= share;
}

void EmbeddedInnoDBEngine::deleteOpenTable(const string &table_name)
{
  embedded_innodb_open_tables.erase(table_name);
}

static pthread_mutex_t embedded_innodb_mutex= PTHREAD_MUTEX_INITIALIZER;

EmbeddedInnoDBTableShare *EmbeddedInnoDBCursor::get_share(const char *table_name, int *rc)
{
  pthread_mutex_lock(&embedded_innodb_mutex);

  EmbeddedInnoDBEngine *a_engine= static_cast<EmbeddedInnoDBEngine *>(engine);
  share= a_engine->findOpenTable(table_name);

  if (!share)
  {
    share= new EmbeddedInnoDBTableShare(table_name);

    if (share == NULL)
    {
      pthread_mutex_unlock(&embedded_innodb_mutex);
      *rc= HA_ERR_OUT_OF_MEM;
      return(NULL);
    }

    a_engine->addOpenTable(share->table_name, share);
    thr_lock_init(&share->lock);
  }
  share->use_count++;

  pthread_mutex_unlock(&embedded_innodb_mutex);

  return(share);
}

int EmbeddedInnoDBCursor::free_share()
{
  pthread_mutex_lock(&embedded_innodb_mutex);
  if (!--share->use_count)
  {
    EmbeddedInnoDBEngine *a_engine= static_cast<EmbeddedInnoDBEngine *>(engine);
    a_engine->deleteOpenTable(share->table_name);
    delete share;
  }
  pthread_mutex_unlock(&embedded_innodb_mutex);

  return 0;
}


THR_LOCK_DATA **EmbeddedInnoDBCursor::store_lock(Session *session,
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
    ib_trx_t *transaction= get_trx(session);
    *transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  }

  if (lock_type != TL_UNLOCK)
  {
    ib_savepoint_take(*get_trx(session), statement_savepoint_name.c_str(),
                      statement_savepoint_name.length());
  }

  /* the below is just copied from ha_archive.cc in some dim hope it's
     kinda right. */

  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK Table or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE)
        && !session_tablespace_op(session))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT)
      lock_type = TL_READ;

    lock.type=lock_type;
  }

  *to++= &lock;

  return to;
}

static void TableIdentifier_to_innodb_name(TableIdentifier &identifier, std::string *str)
{
  str->reserve(identifier.getSchemaName().length() + identifier.getTableName().length() + 1);
  str->append(identifier.getPath().c_str()+2);
/*
  str->assign(identifier.getSchemaName());
  str->append("/");
  str->append(identifier.getTableName());
*/
}

EmbeddedInnoDBCursor::EmbeddedInnoDBCursor(drizzled::plugin::StorageEngine &engine_arg,
                           TableShare &table_arg)
  :Cursor(engine_arg, table_arg)
{ }

int EmbeddedInnoDBCursor::open(const char *name, int, uint32_t)
{
  ib_err_t err= ib_cursor_open_table(name+2, NULL, &cursor);
  assert (err == DB_SUCCESS);

  int rc;
  share= get_share(name, &rc);
  thr_lock_data_init(&share->lock, &lock, NULL);

  return(0);
}

int EmbeddedInnoDBCursor::close(void)
{
  ib_err_t err= ib_cursor_close(cursor);
  if (err != DB_SUCCESS)
    return -1; // FIXME

  free_share();

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

  err= ib_cursor_open_table(INNODB_TABLE_DEFINITIONS_TABLE, transaction, &cursor);
  if (err != DB_SUCCESS)
    return err;

  message_tuple= ib_clust_read_tuple_create(cursor);

  err= ib_col_set_value(message_tuple, 0, table_name, strlen(table_name));
  if (err != DB_SUCCESS)
    goto cleanup;

  table_message.SerializeToString(&serialized_message);

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


int EmbeddedInnoDBEngine::doCreateTable(Session &session,
                                        Table& table_obj,
                                        drizzled::TableIdentifier &identifier,
                                        drizzled::message::Table& table_message)
{
  ib_tbl_sch_t innodb_table_schema= NULL;
//  ib_idx_sch_t innodb_pkey= NULL;
  ib_trx_t innodb_schema_transaction;
  ib_id_t innodb_table_id;
  ib_err_t innodb_err= DB_SUCCESS;
  string innodb_table_name;

  (void)table_obj;

  TableIdentifier_to_innodb_name(identifier, &innodb_table_name);

  innodb_err= ib_table_schema_create(innodb_table_name.c_str(),
                                     &innodb_table_schema, IB_TBL_COMPACT, 0);

  if (innodb_err != DB_SUCCESS)
  {
    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot create table %s. InnoDB Error %d (%s)\n"),
                        innodb_table_name.c_str(), innodb_err, ib_strerror(innodb_err));
    return HA_ERR_GENERIC;
  }

  for (int colnr= 0; colnr < table_message.field_size() ; colnr++)
  {
    const message::Table::Field field = table_message.field(colnr);

    int field_err= create_table_add_field(innodb_table_schema, field,
                                          &innodb_err);

    if (innodb_err != DB_SUCCESS || field_err != 0)
      ib_table_schema_delete(innodb_table_schema); /* cleanup */

    if (innodb_err != DB_SUCCESS)
    {
      push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_CANT_CREATE_TABLE,
                          _("Cannot create field %s on table %s."
                            " InnoDB Error %d (%s)\n"),
                          field.name().c_str(), innodb_table_name.c_str(),
                          innodb_err, ib_strerror(innodb_err));
      return HA_ERR_GENERIC;
    }
    if (field_err != 0)
      return field_err;
  }

  for (int indexnr= 0; indexnr < table_message.indexes_size() ; indexnr++)
  {
    message::Table::Index *index = table_message.mutable_indexes(indexnr);

    ib_idx_sch_t innodb_index;

    innodb_err= ib_table_schema_add_index(innodb_table_schema, index->name().c_str(),
                                   &innodb_index);
    if (innodb_err != DB_SUCCESS)
      goto schema_error;

    if (index->is_primary())
    {
      innodb_err= ib_index_schema_set_clustered(innodb_index);
      if (innodb_err != DB_SUCCESS)
        goto schema_error;
    }

    if (index->is_unique())
    {
      innodb_err= ib_index_schema_set_unique(innodb_index);
      if (innodb_err != DB_SUCCESS)
        goto schema_error;
    }

    if (index->type() == message::Table::Index::UNKNOWN_INDEX)
      index->set_type(message::Table::Index::BTREE);

    for (int partnr= 0; partnr < index->index_part_size(); partnr++)
    {
      /* TODO: Index prefix lengths */
      const message::Table::Index::IndexPart part= index->index_part(partnr);
      innodb_err= ib_index_schema_add_col(innodb_index, table_message.field(part.fieldnr()).name().c_str(), 0);
      if (innodb_err != DB_SUCCESS)
        goto schema_error;
    }
  }

  innodb_schema_transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  innodb_err= ib_schema_lock_exclusive(innodb_schema_transaction);
  if (innodb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(innodb_schema_transaction);
    ib_table_schema_delete(innodb_table_schema);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot Lock Embedded InnoDB Data Dictionary. InnoDB Error %d (%s)\n"),
                        innodb_err, ib_strerror(innodb_err));

    assert (rollback_err == DB_SUCCESS);

    return HA_ERR_GENERIC;
  }

  innodb_err= ib_table_create(innodb_schema_transaction, innodb_table_schema,
                              &innodb_table_id);

  if (innodb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(innodb_schema_transaction);
    ib_table_schema_delete(innodb_table_schema);

    if (innodb_err == DB_TABLE_IS_BEING_USED)
      return EEXIST;

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot create table %s. InnoDB Error %d (%s)\n"),
                        innodb_table_name.c_str(),
                        innodb_err, ib_strerror(innodb_err));

    assert (rollback_err == DB_SUCCESS);
    return HA_ERR_GENERIC;
  }

  innodb_err= store_table_message(innodb_schema_transaction,
                                  innodb_table_name.c_str(),
                                  table_message);

  if (innodb_err == DB_SUCCESS)
    innodb_err= ib_trx_commit(innodb_schema_transaction);
  else
    innodb_err= ib_trx_rollback(innodb_schema_transaction);

schema_error:
  ib_table_schema_delete(innodb_table_schema);

  if (innodb_err != DB_SUCCESS)
  {
    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot create table %s. InnoDB Error %d (%s)\n"),
                        innodb_table_name.c_str(),
                        innodb_err, ib_strerror(innodb_err));
    return HA_ERR_GENERIC;
  }

  return 0;
}

static int delete_table_message_from_innodb(ib_trx_t transaction, const char* table_name)
{
  ib_crsr_t cursor;
  ib_tpl_t search_tuple;
  int res;
  ib_err_t err;

  err= ib_cursor_open_table(INNODB_TABLE_DEFINITIONS_TABLE, transaction, &cursor);
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

int EmbeddedInnoDBEngine::doDropTable(Session &session,
                                      TableIdentifier &identifier)
{
  ib_trx_t innodb_schema_transaction;
  ib_err_t innodb_err;
  string innodb_table_name;

  TableIdentifier_to_innodb_name(identifier, &innodb_table_name);

  innodb_schema_transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  innodb_err= ib_schema_lock_exclusive(innodb_schema_transaction);
  if (innodb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(innodb_schema_transaction);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot Lock Embedded InnoDB Data Dictionary. InnoDB Error %d (%s)\n"),
                        innodb_err, ib_strerror(innodb_err));

    assert (rollback_err == DB_SUCCESS);

    return HA_ERR_GENERIC;
  }

  if (delete_table_message_from_innodb(innodb_schema_transaction, innodb_table_name.c_str()) != DB_SUCCESS)
  {
    ib_schema_unlock(innodb_schema_transaction);
    ib_err_t rollback_err= ib_trx_rollback(innodb_schema_transaction);
    assert(rollback_err == DB_SUCCESS);
    return HA_ERR_GENERIC;
  }

  innodb_err= ib_table_drop(innodb_schema_transaction, innodb_table_name.c_str());

  if (innodb_err == DB_TABLE_NOT_FOUND)
  {
    innodb_err= ib_trx_rollback(innodb_schema_transaction);
    assert(innodb_err == DB_SUCCESS);
    return ENOENT;
  }
  else if (innodb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(innodb_schema_transaction);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot DROP table %s. InnoDB Error %d (%s)\n"),
                        innodb_table_name.c_str(),
                        innodb_err, ib_strerror(innodb_err));

    assert(rollback_err == DB_SUCCESS);

    return HA_ERR_GENERIC;
  }

  innodb_err= ib_trx_commit(innodb_schema_transaction);
  if (innodb_err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(innodb_schema_transaction);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot DROP table %s. InnoDB Error %d (%s)\n"),
                        innodb_table_name.c_str(),
                        innodb_err, ib_strerror(innodb_err));

    assert(rollback_err == DB_SUCCESS);
    return HA_ERR_GENERIC;
  }

  return 0;
}

void EmbeddedInnoDBEngine::doGetTableNames(drizzled::CachedDirectory &,
                                           drizzled::SchemaIdentifier &schema,
                                           drizzled::plugin::TableNameList &set_of_names)
{
  ib_trx_t   transaction;
  ib_crsr_t  cursor;
  string search_string(schema.getLower());

  search_string.append("/");

  transaction = ib_trx_begin(IB_TRX_REPEATABLE_READ);
  ib_err_t innodb_err= ib_schema_lock_exclusive(transaction);
  assert(innodb_err == DB_SUCCESS); /* FIXME: doGetTableNames needs to be able to return error */


  innodb_err= ib_cursor_open_table("SYS_TABLES", transaction, &cursor);
  assert(innodb_err == DB_SUCCESS); /* FIXME */

  ib_tpl_t read_tuple;
  ib_tpl_t search_tuple;

  read_tuple= ib_clust_read_tuple_create(cursor);
  search_tuple= ib_clust_search_tuple_create(cursor);

  innodb_err= ib_col_set_value(search_tuple, 0, search_string.c_str(),
                               search_string.length());
  assert (innodb_err == DB_SUCCESS); // FIXME

  int res;
  innodb_err = ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
  // fixme: check error above

  while (innodb_err == DB_SUCCESS)
  {
    innodb_err= ib_cursor_read_row(cursor, read_tuple);

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
      set_of_names.insert(just_table_name);
    }


    innodb_err= ib_cursor_next(cursor);
    read_tuple= ib_tuple_clear(read_tuple);
  }

  ib_tuple_delete(read_tuple);
  ib_tuple_delete(search_tuple);

  innodb_err= ib_cursor_close(cursor);
  assert(innodb_err == DB_SUCCESS); // FIXME

  innodb_err= ib_trx_commit(transaction);
  assert(innodb_err == DB_SUCCESS); // FIXME
}

static int read_table_message_from_innodb(const char* table_name, drizzled::message::Table *table_message)
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

  err= ib_cursor_open_table(INNODB_TABLE_DEFINITIONS_TABLE, transaction, &cursor);
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

  if (strcmp(table_name, INNODB_TABLE_DEFINITIONS_TABLE) == 0)
  {
    message::Table::StorageEngine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("innodb_table_definitions");
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

int EmbeddedInnoDBEngine::doRenameTable(drizzled::Session&, drizzled::TableIdentifier&, drizzled::TableIdentifier&)
{
  return ENOENT;
}

int EmbeddedInnoDBEngine::doGetTableDefinition(Session&,
                                               TableIdentifier &identifier,
                                               drizzled::message::Table &table)
{
  ib_crsr_t innodb_cursor= NULL;
  string innodb_table_name;

  TableIdentifier_to_innodb_name(identifier, &innodb_table_name);

  if (ib_cursor_open_table(innodb_table_name.c_str(), NULL, &innodb_cursor) != DB_SUCCESS)
    return ENOENT;

  ib_err_t err= ib_cursor_close(innodb_cursor);

  assert (err == DB_SUCCESS);

  read_table_message_from_innodb(innodb_table_name.c_str(), &table);

  return EEXIST;
}

bool EmbeddedInnoDBEngine::doDoesTableExist(Session &,
                                            TableIdentifier& identifier)
{
  ib_crsr_t innodb_cursor;
  string innodb_table_name;

  TableIdentifier_to_innodb_name(identifier, &innodb_table_name);

  if (ib_cursor_open_table(innodb_table_name.c_str(), NULL, &innodb_cursor) != DB_SUCCESS)
    return false;

  ib_err_t err= ib_cursor_close(innodb_cursor);
  assert(err == DB_SUCCESS);

  return true;
}

const char *EmbeddedInnoDBCursor::index_type(uint32_t)
{
  return("BTREE");
}


static ib_err_t write_row_to_innodb_tuple(Field **fields, ib_tpl_t tuple)
{
  int colnr= 0;
  ib_err_t err;

  for (Field **field= fields; *field; field++, colnr++)
  {
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
    else
    {
      err= ib_col_set_value(tuple, colnr, (*field)->ptr, (*field)->data_length());
    }

    assert (err == DB_SUCCESS);
  }

  return err;
}

int EmbeddedInnoDBCursor::write_row(unsigned char *)
{
  if (table->next_number_field)
    update_auto_increment();

  ib_err_t err;
  int ret= 0;

  ib_trx_t transaction= *get_trx(ha_session());

  tuple= ib_clust_read_tuple_create(cursor);

  ib_cursor_attach_trx(cursor, transaction);

  err= ib_cursor_first(cursor);
  assert(err == DB_SUCCESS || err == DB_END_OF_INDEX);

  write_row_to_innodb_tuple(table->field, tuple);

  err= ib_cursor_insert_row(cursor, tuple);

  if (err == DB_DUPLICATE_KEY)
    ret= HA_ERR_FOUND_DUPP_KEY;

  tuple= ib_tuple_clear(tuple);
  ib_tuple_delete(tuple);
  err= ib_cursor_reset(cursor);

  return ret;
}

int EmbeddedInnoDBCursor::update_row(const unsigned char * ,
                                     unsigned char * )
{
  ib_tpl_t update_tuple;
  ib_err_t err;

  err= ib_cursor_prev(cursor);
  assert (err == DB_SUCCESS);

  update_tuple= ib_clust_read_tuple_create(cursor);

  err= ib_tuple_copy(update_tuple, tuple);
  assert(err == DB_SUCCESS);

  write_row_to_innodb_tuple(table->field, update_tuple);

  err= ib_cursor_update_row(cursor, tuple, update_tuple);

  ib_tuple_delete(update_tuple);

  if (err == DB_SUCCESS)
    return 0;
  else
    return -1;
}

int EmbeddedInnoDBCursor::delete_row(const unsigned char *)
{
  ib_err_t err;

  err= ib_cursor_prev(cursor);
  assert (err == DB_SUCCESS);

  err= ib_cursor_delete_row(cursor);
  if (err != DB_SUCCESS)
    return -1; // FIXME

  err= ib_cursor_next(cursor);
  if (err != DB_SUCCESS && err != DB_END_OF_INDEX)
    return -1; // FIXME
  return 0;
}

int EmbeddedInnoDBCursor::delete_all_rows(void)
{
  /* I *think* ib_truncate is non-transactional....
     so only support TRUNCATE and not DELETE FROM t;
     (this is what ha_innodb does)
  */
  if (session_sql_command(ha_session()) != SQLCOM_TRUNCATE)
    return HA_ERR_WRONG_COMMAND;

  ib_id_t id;
  ib_err_t err;

  ib_trx_t transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);

  ib_cursor_attach_trx(cursor, transaction);

  err= ib_schema_lock_exclusive(transaction);
  if (err != DB_SUCCESS)
  {
    ib_err_t rollback_err= ib_trx_rollback(transaction);

    push_warning_printf(ha_session(), DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot Lock Embedded InnoDB Data Dictionary. InnoDB Error %d (%s)\n"),
                        err, ib_strerror(err));

    assert (rollback_err == DB_SUCCESS);

    return HA_ERR_GENERIC;
  }

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

int EmbeddedInnoDBCursor::rnd_init(bool)
{
  ib_trx_t transaction= *get_trx(ha_session());

  ib_cursor_attach_trx(cursor, transaction);

  tuple= ib_clust_read_tuple_create(cursor);

  ib_err_t err= ib_cursor_first(cursor);
  if (err != DB_SUCCESS && err != DB_END_OF_INDEX)
    return -1; // FIXME

  return(0);
}

int read_row_from_innodb(ib_crsr_t cursor, ib_tpl_t tuple, Table* table)
{
  ib_err_t err;

  err= ib_cursor_read_row(cursor, tuple);

  if (err != DB_SUCCESS) // FIXME
    return HA_ERR_END_OF_FILE;

  int colnr= 0;

  for (Field **field=table->field ; *field ; field++, colnr++)
  {
    if (! (**field).isReadSet())
      continue;

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
    else
    {
      ib_col_copy_value(tuple, colnr, (*field)->ptr, (*field)->data_length());
    }
  }

  return 0;
}

int EmbeddedInnoDBCursor::rnd_next(unsigned char *)
{
  ib_err_t err;
  int ret;

  tuple= ib_tuple_clear(tuple);
  ret= read_row_from_innodb(cursor, tuple, table);

  err= ib_cursor_next(cursor);

  return ret;
}

int EmbeddedInnoDBCursor::rnd_end()
{
  ib_err_t err;

  ib_tuple_delete(tuple);
  err= ib_cursor_reset(cursor);
  assert(err == DB_SUCCESS);

  return 0;
}

int EmbeddedInnoDBCursor::rnd_pos(unsigned char *, unsigned char *)
{
  assert(0);
  return(0);
}


void EmbeddedInnoDBCursor::position(const unsigned char *)
{
  assert(0);
  return;
}

double EmbeddedInnoDBCursor::scan_time()
{
  return 0.1;
}

int EmbeddedInnoDBCursor::info(uint32_t flag)
{
  stats.records= 2;

  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  return(0);
}

int EmbeddedInnoDBCursor::index_init(uint32_t keynr, bool)
{
  ib_trx_t transaction= *get_trx(ha_session());

  active_index= keynr;

  ib_cursor_attach_trx(cursor, transaction);

  if (active_index == 0)
  {
    tuple= ib_clust_read_tuple_create(cursor);
  }
  else
  {
    /* Open 2ndary index */
  }

  return 0;
}

int EmbeddedInnoDBCursor::index_read(unsigned char *buf,
                                     const unsigned char *key_ptr,
                                     uint32_t key_len,
                                     drizzled::ha_rkey_function find_flag)
{
  ib_tpl_t search_tuple;
  int res;
  ib_err_t err;
  int ret;
  (void)buf;
  (void)find_flag;

  search_tuple= ib_clust_search_tuple_create(cursor);

  err= ib_col_set_value(search_tuple, 0, key_ptr, key_len);
  assert(err == DB_SUCCESS);

  err= ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
  if (err == DB_RECORD_NOT_FOUND || res != 0)
    return HA_ERR_END_OF_FILE;

  tuple= ib_tuple_clear(tuple);
  ret= read_row_from_innodb(cursor, tuple, table);
  err= ib_cursor_next(cursor);

  return 0;
}

int EmbeddedInnoDBCursor::index_next(unsigned char *)
{
  int ret= HA_ERR_END_OF_FILE;
  ib_err_t err;

  if (active_index == 0)
  {
    tuple= ib_tuple_clear(tuple);
    ret= read_row_from_innodb(cursor, tuple, table);
    err= ib_cursor_next(cursor);
  }

  return ret;
}

int EmbeddedInnoDBCursor::index_end()
{
  active_index= MAX_KEY;

  return rnd_end();
}

int EmbeddedInnoDBCursor::index_prev(unsigned char *)
{
  int ret= HA_ERR_END_OF_FILE;
  ib_err_t err;

  if (active_index == 0)
  {
    tuple= ib_tuple_clear(tuple);
    ret= read_row_from_innodb(cursor, tuple, table);
    err= ib_cursor_prev(cursor);
  }

  return ret;
}


int EmbeddedInnoDBCursor::index_first(unsigned char *)
{
  int ret= HA_ERR_END_OF_FILE;
  ib_err_t err;

  err= ib_cursor_first(cursor);
  if (err != DB_SUCCESS)
  {
    if (err == DB_END_OF_INDEX)
      return HA_ERR_END_OF_FILE;
    else
      return -1; // FIXME
  }

  if (active_index == 0)
  {
    tuple= ib_tuple_clear(tuple);
    ret= read_row_from_innodb(cursor, tuple, table);
    err= ib_cursor_next(cursor);
  }

  return ret;
}


int EmbeddedInnoDBCursor::index_last(unsigned char *)
{
  int ret= HA_ERR_END_OF_FILE;
  ib_err_t err;

  err= ib_cursor_last(cursor);
  if (err != DB_SUCCESS)
  {
    if (err == DB_END_OF_INDEX)
      return HA_ERR_END_OF_FILE;
    else
      return -1; // FIXME
  }

  if (active_index == 0)
  {
    tuple= ib_tuple_clear(tuple);
    ret= read_row_from_innodb(cursor, tuple, table);
    err= ib_cursor_prev(cursor);
  }

  return ret;
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

  err= ib_table_schema_create(INNODB_TABLE_DEFINITIONS_TABLE, &schema,
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

static char  default_innodb_data_file_path[]= "ibdata1:10M:autoextend";
static char* innodb_data_file_path= NULL;

static int64_t innodb_log_file_size;
static int64_t innodb_log_files_in_group;

static int embedded_innodb_init(drizzled::plugin::Context &context)
{
  ib_err_t err;

  err= ib_init();
  if (err != DB_SUCCESS)
    goto innodb_error;

  if (innodb_data_file_path == NULL)
    innodb_data_file_path= default_innodb_data_file_path;

  err= ib_cfg_set_text("data_file_path", innodb_data_file_path);
  if (err != DB_SUCCESS)
    goto innodb_error;

  err= ib_cfg_set_int("log_file_size", innodb_log_file_size);
  if (err != DB_SUCCESS)
    goto innodb_error;

  err= ib_cfg_set_int("log_files_in_group", innodb_log_files_in_group);
  if (err != DB_SUCCESS)
    goto innodb_error;

  err= ib_startup("barracuda");
  if (err != DB_SUCCESS)
    goto innodb_error;

  create_table_message_table();

  embedded_innodb_engine= new EmbeddedInnoDBEngine("InnoDB");
  context.add(embedded_innodb_engine);

  libinnodb_version_func_initialize(context);
  libinnodb_datadict_dump_func_initialize(context);

  return 0;
innodb_error:
  fprintf(stderr, _("Error starting Embedded InnoDB %d (%s)\n"),
          err, ib_strerror(err));
  return -1;
}


EmbeddedInnoDBEngine::~EmbeddedInnoDBEngine()
{
  ib_err_t err;

  err= ib_shutdown(IB_SHUTDOWN_NORMAL);

  if (err != DB_SUCCESS)
  {
    fprintf(stderr,"Error %d shutting down Embedded InnoDB!\n", err);
  }
}

static DRIZZLE_SYSVAR_STR(data_file_path, innodb_data_file_path,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Path to individual files and their sizes.",
  NULL, NULL, NULL);

static DRIZZLE_SYSVAR_LONGLONG(log_file_size, innodb_log_file_size,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Size of each log file in a log group.",
  NULL, NULL, 5*1024*1024L, 1*1024*1024L, INT64_MAX, 1024*1024L);

static DRIZZLE_SYSVAR_LONGLONG(log_files_in_group, innodb_log_files_in_group,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Number of log files in the log group. InnoDB writes to the files in a circular fashion. Value 3 is recommended here.",
  NULL, NULL, 2, 2, 100, 0);


static DRIZZLE_SessionVAR_ULONG(lock_wait_timeout, PLUGIN_VAR_RQCMDARG,
  "Placeholder: to be compatible with InnoDB plugin.",
  NULL, NULL, 50, 1, 1024 * 1024 * 1024, 0);

static drizzle_sys_var* innobase_system_variables[]= {
  DRIZZLE_SYSVAR(data_file_path),
  DRIZZLE_SYSVAR(lock_wait_timeout),
  DRIZZLE_SYSVAR(log_file_size),
  DRIZZLE_SYSVAR(log_files_in_group),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "INNODB",
  "1.0",
  "Stewart Smith",
  "Transactional Storage Engine using the Embedded InnoDB Library",
  PLUGIN_LICENSE_GPL,
  embedded_innodb_init,     /* Plugin Init */
  innobase_system_variables, /* system variables */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
