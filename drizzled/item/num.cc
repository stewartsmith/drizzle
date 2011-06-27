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

#include <config.h>

#include <drizzled/item/uint.h>
#include <drizzled/item/num.h>
#include <drizzled/item/string.h>
#include <drizzled/item/decimal.h>

namespace drizzled
{

/**
  @details
  Created mostly for prepare_table(). Important
  when a string ENUM/SET column is described with a numeric default value:

  CREATE TABLE t1(a SET('a') DEFAULT 1);

  We cannot use generic Item::safe_charset_converter(), because
  the latter returns a non-fixed Item, so val_str() crashes afterwards.
  Override Item_num method, to return a fixed item.
*/
Item *Item_num::safe_charset_converter(const charset_info_st * const)
{
  Item_string *conv;
  char buf[64];
  String *s, tmp(buf, sizeof(buf), &my_charset_bin);
  s= val_str(&tmp);
  conv= new Item_string(s->ptr(), s->length(), s->charset());
  conv->str_value.copy();
  conv->str_value.mark_as_const();
  return conv;
}

Item_num *Item_uint::neg()
{
  Item_decimal *item= new Item_decimal(value, 1);
  return item->neg();
}

} /* namespace drizzled */
