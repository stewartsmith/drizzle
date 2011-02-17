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

#include <drizzled/function/field.h>
#include <drizzled/item/cmpfunc.h>

// Conversion functions

namespace drizzled
{

int64_t Item_func_field::val_int()
{
  assert(fixed == 1);

  if (cmp_type == STRING_RESULT)
  {
    String *field;
    if (!(field= args[0]->val_str(&value)))
      return 0;
    for (uint32_t i=1 ; i < arg_count ; i++)
    {
      String *tmp_value=args[i]->val_str(&tmp);
      if (tmp_value && !sortcmp(field,tmp_value,cmp_collation.collation))
        return (int64_t) (i);
    }
  }
  else if (cmp_type == INT_RESULT)
  {
    int64_t val= args[0]->val_int();
    if (args[0]->null_value)
      return 0;
    for (uint32_t i=1; i < arg_count ; i++)
    {
      if (val == args[i]->val_int() && !args[i]->null_value)
        return (int64_t) (i);
    }
  }
  else if (cmp_type == DECIMAL_RESULT)
  {
    type::Decimal dec_arg_buf, *dec_arg,
               dec_buf, *dec= args[0]->val_decimal(&dec_buf);
    if (args[0]->null_value)
      return 0;
    for (uint32_t i=1; i < arg_count; i++)
    {
      dec_arg= args[i]->val_decimal(&dec_arg_buf);
      if (!args[i]->null_value && !class_decimal_cmp(dec_arg, dec))
        return (int64_t) (i);
    }
  }
  else
  {
    double val= args[0]->val_real();
    if (args[0]->null_value)
      return 0;
    for (uint32_t i=1; i < arg_count ; i++)
    {
      if (val == args[i]->val_real() && !args[i]->null_value)
        return (int64_t) (i);
    }
  }
  return 0;
}

void Item_func_field::fix_length_and_dec()
{
  maybe_null=0; max_length=3;
  cmp_type= args[0]->result_type();
  for (uint32_t i=1; i < arg_count ; i++)
    cmp_type= item_cmp_type(cmp_type, args[i]->result_type());
  if (cmp_type == STRING_RESULT)
    agg_arg_charsets(cmp_collation, args, arg_count, MY_COLL_CMP_CONV, 1);
}

} /* namespace drizzled */
