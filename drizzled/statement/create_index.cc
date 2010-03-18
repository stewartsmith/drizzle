/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include "config.h"
#include "drizzled/show.h"
#include "drizzled/session.h"
#include "drizzled/statement/create_index.h"
#include "drizzled/statement/alter_table.h"
#include "drizzled/db.h"

namespace drizzled
{

bool statement::CreateIndex::execute()
{
  TableList *first_table= (TableList *) session->lex->select_lex.table_list.first;
  TableList *all_tables= session->lex->query_tables;
  /*
    CREATE INDEX and DROP INDEX are implemented by calling ALTER
    TABLE with proper arguments.

    In the future ALTER TABLE will notice that the request is to
    only add indexes and create these one by one for the existing
    table without having to do a full rebuild.
  */

  assert(first_table == all_tables && first_table != 0);
  if (! session->endActiveTransaction())
  {
    return true;
  }

  create_info.row_type= ROW_TYPE_NOT_USED;
  create_info.default_table_charset= plugin::StorageEngine::getSchemaCollation(session->db.c_str());

  bool res= alter_table(session, 
                        first_table->db, 
                        first_table->table_name,
                        &create_info, 
                        create_table_message, 
                        first_table,
                        &alter_info,
                        0, (order_st*) 0, 0);
  return res;
}

} /* namespace drizzled */
