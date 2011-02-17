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

#include <drizzled/function/math/int_divide.h>

namespace drizzled
{

/* Integer division */
int64_t Item_func_int_div::val_int()
{
  assert(fixed == 1);
  int64_t value=args[0]->val_int();
  int64_t val2=args[1]->val_int();
  if ((null_value= (args[0]->null_value || args[1]->null_value)))
    return 0;
  if (val2 == 0)
  {
    signal_divide_by_null();
    return 0;
  }
  return (unsigned_flag ?
          (int64_t)((uint64_t) value / (uint64_t) val2) :
          value / val2);
}

void Item_func_int_div::fix_length_and_dec()
{
  Item_result argtype= args[0]->result_type();
  /* use precision ony for the data type it is applicable for and valid */
  max_length=args[0]->max_length -
    (argtype == DECIMAL_RESULT || argtype == INT_RESULT ?
     args[0]->decimals : 0);
  maybe_null=1;
  unsigned_flag=args[0]->unsigned_flag | args[1]->unsigned_flag;
}

} /* namespace drizzled */
