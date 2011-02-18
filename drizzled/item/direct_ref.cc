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

#include <drizzled/item/direct_ref.h>

namespace drizzled
{

double Item_direct_ref::val_real()
{
  double tmp=(*ref)->val_real();
  null_value=(*ref)->null_value;
  return tmp;
}


int64_t Item_direct_ref::val_int()
{
  int64_t tmp=(*ref)->val_int();
  null_value=(*ref)->null_value;
  return tmp;
}


String *Item_direct_ref::val_str(String* tmp)
{
  tmp=(*ref)->val_str(tmp);
  null_value=(*ref)->null_value;
  return tmp;
}

type::Decimal *Item_direct_ref::val_decimal(type::Decimal *decimal_value)
{
  type::Decimal *tmp= (*ref)->val_decimal(decimal_value);
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_direct_ref::val_bool()
{
  bool tmp= (*ref)->val_bool();
  null_value=(*ref)->null_value;
  return tmp;
}


bool Item_direct_ref::is_null()
{
  return (*ref)->is_null();
}


bool Item_direct_ref::get_date(type::Time &ltime, uint32_t fuzzydate)
{
  return (null_value=(*ref)->get_date(ltime, fuzzydate));
}

} /* namespace drizzled */
