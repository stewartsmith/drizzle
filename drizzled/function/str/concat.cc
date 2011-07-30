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

#include <drizzled/function/str/concat.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/system_variables.h>

#include <algorithm>

using namespace std;

namespace drizzled {

String *Item_func_concat::val_str(String *str)
{
  assert(fixed == 1);
  String *res,*res2,*use_as_buff;
  uint32_t i;
  bool is_const= 0;

  null_value=0;
  if (!(res=args[0]->val_str(str)))
    goto null;
  use_as_buff= &tmp_value;
  /* Item_subselect in --ps-protocol mode will state it as a non-const */
  is_const= args[0]->const_item() || !args[0]->used_tables();
  for (i=1 ; i < arg_count ; i++)
  {
    if (res->length() == 0)
    {
      if (!(res=args[i]->val_str(str)))
        goto null;
    }
    else
    {
      if (!(res2=args[i]->val_str(use_as_buff)))
        goto null;
      if (res2->length() == 0)
        continue;
      if (res->length()+res2->length() >
          session.variables.max_allowed_packet)
      {
        push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                            ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                            ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
                            session.variables.max_allowed_packet);
        goto null;
      }
      if (!is_const && res->alloced_length() >= res->length()+res2->length())
      {						// Use old buffer
        res->append(*res2);
      }
      else if (str->alloced_length() >= res->length()+res2->length())
      {
        if (str == res2)
          str->replace(0,0,*res);
        else
        {
          str->copy(*res);
          str->append(*res2);
        }
        res= str;
        use_as_buff= &tmp_value;
      }
      else if (res == &tmp_value)
      {
        res->append(*res2);
      }
      else if (res2 == &tmp_value)
      {						// This can happend only 1 time
        tmp_value.replace(0,0,*res);
        res= &tmp_value;
        use_as_buff=str;			// Put next arg here
      }
      else if (tmp_value.is_alloced() && res2->ptr() >= tmp_value.ptr() &&
               res2->ptr() <= tmp_value.ptr() + tmp_value.alloced_length())
      {
        /*
          This happens really seldom:
          In this case res2 is sub string of tmp_value.  We will
          now work in place in tmp_value to set it to res | res2
        */
        /* Chop the last characters in tmp_value that isn't in res2 */
        tmp_value.length((uint32_t) (res2->ptr() - tmp_value.ptr()) +
                         res2->length());
        /* Place res2 at start of tmp_value, remove chars before res2 */
        tmp_value.replace(0,(uint32_t) (res2->ptr() - tmp_value.ptr()), *res);
        res= &tmp_value;
        use_as_buff=str;			// Put next arg here
      }
      else
      {						// Two big const strings
        /*
          @note We should be prudent in the initial allocation unit -- the
          size of the arguments is a function of data distribution, which
          can be any. Instead of overcommitting at the first row, we grow
          the allocated amount by the factor of 2. This ensures that no
          more than 25% of memory will be overcommitted on average.
        */

        size_t concat_len= res->length() + res2->length();

        if (tmp_value.alloced_length() < concat_len)
        {
          if (tmp_value.alloced_length() == 0)
          {
            tmp_value.alloc(concat_len);
          }
          else
          {
            uint32_t new_len= max(tmp_value.alloced_length() * 2, concat_len);

            tmp_value.realloc(new_len);
          }
        }

        tmp_value.copy(*res);
        tmp_value.append(*res2);

        res= &tmp_value;
        use_as_buff=str;
      }
      is_const= 0;
    }
  }
  res->set_charset(collation.collation);
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_concat::fix_length_and_dec()
{
  uint64_t max_result_length= 0;

  if (agg_arg_charsets(collation, args, arg_count, MY_COLL_ALLOW_CONV, 1))
    return;

  for (uint32_t i=0 ; i < arg_count ; i++)
  {
    if (args[i]->collation.collation->mbmaxlen != collation.collation->mbmaxlen)
      max_result_length+= (args[i]->max_length /
                           args[i]->collation.collation->mbmaxlen) *
        collation.collation->mbmaxlen;
    else
      max_result_length+= args[i]->max_length;
  }

  if (max_result_length >= MAX_BLOB_WIDTH)
  {
    max_result_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
  max_length= (ulong) max_result_length;
}


/**
  concat with separator. First arg is the separator
  concat_ws takes at least two arguments.
*/

String *Item_func_concat_ws::val_str(String *str)
{
  assert(fixed == 1);
  char tmp_str_buff[10];
  String tmp_sep_str(tmp_str_buff, sizeof(tmp_str_buff),default_charset_info),
         *sep_str, *res, *res2,*use_as_buff;
  uint32_t i;

  null_value=0;
  if (!(sep_str= args[0]->val_str(&tmp_sep_str)))
    goto null;

  use_as_buff= &tmp_value;
  str->length(0);				// QQ; Should be removed
  res=str;

  // Skip until non-null argument is found.
  // If not, return the empty string
  for (i=1; i < arg_count; i++)
    if ((res= args[i]->val_str(str)))
      break;
  if (i ==  arg_count)
    return &my_empty_string;

  for (i++; i < arg_count ; i++)
  {
    if (!(res2= args[i]->val_str(use_as_buff)))
      continue;					// Skip NULL

    if (res->length() + sep_str->length() + res2->length() >
        session.variables.max_allowed_packet)
    {
      push_warning_printf(&session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                          ER_WARN_ALLOWED_PACKET_OVERFLOWED,
                          ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
                          session.variables.max_allowed_packet);
      goto null;
    }
    if (res->alloced_length() >=
        res->length() + sep_str->length() + res2->length())
    {						// Use old buffer
      res->append(*sep_str);			// res->length() > 0 always
      res->append(*res2);
    }
    else if (str->alloced_length() >=
             res->length() + sep_str->length() + res2->length())
    {
      /* We have room in str;  We can't get any errors here */
      if (str == res2)
      {						// This is quote uncommon!
        str->replace(0,0,*sep_str);
        str->replace(0,0,*res);
      }
      else
      {
        str->copy(*res);
        str->append(*sep_str);
        str->append(*res2);
      }
      res=str;
      use_as_buff= &tmp_value;
    }
    else if (res == &tmp_value)
    {
      res->append(*sep_str);
      res->append(*res2);
    }
    else if (res2 == &tmp_value)
    {						// This can happend only 1 time
      tmp_value.replace(0,0,*sep_str);
      tmp_value.replace(0,0,*res);
      res= &tmp_value;
      use_as_buff=str;				// Put next arg here
    }
    else if (tmp_value.is_alloced() && res2->ptr() >= tmp_value.ptr() &&
             res2->ptr() < tmp_value.ptr() + tmp_value.alloced_length())
    {
      /*
        This happens really seldom:
        In this case res2 is sub string of tmp_value.  We will
        now work in place in tmp_value to set it to res | sep_str | res2
      */
      /* Chop the last characters in tmp_value that isn't in res2 */
      tmp_value.length((uint32_t) (res2->ptr() - tmp_value.ptr()) +
                       res2->length());
      /* Place res2 at start of tmp_value, remove chars before res2 */
      tmp_value.replace(0,(uint32_t) (res2->ptr() - tmp_value.ptr()), *res);
      tmp_value.replace(res->length(),0, *sep_str);
      res= &tmp_value;
      use_as_buff=str;			// Put next arg here
    }
    else
    {						// Two big const strings
      /*
        @note We should be prudent in the initial allocation unit -- the
        size of the arguments is a function of data distribution, which can
        be any. Instead of overcommitting at the first row, we grow the
        allocated amount by the factor of 2. This ensures that no more than
        25% of memory will be overcommitted on average.
      */

      size_t concat_len= res->length() + sep_str->length() + res2->length();

      if (tmp_value.alloced_length() < concat_len)
      {
        if (tmp_value.alloced_length() == 0)
        {
          tmp_value.alloc(concat_len);
        }
        else
        {
          uint32_t new_len= max(tmp_value.alloced_length() * 2, concat_len);

          tmp_value.realloc(new_len);
        }
      }

      tmp_value.copy(*res);
      tmp_value.append(*sep_str);
      tmp_value.append(*res2);
      res= &tmp_value;
      use_as_buff=str;
    }
  }
  res->set_charset(collation.collation);
  return res;

null:
  null_value=1;
  return 0;
}


void Item_func_concat_ws::fix_length_and_dec()
{
  uint64_t max_result_length;

  if (agg_arg_charsets(collation, args, arg_count, MY_COLL_ALLOW_CONV, 1))
    return;

  /*
    arg_count cannot be less than 2,
    it is done on parser level in sql_yacc.yy
    so, (arg_count - 2) is safe here.
  */
  max_result_length= (uint64_t) args[0]->max_length * (arg_count - 2);
  for (uint32_t i=1 ; i < arg_count ; i++)
    max_result_length+=args[i]->max_length;

  if (max_result_length >= MAX_BLOB_WIDTH)
  {
    max_result_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
  max_length= (ulong) max_result_length;
}

} /* namespace drizzled */
