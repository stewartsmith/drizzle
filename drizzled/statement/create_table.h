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

#include <drizzled/statement.h>

class Session;

namespace drizzled
{
namespace statement
{

class CreateTable : public Statement
{
public:
  CreateTable(Session *in_session)
    :
      Statement(in_session)
  {
    memset(&create_info, 0, sizeof(create_info));
  }

  bool execute();
  drizzled::message::Table create_table_proto;
  drizzled::message::Table::Field *current_proto_field;
  HA_CREATE_INFO create_info;
  AlterInfo alter_info;
  KEY_CREATE_INFO key_create_info;
  enum Foreign_key::fk_match_opt fk_match_option;
  enum Foreign_key::fk_option fk_update_opt;
  enum Foreign_key::fk_option fk_delete_opt;

  /* The text in a CHANGE COLUMN clause in ALTER TABLE */
  char *change;

  /* An item representing the DEFAULT clause in CREATE/ALTER TABLE */
  Item *default_value;

  /* An item representing the ON UPDATE clause in CREATE/ALTER TABLE */
  Item *on_update_value;

  enum column_format_type column_format;

  /* Poly-use */
  LEX_STRING comment;
};

} /* end namespace statement */

} /* end namespace drizzled */

#endif /* DRIZZLED_STATEMENT_CREATE_TABLE_H */
