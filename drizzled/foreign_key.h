/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#include <drizzled/memory/sql_alloc.h>
#include <drizzled/key.h>
#include <drizzled/key_part_spec.h>
#include <drizzled/sql_list.h>
#include <drizzled/cursor.h> /* for default_key_create_info */
#include <drizzled/message/table.pb.h>

namespace drizzled {

void add_foreign_key_to_table_message(
    message::Table *table_message,
    const char* fkey_name,
    List<Key_part_spec> &cols,
    Table_ident *table,
    List<Key_part_spec> &ref_cols,
    message::Table::ForeignKeyConstraint::ForeignKeyOption delete_opt_arg,
    message::Table::ForeignKeyConstraint::ForeignKeyOption update_opt_arg,
    message::Table::ForeignKeyConstraint::ForeignKeyMatchOption match_opt_arg);


class Foreign_key: public Key 
{
public:
  Table_ident *ref_table;
  List<Key_part_spec> ref_columns;

  message::Table::ForeignKeyConstraint::ForeignKeyOption delete_opt;
  message::Table::ForeignKeyConstraint::ForeignKeyOption update_opt;
  message::Table::ForeignKeyConstraint::ForeignKeyMatchOption match_opt;

  Foreign_key(const LEX_STRING &name_arg,
              List<Key_part_spec> &cols,
              Table_ident *table,
              List<Key_part_spec> &ref_cols,
              message::Table::ForeignKeyConstraint::ForeignKeyOption delete_opt_arg,
              message::Table::ForeignKeyConstraint::ForeignKeyOption update_opt_arg,
              message::Table::ForeignKeyConstraint::ForeignKeyMatchOption match_opt_arg) :
    Key(FOREIGN_KEY, name_arg, &default_key_create_info, 0, cols), ref_table(table),
    ref_columns(ref_cols),
    delete_opt(delete_opt_arg),
    update_opt(update_opt_arg),
    match_opt(match_opt_arg)
  { }


  /**
   * Constructs an (almost) deep copy of this foreign key. Only those
   * elements that are known to never change are not copied.
   * If out of memory, a partial copy is returned and an error is set
   * in Session.
   */
  Foreign_key(const Foreign_key &rhs, memory::Root *mem_root);

  /* Used to validate foreign key options */
  bool validate(List<CreateField> &table_fields);
};

} /* namespace drizzled */

