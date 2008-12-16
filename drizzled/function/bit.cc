/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include CSTDINT_H
#include <drizzled/function/bit.h>

int64_t Item_func_bit_neg::val_int()
{
  assert(fixed == 1);
  uint64_t res= (uint64_t) args[0]->val_int();
  if ((null_value=args[0]->null_value))
    return 0;
  return ~res;
}

int64_t Item_func_bit_xor::val_int()
{
  assert(fixed == 1);
  uint64_t arg1= (uint64_t) args[0]->val_int();
  uint64_t arg2= (uint64_t) args[1]->val_int();
  if ((null_value= (args[0]->null_value || args[1]->null_value)))
    return 0;
  return (int64_t) (arg1 ^ arg2);
}


