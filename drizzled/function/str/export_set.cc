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

#include <drizzled/function/str/export_set.h>

#include <algorithm>

using namespace std;

namespace drizzled {

String* Item_func_export_set::val_str(String* str)
{
  assert(fixed == 1);
  uint64_t the_set = (uint64_t) args[0]->val_int();
  String yes_buf, *yes;
  yes = args[1]->val_str(&yes_buf);
  String no_buf, *no;
  no = args[2]->val_str(&no_buf);
  String *sep = NULL, sep_buf ;

  uint32_t num_set_values = 64;
  uint64_t mask = 0x1;
  str->length(0);
  str->set_charset(collation.collation);

  /* Check if some argument is a NULL value */
  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {
    null_value=1;
    return 0;
  }
  /*
    Arg count can only be 3, 4 or 5 here. This is guaranteed from the
    grammar for EXPORT_SET()
  */
  switch(arg_count) {
  case 5:
    num_set_values = (uint) args[4]->val_int();
    if (num_set_values > 64)
      num_set_values=64;
    if (args[4]->null_value)
    {
      null_value=1;
      return 0;
    }
    /* Fall through */
  case 4:
    if (!(sep = args[3]->val_str(&sep_buf)))	// Only true if NULL
    {
      null_value=1;
      return 0;
    }
    break;
  case 3:
    {
      sep_buf.copy(STRING_WITH_LEN(","), collation.collation);
      sep = &sep_buf;
    }
    break;
  default:
    assert(0); // cannot happen
  }
  null_value=0;

  for (uint32_t i = 0; i < num_set_values; i++, mask = (mask << 1))
  {
    if (the_set & mask)
      str->append(*yes);
    else
      str->append(*no);
    if (i != num_set_values - 1)
      str->append(*sep);
  }
  return str;
}

void Item_func_export_set::fix_length_and_dec()
{
  uint32_t length= max(args[1]->max_length,args[2]->max_length);
  uint32_t sep_length= (arg_count > 3 ? args[3]->max_length : 1);
  max_length= length*64+sep_length*63;

  if (agg_arg_charsets(collation, args+1, min(4U,arg_count)-1,
                       MY_COLL_ALLOW_CONV, 1))
    return;
}

} /* namespace drizzled */
