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

#include <drizzled/function/str/pad.h>
#include <drizzled/error.h>
#include <drizzled/function/str/alloc_buffer.h>
#include <drizzled/session.h>
#include <drizzled/system_variables.h>

namespace drizzled {

void Item_func_rpad::fix_length_and_dec()
{
  // Handle character set for args[0] and args[2].
  if (agg_arg_charsets(collation, &args[0], 2, MY_COLL_ALLOW_CONV, 2))
    return;
  if (args[1]->const_item())
  {
    uint64_t length= 0;

    if (collation.collation->mbmaxlen > 0)
    {
      uint64_t temp= (uint64_t) args[1]->val_int();

      /* Assumes that the maximum length of a String is < INT32_MAX. */
      /* Set here so that rest of code sees out-of-bound value as such. */
      if (temp > INT32_MAX)
	temp = INT32_MAX;

      length= temp * collation.collation->mbmaxlen;
    }

    if (length >= MAX_BLOB_WIDTH)
    {
      length= MAX_BLOB_WIDTH;
      maybe_null= 1;
    }
    max_length= (ulong) length;
  }
  else
  {
    max_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
}


String *Item_func_rpad::val_str(String *str)
{
  assert(fixed == 1);
  null_value=1;
  uint32_t res_byte_length,res_char_length,pad_char_length,pad_byte_length;
  const char *ptr_pad;
  /* must be int64_t to avoid truncation */
  int64_t count= args[1]->val_int();
  String *res= args[0]->val_str(str);
  String *rpad= args[2]->val_str(&rpad_str);

  if (!res || args[1]->null_value || !rpad || ((count < 0) && !args[1]->unsigned_flag))
    return 0;
  /* Assumes that the maximum length of a String is < INT32_MAX. */
  /* Set here so that rest of code sees out-of-bound value as such. */
  if ((uint64_t) count > INT32_MAX)
    count= INT32_MAX;
  if (count <= (res_char_length= res->numchars()))
  {						// String to pad is big enough
    res->length(res->charpos((int) count));	// Shorten result if longer
    null_value=0;
    return res;
  }
  pad_char_length= rpad->numchars();

  int64_t byte_count= count * collation.collation->mbmaxlen;
  if ((uint64_t) byte_count > session.variables.max_allowed_packet)
  {
    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), session.variables.max_allowed_packet);
    return 0;
  }
  if (args[2]->null_value || !pad_char_length)
    return 0;
  res_byte_length= res->length();	/* Must be done before alloc_buffer */
  res= alloc_buffer(res,str,&tmp_value, (ulong) byte_count);
  char* to= (char*) res->ptr()+res_byte_length;
  ptr_pad=rpad->ptr();
  pad_byte_length= rpad->length();
  count-= res_char_length;
  for ( ; (uint32_t) count > pad_char_length; count-= pad_char_length)
  {
    memcpy(to,ptr_pad,pad_byte_length);
    to+= pad_byte_length;
  }
  if (count)
  {
    pad_byte_length= rpad->charpos((int) count);
    memcpy(to,ptr_pad,(size_t) pad_byte_length);
    to+= pad_byte_length;
  }
  res->length(to- (char*) res->ptr());
  null_value=0;
  return res;
}


void Item_func_lpad::fix_length_and_dec()
{
  // Handle character set for args[0] and args[2].
  if (agg_arg_charsets(collation, &args[0], 2, MY_COLL_ALLOW_CONV, 2))
    return;

  if (args[1]->const_item())
  {
    uint64_t length= 0;

    if (collation.collation->mbmaxlen > 0)
    {
      uint64_t temp= (uint64_t) args[1]->val_int();

      /* Assumes that the maximum length of a String is < INT32_MAX. */
      /* Set here so that rest of code sees out-of-bound value as such. */
      if (temp > INT32_MAX)
        temp= INT32_MAX;

      length= temp * collation.collation->mbmaxlen;
    }

    if (length >= MAX_BLOB_WIDTH)
    {
      length= MAX_BLOB_WIDTH;
      maybe_null= 1;
    }
    max_length= (ulong) length;
  }
  else
  {
    max_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
}


String *Item_func_lpad::val_str(String *str)
{
  assert(fixed == 1);
  uint32_t res_char_length,pad_char_length;
  /* must be int64_t to avoid truncation */
  int64_t count= args[1]->val_int();
  int64_t byte_count;
  String *res= args[0]->val_str(&tmp_value);
  String *pad= args[2]->val_str(&lpad_str);

  if (!res || args[1]->null_value || !pad ||
      ((count < 0) && !args[1]->unsigned_flag))
    goto err;
  null_value=0;
  /* Assumes that the maximum length of a String is < INT32_MAX. */
  /* Set here so that rest of code sees out-of-bound value as such. */
  if ((uint64_t) count > INT32_MAX)
    count= INT32_MAX;

  res_char_length= res->numchars();

  if (count <= res_char_length)
  {
    res->length(res->charpos((int) count));
    return res;
  }

  pad_char_length= pad->numchars();
  byte_count= count * collation.collation->mbmaxlen;

  if ((uint64_t) byte_count > session.variables.max_allowed_packet)
  {
    push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), session.variables.max_allowed_packet);
    goto err;
  }

  if (args[2]->null_value || !pad_char_length)
    goto err;
  str->alloc((size_t) byte_count);
  str->length(0);
  str->set_charset(collation.collation);
  count-= res_char_length;
  while (count >= pad_char_length)
  {
    str->append(*pad);
    count-= pad_char_length;
  }
  if (count > 0)
    str->append(pad->ptr(), pad->charpos((int) count), collation.collation);

  str->append(*res);
  null_value= 0;
  return str;

err:
  null_value= 1;
  return 0;
}

} /* namespace drizzled */
