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

#include <drizzled/item/cache_int.h>

namespace drizzled
{

void Item_cache_int::store(Item *item)
{
  value= item->val_int_result();
  null_value= item->null_value;
  unsigned_flag= item->unsigned_flag;
}


void Item_cache_int::store(Item *item, int64_t val_arg)
{
  value= val_arg;
  null_value= item->null_value;
  unsigned_flag= item->unsigned_flag;
}


String *Item_cache_int::val_str(String *str)
{
  assert(fixed == 1);
  str->set(value, default_charset());
  return str;
}

type::Decimal *Item_cache_int::val_decimal(type::Decimal *decimal_val)
{
  assert(fixed == 1);
  int2_class_decimal(E_DEC_FATAL_ERROR, value, unsigned_flag, decimal_val);
  return decimal_val;
}


} /* namespace drizzled */
