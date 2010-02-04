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

#ifndef DRIZZLED_ITEM_REF_NULL_HELPER_H
#define DRIZZLED_ITEM_REF_NULL_HELPER_H

/*
  An object of this class:
   - Converts val_XXX() calls to ref->val_XXX_result() calls, like Item_ref.
   - Sets owner->was_null=true if it has returned a NULL value from any
     val_XXX() function. This allows to inject an Item_ref_null_helper
     object into subquery and then check if the subquery has produced a row
     with NULL value.
*/

namespace drizzled
{

class Item_ref_null_helper: public Item_ref
{
protected:
  Item_in_subselect* owner;
public:
  Item_ref_null_helper(Name_resolution_context *context_arg,
                       Item_in_subselect* master, Item **item,
                       const char *table_name_arg, const char *field_name_arg)
    :Item_ref(context_arg, item, table_name_arg, field_name_arg),
     owner(master) {}
  double val_real();
  int64_t val_int();
  String* val_str(String* s);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  bool get_date(DRIZZLE_TIME *ltime, uint32_t fuzzydate);
  virtual void print(String *str, enum_query_type query_type);
  /*
    we add RAND_TABLE_BIT to prevent moving this item from HAVING to WHERE
  */
  table_map used_tables() const
  {
    return (depended_from ?
            OUTER_REF_TABLE_BIT :
            (*ref)->used_tables() | RAND_TABLE_BIT);
  }
};

} /* namespace drizzled */

#endif /* DRIZZLED_ITEM_REF_NULL_HELPER_H */
