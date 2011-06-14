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

#include <drizzled/function/str/insert.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/system_variables.h>

namespace drizzled {

String *Item_func_insert::val_str(String *str)
{
  assert(fixed == 1);
  String *res,*res2;
  int64_t start, length;  /* must be int64_t to avoid truncation */

  null_value=0;
  res=args[0]->val_str(str);
  res2=args[3]->val_str(&tmp_value);
  start= args[1]->val_int() - 1;
  length= args[2]->val_int();

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      args[3]->null_value)
    goto null;

  if ((start < 0) || (start > static_cast<int64_t>(res->length())))
    return res;                                 // Wrong param; skip insert
  if ((length < 0) || (length > static_cast<int64_t>(res->length())))
    length= res->length();

  /* start and length are now sufficiently valid to pass to charpos function */
   start= res->charpos((int) start);
   length= res->charpos((int) length, (uint32_t) start);

  /* Re-testing with corrected params */
  if (start > static_cast<int64_t>(res->length()))
    return res;
  if (length > static_cast<int64_t>(res->length()) - start)
    length= res->length() - start;

  if ((uint64_t) (res->length() - length + res2->length()) >
      (uint64_t) session.variables.max_allowed_packet)
  {
    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), session.variables.max_allowed_packet);
    goto null;
  }
  res=copy_if_not_alloced(str,res,res->length());
  res->replace((uint32_t) start,(uint32_t) length,*res2);
  return res;
null:
  null_value=1;
  return 0;
}

void Item_func_insert::fix_length_and_dec()
{
    // Handle character set for args[0] and args[3].
  if (agg_arg_charsets(collation, &args[0], 2, MY_COLL_ALLOW_CONV, 3))
    return;
  uint64_t max_result_length= ((uint64_t) args[0]->max_length + (uint64_t) args[3]->max_length);
  if (max_result_length >= MAX_BLOB_WIDTH)
  {
    max_result_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
  max_length= (ulong) max_result_length;
}

} /* namespace drizzled */
