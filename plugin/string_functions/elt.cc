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

#include "elt.h"

namespace drizzled
{

void Item_func_elt::fix_length_and_dec()
{
  max_length=0;
  decimals=0;

  if (agg_arg_charsets(collation, args+1, arg_count-1, MY_COLL_ALLOW_CONV, 1))
    return;

  for (uint32_t i= 1 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
  maybe_null=1;                                 // NULL if wrong first arg
}


double Item_func_elt::val_real()
{
  assert(fixed == 1);
  uint32_t tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return 0.0;
  double result= args[tmp]->val_real();
  null_value= args[tmp]->null_value;
  return result;
}

int64_t Item_func_elt::val_int()
{
  assert(fixed == 1);
  uint32_t tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return 0;

  int64_t result= args[tmp]->val_int();
  null_value= args[tmp]->null_value;
  return result;
}


String *Item_func_elt::val_str(String *str)
{
  assert(fixed == 1);
  uint32_t tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return NULL;

  String *result= args[tmp]->val_str(str);
  if (result)
    result->set_charset(collation.collation);
  null_value= args[tmp]->null_value;
  return result;
}

} /* namespace drizzled */
