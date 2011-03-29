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

#pragma once

#include <drizzled/alter_info.h>
#include <drizzled/statement.h>
#include <drizzled/foreign_key.h>
#include <drizzled/sql_lex.h>

namespace drizzled {
namespace statement {

class CreateTable : public Statement
{
  virtual bool check(const identifier::Table&);

public:
  CreateTable(Session *in_session, Table_ident *ident, bool is_temporary);
  CreateTable(Session *in_session);

  virtual bool is_alter() const
  {
    return false;
  }

  bool execute();

  virtual bool executeInner(const identifier::Table&);

public:
  message::Table &createTableMessage()
  {
    return *lex().table();
  };

private:
  HA_CREATE_INFO _create_info;

public:

  HA_CREATE_INFO &create_info()
  {
    if (createTableMessage().options().auto_increment_value())
    {
      _create_info.auto_increment_value= createTableMessage().options().auto_increment_value();
      _create_info.used_fields|= HA_CREATE_USED_AUTO;
    }

    return _create_info;
  }

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

  bool is_engine_set;
  bool is_create_table_like;
  bool lex_identified_temp_table;
  bool link_to_local;
  TableList *create_table_list;

  bool validateCreateTableOption();
};

} /* namespace statement */

} /* namespace drizzled */

