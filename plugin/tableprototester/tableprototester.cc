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

#include <config.h>

#include "tableprototester.h"

#include <fcntl.h>

#include <string>
#include <map>
#include <fstream>

#include <drizzled/error.h>
#include <drizzled/charset.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/internal/my_pthread.h>
#include <drizzled/message/table.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/table.h>


using namespace std;
using namespace google;
using namespace drizzled;

#define TABLEPROTOTESTER_EXT ".TBT"

static const char *TableProtoTesterCursor_exts[] = {
  NULL
};

class TableProtoTesterEngine : public drizzled::plugin::StorageEngine
{
public:
  TableProtoTesterEngine(const string &name_arg)
   : drizzled::plugin::StorageEngine(name_arg,
                                     HTON_NULL_IN_KEY |
                                     HTON_CAN_INDEX_BLOBS |
                                     HTON_SKIP_STORE_LOCK |
                                     HTON_AUTO_PART_KEY)
  {
    table_definition_ext= TABLEPROTOTESTER_EXT;
  }

  virtual Cursor *create(Table &table)
  {
    return new TableProtoTesterCursor(*this, table);
  }

  const char **bas_ext() const {
    return TableProtoTesterCursor_exts;
  }

  int doCreateTable(Session&,
                    Table&,
                    const drizzled::identifier::Table &identifier,
                    const drizzled::message::Table&);

  int doDropTable(Session&, const drizzled::identifier::Table &identifier);

  int doGetTableDefinition(Session &session,
                           const drizzled::identifier::Table &identifier,
                           drizzled::message::Table &table_proto);

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

  bool doDoesTableExist(Session &session, const drizzled::identifier::Table &identifier);

  int doRenameTable(Session&, const drizzled::identifier::Table&, const drizzled::identifier::Table&)
  {
    return HA_ERR_NO_SUCH_TABLE;
  }

  void doGetTableIdentifiers(drizzled::CachedDirectory &directory,
                             const drizzled::identifier::Schema &schema_identifier,
                             drizzled::identifier::table::vector &set_of_identifiers);
};

void TableProtoTesterEngine::doGetTableIdentifiers(drizzled::CachedDirectory&,
                                                   const drizzled::identifier::Schema &schema_identifier,
                                                   drizzled::identifier::table::vector &set_of_identifiers)
{
  if (schema_identifier.compare("test"))
  {
    set_of_identifiers.push_back(identifier::Table(schema_identifier, "t1"));
    set_of_identifiers.push_back(identifier::Table(schema_identifier, "too_many_enum_values"));
    set_of_identifiers.push_back(identifier::Table(schema_identifier, "invalid_table_collation"));
  }
}

bool TableProtoTesterEngine::doDoesTableExist(Session&, const drizzled::identifier::Table &identifier)
{
  if (not identifier.getPath().compare("test/t1"))
    return true;
  if (not identifier.getPath().compare("test/too_many_enum_values"))
    return true;
  if (not identifier.getPath().compare("test/invalid_table_collation"))
    return true;

  return false;
}

TableProtoTesterCursor::TableProtoTesterCursor(drizzled::plugin::StorageEngine &engine_arg,
                                               Table &table_arg) :
  Cursor(engine_arg, table_arg)
{ }

int TableProtoTesterCursor::open(const char *, int, uint32_t)
{
  return 0;
}

int TableProtoTesterCursor::close(void)
{
  return 0;
}

int TableProtoTesterEngine::doCreateTable(Session&,
                                          Table&,
                                          const drizzled::identifier::Table&,
                                          const drizzled::message::Table&)
{
  return EEXIST;
}


int TableProtoTesterEngine::doDropTable(Session&, const drizzled::identifier::Table&)
{
  return HA_ERR_NO_SUCH_TABLE;
}

static void fill_table1(message::Table &table)
{
  message::Table::Field *field;
  message::Table::TableOptions *tableopts;

  table.set_name("t1");
  table.set_type(message::Table::INTERNAL);

  tableopts= table.mutable_options();
  tableopts->set_comment("Table without a StorageEngine message");

  {
    field= table.add_field();
    field->set_name("number");
    field->set_type(message::Table::Field::INTEGER);
  }

}

static void fill_table_too_many_enum_values(message::Table &table)
{
  message::Table::Field *field;
  message::Table::TableOptions *tableopts;

  table.set_schema("test");
  table.set_name("too_many_enum_values");
  table.set_type(message::Table::STANDARD);
  table.mutable_engine()->set_name("tableprototester");
  table.set_creation_timestamp(0);
  table.set_update_timestamp(0);

  tableopts= table.mutable_options();
  tableopts->set_comment("Table with too many enum options");
  tableopts->set_collation("utf8_general_ci");
  tableopts->set_collation_id(45);

  {
    field= table.add_field();
    field->set_name("many_values");
    field->set_type(message::Table::Field::ENUM);

    message::Table::Field::EnumerationValues *field_options= field->mutable_enumeration_values();
    for(int i=0; i<70000; i++)
    {
      char enum_value[100];
      snprintf(enum_value, sizeof(enum_value), "a%d", i);
      field_options->add_field_value(enum_value);
    }
  }

}

static void fill_table_invalid_table_collation(message::Table &table)
{
  message::Table::Field *field;
  message::Table::TableOptions *tableopts;

  table.set_name("invalid_table_collation");
  table.set_type(message::Table::STANDARD);
  table.set_schema("test");
  table.set_creation_timestamp(0);
  table.set_update_timestamp(0);
  table.mutable_engine()->set_name("tableprototester");

  tableopts= table.mutable_options();
  tableopts->set_comment("Invalid table collation ");

  {
    field= table.add_field();
    field->set_name("number");
    field->set_type(message::Table::Field::INTEGER);
  }

  tableopts->set_collation("pi_pi_pi");
  tableopts->set_collation_id(123456);

}

int TableProtoTesterEngine::doGetTableDefinition(Session&,
                                                 const drizzled::identifier::Table &identifier,
                                                 drizzled::message::Table &table_proto)
{
  if (not identifier.getPath().compare("test/t1"))
  {
    fill_table1(table_proto);
    return EEXIST;
  }
  else if (not identifier.getPath().compare("test/too_many_enum_values"))
  {
    fill_table_too_many_enum_values(table_proto);
    return EEXIST;
  }
  else if (not identifier.getPath().compare("test/invalid_table_collation"))
  {
    fill_table_invalid_table_collation(table_proto);
    return EEXIST;
  }
  return ENOENT;
}

const char *TableProtoTesterCursor::index_type(uint32_t)
{
  return("BTREE");
}

int TableProtoTesterCursor::doInsertRecord(unsigned char *)
{
  return(getTable()->next_number_field ? update_auto_increment() : 0);
}

int TableProtoTesterCursor::doStartTableScan(bool)
{
  return(0);
}


int TableProtoTesterCursor::rnd_next(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int TableProtoTesterCursor::rnd_pos(unsigned char *, unsigned char *)
{
  assert(0);
  return(0);
}


void TableProtoTesterCursor::position(const unsigned char *)
{
  assert(0);
  return;
}


int TableProtoTesterCursor::info(uint32_t flag)
{
  memset(&stats, 0, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  return(0);
}


int TableProtoTesterCursor::index_read_map(unsigned char *, const unsigned char *,
                                 key_part_map, enum ha_rkey_function)
{
  return(HA_ERR_END_OF_FILE);
}


int TableProtoTesterCursor::index_read_idx_map(unsigned char *, uint32_t, const unsigned char *,
                                     key_part_map, enum ha_rkey_function)
{
  return(HA_ERR_END_OF_FILE);
}


int TableProtoTesterCursor::index_read_last_map(unsigned char *, const unsigned char *, key_part_map)
{
  return(HA_ERR_END_OF_FILE);
}


int TableProtoTesterCursor::index_next(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int TableProtoTesterCursor::index_prev(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int TableProtoTesterCursor::index_first(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}


int TableProtoTesterCursor::index_last(unsigned char *)
{
  return(HA_ERR_END_OF_FILE);
}

static drizzled::plugin::StorageEngine *tableprototester_engine= NULL;

static int tableprototester_init(drizzled::module::Context &context)
{

  tableprototester_engine= new TableProtoTesterEngine("TABLEPROTOTESTER");
  context.add(tableprototester_engine);

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "TABLEPROTOTESTER",
  "1.0",
  "Stewart Smith",
  "Used to test rest of server with various table proto messages",
  PLUGIN_LICENSE_GPL,
  tableprototester_init,     /* Plugin Init */
  NULL,               /* depends */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
