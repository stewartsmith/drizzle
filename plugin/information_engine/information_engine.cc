/* Copyright (C) 2005 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <plugin/information_engine/information_engine.h>
#include <drizzled/plugin/info_schema_table.h>
#include <drizzled/session.h>

#include <plugin/information_engine/information_engine.h>

#include <string>

using namespace std;
using namespace drizzled;

static const string engine_name("INFORMATION_ENGINE");

/*****************************************************************************
** INFORMATION_ENGINE tables
*****************************************************************************/

InformationCursor::InformationCursor(plugin::StorageEngine *engine_arg,
                                     TableShare *table_arg) :
  Cursor(engine_arg, table_arg)
{}

uint32_t InformationCursor::index_flags(uint32_t, uint32_t, bool) const
{
  return 0;
}

int InformationCursor::open(const char *name, int, uint32_t)
{
  InformationShare *shareable= InformationShare::get(name);

  if (! shareable)
  {
    return HA_ERR_OUT_OF_MEM;
  }

  thr_lock_data_init(&shareable->lock, &lock, NULL);

  return 0;
}

int InformationCursor::close(void)
{
  InformationShare::free(share);

  return 0;
}

int InformationEngine::doGetTableDefinition(Session &,
                                            const char *,
                                            const char *,
                                            const char *table_name,
                                            const bool,
                                            message::Table *table_proto)
{
  if (! table_proto)
  {
    return EEXIST;
  }

  plugin::InfoSchemaTable *schema_table= plugin::InfoSchemaTable::getTable(table_name);

  if (! schema_table)
  {
    return ENOENT;
  }

  table_proto->set_name(table_name);
  table_proto->set_type(message::Table::STANDARD);

  message::Table::StorageEngine *protoengine= table_proto->mutable_engine();
  protoengine->set_name(engine_name);

  message::Table::TableOptions *table_options= table_proto->mutable_options();
  table_options->set_collation_id(default_charset_info->number);
  table_options->set_collation(default_charset_info->name);

  const plugin::InfoSchemaTable::Columns &columns= schema_table->getColumns();
  plugin::InfoSchemaTable::Columns::const_iterator iter= columns.begin();

  while (iter != columns.end())
  {
    const plugin::ColumnInfo *column= *iter;
    /* get the various proto variables we need */
    message::Table::Field *proto_field= table_proto->add_field();
    message::Table::Field::FieldOptions *field_options=
      proto_field->mutable_options();
    message::Table::Field::FieldConstraints *field_constraints=
      proto_field->mutable_constraints();

    proto_field->set_name(column->getName());
    field_options->set_default_value("0");

    if (column->getFlags() & MY_I_S_MAYBE_NULL)
    {
      field_options->set_default_null(true);
      field_constraints->set_is_nullable(true);
    }

    if (column->getFlags() & MY_I_S_UNSIGNED)
    {
      field_constraints->set_is_unsigned(true);
    }

    switch(column->getType())
    {
    case DRIZZLE_TYPE_LONG:
      proto_field->set_type(message::Table::Field::INTEGER);
      field_options->set_length(MAX_INT_WIDTH);
      break;
    case DRIZZLE_TYPE_DOUBLE:
      proto_field->set_type(message::Table::Field::DOUBLE);
      break;
    case DRIZZLE_TYPE_NULL:
      assert(true);
      break;
    case DRIZZLE_TYPE_TIMESTAMP:
      proto_field->set_type(message::Table::Field::TIMESTAMP);
      field_options->set_default_value("NOW()");
      break;
    case DRIZZLE_TYPE_LONGLONG:
      proto_field->set_type(message::Table::Field::BIGINT);
      field_options->set_length(MAX_BIGINT_WIDTH);
      break;
    case DRIZZLE_TYPE_DATETIME:
      proto_field->set_type(message::Table::Field::DATETIME);
      field_options->set_default_value("NOW()");
      break;
    case DRIZZLE_TYPE_DATE:
      proto_field->set_type(message::Table::Field::DATE);
      field_options->set_default_value("NOW()");
      break;
    case DRIZZLE_TYPE_VARCHAR:
    {
      message::Table::Field::StringFieldOptions *str_field_options=
        proto_field->mutable_string_options();
      proto_field->set_type(message::Table::Field::VARCHAR);
      str_field_options->set_length(column->getLength());
      field_options->set_length(column->getLength() * 4);
      field_options->set_default_value("");
      str_field_options->set_collation_id(default_charset_info->number);
      str_field_options->set_collation(default_charset_info->name);
      break;
    }
    case DRIZZLE_TYPE_NEWDECIMAL:
    {
      message::Table::Field::NumericFieldOptions *num_field_options=
        proto_field->mutable_numeric_options();
      proto_field->set_type(message::Table::Field::DECIMAL);
      num_field_options->set_precision(column->getLength());
      num_field_options->set_scale(column->getLength() % 10);
      break;
    }
    default:
      assert(true);
      break;
    }

    ++iter;
  }

  return EEXIST;
}


void InformationEngine::doGetTableNames(CachedDirectory&, string& db, set<string>& set_of_names)
{
  if (db.compare("information_schema"))
    return;

  plugin::InfoSchemaTable::getTableNames(set_of_names);
}



int InformationCursor::rnd_init(bool)
{
  TableList *tmp= table->pos_in_table_list;
  plugin::InfoSchemaTable *sch_table= share->getInfoSchemaTable();
  if (sch_table)
  {
    sch_table->fillTable(ha_session(),
                         tmp);
    iter= sch_table->getRows().begin();
  }
  return 0;
}


int InformationCursor::rnd_next(unsigned char *buf)
{
  ha_statistic_increment(&SSV::ha_read_rnd_next_count);
  plugin::InfoSchemaTable *sch_table= share->getInfoSchemaTable();

  if (iter != sch_table->getRows().end() &&
      ! sch_table->getRows().empty())
  {
    (*iter)->copyRecordInto(buf);
    ++iter;
    return 0;
  }

  sch_table->clearRows();

  return HA_ERR_END_OF_FILE;
}


int InformationCursor::rnd_pos(unsigned char *, unsigned char *)
{
  assert(1);

  return 0;
}


void InformationCursor::position(const unsigned char *)
{
  assert(1);
}


int InformationCursor::info(uint32_t flag)
{
  memset(&stats, 0, sizeof(stats));
  if (flag & HA_STATUS_AUTO)
    stats.auto_increment_value= 1;
  return(0);
}


THR_LOCK_DATA **InformationCursor::store_lock(Session *session,
                                         THR_LOCK_DATA **to,
                                         enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK Table or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT &&
         lock_type <= TL_WRITE) && !session_tablespace_op(session))
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

    lock.type= lock_type;
  }
  *to++= &lock;

  return to;
}

static plugin::StorageEngine *information_engine= NULL;

static int init(plugin::Registry &registry)
{
  information_engine= new InformationEngine(engine_name);
  registry.add(information_engine);
  
  InformationShare::start();

  return 0;
}

static int finalize(plugin::Registry &registry)
{
  registry.remove(information_engine);
  delete information_engine;

  InformationShare::stop();

  return 0;
}

drizzle_declare_plugin(information_engine)
{
  "INFORMATION_ENGINE",
  "1.0",
  "Sun Microsystems ala Brian Aker with some input from Padraig",
  "Engine which provides information schema tables",
  PLUGIN_LICENSE_GPL,
  init,     /* Plugin Init */
  finalize,     /* Plugin Deinit */
  NULL,               /* status variables */
  NULL,               /* system variables */
  NULL                /* config options   */
}
drizzle_declare_plugin_end;
