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

#include <drizzled/function/str/replace.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/system_variables.h>

namespace drizzled {

/**
  Replace all occurences of string2 in string1 with string3.

  Don't reallocate val_str() if not needed.

  @todo
    Fix that this works with binary strings
*/

String *Item_func_replace::val_str(String *str)
{
  assert(fixed == 1);
  String *res,*res2,*res3;
  int offset;
  uint32_t from_length,to_length;
  bool alloced=0;
  const char *ptr,*end,*strend,*search,*search_end;
  uint32_t l;
  bool binary_cmp;

  null_value=0;
  res=args[0]->val_str(str);
  if (args[0]->null_value)
    goto null;
  res2=args[1]->val_str(&tmp_value);
  if (args[1]->null_value)
    goto null;

  res->set_charset(collation.collation);

  binary_cmp = ((res->charset()->state & MY_CS_BINSORT) || !use_mb(res->charset()));

  if (res2->length() == 0)
    return res;
  offset=0;
  if (binary_cmp && (offset=res->strstr(*res2)) < 0)
    return res;
  if (!(res3=args[2]->val_str(&tmp_value2)))
    goto null;
  from_length= res2->length();
  to_length=   res3->length();

  if (!binary_cmp)
  {
    search=res2->ptr();
    search_end=search+from_length;
redo:
    ptr=res->ptr()+offset;
    strend=res->ptr()+res->length();
    end=strend-from_length+1;
    while (ptr < end)
    {
        if (*ptr == *search)
        {
          char *i,*j;
          i=(char*) ptr+1; j=(char*) search+1;
          while (j != search_end)
            if (*i++ != *j++) goto skip;
          offset= (int) (ptr-res->ptr());
          if (res->length()-from_length + to_length >
              session.variables.max_allowed_packet)
          {
            push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                                ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
                                func_name(),
                                session.variables.max_allowed_packet);

            goto null;
          }
          if (!alloced)
          {
            alloced=1;
            res=copy_if_not_alloced(str,res,res->length()+to_length);
          }
          res->replace((uint) offset,from_length,*res3);
          offset+=(int) to_length;
          goto redo;
        }
skip:
        if ((l=my_ismbchar(res->charset(), ptr,strend))) ptr+=l;
        else ++ptr;
    }
  }
  else
    do
    {
      if (res->length()-from_length + to_length >
          session.variables.max_allowed_packet)
      {
        push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                            ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
                            session.variables.max_allowed_packet);
        goto null;
      }
      if (!alloced)
      {
        alloced=1;
        res=copy_if_not_alloced(str,res,res->length()+to_length);
      }
      res->replace((uint) offset,from_length,*res3);
      offset+=(int) to_length;
    }
    while ((offset=res->strstr(*res2,(uint) offset)) >= 0);
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_replace::fix_length_and_dec()
{
  uint64_t max_result_length= args[0]->max_length;
  int diff=(int) (args[2]->max_length - args[1]->max_length);
  if (diff > 0 && args[1]->max_length)
  {                                             // Calculate of maxreplaces
    uint64_t max_substrs= max_result_length/args[1]->max_length;
    max_result_length+= max_substrs * (uint) diff;
  }
  if (max_result_length >= MAX_BLOB_WIDTH)
  {
    max_result_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
  max_length= (ulong) max_result_length;

  if (agg_arg_charsets(collation, args, 3, MY_COLL_CMP_CONV, 1))
    return;
}


} /* namespace drizzled */
