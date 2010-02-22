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
 *   Implementation of helper methods for I_S tables.
 */

#include "config.h"
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/sql_base.h"
#include "drizzled/plugin/client.h"
#include "drizzled/join_table.h"
#include "drizzled/global_charset_info.h"
#include "drizzled/pthread_globals.h"
#include "drizzled/internal/m_string.h"
#include "plugin/myisam/myisam.h" // needed for dflt_key_cache
#include "helper_methods.h"

#include <vector>
#include <string>
#include <sstream>

using namespace std;
using namespace drizzled;

static inline void make_upper(char *buf)
{
  for (; *buf; buf++)
    *buf= my_toupper(system_charset_info, *buf);
}

void store_key_column_usage(Table *table, 
                            LEX_STRING *db_name,
                            LEX_STRING *table_name, 
                            const char *key_name,
                            uint32_t key_len, 
                            const char *con_type, 
                            uint32_t con_len,
                            int64_t idx)
{
  const CHARSET_INFO * const cs= system_charset_info;
  /* set the appropriate bits in the write bitset */
  table->setWriteSet(1);
  table->setWriteSet(2);
  table->setWriteSet(4);
  table->setWriteSet(5);
  table->setWriteSet(6);
  table->setWriteSet(7);
  table->field[1]->store(db_name->str, db_name->length, cs);
  table->field[2]->store(key_name, key_len, cs);
  table->field[4]->store(db_name->str, db_name->length, cs);
  table->field[5]->store(table_name->str, table_name->length, cs);
  table->field[6]->store(con_type, con_len, cs);
  table->field[7]->store((int64_t) idx, true);
}

/*
 * Function object used for deleting the memory allocated
 * for the columns contained with the vector of columns.
 */
class DeleteColumns
{
public:
  template<typename T>
  inline void operator()(const T *ptr) const
  {
    delete ptr;
  }
};

void clearColumns(vector<const drizzled::plugin::ColumnInfo *> &cols)
{
  for_each(cols.begin(), cols.end(), DeleteColumns());
  cols.clear();
}
