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
#include <math.h>
#include <drizzled/function/math/real.h>

namespace drizzled
{


String *Item_real_func::val_str(String *str)
{
  assert(fixed == 1);
  double nr= val_real();
  if (null_value)
    return 0;
  str->set_real(nr,decimals, &my_charset_bin);
  return str;
}


type::Decimal *Item_real_func::val_decimal(type::Decimal *decimal_value)
{
  assert(fixed);
  double nr= val_real();
  if (null_value)
    return 0;
  double2_class_decimal(E_DEC_FATAL_ERROR, nr, decimal_value);
  return decimal_value;
}

int64_t Item_real_func::val_int()
{
  assert(fixed == 1);
  return (int64_t) rint(val_real());
}

void Item_real_func::fix_length_and_dec()
{
  decimals= NOT_FIXED_DEC;
  max_length= float_length(decimals);
}

} /* namespace drizzled */
