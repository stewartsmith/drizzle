/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
#include "drizzled/statement/drop_index.h"
#include "drizzled/statement/alter_table.h"
#include "drizzled/db.h"

namespace drizzled
{

bool statement::DropIndex::execute()
{
  TableList *first_table= (TableList *) session->lex->select_lex.table_list.first;
  TableList *all_tables= session->lex->query_tables;

  /* Chicken/Egg... we need to search for the table, to know if the table exists, so we can build a full identifier from it */
  message::table::shared_ptr original_table_message;
  {
    TableIdentifier identifier(first_table->getSchemaName(), first_table->getTableName());
    if (plugin::StorageEngine::getTableDefinition(*session, identifier, original_table_message) != EEXIST)
    {
      std::string path;
      identifier.getSQLPath(path);
      my_error(ER_BAD_TABLE_ERROR, MYF(0), path.c_str());
      return true;
    }
  }

  /*
     CREATE INDEX and DROP INDEX are implemented by calling ALTER
     TABLE with proper arguments.

     In the future ALTER TABLE will notice that the request is to
     only add indexes and create these one by one for the existing
     table without having to do a full rebuild.
   */
  /* Prepare stack copies to be re-execution safe */
  HA_CREATE_INFO create_info;

  assert(first_table == all_tables && first_table != 0);
  if (session->inTransaction())
  {
    my_error(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, MYF(0));
    return true;
  }

  memset(&create_info, 0, sizeof(create_info));
  create_info.db_type= 0;

  bool res;
  if (original_table_message->type() == message::Table::STANDARD )
  {
    TableIdentifier identifier(first_table->getSchemaName(), first_table->getTableName());

    create_info.default_table_charset= plugin::StorageEngine::getSchemaCollation(identifier);

    res= alter_table(session, 
                     identifier,
                     identifier,
                     &create_info, 
                     *original_table_message,
                     create_table_proto, 
                     first_table,
                     &alter_info,
                     0, (Order*) 0, 0);
  }
  else
  {
    TableIdentifier catch22(first_table->getSchemaName(), first_table->getTableName());
    Table *table= session->find_temporary_table(catch22);
    assert(table);
    {
      TableIdentifier identifier(first_table->getSchemaName(), first_table->getTableName(), table->getShare()->getPath());
      create_info.default_table_charset= plugin::StorageEngine::getSchemaCollation(identifier);

      res= alter_table(session, 
                       identifier,
                       identifier,
                       &create_info, 
                       *original_table_message,
                       create_table_proto, 
                       first_table,
                       &alter_info,
                       0, (Order*) 0, 0);
    }
  }
  return res;
}

} /* namespace drizzled */
