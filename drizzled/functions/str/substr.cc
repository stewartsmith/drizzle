/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include <drizzled/server_includes.h>
#include CSTDINT_H
#include <drizzled/functions/str/substr.h>

String *Item_func_substr::val_str(String *str)
{
  assert(fixed == 1);
  String *res  = args[0]->val_str(str);
  /* must be int64_t to avoid truncation */
  int64_t start= args[1]->val_int();
  /* Assumes that the maximum length of a String is < INT32_MAX. */
  /* Limit so that code sees out-of-bound value properly. */
  int64_t length= arg_count == 3 ? args[2]->val_int() : INT32_MAX;
  int64_t tmp_length;

  if ((null_value=(args[0]->null_value || args[1]->null_value ||
		   (arg_count == 3 && args[2]->null_value))))
    return 0; /* purecov: inspected */

  /* Negative or zero length, will return empty string. */
  if ((arg_count == 3) && (length <= 0) && 
      (length == 0 || !args[2]->unsigned_flag))
    return &my_empty_string;

  /* Assumes that the maximum length of a String is < INT32_MAX. */
  /* Set here so that rest of code sees out-of-bound value as such. */
  if ((length <= 0) || (length > INT32_MAX))
    length= INT32_MAX;

  /* if "unsigned_flag" is set, we have a *huge* positive number. */
  /* Assumes that the maximum length of a String is < INT32_MAX. */
  if ((!args[1]->unsigned_flag && (start < INT32_MIN || start > INT32_MAX)) ||
      (args[1]->unsigned_flag && ((uint64_t) start > INT32_MAX)))
    return &my_empty_string;

  start= ((start < 0) ? res->numchars() + start : start - 1);
  start= res->charpos((int) start);
  if ((start < 0) || ((uint) start + 1 > res->length()))
    return &my_empty_string;

  length= res->charpos((int) length, (uint32_t) start);
  tmp_length= res->length() - start;
  length= cmin(length, tmp_length);

  if (!start && (int64_t) res->length() == length)
    return res;
  tmp_value.set(*res, (uint32_t) start, (uint32_t) length);
  return &tmp_value;
}

void Item_func_substr::fix_length_and_dec()
{
  max_length=args[0]->max_length;

  collation.set(args[0]->collation);
  if (args[1]->const_item())
  {
    int32_t start= (int32_t) args[1]->val_int();
    if (start < 0)
      max_length= ((uint)(-start) > max_length) ? 0 : (uint)(-start);
    else
      max_length-= cmin((uint)(start - 1), max_length);
  }
  if (arg_count == 3 && args[2]->const_item())
  {
    int32_t length= (int32_t) args[2]->val_int();
    if (length <= 0)
      max_length=0; /* purecov: inspected */
    else
      set_if_smaller(max_length,(uint) length);
  }
  max_length*= collation.collation->mbmaxlen;
}


void Item_func_substr_index::fix_length_and_dec()
{ 
  max_length= args[0]->max_length;

  if (agg_arg_charsets(collation, args, 2, MY_COLL_CMP_CONV, 1))
    return;
}


String *Item_func_substr_index::val_str(String *str)
{
  assert(fixed == 1);
  String *res= args[0]->val_str(str);
  String *delimiter= args[1]->val_str(&tmp_value);
  int32_t count= (int32_t) args[2]->val_int();
  uint32_t offset;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {					// string and/or delim are null
    null_value=1;
    return 0;
  }
  null_value=0;
  uint32_t delimiter_length= delimiter->length();
  if (!res->length() || !delimiter_length || !count)
    return &my_empty_string;		// Wrong parameters

  res->set_charset(collation.collation);

#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    const char *ptr= res->ptr();
    const char *strend= ptr+res->length();
    const char *end= strend-delimiter_length+1;
    const char *search= delimiter->ptr();
    const char *search_end= search+delimiter_length;
    int32_t n=0,c=count,pass;
    register uint32_t l;
    for (pass=(count>0);pass<2;++pass)
    {
      while (ptr < end)
      {
        if (*ptr == *search)
        {
	  register char *i,*j;
	  i=(char*) ptr+1; j=(char*) search+1;
	  while (j != search_end)
	    if (*i++ != *j++) goto skip;
	  if (pass==0) ++n;
	  else if (!--c) break;
	  ptr+= delimiter_length;
	  continue;
	}
    skip:
        if ((l=my_ismbchar(res->charset(), ptr,strend))) ptr+=l;
        else ++ptr;
      } /* either not found or got total number when count<0 */
      if (pass == 0) /* count<0 */
      {
        c+=n+1;
        if (c<=0) return res; /* not found, return original string */
        ptr=res->ptr();
      }
      else
      {
        if (c) return res; /* Not found, return original string */
        if (count>0) /* return left part */
        {
	  tmp_value.set(*res,0,(ulong) (ptr-res->ptr()));
        }
        else /* return right part */
        {
	  ptr+= delimiter_length;
	  tmp_value.set(*res,(ulong) (ptr-res->ptr()), (ulong) (strend-ptr));
        }
      }
    }
  }
  else
#endif /* USE_MB */
  {
    if (count > 0)
    {					// start counting from the beginning
      for (offset=0; ; offset+= delimiter_length)
      {
	if ((int) (offset= res->strstr(*delimiter, offset)) < 0)
	  return res;			// Didn't find, return org string
	if (!--count)
	{
	  tmp_value.set(*res,0,offset);
	  break;
	}
      }
    }
    else
    {
      /*
        Negative index, start counting at the end
      */
      for (offset=res->length(); offset ;)
      {
        /* 
          this call will result in finding the position pointing to one 
          address space less than where the found substring is located
          in res
        */
	if ((int) (offset= res->strrstr(*delimiter, offset)) < 0)
	  return res;			// Didn't find, return org string
        /*
          At this point, we've searched for the substring
          the number of times as supplied by the index value
        */
	if (!++count)
	{
	  offset+= delimiter_length;
	  tmp_value.set(*res,offset,res->length()- offset);
	  break;
	}
      }
    }
  }
  /*
    We always mark tmp_value as const so that if val_str() is called again
    on this object, we don't disrupt the contents of tmp_value when it was
    derived from another String.
  */
  tmp_value.mark_as_const();
  return (&tmp_value);
}
