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

#include <drizzled/function/str/make_set.h>
#include <drizzled/session.h>

namespace drizzled {

void Item_func_make_set::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
}


void Item_func_make_set::split_sum_func(Session *session_arg, Item **ref_pointer_array,
					List<Item> &fields)
{
  item->split_sum_func(session_arg, ref_pointer_array, fields, &item, true);
  Item_str_func::split_sum_func(session_arg, ref_pointer_array, fields);
}


void Item_func_make_set::fix_length_and_dec()
{
  max_length=arg_count-1;

  if (agg_arg_charsets(collation, args, arg_count, MY_COLL_ALLOW_CONV, 1))
    return;

  for (uint32_t i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;

  used_tables_cache|=	  item->used_tables();
  not_null_tables_cache&= item->not_null_tables();
  const_item_cache&=	  item->const_item();
  with_sum_func= with_sum_func || item->with_sum_func;
}

String *Item_func_make_set::val_str(String *str)
{
  assert(fixed == 1);
  uint64_t bits;
  bool first_found=0;
  Item **ptr=args;
  String *result=&my_empty_string;

  bits=item->val_int();
  if ((null_value=item->null_value))
    return NULL;

  if (arg_count < 64)
    bits &= ((uint64_t) 1 << arg_count)-1;

  for (; bits; bits >>= 1, ptr++)
  {
    if (bits & 1)
    {
      String *res= (*ptr)->val_str(str);
      if (res)					// Skip nulls
      {
	if (!first_found)
	{					// First argument
	  first_found=1;
	  if (res != str)
	    result=res;				// Use original string
	  else
	  {
	    tmp_str.copy(*res);
	    result= &tmp_str;
	  }
	}
	else
	{
	  if (result != &tmp_str)
	  {					// Copy data to tmp_str
      tmp_str.alloc(result->length()+res->length()+1);
	    tmp_str.copy(*result);
	    result= &tmp_str;
	  }
	  tmp_str.append(STRING_WITH_LEN(","), &my_charset_bin);
    tmp_str.append(*res);
	}
      }
    }
  }
  return result;
}


Item *Item_func_make_set::transform(Item_transformer transformer, unsigned char *arg)
{
  Item *new_item= item->transform(transformer, arg);
  if (!new_item)
    return 0;
  item= new_item;
  return Item_str_func::transform(transformer, arg);
}


void Item_func_make_set::print(String *str)
{
  str->append(STRING_WITH_LEN("make_set("));
  item->print(str);
  if (arg_count)
  {
    str->append(',');
    print_args(str, 0);
  }
  str->append(')');
}

} /* namespace drizzled */
