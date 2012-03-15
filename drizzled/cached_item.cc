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

/**
  @file

  @brief
  Buffers to save and compare item values
*/

#include <config.h>
#include <drizzled/cached_item.h>
#include <drizzled/field.h>
#include <drizzled/sql_string.h>
#include <drizzled/session.h>
#include <drizzled/item/field.h>
#include <drizzled/system_variables.h>
#include <algorithm>

using namespace std;

namespace drizzled {

/**
  Create right type of Cached_item for an item.
*/

Cached_item *new_Cached_item(Session *session, Item *item)
{
  if (item->real_item()->type() == Item::FIELD_ITEM &&
      !(((Item_field *) (item->real_item()))->field->flags & BLOB_FLAG))
  {
    Item_field *real_item= (Item_field *) item->real_item();
    Field *cached_field= real_item->field;
    return new Cached_item_field(cached_field);
  }

  switch (item->result_type()) {
  case STRING_RESULT:
    return new Cached_item_str(session, (Item_field *) item);
  case INT_RESULT:
    return new Cached_item_int((Item_field *) item);
  case REAL_RESULT:
    return new Cached_item_real(item);
  case DECIMAL_RESULT:
    return new Cached_item_decimal(item);
  case ROW_RESULT:
    assert(0);
    return 0;
  }

  abort();
}

Cached_item::~Cached_item() {}

/**
  Compare with old value and replace value with new value.

  @return
    Return true if values have changed
*/

Cached_item_str::Cached_item_str(Session *session, Item *arg)
  :item(arg), value(min(arg->max_length,
                        (uint32_t)session->variables.max_sort_length))
{}

bool Cached_item_str::cmp(void)
{
  String *res;
  bool tmp;

  if ((res=item->val_str(&tmp_value)))
    res->length(min(res->length(), value.alloced_length()));

  if (null_value != item->null_value)
  {
    if ((null_value= item->null_value))
      // New value was null
      return true;
    tmp=true;
  }
  else if (null_value)
    // new and old value was null
    return 0;
  else
    tmp= sortcmp(&value,res,item->collation.collation) != 0;
  if (tmp)
    // Remember for next cmp
    value.copy(*res);
  return(tmp);
}

Cached_item_str::~Cached_item_str()
{
  // Safety
  item=0;
}

bool Cached_item_real::cmp(void)
{
  double nr= item->val_real();
  if (null_value != item->null_value || nr != value)
  {
    null_value= item->null_value;
    value=nr;
    return true;
  }
  return false;
}

bool Cached_item_int::cmp(void)
{
  int64_t nr=item->val_int();
  if (null_value != item->null_value || nr != value)
  {
    null_value= item->null_value;
    value=nr;
    return true;
  }
  return false;
}


Cached_item_field::Cached_item_field(Field *arg_field) 
  : 
    field(arg_field)
{
  /* TODO: take the memory allocation below out of the constructor. */
  buff= (unsigned char*) memory::sql_calloc(length= field->pack_length());
}

bool Cached_item_field::cmp(void)
{
  // This is not a blob!
  bool tmp= field->cmp_internal(buff) != 0;

  if (tmp)
    field->get_image(buff,length,field->charset());
  if (null_value != field->is_null())
  {
    null_value= !null_value;
    tmp=true;
  }
  return(tmp);
}


Cached_item_decimal::Cached_item_decimal(Item *it)
  :item(it)
{
  value.set_zero();
}


bool Cached_item_decimal::cmp()
{
  type::Decimal tmp;
  type::Decimal *ptmp= item->val_decimal(&tmp);
  if (null_value != item->null_value ||
      (!item->null_value && class_decimal_cmp(&value, ptmp)))
  {
    null_value= item->null_value;
    /* Save only not null values */
    if (!null_value)
    {
      class_decimal2decimal(ptmp, &value);
      return true;
    }
    return false;
  }
  return false;
}

} /* namespace drizzled */
