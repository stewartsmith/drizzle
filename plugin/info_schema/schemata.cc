/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/**
 * @file 
 *   schemata I_S table methods.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/join_table.h"

#include "helper_methods.h"
#include "schemata.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the schemata I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the schemata I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * schemata I_S table.
 */
static plugin::InfoSchemaTable *sch_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *SchemataIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*columns);
  }

  columns->push_back(new plugin::ColumnInfo("CATALOG_NAME",
                                            FN_REFLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0, 
                                            1, 
                                            ""));

  columns->push_back(new plugin::ColumnInfo("SCHEMA_NAME",
                                            NAME_CHAR_LEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0, 
                                            0, 
                                            "Database"));

  columns->push_back(new plugin::ColumnInfo("DEFAULT_CHARACTER_SET_NAME",
                                            64, 
                                            DRIZZLE_TYPE_VARCHAR, 
                                            0, 
                                            0, 
                                            ""));

  columns->push_back(new plugin::ColumnInfo("DEFAULT_COLLATION_NAME",
                                            64, 
                                            DRIZZLE_TYPE_VARCHAR, 
                                            0, 
                                            0, 
                                            ""));

  columns->push_back(new plugin::ColumnInfo("SQL_PATH",
                                            FN_REFLEN,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0, 
                                            1, 
                                            ""));

  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *SchemataIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new SchemataISMethods();
  }

  if (sch_table == NULL)
  {
    sch_table= new plugin::InfoSchemaTable("SCHEMATA",
                                           *columns,
                                           1, -1, false, false, 0,
                                           methods);
  }

  return sch_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void SchemataIS::cleanup()
{
  clearColumns(*columns);
  delete sch_table;
  delete methods;
  delete columns;
}

static bool store_schema_schemata(Session *, 
                                  Table *table, 
                                  LEX_STRING *db_name,
                                  const CHARSET_INFO * const cs,
                                  plugin::InfoSchemaTable *schema_table)
{
  table->restoreRecordAsDefault();
  table->setWriteSet(1);
  table->setWriteSet(2);
  table->setWriteSet(3);
  table->field[1]->store(db_name->str, db_name->length, system_charset_info);
  table->field[2]->store(cs->csname, strlen(cs->csname), system_charset_info);
  table->field[3]->store(cs->name, strlen(cs->name), system_charset_info);
  schema_table->addRow(table->record[0], table->s->reclength);
  return false;
}

int SchemataISMethods::fillTable(Session *session, 
                                 Table *table,
                                 plugin::InfoSchemaTable *schema_table)
{
  /*
    TODO: fill_schema_shemata() is called when new client is connected.
    Returning error status in this case leads to client hangup.
  */

  LOOKUP_FIELD_VALUES lookup_field_vals;
  vector<LEX_STRING*> db_names;
  bool with_i_schema;
  /* the WHERE condition */
  COND *cond= table->reginfo.join_tab->select_cond;

  if (get_lookup_field_values(session, cond, table->pos_in_table_list, &lookup_field_vals))
  {
    return 0;
  }

  if (make_db_list(session, 
                   db_names, 
                   &lookup_field_vals,
                   &with_i_schema))
  {
    return 1;
  }

  /*
    If we have lookup db value we should check that the database exists
  */
  if (lookup_field_vals.db_value.str && 
      ! lookup_field_vals.wild_db_value &&
      ! with_i_schema)
  {
    char path[FN_REFLEN+16];
    uint32_t path_len;
    struct stat stat_info;
    if (! lookup_field_vals.db_value.str[0])
    {
      return 0;
    }

    path_len= build_table_filename(path, 
                                   sizeof(path),
                                   lookup_field_vals.db_value.str, 
                                   "", 
                                   false);
    path[path_len-1]= 0;
    if (stat(path,&stat_info))
    {
      return 0;
    }
  }

  vector<LEX_STRING*>::iterator db_name= db_names.begin();
  while (db_name != db_names.end())
  {
    if (with_i_schema)       // information schema name is always first in list
    {
      if (store_schema_schemata(session, table, *db_name, system_charset_info, schema_table))
      {
        return 1;
      }
      with_i_schema= 0;
    }
    else
    {
      const CHARSET_INFO *cs= get_default_db_collation((*db_name)->str);

      if (store_schema_schemata(session, table, *db_name, cs, schema_table))
      {
        return 1;
      }
    }
    ++db_name;
  }
  return 0;
}

int SchemataISMethods::oldFormat(Session *session, drizzled::plugin::InfoSchemaTable *schema_table)
  const
{
  char tmp[128];
  LEX *lex= session->lex;
  Select_Lex *sel= lex->current_select;
  Name_resolution_context *context= &sel->context;
  const drizzled::plugin::InfoSchemaTable::Columns sch_columns= schema_table->getColumns();

  if (! sel->item_list.elements)
  {
    const drizzled::plugin::ColumnInfo *column= sch_columns[1];
    String buffer(tmp,sizeof(tmp), system_charset_info);
    Item_field *field= new Item_field(context,
                                      NULL, 
                                      NULL, 
                                      column->getName().c_str());
    if (! field || session->add_item_to_list(field))
    {
      return 1;
    }
    buffer.length(0);
    buffer.append(column->getOldName().c_str());
    if (lex->wild && lex->wild->ptr())
    {
      buffer.append(STRING_WITH_LEN(" ("));
      buffer.append(lex->wild->ptr());
      buffer.append(')');
    }
    field->set_name(buffer.ptr(), buffer.length(), system_charset_info);
  }
  return 0;
}

