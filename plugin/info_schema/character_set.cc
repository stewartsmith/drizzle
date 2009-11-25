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
 *   Character Set I_S table methods.
 */

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"

#include "helper_methods.h"
#include "character_set.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the character set I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the character set I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * character set I_S table.
 */
static plugin::InfoSchemaTable *char_set_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return false on success; true on failure.
 */
vector<const plugin::ColumnInfo *> *CharacterSetIS::createColumns()
{
  if (columns == NULL)
  {
    columns= new vector<const plugin::ColumnInfo *>;
  }
  else
  {
    clearColumns(*columns);
  }

  /*
   * Create each column for the CHARACTER_SET table.
   */
  columns->push_back(new plugin::ColumnInfo("CHARACTER_SET_NAME",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Charset",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("DEFAULT_COLLATE_NAME",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Default collation",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("DESCRIPTION",
                                            60,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            "Description",
                                            SKIP_OPEN_TABLE));

  columns->push_back(new plugin::ColumnInfo("MAXLEN",
                                            3,
                                            DRIZZLE_TYPE_LONGLONG,
                                            0,
                                            0,
                                            "Maxlen",
                                            SKIP_OPEN_TABLE));

  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *CharacterSetIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new CharSetISMethods();
  }

  if (char_set_table == NULL)
  {
    char_set_table= new plugin::InfoSchemaTable("CHARACTER_SETS",
                                                *columns,
                                                -1, -1, false, false, 0,
                                                methods);
  }

  return char_set_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void CharacterSetIS::cleanup()
{
  clearColumns(*columns);
  delete char_set_table;
  delete methods;
  delete columns;
}

int CharSetISMethods::fillTable(Session *session, TableList *tables)
{
  CHARSET_INFO **cs;
  const char *wild= session->lex->wild ? session->lex->wild->ptr() : NULL;
  Table *table= tables->table;
  const CHARSET_INFO * const scs= system_charset_info;

  for (cs= all_charsets ; cs < all_charsets+255 ; cs++)
  {
    const CHARSET_INFO * const tmp_cs= cs[0];
    if (tmp_cs && (tmp_cs->state & MY_CS_PRIMARY) &&
        (tmp_cs->state & MY_CS_AVAILABLE) &&
        !(tmp_cs->state & MY_CS_HIDDEN) &&
        !(wild && wild[0] &&
          wild_case_compare(scs, tmp_cs->csname,wild)))
    {
      const char *comment;
      table->restoreRecordAsDefault();
      table->setWriteSet(0);
      table->setWriteSet(1);
      table->setWriteSet(2);
      table->setWriteSet(3);
      table->field[0]->store(tmp_cs->csname, strlen(tmp_cs->csname), scs);
      table->field[1]->store(tmp_cs->name, strlen(tmp_cs->name), scs);
      comment= tmp_cs->comment ? tmp_cs->comment : "";
      table->field[2]->store(comment, strlen(comment), scs);
      table->field[3]->store((int64_t) tmp_cs->mbmaxlen, true);
      tables->schema_table->addRow(table->record[0], table->s->reclength);
    }
  }
  return 0;
}

int CharSetISMethods::oldFormat(Session *session, drizzled::plugin::InfoSchemaTable *schema_table)
  const
{
  int fields_arr[]= {0, 2, 1, 3, -1};
  int *field_num= fields_arr;
  const drizzled::plugin::InfoSchemaTable::Columns tab_columns= schema_table->getColumns();
  const drizzled::plugin::ColumnInfo *column= NULL;
  Name_resolution_context *context= &session->lex->select_lex.context;

  for (; *field_num >= 0; field_num++)
  {
    column= tab_columns[*field_num];
    Item_field *field= new Item_field(context,
                                      NULL, NULL, column->getName().c_str());
    if (field)
    {
      field->set_name(column->getOldName().c_str(),
                      column->getOldName().length(),
                      system_charset_info);
      if (session->add_item_to_list(field))
        return 1;
    }
  }
  return 0;
}
