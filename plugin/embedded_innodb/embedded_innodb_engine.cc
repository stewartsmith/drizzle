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

using namespace std;
using namespace google;
using namespace drizzled;

#define EMBEDDED_INNODB_EXT ".EID"

static const char *EmbeddedInnoDBCursor_exts[] = {
  NULL
};

class EmbeddedInnoDBEngine : public drizzled::plugin::StorageEngine
{
public:
  EmbeddedInnoDBEngine(const string &name_arg)
   : drizzled::plugin::StorageEngine(name_arg,
                                     HTON_NULL_IN_KEY |
                                     HTON_CAN_INDEX_BLOBS |
                                     HTON_SKIP_STORE_LOCK |
                                     HTON_AUTO_PART_KEY |
                                     HTON_HAS_DATA_DICTIONARY)
  {
    table_definition_ext= EMBEDDED_INNODB_EXT;
  }

  virtual Cursor *create(TableShare &table,
                         drizzled::memory::Root *mem_root)
  {
    return new (mem_root) EmbeddedInnoDBCursor(*this, table);
  }

  const char **bas_ext() const {
    return EmbeddedInnoDBCursor_exts;
  }

  int doCreateTable(Session*,
                    const char *,
                    Table&,
                    drizzled::message::Table&);

  int doDropTable(Session&, const string table_name);

  int doGetTableDefinition(Session& session,
                           const char* path,
                           const char *db,
                           const char *table_name,
                           const bool is_tmp,
                           drizzled::message::Table *table_proto);

  void doGetTableNames(drizzled::CachedDirectory &,
                       string& database_name, set<string>& set_of_names);

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

};

EmbeddedInnoDBCursor::EmbeddedInnoDBCursor(drizzled::plugin::StorageEngine &engine_arg,
                           TableShare &table_arg)
  :Cursor(engine_arg, table_arg)
{ }

int EmbeddedInnoDBCursor::open(const char *name, int, uint32_t)
{
  ib_err_t err= ib_cursor_open_table(name+2, NULL, &cursor);
  assert (err==DB_SUCCESS);

  return(0);
}

int EmbeddedInnoDBCursor::close(void)
{
  ib_cursor_close(cursor);
  return 0;
}

static int create_table_add_field(ib_tbl_sch_t schema,
                                  message::Table::Field field,
                                  ib_err_t *err)
{
  ib_col_attr_t column_attr= IB_COL_NONE;

  if (!( field.has_constraints()
         && field.constraints().is_nullable()))
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

static int store_table_proto(ib_trx_t transaction, const char* table_name, drizzled::message::Table& table_message)
{
  ib_crsr_t cursor;
  ib_tpl_t proto_tuple;
  ib_err_t err;

  err= ib_cursor_open_table("data_dictionary/innodb_table_proto", transaction, &cursor);
  assert (err==DB_SUCCESS);

  proto_tuple= ib_clust_read_tuple_create(cursor);

  err= ib_col_set_value(proto_tuple, 0, table_name, strlen(table_name));
  assert (err==DB_SUCCESS);

  string serialized_proto;
  table_message.SerializeToString(&serialized_proto);

  err= ib_col_set_value(proto_tuple, 1, serialized_proto.c_str(), serialized_proto.length());
  assert (err==DB_SUCCESS);

  err= ib_cursor_insert_row(cursor, proto_tuple);
  assert (err==DB_SUCCESS);

  ib_tuple_delete(proto_tuple);

  ib_cursor_close(cursor);

  return 0;
}

int EmbeddedInnoDBEngine::doCreateTable(Session* session, const char *path,
                                   Table& table_obj,
                                   drizzled::message::Table& table_message)
{
  ib_tbl_sch_t innodb_table_schema= NULL;
//  ib_idx_sch_t innodb_pkey= NULL;
  ib_trx_t innodb_schema_transaction;
  ib_id_t innodb_table_id;
  ib_err_t innodb_err= DB_SUCCESS;

  (void)session;
  (void)table_obj;

  innodb_err= ib_table_schema_create(path+2, &innodb_table_schema, IB_TBL_COMPACT, 0);

  if (innodb_err != DB_SUCCESS)
  {
    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot create table %s. InnoDB Error %d (%s)\n"),
                        path, innodb_err, ib_strerror(innodb_err));
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
      push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                          ER_CANT_CREATE_TABLE,
                          _("Cannot create field %s on table %s."
                            " InnoDB Error %d (%s)\n"),
                          field.name().c_str(), path,
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

    ib_table_schema_add_index(innodb_table_schema, index->name().c_str(),
                              &innodb_index);

    if (index->is_primary())
      ib_index_schema_set_clustered(innodb_index);

    if (index->is_unique())
      ib_index_schema_set_unique(innodb_index);

    if (index->type() == message::Table::Index::UNKNOWN_INDEX)
      index->set_type(message::Table::Index::BTREE);

    for (int partnr= 0; partnr < index->index_part_size(); partnr++)
    {
      /* TODO: Index prefix lengths */
      const message::Table::Index::IndexPart part= index->index_part(partnr);
      ib_index_schema_add_col(innodb_index, table_message.field(part.fieldnr()).name().c_str(), 0);
    }
  }

  innodb_schema_transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  ib_schema_lock_exclusive(innodb_schema_transaction);

  innodb_err= ib_table_create(innodb_schema_transaction, innodb_table_schema,
                              &innodb_table_id);

  if (innodb_err != DB_SUCCESS)
  {
    ib_trx_rollback(innodb_schema_transaction);
    ib_table_schema_delete(innodb_table_schema);

    if (innodb_err == DB_TABLE_IS_BEING_USED)
      return EEXIST;

    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot create table %s. InnoDB Error %d (%s)\n"),
                        path, innodb_err, ib_strerror(innodb_err));
    return HA_ERR_GENERIC;
  }

  store_table_proto(innodb_schema_transaction, path+2, table_message);

  innodb_err= ib_trx_commit(innodb_schema_transaction);

  ib_table_schema_delete(innodb_table_schema);

  if (innodb_err != DB_SUCCESS)
  {
    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_CREATE_TABLE,
                        _("Cannot create table %s. InnoDB Error %d (%s)\n"),
                        path, innodb_err, ib_strerror(innodb_err));
    return HA_ERR_GENERIC;
  }

  return 0;
}

static int delete_table_proto_from_innodb(ib_trx_t transaction, const char* table_name)
{
  ib_crsr_t cursor;
  ib_tpl_t search_tuple;
  int res;
  ib_err_t err;

  ib_cursor_open_table("data_dictionary/innodb_table_proto", transaction, &cursor);
  search_tuple= ib_clust_search_tuple_create(cursor);

  ib_col_set_value(search_tuple, 0, table_name, strlen(table_name));

//  ib_cursor_set_match_mode(cursor, IB_EXACT_MATCH);

  err= ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
  if (err == DB_RECORD_NOT_FOUND || res != 0)
    goto rollback;

  err= ib_cursor_delete_row(cursor);
  assert (err == DB_SUCCESS);

rollback:
  ib_cursor_close(cursor);
  ib_schema_unlock(transaction);
  ib_tuple_delete(search_tuple);

  return err;
}

int EmbeddedInnoDBEngine::doDropTable(Session& session, const string table_name)
{
  ib_trx_t innodb_schema_transaction;
  ib_err_t innodb_err;

  innodb_schema_transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);

  if(delete_table_proto_from_innodb(innodb_schema_transaction, table_name.c_str()+2) != DB_SUCCESS)
  {
    ib_trx_rollback(innodb_schema_transaction);
    return HA_ERR_GENERIC;
  }

  innodb_err= ib_table_drop(table_name.c_str()+2);

  if (innodb_err == DB_TABLE_NOT_FOUND)
  {
    ib_trx_rollback(innodb_schema_transaction);
    return ENOENT;
  }
  else if (innodb_err != DB_SUCCESS)
  {
    ib_trx_rollback(innodb_schema_transaction);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot DROP table %s. InnoDB Error %d (%s)\n"),
                        table_name.c_str(),
                        innodb_err, ib_strerror(innodb_err));
    return HA_ERR_GENERIC;
  }

  innodb_err= ib_trx_commit(innodb_schema_transaction);
  if (innodb_err != DB_SUCCESS)
  {
    ib_trx_rollback(innodb_schema_transaction);

    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_CANT_DELETE_FILE,
                        _("Cannot DROP table %s. InnoDB Error %d (%s)\n"),
                        table_name.c_str(),
                        innodb_err, ib_strerror(innodb_err));
    return HA_ERR_GENERIC;
  }

  return 0;
}

void EmbeddedInnoDBEngine::doGetTableNames(drizzled::CachedDirectory &,
                                           string& database_name,
                                           set<string>& set_of_names)
{
  ib_trx_t   transaction;
  ib_crsr_t  cursor;
  string search_string(database_name);

  search_string.append("/");

  transaction = ib_trx_begin(IB_TRX_REPEATABLE_READ);
  ib_schema_lock_exclusive(transaction);

  int err= ib_cursor_open_table("SYS_TABLES", transaction, &cursor);
  assert(err == DB_SUCCESS);

  ib_cursor_first(cursor);

  ib_tpl_t read_tuple;

  read_tuple= ib_clust_read_tuple_create(cursor);
  /*
    What we really want to do here is not the ib_cursor_first() as above
    but instead using a search tuple to find the first instance of
    "database_name/" in SYS_TABLES.NAME.

    There is currently a bug in libinnodb (1.0.3.5325) that means this ends in
    a failed assert() rather than speeedy bliss.

    It should be fixed in the next release.

    See: http://forums.innodb.com/read.php?8,1090,1094#msg-1094

    we want something like:

    ib_col_set_value(read_tuple, 0,
    search_string.c_str(), search_string.length());

    err = ib_cursor_moveto(cursor, read_tuple, IB_CUR_GE, &res);
  */

  while (err == DB_SUCCESS)
  {
    err= ib_cursor_read_row(cursor, read_tuple);

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


    err= ib_cursor_next(cursor);
    read_tuple= ib_tuple_clear(read_tuple);
  }

  ib_tuple_delete(read_tuple);

  ib_cursor_close(cursor);

  ib_trx_commit(transaction);
}

static int read_table_proto_from_innodb(const char* table_name, drizzled::message::Table *table_message)
{
  ib_trx_t transaction;
  ib_tpl_t search_tuple;
  ib_tpl_t read_tuple;
  ib_crsr_t cursor;
  const char *proto;
  ib_ulint_t proto_len;
  ib_col_meta_t col_meta;
  int res;
  ib_err_t err;

  transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  ib_schema_lock_exclusive(transaction);

  ib_cursor_open_table("data_dictionary/innodb_table_proto", transaction, &cursor);
  search_tuple= ib_clust_search_tuple_create(cursor);
  read_tuple= ib_clust_read_tuple_create(cursor);

  ib_col_set_value(search_tuple, 0, table_name, strlen(table_name));

//  ib_cursor_set_match_mode(cursor, IB_EXACT_MATCH);

  err= ib_cursor_moveto(cursor, search_tuple, IB_CUR_GE, &res);
  if (err == DB_RECORD_NOT_FOUND || res != 0)
    goto rollback;

  err= ib_cursor_read_row(cursor, read_tuple);
  if (err == DB_RECORD_NOT_FOUND || res != 0)
    goto rollback;

  proto= (const char*)ib_col_get_value(read_tuple, 1);
  proto_len= ib_col_get_meta(read_tuple, 1, &col_meta);

  if (table_message->ParseFromArray(proto, proto_len) == false)
    goto rollback;

  ib_tuple_delete(search_tuple);
  ib_tuple_delete(read_tuple);
  ib_cursor_close(cursor);
  ib_trx_commit(transaction);

  return 0;

rollback:
  ib_tuple_delete(search_tuple);
  ib_tuple_delete(read_tuple);
  ib_cursor_close(cursor);
  ib_schema_unlock(transaction);
  ib_trx_rollback(transaction);

  if (strcmp(table_name, "data_dictionary/innodb_table_proto") == 0)
  {
    message::Table::StorageEngine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("innodb_table_proto");
    table_message->set_type(message::Table::STANDARD);

    message::Table::Field *field= table_message->add_field();
    field->set_name("table_name");
    field->set_type(message::Table::Field::VARCHAR);
    message::Table::Field::StringFieldOptions *stropt= field->mutable_string_options();
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);
    stropt->set_collation(my_charset_bin.csname);
    stropt->set_collation_id(my_charset_bin.number);

    field= table_message->add_field();
    field->set_name("proto");
    field->set_type(message::Table::Field::BLOB);

/*
    message::Table::Index *index= table_message->add_indexes();
    index->set_name("PRIMARY");
    index->set_is_primary(true);
    index->set_is_unique(true);
    index->set_type(message::Table::Index::BTREE);
    index->set_key_length(IB_MAX_TABLE_NAME_LEN);
    message::Table::Index::IndexPart *part= index->add_index_part();
    part->set_fieldnr(0);
    part->set_compare_length(IB_MAX_TABLE_NAME_LEN);
*/
    return 0;
  }

  return -1;
}

int EmbeddedInnoDBEngine::doGetTableDefinition(Session&,
                                          const char* path,
                                          const char *,
                                          const char *,
                                          const bool,
                                          drizzled::message::Table *table)
{
  ib_crsr_t innodb_cursor= NULL;

  if (ib_cursor_open_table(path+2, NULL, &innodb_cursor) != DB_SUCCESS)
    return ENOENT;
  ib_cursor_close(innodb_cursor);

  if (table)
  {
    read_table_proto_from_innodb(path+2, table);
  }

  return EEXIST;
}

const char *EmbeddedInnoDBCursor::index_type(uint32_t)
{
  return("BTREE");
}

int EmbeddedInnoDBCursor::write_row(unsigned char *)
{
  if (table->next_number_field)
    update_auto_increment();

  ib_err_t err;
  int colnr= 0;

  transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);

  tuple= ib_clust_read_tuple_create(cursor);

  ib_cursor_attach_trx(cursor, transaction);

  ib_cursor_first(cursor);

  for (Field **field=table->field ; *field ; field++, colnr++)
  {
    err= ib_col_set_value(tuple, colnr, (*field)->ptr, (*field)->data_length());
    assert (err==DB_SUCCESS);
  }

  err= ib_cursor_insert_row(cursor, tuple);
  assert (err==DB_SUCCESS);

  ib_tuple_clear(tuple);
  ib_tuple_delete(tuple);
  ib_cursor_reset(cursor);
  ib_trx_commit(transaction);

  return 0;
}

int EmbeddedInnoDBCursor::rnd_init(bool)
{
  transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);

  ib_cursor_attach_trx(cursor, transaction);

  tuple= ib_clust_read_tuple_create(cursor);

  ib_cursor_first(cursor);

  return(0);
}


int EmbeddedInnoDBCursor::rnd_next(unsigned char *)
{
  ib_err_t err;

  err= ib_cursor_read_row(cursor, tuple);

  if (err != DB_SUCCESS) // FIXME
    return HA_ERR_END_OF_FILE;

  int colnr= 0;

  for (Field **field=table->field ; *field ; field++, colnr++)
  {
    ib_col_copy_value(tuple, colnr, (*field)->ptr, (*field)->data_length());
    (**field).set_notnull();
  }

  ib_tuple_clear(tuple);

  err= ib_cursor_next(cursor);

  return 0;
}

int EmbeddedInnoDBCursor::rnd_end()
{
  ib_tuple_delete(tuple);
  ib_cursor_reset(cursor);
  ib_trx_commit(transaction);
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


int EmbeddedInnoDBCursor::info(uint32_t flag)
{
  stats.records= 100;

  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  return(0);
}


int EmbeddedInnoDBCursor::index_read_map(unsigned char *, const unsigned char *,
                                 key_part_map, enum ha_rkey_function)
{
  return(HA_ERR_END_OF_FILE);
}


int EmbeddedInnoDBCursor::index_read_idx_map(unsigned char *, uint32_t, const unsigned char *,
                                     key_part_map, enum ha_rkey_function)
{
  return(HA_ERR_END_OF_FILE);
}


int EmbeddedInnoDBCursor::index_read_last_map(unsigned char *, const unsigned char *, key_part_map)
{
  return(HA_ERR_END_OF_FILE);
}


int EmbeddedInnoDBCursor::index_next(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int EmbeddedInnoDBCursor::index_prev(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int EmbeddedInnoDBCursor::index_first(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int EmbeddedInnoDBCursor::index_last(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}

static int create_table_proto_table()
{
  ib_tbl_sch_t schema;
  ib_idx_sch_t index_schema;
  ib_trx_t transaction;
  ib_id_t table_id;

  ib_database_create("data_dictionary");

  ib_table_schema_create("data_dictionary/innodb_table_proto", &schema,
                         IB_TBL_COMPACT, 0);
  ib_table_schema_add_col(schema, "table_name", IB_VARCHAR, IB_COL_NONE, 0,
                          IB_MAX_TABLE_NAME_LEN);
  ib_table_schema_add_col(schema, "proto", IB_BLOB, IB_COL_NONE, 0, 0);

  ib_table_schema_add_index(schema, "PRIMARY_KEY", &index_schema);
  ib_index_schema_add_col(index_schema, "table_name", 0);
  ib_index_schema_set_clustered(index_schema);

  transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  ib_schema_lock_exclusive(transaction);

  ib_table_create(transaction, schema, &table_id);

  ib_trx_commit(transaction);

  ib_table_schema_delete(schema);

  return 0;
}

static drizzled::plugin::StorageEngine *embedded_innodb_engine= NULL;

static int embedded_innodb_init(drizzled::plugin::Registry &registry)
{
  int err;

  ib_init();
  /* call ib_cfg_*() */

  err= ib_startup("barracuda");

  if (err != DB_SUCCESS)
  {
    fprintf(stderr, "Error starting Embedded InnoDB %d\n", err);
    return -1;
  }

  create_table_proto_table();

  embedded_innodb_engine= new EmbeddedInnoDBEngine("InnoDB");
  registry.add(embedded_innodb_engine);

  libinnodb_version_func_initialize(registry);
  libinnodb_datadict_dump_func_initialize(registry);

  return 0;
}

static int embedded_innodb_fini(drizzled::plugin::Registry &registry)
{
  int err;

  registry.remove(embedded_innodb_engine);
  delete embedded_innodb_engine;

  libinnodb_version_func_finalize(registry);
  libinnodb_datadict_dump_func_finalize(registry);

  err= ib_shutdown();

  if (err != DB_SUCCESS)
  {
    fprintf(stderr,"Error %d shutting down Embedded InnoDB!", err);
    return err;
  }

  return 0;
}

static char* innodb_data_file_path= NULL;

static DRIZZLE_SYSVAR_STR(data_file_path, innodb_data_file_path,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Placeholder to be compatible with InnoDB plugin.",
  NULL, NULL, NULL);
static DRIZZLE_SessionVAR_ULONG(lock_wait_timeout, PLUGIN_VAR_RQCMDARG,
  "Placeholder: to be compatible with InnoDB plugin.",
  NULL, NULL, 50, 1, 1024 * 1024 * 1024, 0);

static drizzle_sys_var* innobase_system_variables[]= {
  DRIZZLE_SYSVAR(data_file_path),
  DRIZZLE_SYSVAR(lock_wait_timeout),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "INNODB",
  "1.0",
  "Stewart Smith",
  "Used to test rest of server with various table proto messages",
  PLUGIN_LICENSE_GPL,
  embedded_innodb_init,     /* Plugin Init */
  embedded_innodb_fini,     /* Plugin Deinit */
  innobase_system_variables, /* system variables */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
