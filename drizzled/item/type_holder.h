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

namespace drizzled {

/*
  Item_type_holder used to store type. name, length of Item for UNIONS &
  derived tables.

  Item_type_holder do not need cleanup() because its time of live limited by
  single SP/PS execution.
*/
class Item_type_holder: public Item
{
protected:
  TYPELIB *enum_set_typelib;
  enum_field_types fld_type;

  /**
    Get full information from Item about enum/set fields to be able to create
    them later.

    @param item    Item for information collection
  */
  void get_full_info(Item *item);

  /* It is used to count decimal precision in join_types */
  int prev_decimal_int_part;
public:
  Item_type_holder(Session *session, Item *item);

  /**
     Return expression type of Item_type_holder.

     @return
     Item_result (type of internal Drizzle expression result)
  */
  Item_result result_type() const;

  enum_field_types field_type() const { return fld_type; };
  enum Type type() const { return TYPE_HOLDER; }
  double val_real();
  int64_t val_int();
  type::Decimal *val_decimal(type::Decimal *val);
  String *val_str(String* val);

  /**
    Find field type which can carry current Item_type_holder type and
    type of given Item.

    @param session     thread handler
    @param item    given item to join its parameters with this item ones

    @retval
      true   error - types are incompatible
    @retval
      false  OK
  */
  bool join_types(Session *session, Item *item);

  /**
    Make temporary table field according collected information about type
    of UNION result.

    @param table  temporary table for which we create fields

    @return
      created field
  */
  Field *make_field_by_type(Table *table);

  /**
    Calculate lenth for merging result for given Item type.

    @param item  Item for length detection

    @return
      length
  */
  static uint32_t display_length(Item *item);

  /**
     Find real field type of item.

     @return
     type of field which should be created to store item value
  */
  static enum_field_types get_real_type(Item *item);
};

} /* namespace drizzled */

