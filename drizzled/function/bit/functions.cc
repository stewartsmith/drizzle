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

#include <drizzled/function/bit/functions.h>

namespace drizzled
{

namespace function
{

namespace bit
{

int64_t ShiftLeft::val_int()
{
  assert(fixed == 1);
  uint32_t shift;
  uint64_t res= ((uint64_t) args[0]->val_int() <<
		  (shift=(uint) args[1]->val_int()));
  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;

  return (shift < sizeof(int64_t)*8 ? (int64_t) res : 0LL);
}

int64_t ShiftRight::val_int()
{
  assert(fixed == 1);
  uint32_t shift;
  uint64_t res= (uint64_t) args[0]->val_int() >>
    (shift=(uint) args[1]->val_int());

  if (args[0]->null_value || args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;

  return (shift < sizeof(int64_t)*8 ? (int64_t) res : 0LL);
}


int64_t Neg::val_int()
{
  assert(fixed == 1);
  uint64_t res= (uint64_t) args[0]->val_int();

  if ((null_value=args[0]->null_value))
    return 0;

  return ~res;
}


int64_t Xor::val_int()
{
  assert(fixed == 1);
  uint64_t arg1= (uint64_t) args[0]->val_int();
  uint64_t arg2= (uint64_t) args[1]->val_int();

  if ((null_value= (args[0]->null_value || args[1]->null_value)))
    return 0;

  return (int64_t) (arg1 ^ arg2);
}

int64_t Or::val_int()
{
  assert(fixed == 1);

  uint64_t arg1= (uint64_t) args[0]->val_int();
  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }

  uint64_t arg2= (uint64_t) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1;
    return 0;
  }
  null_value=0;

  return (int64_t) (arg1 | arg2);
}

int64_t And::val_int()
{
  assert(fixed == 1);

  uint64_t arg1= (uint64_t) args[0]->val_int();

  if (args[0]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }

  uint64_t arg2= (uint64_t) args[1]->val_int();

  if (args[1]->null_value)
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;

  return (int64_t) (arg1 & arg2);
}

} /* namespace bit */
} /* namespace function */
} // namespace drizzled
