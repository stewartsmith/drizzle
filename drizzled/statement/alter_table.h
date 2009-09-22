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

#ifndef DRIZZLED_STATEMENT_ALTER_TABLE_H
#define DRIZZLED_STATEMENT_ALTER_TABLE_H

#include <drizzled/statement.h>

class Session;

namespace drizzled
{
namespace statement
{

class AlterTable : public Statement
{
public:
  AlterTable(Session *in_session)
    :
      Statement(in_session, SQLCOM_ALTER_TABLE)
  {}

  bool execute();
};

} /* end namespace statement */

} /* end namespace drizzled */

class Session;
class TableList;

namespace drizzled
{
  namespace message
  {
    class Table;
  }
}

/** @TODO This should die with I_S engine work from Padraig */
bool mysql_create_like_schema_frm(Session* session,
                                  TableList* schema_table,
                                  HA_CREATE_INFO *create_info,
                                  drizzled::message::Table* table_proto);

#endif /* DRIZZLED_STATEMENT_ALTER_TABLE_H */
