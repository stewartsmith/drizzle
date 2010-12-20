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

#include "config.h"

#include <drizzled/item/copy_string.h>

namespace drizzled
{

void Item_copy_string::copy()
{
  String *res=item->val_str(&str_value);
  if (res && res != &str_value)
    str_value.copy(*res);
  null_value=item->null_value;
}

/* ARGSUSED */
String *Item_copy_string::val_str(String *)
{
  // Item_copy_string is used without fix_fields call
  if (null_value)
    return (String*) 0;
  return &str_value;
}


my_decimal *Item_copy_string::val_decimal(my_decimal *decimal_value)
{
  // Item_copy_string is used without fix_fields call
  if (null_value)
    return 0;
  string2my_decimal(E_DEC_FATAL_ERROR, &str_value, decimal_value);
  return (decimal_value);
}


} /* namespace drizzled */
