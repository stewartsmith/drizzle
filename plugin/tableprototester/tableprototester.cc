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

#include "tableprototester.h"

#include <fcntl.h>

#include <string>
#include <map>
#include <fstream>
#include <drizzled/message/table.pb.h>
#include "drizzled/internal/m_string.h"

#include "drizzled/global_charset_info.h"


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
                                     HTON_AUTO_PART_KEY |
                                     HTON_HAS_DATA_DICTIONARY)
  {
    table_definition_ext= TABLEPROTOTESTER_EXT;
  }

  virtual Cursor *create(TableShare &table,
                         drizzled::memory::Root *mem_root)
  {
    return new (mem_root) TableProtoTesterCursor(*this, table);
  }

  const char **bas_ext() const {
    return TableProtoTesterCursor_exts;
  }

  int doCreateTable(Session*,
                    const char *,
                    Table&,
                    drizzled::TableIdentifier &identifier,
                    drizzled::message::Table&);

  int doDropTable(Session&, drizzled::TableIdentifier &identifier, const string &table_name);

  int doGetTableDefinition(Session& session,
                           const char* path,
                           const char *db,
                           const char *table_name,
                           const bool is_tmp,
                           drizzled::TableIdentifier &identifier,
                           drizzled::message::Table &table_proto);

  void doGetTableNames(drizzled::CachedDirectory &directory,
                       string&, set<string>& set_of_names)
  {
    (void)directory;
    set_of_names.insert("t1");

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

  bool doDoesTableExist(Session& session, TableIdentifier &identifier);
};


bool TableProtoTesterEngine::doDoesTableExist(Session&, TableIdentifier &identifier)
{
  if (strcmp(identifier.getPath(), "./test/t1") == 0)
    return true;

  return false;
}

TableProtoTesterCursor::TableProtoTesterCursor(drizzled::plugin::StorageEngine &engine_arg,
                           TableShare &table_arg) :
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

int TableProtoTesterEngine::doCreateTable(Session*, const char *,
                                          Table&,
                                          drizzled::TableIdentifier &,
                                          drizzled::message::Table&)
{
  return EEXIST;
}


int TableProtoTesterEngine::doDropTable(Session&, drizzled::TableIdentifier&, const string&)
{
  return EPERM;
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
int TableProtoTesterEngine::doGetTableDefinition(Session&,
                                          const char* path,
                                          const char *,
                                          const char *,
                                          const bool,
                                          drizzled::TableIdentifier &,
                                          drizzled::message::Table &table_proto)
{
  if (strcmp(path, "./test/t1") == 0)
  {
    fill_table1(table_proto);
    return EEXIST;
  }
  return ENOENT;
}

const char *TableProtoTesterCursor::index_type(uint32_t)
{
  return("BTREE");
}

int TableProtoTesterCursor::write_row(unsigned char *)
{
  return(table->next_number_field ? update_auto_increment() : 0);
}

int TableProtoTesterCursor::rnd_init(bool)
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

static int tableprototester_init(drizzled::plugin::Registry &registry)
{

  tableprototester_engine= new TableProtoTesterEngine("TABLEPROTOTESTER");
  registry.add(tableprototester_engine);

  return 0;
}

static int tableprototester_fini(drizzled::plugin::Registry &registry)
{
  registry.remove(tableprototester_engine);
  delete tableprototester_engine;

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
  tableprototester_fini,     /* Plugin Deinit */
  NULL,               /* system variables */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
