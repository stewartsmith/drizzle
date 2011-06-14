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

#include <drizzled/item.h>

namespace drizzled {

extern uint32_t lower_case_table_names;

class Item_ident :public Item
{
protected:
  /*
    We have to store initial values of db_name, table_name and field_name
    to be able to restore them during cleanup() because they can be
    updated during fix_fields() to values from Field object and life-time
    of those is shorter than life-time of Item_field.
  */
  const char *orig_db_name;
  const char *orig_table_name;
  const char *orig_field_name;

public:
  Name_resolution_context *context;
  const char *db_name;
  const char *table_name;
  const char *field_name;
  bool alias_name_used; /* true if item was resolved against alias */
  /*
    Cached value of index for this field in table->field array, used by prep.
    stmts for speeding up their re-execution. Holds NO_CACHED_FIELD_INDEX
    if index value is not known.
  */
  uint32_t cached_field_index;
  /*
    Cached pointer to table which contains this field, used for the same reason
    by prep. stmt. too in case then we have not-fully qualified field.
    0 - means no cached value.
  */
  TableList *cached_table;
  Select_Lex *depended_from;
  Item_ident(Name_resolution_context *context_arg,
             const char *db_name_arg, const char *table_name_arg,
             const char *field_name_arg);
  Item_ident(Session *session, Item_ident *item);
  const char *full_name() const;
  void cleanup();
  bool remove_dependence_processor(unsigned char * arg);
  virtual void print(String *str);
  virtual bool change_context_processor(unsigned char *cntx)
    { context= (Name_resolution_context *)cntx; return false; }
  friend bool insert_fields(Session *session, Name_resolution_context *context,
                            const char *db_name,
                            const char *table_name, List<Item>::iterator *it,
                            bool any_privileges);
};


class Item_ident_for_show :public Item
{
public:
  Field *field;
  const char *db_name;
  const char *table_name;

  Item_ident_for_show(Field *par_field, const char *db_arg,
                      const char *table_name_arg)
    :field(par_field), db_name(db_arg), table_name(table_name_arg)
  {}

  enum Type type() const { return FIELD_ITEM; }
  double val_real();
  int64_t val_int();
  String *val_str(String *str);
  type::Decimal *val_decimal(type::Decimal *dec);
  void make_field(SendField *tmp_field);
};

} /* namespace drizzled */

