/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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


#ifndef DRIZZLED_FOREIGN_KEY_H
#define DRIZZLED_FOREIGN_KEY_H

#include <drizzled/sql_alloc.h>
#include <drizzled/key.h>
#include <drizzled/sql_list.h>

class Item;
typedef struct st_mem_root MEM_ROOT;

class Foreign_key: public Key {
public:
  enum fk_match_opt { FK_MATCH_UNDEF, FK_MATCH_FULL,
                      FK_MATCH_PARTIAL, FK_MATCH_SIMPLE};
  enum fk_option { FK_OPTION_UNDEF, FK_OPTION_RESTRICT, FK_OPTION_CASCADE,
                   FK_OPTION_SET_NULL, FK_OPTION_NO_ACTION, FK_OPTION_DEFAULT};

  Table_ident *ref_table;
  List<Key_part_spec> ref_columns;
  uint32_t delete_opt, update_opt, match_opt;
Foreign_key(const LEX_STRING &name_arg, List<Key_part_spec> &cols,
            Table_ident *table,   List<Key_part_spec> &ref_cols,
            uint32_t delete_opt_arg, uint32_t update_opt_arg,
            uint32_t match_opt_arg)
  :Key(FOREIGN_KEY, name_arg, &default_key_create_info, 0, cols),
    ref_table(table), ref_columns(ref_cols),
    delete_opt(delete_opt_arg), update_opt(update_opt_arg),
    match_opt(match_opt_arg)
    {}
  Foreign_key(const Foreign_key &rhs, MEM_ROOT *mem_root);
  /**
     Used to make a clone of this object for ALTER/CREATE TABLE
     @sa comment for Key_part_spec::clone
  */
  virtual Key *clone(MEM_ROOT *mem_root) const
  { return new (mem_root) Foreign_key(*this, mem_root); }
  /* Used to validate foreign key options */
  bool validate(List<Create_field> &table_fields);
};

#endif /* DRIZZLED_FOREIGN_KEY_H */
