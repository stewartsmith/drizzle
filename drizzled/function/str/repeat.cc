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

#include <drizzled/function/str/repeat.h>
#include <drizzled/error.h>
#include <drizzled/function/str/alloc_buffer.h>
#include <drizzled/session.h>
#include <drizzled/system_variables.h>

namespace drizzled {

void Item_func_repeat::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  if (args[1]->const_item())
  {
    /* must be int64_t to avoid truncation */
    int64_t count= args[1]->val_int();

    /* Assumes that the maximum length of a String is < INT32_MAX. */
    /* Set here so that rest of code sees out-of-bound value as such. */
    if (count > INT32_MAX)
      count= INT32_MAX;

    uint64_t max_result_length= (uint64_t) args[0]->max_length * count;
    if (max_result_length >= MAX_BLOB_WIDTH)
    {
      max_result_length= MAX_BLOB_WIDTH;
      maybe_null= 1;
    }
    max_length= (ulong) max_result_length;
  }
  else
  {
    max_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
}

/**
  Item_func_repeat::str is carefully written to avoid reallocs
  as much as possible at the cost of a local buffer
*/

String *Item_func_repeat::val_str(String *str)
{
  assert(fixed == 1);
  uint32_t length,tot_length;
  char *to;
  /* must be int64_t to avoid truncation */
  int64_t count= args[1]->val_int();
  String *res= args[0]->val_str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;                           // string and/or delim are null
  null_value= 0;

  if (count <= 0 && (count == 0 || !args[1]->unsigned_flag))
    return &my_empty_string;

  /* Assumes that the maximum length of a String is < INT32_MAX. */
  /* Bounds check on count:  If this is triggered, we will error. */
  if ((uint64_t) count > INT32_MAX)
    count= INT32_MAX;
  if (count == 1)                       // To avoid reallocs
    return res;
  length=res->length();
  // Safe length check
  if (length > session.variables.max_allowed_packet / (uint) count)
  {
    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                        ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
                        func_name(), session.variables.max_allowed_packet);
    goto err;
  }
  tot_length= length*(uint) count;
  if (!(res= alloc_buffer(res,str,&tmp_value,tot_length)))
    goto err;

  to=(char*) res->ptr()+length;
  while (--count)
  {
    memcpy(to,res->ptr(),length);
    to+=length;
  }
  return (res);

err:
  null_value=1;
  return 0;
}

} /* namespace drizzled */
