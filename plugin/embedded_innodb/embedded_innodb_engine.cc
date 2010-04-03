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

#include "embedded_innodb_engine.h"

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
                                     HTON_AUTO_PART_KEY)
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
                       drizzled::SchemaIdentifier &,
                       drizzled::plugin::TableNameList &)
  {
  }

  void doGetTableIdentifiers(drizzled::CachedDirectory &,
                             drizzled::SchemaIdentifier &,
                             drizzled::TableIdentifiers &)
  {
  }

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

int EmbeddedInnoDBCursor::open(const char *, int, uint32_t)
{
  return(0);
}

int EmbeddedInnoDBCursor::close(void)
{
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
  (void)table_message;

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
/*
  ib_table_schema_add_col(innodb_table_schema, "c1", IB_VARCHAR, IB_COL_NONE, 0, 32);

  ib_table_schema_add_index(innodb_table_schema, "PRIMARY_KEY", &innodb_pkey);
  ib_index_schema_add_col(innodb_pkey, "c1", 0);

  ib_index_schema_set_clustered(innodb_pkey);
*/
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

  innodb_err= ib_trx_commit(innodb_schema_transaction);

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

  message::Table::StorageEngine *engine= table.mutable_engine();
  engine->set_name("innodb");

  ib_err_t err= ib_cursor_close(innodb_cursor);

  assert (err == DB_SUCCESS);

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

int EmbeddedInnoDBCursor::write_row(unsigned char *)
{
  return(table->next_number_field ? update_auto_increment() : 0);
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

static drizzled::plugin::StorageEngine *embedded_innodb_engine= NULL;

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
