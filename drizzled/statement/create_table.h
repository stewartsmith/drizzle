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

#ifndef DRIZZLED_STATEMENT_CREATE_TABLE_H
#define DRIZZLED_STATEMENT_CREATE_TABLE_H

#include "drizzled/statement.h"
#include "drizzled/foreign_key.h"

namespace drizzled
{
class Session;

namespace statement
{

class CreateTable : public Statement
{
public:
  CreateTable(Session *in_session)
    :
      Statement(in_session),
      is_create_table_like(false),
      is_if_not_exists(false),
      is_engine_set(false)
  {
    memset(&create_info, 0, sizeof(create_info));
  }

  bool execute();
  message::Table create_table_message;
  message::Table &createTableMessage()
  {
    return create_table_message;
  };
  message::Table::Field *current_proto_field;
  HA_CREATE_INFO create_info;
  AlterInfo alter_info;
  KEY_CREATE_INFO key_create_info;
  message::Table::ForeignKeyConstraint::ForeignKeyMatchOption fk_match_option;
  message::Table::ForeignKeyConstraint::ForeignKeyOption fk_update_opt;
  message::Table::ForeignKeyConstraint::ForeignKeyOption fk_delete_opt;

  /* The text in a CHANGE COLUMN clause in ALTER TABLE */
  char *change;

  /* An item representing the DEFAULT clause in CREATE/ALTER TABLE */
  Item *default_value;

  /* An item representing the ON UPDATE clause in CREATE/ALTER TABLE */
  Item *on_update_value;

  enum column_format_type column_format;

  /* Poly-use */
  LEX_STRING comment;

  bool is_create_table_like;
  bool is_if_not_exists;
  bool is_engine_set;

  bool validateCreateTableOption();
};

} /* namespace statement */

} /* namespace drizzled */

#endif /* DRIZZLED_STATEMENT_CREATE_TABLE_H */
