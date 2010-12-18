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

#ifndef DRIZZLED_FUNCTION_USER_VAR_AS_OUT_PARAM_H
#define DRIZZLED_FUNCTION_USER_VAR_AS_OUT_PARAM_H

#include <drizzled/function/func.h>

namespace drizzled
{

/*
  This item represents user variable used as out parameter (e.g in LOAD DATA),
  and it is supposed to be used only for this purprose. So it is simplified
  a lot. Actually you should never obtain its value.

  The only two reasons for this thing being an Item is possibility to store it
  in List<Item> and desire to place this code somewhere near other functions
  working with user variables.
*/
class Item_user_var_as_out_param :public Item
{
  LEX_STRING name;
  user_var_entry *entry;
public:
  Item_user_var_as_out_param(LEX_STRING a) : name(a) {}
  /* We should return something different from FIELD_ITEM here */
  enum Type type() const { return STRING_ITEM;}
  double val_real();
  int64_t val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *decimal_buffer);
  /* fix_fields() binds variable name with its entry structure */
  bool fix_fields(Session *session, Item **ref);
  virtual void print(String *str, enum_query_type query_type);
  void set_null_value(const CHARSET_INFO * const cs);
  void set_value(const char *str, uint32_t length, const CHARSET_INFO * const cs);
};

} /* namespace drizzled */

#endif /* DRIZZLED_FUNCTION_USER_VAR_AS_OUT_PARAM_H */
