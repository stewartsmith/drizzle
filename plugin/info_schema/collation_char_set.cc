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
 *   Collation Character Set I_S table methods.
 */

#include "config.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/global_charset_info.h"
#include "drizzled/charset.h"


#include "helper_methods.h"
#include "collation_char_set.h"

#include <vector>

using namespace drizzled;
using namespace std;

/*
 * Vectors of columns for the collation char set I_S table.
 */
static vector<const plugin::ColumnInfo *> *columns= NULL;

/*
 * Methods for the collation char set I_S table.
 */
static plugin::InfoSchemaMethods *methods= NULL;

/*
 * collation char set I_S table.
 */
static plugin::InfoSchemaTable *coll_cs_table= NULL;

/**
 * Populate the vectors of columns for the I_S table.
 *
 * @return a pointer to a std::vector of Columns.
 */
vector<const plugin::ColumnInfo *> *CollationCharSetIS::createColumns()
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
   * Create each column for the COLLATION_CHAR_SET table.
   */
  columns->push_back(new plugin::ColumnInfo("COLLATION_NAME",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  columns->push_back(new plugin::ColumnInfo("CHARACTER_SET_NAME",
                                            64,
                                            DRIZZLE_TYPE_VARCHAR,
                                            0,
                                            0,
                                            ""));

  return columns;
}

/**
 * Initialize the I_S table.
 *
 * @return a pointer to an I_S table
 */
plugin::InfoSchemaTable *CollationCharSetIS::getTable()
{
  columns= createColumns();

  if (methods == NULL)
  {
    methods= new CollCharISMethods();
  }

  if (coll_cs_table == NULL)
  {
    coll_cs_table= new plugin::InfoSchemaTable("OLD_COLLATION_CHARACTER_SET_APPLICABILITY",
                                               *columns,
                                               -1, -1, false, false, 0,
                                               methods);
  }

  return coll_cs_table;
}

/**
 * Delete memory allocated for the table, columns and methods.
 */
void CollationCharSetIS::cleanup()
{
  clearColumns(*columns);
  delete coll_cs_table;
  delete methods;
  delete columns;
}

int CollCharISMethods::fillTable(Session *,  
                                 Table *table,
                                 plugin::InfoSchemaTable *schema_table)
{
  CHARSET_INFO **cs;
  const CHARSET_INFO * const scs= system_charset_info;
  for (cs= all_charsets ; cs < all_charsets+255 ; cs++ )
  {
    CHARSET_INFO **cl;
    const CHARSET_INFO *tmp_cs= cs[0];
    if (! tmp_cs || ! (tmp_cs->state & MY_CS_AVAILABLE) ||
        ! (tmp_cs->state & MY_CS_PRIMARY))
      continue;
    for (cl= all_charsets; cl < all_charsets+255 ;cl ++)
    {
      const CHARSET_INFO *tmp_cl= cl[0];
      if (! tmp_cl || ! (tmp_cl->state & MY_CS_AVAILABLE) ||
          ! my_charset_same(tmp_cs,tmp_cl))
        continue;
      table->restoreRecordAsDefault();
      table->setWriteSet(0);
      table->setWriteSet(1);
      table->field[0]->store(tmp_cl->name, strlen(tmp_cl->name), scs);
      table->field[1]->store(tmp_cl->csname , strlen(tmp_cl->csname), scs);
      schema_table->addRow(table->record[0], table->s->reclength);
    }
  }
  return 0;
}

