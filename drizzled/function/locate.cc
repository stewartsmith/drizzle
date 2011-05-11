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

#include <drizzled/charset.h>
#include <drizzled/function/locate.h>
#include <drizzled/lex_string.h>

namespace drizzled
{

void Item_func_locate::fix_length_and_dec()
{
  max_length= MY_INT32_NUM_DECIMAL_DIGITS;
  agg_arg_charsets(cmp_collation, args, 2, MY_COLL_CMP_CONV, 1);
}

int64_t Item_func_locate::val_int()
{
  assert(fixed == 1);
  String *a=args[0]->val_str(&value1);
  String *b=args[1]->val_str(&value2);
  if (!a || !b)
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  /* must be int64_t to avoid truncation */
  int64_t start=  0;
  int64_t start0= 0;
  my_match_t match;

  if (arg_count == 3)
  {
    start0= start= args[2]->val_int() - 1;

    if ((start < 0) || (start > static_cast<int64_t>(a->length())))
      return 0;

    /* start is now sufficiently valid to pass to charpos function */
    start= a->charpos((int) start);

    if (start + b->length() > a->length())
      return 0;
  }
  if (!b->length())                             // Found empty string at start
    return start + 1;

  if (!cmp_collation.collation->coll->instr(cmp_collation.collation,
                                            a->ptr()+start,
                                            (uint32_t) (a->length()-start),
                                            b->ptr(), b->length(),
                                            &match, 1))
    return 0;
  return (int64_t) match.mb_len + start0 + 1;
}


void Item_func_locate::print(String *str)
{
  str->append(STRING_WITH_LEN("locate("));
  args[1]->print(str);
  str->append(',');
  args[0]->print(str);
  if (arg_count == 3)
  {
    str->append(',');
    args[2]->print(str);
  }
  str->append(')');
}

} /* namespace drizzled */
