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

#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>

#include <drizzled/item/cache_row.h>

namespace drizzled
{

void Item_cache_row::make_field(SendField *)
{
  illegal_method_call((const char*)"make_field");
}


double Item_cache_row::val_real()
{
  illegal_method_call((const char*)"val");
  return 0;
}


int64_t Item_cache_row::val_int()
{
  illegal_method_call((const char*)"val_int");
  return 0;
}


String *Item_cache_row::val_str(String *)
{
  illegal_method_call((const char*)"val_str");
  return 0;
}


type::Decimal *Item_cache_row::val_decimal(type::Decimal *)
{
  illegal_method_call((const char*)"val_decimal");
  return 0;
}


enum Item_result Item_cache_row::result_type() const
{
  return ROW_RESULT;
}


uint32_t Item_cache_row::cols()
{
  return item_count;
}


Item *Item_cache_row::element_index(uint32_t i)
{
  return values[i];
}


Item **Item_cache_row::addr(uint32_t i)
{
  return (Item **) (values + i);
}


void Item_cache_row::allocate(uint32_t num)
{
  item_count= num;
  values= (Item_cache **) getSession().mem.calloc(sizeof(Item_cache *)*item_count);
}


bool Item_cache_row::setup(Item * item)
{
  example= item;
  if (!values)
    allocate(item->cols());
  for (uint32_t i= 0; i < item_count; i++)
  {
    Item *el= item->element_index(i);
    Item_cache *tmp= values[i]= Item_cache::get_cache(el);
    if (!tmp)
      return 1;
    tmp->setup(el);
  }
  return 0;
}


void Item_cache_row::store(Item * item)
{
  null_value= 0;
  item->bring_value();
  for (uint32_t i= 0; i < item_count; i++)
  {
    values[i]->store(item->element_index(i));
    null_value|= values[i]->null_value;
  }
}


void Item_cache_row::illegal_method_call(const char *)
{
  assert(0);
  my_error(ER_OPERAND_COLUMNS, MYF(0), 1);
  return;
}


bool Item_cache_row::check_cols(uint32_t c)
{
  if (c != item_count)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), c);
    return 1;
  }
  return 0;
}


bool Item_cache_row::null_inside()
{
  for (uint32_t i= 0; i < item_count; i++)
  {
    if (values[i]->cols() > 1)
    {
      if (values[i]->null_inside())
        return 1;
    }
    else
    {
      values[i]->update_null_value();
      if (values[i]->null_value)
        return 1;
    }
  }
  return 0;
}


void Item_cache_row::bring_value()
{
  for (uint32_t i= 0; i < item_count; i++)
    values[i]->bring_value();
  return;
}


void Item_cache_row::keep_array()
{
  save_array= 1;
}


void Item_cache_row::cleanup()
{
  Item_cache::cleanup();
  if (save_array)
    memset(values, 0, item_count*sizeof(Item**));
  else
    values= 0;
  return;
}


} /* namespace drizzled */
