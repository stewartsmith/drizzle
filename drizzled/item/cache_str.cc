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

#include <drizzled/item/cache_str.h>
#include <drizzled/field.h>

namespace drizzled
{

Item_cache_str::Item_cache_str(const Item *item) :
  Item_cache(), value(0),
  is_varbinary(item->type() == FIELD_ITEM &&
               ((const Item_field *) item)->field->type() ==
               DRIZZLE_TYPE_VARCHAR &&
               !((const Item_field *) item)->field->has_charset())
{}

void Item_cache_str::store(Item *item)
{
  value_buff.set(buffer, sizeof(buffer), item->collation.collation);
  value= item->str_result(&value_buff);
  if ((null_value= item->null_value))
    value= 0;
  else if (value != &value_buff)
  {
    /*
      We copy string value to avoid changing value if 'item' is table field
      in queries like following (where t1.c is varchar):
      select a,
             (select a,b,c from t1 where t1.a=t2.a) = ROW(a,2,'a'),
             (select c from t1 where a=t2.a)
        from t2;
    */
    value_buff.copy(*value);
    value= &value_buff;
  }
}

double Item_cache_str::val_real()
{
  assert(fixed == 1);
  int err_not_used;
  char *end_not_used;
  if (value)
    return my_strntod(value->charset(), (char*) value->ptr(),
                      value->length(), &end_not_used, &err_not_used);
  return (double) 0;
}


int64_t Item_cache_str::val_int()
{
  assert(fixed == 1);
  int err;
  if (value)
    return my_strntoll(value->charset(), value->ptr(),
                       value->length(), 10, (char**) 0, &err);
  else
    return (int64_t)0;
}

type::Decimal *Item_cache_str::val_decimal(type::Decimal *decimal_val)
{
  assert(fixed == 1);
  if (value)
    decimal_val->store(E_DEC_FATAL_ERROR, value);
  else
    decimal_val= 0;
  return decimal_val;
}

int Item_cache_str::save_in_field(Field *field, bool no_conversions)
{
  int res= Item_cache::save_in_field(field, no_conversions);

  return res;
}

} /* namespace drizzled */
