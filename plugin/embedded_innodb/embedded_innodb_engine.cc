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

const char INNODB_TABLE_DEFINITIONS_TABLE[]= "data_dictionary/innodb_table_definitions";

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
  transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);

  ib_err_t err= ib_cursor_open_table(name+2, transaction, &cursor);
  assert (err == DB_SUCCESS);

  tuple= ib_clust_read_tuple_create(cursor);

  return(0);
}

int EmbeddedInnoDBCursor::close(void)
{
  ib_tuple_delete(tuple);
  ib_cursor_close(cursor);
  ib_trx_commit(transaction);

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

  ib_cursor_close(cursor);

  return err;
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
  (void)table_message;

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
/*
  ib_table_schema_add_col(innodb_table_schema, "c1", IB_VARCHAR, IB_COL_NONE, 0, 32);

  ib_table_schema_add_index(innodb_table_schema, "PRIMARY_KEY", &innodb_pkey);
  ib_index_schema_add_col(innodb_pkey, "c1", 0);

  ib_index_schema_set_clustered(innodb_pkey);
*/
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

  innodb_err= store_table_message(innodb_schema_transaction, path+2,
                                  table_message);

  if (innodb_err == DB_SUCCESS)
    innodb_err= ib_trx_commit(innodb_schema_transaction);
  else
    innodb_err= ib_trx_rollback(innodb_schema_transaction);

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

static int delete_table_message_from_innodb(ib_trx_t transaction, const char* table_name)
{
  ib_crsr_t cursor;
  ib_tpl_t search_tuple;
  int res;
  ib_err_t err;

  ib_cursor_open_table(INNODB_TABLE_DEFINITIONS_TABLE, transaction, &cursor);
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

  if (delete_table_message_from_innodb(innodb_schema_transaction, table_name.c_str()+2) != DB_SUCCESS)
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

  transaction= ib_trx_begin(IB_TRX_REPEATABLE_READ);
  ib_schema_lock_exclusive(transaction);

  ib_cursor_open_table(INNODB_TABLE_DEFINITIONS_TABLE, transaction, &cursor);
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

  message= (const char*)ib_col_get_value(read_tuple, 1);
  message_len= ib_col_get_meta(read_tuple, 1, &col_meta);

  if (table_message->ParseFromArray(message, message_len) == false)
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

  if (strcmp(table_name, INNODB_TABLE_DEFINITIONS_TABLE) == 0)
  {
    message::Table::StorageEngine *engine= table_message->mutable_engine();
    engine->set_name("InnoDB");
    table_message->set_name("innodb_table_message");
    table_message->set_type(message::Table::STANDARD);

    message::Table::Field *field= table_message->add_field();
    field->set_name("table_name");
    field->set_type(message::Table::Field::VARCHAR);
    message::Table::Field::StringFieldOptions *stropt= field->mutable_string_options();
    stropt->set_length(IB_MAX_TABLE_NAME_LEN);

    field= table_message->add_field();
    field->set_name("message");
    field->set_type(message::Table::Field::BLOB);

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
    read_table_message_from_innodb(path+2, table);
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

  for (Field **field=table->field ; *field ; field++, colnr++)
  {
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

  err= ib_cursor_insert_row(cursor, tuple);
  assert (err == DB_SUCCESS);

  ib_tuple_clear(tuple);

  return 0;
}

int EmbeddedInnoDBCursor::rnd_init(bool)
{
  return(0);
}


int EmbeddedInnoDBCursor::rnd_next(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
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
  memset(&stats, 0, sizeof(stats));
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

static int create_table_message_table()
{
  ib_tbl_sch_t schema;
  ib_idx_sch_t index_schema;
  ib_trx_t transaction;
  ib_id_t table_id;

  ib_database_create("data_dictionary");

  ib_table_schema_create(INNODB_TABLE_DEFINITIONS_TABLE, &schema,
                         IB_TBL_COMPACT, 0);
  ib_table_schema_add_col(schema, "table_name", IB_VARCHAR, IB_COL_NONE, 0,
                          IB_MAX_TABLE_NAME_LEN);
  ib_table_schema_add_col(schema, "message", IB_BLOB, IB_COL_NONE, 0, 0);

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

static char  default_innodb_data_file_path[]= "ibdata1:10M:autoextend";
static char* innodb_data_file_path= NULL;

static int64_t innodb_log_file_size;
static int64_t innodb_log_files_in_group;

static int embedded_innodb_init(drizzled::plugin::Registry &registry)
{
  int err;

  ib_init();
  /* call ib_cfg_*() */

  if (innodb_data_file_path == NULL)
    innodb_data_file_path= default_innodb_data_file_path;

  ib_cfg_set_text("data_file_path", innodb_data_file_path);
  ib_cfg_set_int("log_file_size", innodb_log_file_size);
  ib_cfg_set_int("log_files_in_group", innodb_log_files_in_group);

  err= ib_startup("barracuda");

  if (err != DB_SUCCESS)
  {
    fprintf(stderr, "Error starting Embedded InnoDB %d\n", err);
    return -1;
  }

  create_table_message_table();

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
  "Used to test rest of server with various table proto messages",
  PLUGIN_LICENSE_GPL,
  embedded_innodb_init,     /* Plugin Init */
  embedded_innodb_fini,     /* Plugin Deinit */
  innobase_system_variables, /* system variables */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
