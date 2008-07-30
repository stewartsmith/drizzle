/* Copyright (C) 2000-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/**
  @file

  @brief
  This file defines all string functions

  @warning
    Some string functions don't always put and end-null on a String.
    (This shouldn't be needed)
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif


#include "mysql_priv.h"
#include <m_ctype.h>
#include <mysys/sha1.h>
#include <zlib.h>
C_MODE_START
#include <mysys/my_static.h>			// For soundex_map
C_MODE_END

String my_empty_string("",default_charset_info);




bool Item_str_func::fix_fields(THD *thd, Item **ref)
{
  bool res= Item_func::fix_fields(thd, ref);
  /*
    In Item_str_func::check_well_formed_result() we may set null_value
    flag on the same condition as in test() below.
  */
  maybe_null= (maybe_null ||
               test(thd->variables.sql_mode &
                    (MODE_STRICT_TRANS_TABLES | MODE_STRICT_ALL_TABLES)));
  return res;
}


my_decimal *Item_str_func::val_decimal(my_decimal *decimal_value)
{
  assert(fixed == 1);
  char buff[64];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  if (!res)
    return 0;
  (void)str2my_decimal(E_DEC_FATAL_ERROR, (char*) res->ptr(),
                       res->length(), res->charset(), decimal_value);
  return decimal_value;
}


double Item_str_func::val_real()
{
  assert(fixed == 1);
  int err_not_used;
  char *end_not_used, buff[64];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  return res ? my_strntod(res->charset(), (char*) res->ptr(), res->length(),
			  &end_not_used, &err_not_used) : 0.0;
}


int64_t Item_str_func::val_int()
{
  assert(fixed == 1);
  int err;
  char buff[22];
  String *res, tmp(buff,sizeof(buff), &my_charset_bin);
  res= val_str(&tmp);
  return (res ?
	  my_strntoll(res->charset(), res->ptr(), res->length(), 10, NULL,
		      &err) :
	  (int64_t) 0);
}

/**
  Concatenate args with the following premises:
  If only one arg (which is ok), return value of arg;
  Don't reallocate val_str() if not absolute necessary.
*/

String *Item_func_concat::val_str(String *str)
{
  assert(fixed == 1);
  String *res,*res2,*use_as_buff;
  uint i;
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
	  current_thd->variables.max_allowed_packet)
      {
	push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			    ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			    ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
			    current_thd->variables.max_allowed_packet);
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
	if (res->append(*res2))			// Must be a blob
	  goto null;
      }
      else if (res2 == &tmp_value)
      {						// This can happend only 1 time
	if (tmp_value.replace(0,0,*res))
	  goto null;
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
	if (tmp_value.replace(0,(uint32_t) (res2->ptr() - tmp_value.ptr()),
			      *res))
	  goto null;
	res= &tmp_value;
	use_as_buff=str;			// Put next arg here
      }
      else
      {						// Two big const strings
        /*
          NOTE: We should be prudent in the initial allocation unit -- the
          size of the arguments is a function of data distribution, which
          can be any. Instead of overcommitting at the first row, we grow
          the allocated amount by the factor of 2. This ensures that no
          more than 25% of memory will be overcommitted on average.
        */

        uint concat_len= res->length() + res2->length();

        if (tmp_value.alloced_length() < concat_len)
        {
          if (tmp_value.alloced_length() == 0)
          {
            if (tmp_value.alloc(concat_len))
              goto null;
          }
          else
          {
            uint new_len = max(tmp_value.alloced_length() * 2, concat_len);

            if (tmp_value.realloc(new_len))
              goto null;
          }
        }

	if (tmp_value.copy(*res) || tmp_value.append(*res2))
	  goto null;

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

  for (uint i=0 ; i < arg_count ; i++)
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
  uint i;

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
	current_thd->variables.max_allowed_packet)
    {
      push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			  ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			  ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
			  current_thd->variables.max_allowed_packet);
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
      if (res->append(*sep_str) || res->append(*res2))
	goto null; // Must be a blob
    }
    else if (res2 == &tmp_value)
    {						// This can happend only 1 time
      if (tmp_value.replace(0,0,*sep_str) || tmp_value.replace(0,0,*res))
	goto null;
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
      if (tmp_value.replace(0,(uint32_t) (res2->ptr() - tmp_value.ptr()),
			    *res) ||
	  tmp_value.replace(res->length(),0, *sep_str))
	goto null;
      res= &tmp_value;
      use_as_buff=str;			// Put next arg here
    }
    else
    {						// Two big const strings
      /*
        NOTE: We should be prudent in the initial allocation unit -- the
        size of the arguments is a function of data distribution, which can
        be any. Instead of overcommitting at the first row, we grow the
        allocated amount by the factor of 2. This ensures that no more than
        25% of memory will be overcommitted on average.
      */

      uint concat_len= res->length() + sep_str->length() + res2->length();

      if (tmp_value.alloced_length() < concat_len)
      {
        if (tmp_value.alloced_length() == 0)
        {
          if (tmp_value.alloc(concat_len))
            goto null;
        }
        else
        {
          uint new_len = max(tmp_value.alloced_length() * 2, concat_len);

          if (tmp_value.realloc(new_len))
            goto null;
        }
      }

      if (tmp_value.copy(*res) ||
	  tmp_value.append(*sep_str) ||
	  tmp_value.append(*res2))
	goto null;
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
  for (uint i=1 ; i < arg_count ; i++)
    max_result_length+=args[i]->max_length;

  if (max_result_length >= MAX_BLOB_WIDTH)
  {
    max_result_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
  max_length= (ulong) max_result_length;
}


String *Item_func_reverse::val_str(String *str)
{
  assert(fixed == 1);
  String *res = args[0]->val_str(str);
  char *ptr, *end, *tmp;

  if ((null_value=args[0]->null_value))
    return 0;
  /* An empty string is a special case as the string pointer may be null */
  if (!res->length())
    return &my_empty_string;
  if (tmp_value.alloced_length() < res->length() &&
      tmp_value.realloc(res->length()))
  {
    null_value= 1;
    return 0;
  }
  tmp_value.length(res->length());
  tmp_value.set_charset(res->charset());
  ptr= (char *) res->ptr();
  end= ptr + res->length();
  tmp= (char *) tmp_value.ptr() + tmp_value.length();
#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    register uint32_t l;
    while (ptr < end)
    {
      if ((l= my_ismbchar(res->charset(),ptr,end)))
      {
        tmp-= l;
        memcpy(tmp,ptr,l);
        ptr+= l;
      }
      else
        *--tmp= *ptr++;
    }
  }
  else
#endif /* USE_MB */
  {
    while (ptr < end)
      *--tmp= *ptr++;
  }
  return &tmp_value;
}


void Item_func_reverse::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  max_length = args[0]->max_length;
}

/**
  Replace all occurences of string2 in string1 with string3.

  Don't reallocate val_str() if not needed.

  @todo
    Fix that this works with binary strings when using USE_MB 
*/

String *Item_func_replace::val_str(String *str)
{
  assert(fixed == 1);
  String *res,*res2,*res3;
  int offset;
  uint from_length,to_length;
  bool alloced=0;
#ifdef USE_MB
  const char *ptr,*end,*strend,*search,*search_end;
  register uint32_t l;
  bool binary_cmp;
#endif

  null_value=0;
  res=args[0]->val_str(str);
  if (args[0]->null_value)
    goto null;
  res2=args[1]->val_str(&tmp_value);
  if (args[1]->null_value)
    goto null;

  res->set_charset(collation.collation);

#ifdef USE_MB
  binary_cmp = ((res->charset()->state & MY_CS_BINSORT) || !use_mb(res->charset()));
#endif

  if (res2->length() == 0)
    return res;
#ifndef USE_MB
  if ((offset=res->strstr(*res2)) < 0)
    return res;
#else
  offset=0;
  if (binary_cmp && (offset=res->strstr(*res2)) < 0)
    return res;
#endif
  if (!(res3=args[2]->val_str(&tmp_value2)))
    goto null;
  from_length= res2->length();
  to_length=   res3->length();

#ifdef USE_MB
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
          register char *i,*j;
          i=(char*) ptr+1; j=(char*) search+1;
          while (j != search_end)
            if (*i++ != *j++) goto skip;
          offset= (int) (ptr-res->ptr());
          if (res->length()-from_length + to_length >
	      current_thd->variables.max_allowed_packet)
	  {
	    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
				ER_WARN_ALLOWED_PACKET_OVERFLOWED,
				ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
				func_name(),
				current_thd->variables.max_allowed_packet);

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
#endif /* USE_MB */
    do
    {
      if (res->length()-from_length + to_length >
	  current_thd->variables.max_allowed_packet)
      {
	push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			    ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			    ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED), func_name(),
			    current_thd->variables.max_allowed_packet);
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
  {						// Calculate of maxreplaces
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
    goto null; /* purecov: inspected */

  if ((start < 0) || (start > res->length()))
    return res;                                 // Wrong param; skip insert
  if ((length < 0) || (length > res->length()))
    length= res->length();

  /* start and length are now sufficiently valid to pass to charpos function */
   start= res->charpos((int) start);
   length= res->charpos((int) length, (uint32_t) start);

  /* Re-testing with corrected params */
  if (start > res->length())
    return res; /* purecov: inspected */        // Wrong param; skip insert
  if (length > res->length() - start)
    length= res->length() - start;

  if ((uint64_t) (res->length() - length + res2->length()) >
      (uint64_t) current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
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
  uint64_t max_result_length;

  // Handle character set for args[0] and args[3].
  if (agg_arg_charsets(collation, &args[0], 2, MY_COLL_ALLOW_CONV, 3))
    return;
  max_result_length= ((uint64_t) args[0]->max_length+
                      (uint64_t) args[3]->max_length);
  if (max_result_length >= MAX_BLOB_WIDTH)
  {
    max_result_length= MAX_BLOB_WIDTH;
    maybe_null= 1;
  }
  max_length= (ulong) max_result_length;
}


String *Item_str_conv::val_str(String *str)
{
  assert(fixed == 1);
  String *res;
  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }
  null_value=0;
  if (multiply == 1)
  {
    uint len;
    res= copy_if_not_alloced(str,res,res->length());
    len= converter(collation.collation, (char*) res->ptr(), res->length(),
                                        (char*) res->ptr(), res->length());
    assert(len <= res->length());
    res->length(len);
  }
  else
  {
    uint len= res->length() * multiply;
    tmp_value.alloc(len);
    tmp_value.set_charset(collation.collation);
    len= converter(collation.collation, (char*) res->ptr(), res->length(),
                                        (char*) tmp_value.ptr(), len);
    tmp_value.length(len);
    res= &tmp_value;
  }
  return res;
}


void Item_func_lcase::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  multiply= collation.collation->casedn_multiply;
  converter= collation.collation->cset->casedn;
  max_length= args[0]->max_length * multiply;
}

void Item_func_ucase::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  multiply= collation.collation->caseup_multiply;
  converter= collation.collation->cset->caseup;
  max_length= args[0]->max_length * multiply;
}


String *Item_func_left::val_str(String *str)
{
  assert(fixed == 1);
  String *res= args[0]->val_str(str);

  /* must be int64_t to avoid truncation */
  int64_t length= args[1]->val_int();
  uint char_pos;

  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0;

  /* if "unsigned_flag" is set, we have a *huge* positive number. */
  if ((length <= 0) && (!args[1]->unsigned_flag))
    return &my_empty_string;

  if ((res->length() <= (uint64_t) length) ||
      (res->length() <= (char_pos= res->charpos((int) length))))
    return res;

  tmp_value.set(*res, 0, char_pos);
  return &tmp_value;
}


void Item_str_func::left_right_max_length()
{
  max_length=args[0]->max_length;
  if (args[1]->const_item())
  {
    int length=(int) args[1]->val_int()*collation.collation->mbmaxlen;
    if (length <= 0)
      max_length=0;
    else
      set_if_smaller(max_length,(uint) length);
  }
}


void Item_func_left::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  left_right_max_length();
}


String *Item_func_right::val_str(String *str)
{
  assert(fixed == 1);
  String *res= args[0]->val_str(str);
  /* must be int64_t to avoid truncation */
  int64_t length= args[1]->val_int();

  if ((null_value=(args[0]->null_value || args[1]->null_value)))
    return 0; /* purecov: inspected */

  /* if "unsigned_flag" is set, we have a *huge* positive number. */
  if ((length <= 0) && (!args[1]->unsigned_flag))
    return &my_empty_string; /* purecov: inspected */

  if (res->length() <= (uint64_t) length)
    return res; /* purecov: inspected */

  uint start=res->numchars();
  if (start <= (uint) length)
    return res;
  start=res->charpos(start - (uint) length);
  tmp_value.set(*res,start,res->length()-start);
  return &tmp_value;
}


void Item_func_right::fix_length_and_dec()
{
  collation.set(args[0]->collation);
  left_right_max_length();
}


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
  length= min(length, tmp_length);

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
      max_length-= min((uint)(start - 1), max_length);
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
  uint offset;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value)
  {					// string and/or delim are null
    null_value=1;
    return 0;
  }
  null_value=0;
  uint delimiter_length= delimiter->length();
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

/*
** The trim functions are extension to ANSI SQL because they trim substrings
** They ltrim() and rtrim() functions are optimized for 1 byte strings
** They also return the original string if possible, else they return
** a substring that points at the original string.
*/


String *Item_func_ltrim::val_str(String *str)
{
  assert(fixed == 1);
  char buff[MAX_FIELD_WIDTH], *ptr, *end;
  String tmp(buff,sizeof(buff),system_charset_info);
  String *res, *remove_str;
  uint remove_length;

  res= args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  remove_str= &remove;                          /* Default value. */
  if (arg_count == 2)
  {
    remove_str= args[1]->val_str(&tmp);
    if ((null_value= args[1]->null_value))
      return 0;
  }

  if ((remove_length= remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  ptr= (char*) res->ptr();
  end= ptr+res->length();
  if (remove_length == 1)
  {
    char chr=(*remove_str)[0];
    while (ptr != end && *ptr == chr)
      ptr++;
  }
  else
  {
    const char *r_ptr=remove_str->ptr();
    end-=remove_length;
    while (ptr <= end && !memcmp(ptr, r_ptr, remove_length))
      ptr+=remove_length;
    end+=remove_length;
  }
  if (ptr == res->ptr())
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}


String *Item_func_rtrim::val_str(String *str)
{
  assert(fixed == 1);
  char buff[MAX_FIELD_WIDTH], *ptr, *end;
  String tmp(buff, sizeof(buff), system_charset_info);
  String *res, *remove_str;
  uint remove_length;

  res= args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  remove_str= &remove;                          /* Default value. */
  if (arg_count == 2)
  {
    remove_str= args[1]->val_str(&tmp);
    if ((null_value= args[1]->null_value))
      return 0;
  }

  if ((remove_length= remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  ptr= (char*) res->ptr();
  end= ptr+res->length();
#ifdef USE_MB
  char *p=ptr;
  register uint32_t l;
#endif
  if (remove_length == 1)
  {
    char chr=(*remove_str)[0];
#ifdef USE_MB
    if (use_mb(res->charset()))
    {
      while (ptr < end)
      {
	if ((l=my_ismbchar(res->charset(), ptr,end))) ptr+=l,p=ptr;
	else ++ptr;
      }
      ptr=p;
    }
#endif
    while (ptr != end  && end[-1] == chr)
      end--;
  }
  else
  {
    const char *r_ptr=remove_str->ptr();
#ifdef USE_MB
    if (use_mb(res->charset()))
    {
  loop:
      while (ptr + remove_length < end)
      {
	if ((l=my_ismbchar(res->charset(), ptr,end))) ptr+=l;
	else ++ptr;
      }
      if (ptr + remove_length == end && !memcmp(ptr,r_ptr,remove_length))
      {
	end-=remove_length;
	ptr=p;
	goto loop;
      }
    }
    else
#endif /* USE_MB */
    {
      while (ptr + remove_length <= end &&
	     !memcmp(end-remove_length, r_ptr, remove_length))
	end-=remove_length;
    }
  }
  if (end == res->ptr()+res->length())
    return res;
  tmp_value.set(*res,0,(uint) (end-res->ptr()));
  return &tmp_value;
}


String *Item_func_trim::val_str(String *str)
{
  assert(fixed == 1);
  char buff[MAX_FIELD_WIDTH], *ptr, *end;
  const char *r_ptr;
  String tmp(buff, sizeof(buff), system_charset_info);
  String *res, *remove_str;
  uint remove_length;

  res= args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  remove_str= &remove;                          /* Default value. */
  if (arg_count == 2)
  {
    remove_str= args[1]->val_str(&tmp);
    if ((null_value= args[1]->null_value))
      return 0;
  }

  if ((remove_length= remove_str->length()) == 0 ||
      remove_length > res->length())
    return res;

  ptr= (char*) res->ptr();
  end= ptr+res->length();
  r_ptr= remove_str->ptr();
  while (ptr+remove_length <= end && !memcmp(ptr,r_ptr,remove_length))
    ptr+=remove_length;
#ifdef USE_MB
  if (use_mb(res->charset()))
  {
    char *p=ptr;
    register uint32_t l;
 loop:
    while (ptr + remove_length < end)
    {
      if ((l=my_ismbchar(res->charset(), ptr,end))) ptr+=l;
      else ++ptr;
    }
    if (ptr + remove_length == end && !memcmp(ptr,r_ptr,remove_length))
    {
      end-=remove_length;
      ptr=p;
      goto loop;
    }
    ptr=p;
  }
  else
#endif /* USE_MB */
  {
    while (ptr + remove_length <= end &&
	   !memcmp(end-remove_length,r_ptr,remove_length))
      end-=remove_length;
  }
  if (ptr == res->ptr() && end == ptr+res->length())
    return res;
  tmp_value.set(*res,(uint) (ptr - res->ptr()),(uint) (end-ptr));
  return &tmp_value;
}

void Item_func_trim::fix_length_and_dec()
{
  max_length= args[0]->max_length;
  if (arg_count == 1)
  {
    collation.set(args[0]->collation);
    remove.set_charset(collation.collation);
    remove.set_ascii(" ",1);
  }
  else
  {
    // Handle character set for args[1] and args[0].
    // Note that we pass args[1] as the first item, and args[0] as the second.
    if (agg_arg_charsets(collation, &args[1], 2, MY_COLL_CMP_CONV, -1))
      return;
  }
}

void Item_func_trim::print(String *str, enum_query_type query_type)
{
  if (arg_count == 1)
  {
    Item_func::print(str, query_type);
    return;
  }
  str->append(Item_func_trim::func_name());
  str->append('(');
  str->append(mode_name());
  str->append(' ');
  args[1]->print(str, query_type);
  str->append(STRING_WITH_LEN(" from "));
  args[0]->print(str, query_type);
  str->append(')');
}


/* Item_func_password */

String *Item_func_password::val_str(String *str)
{
  assert(fixed == 1);
  String *res= args[0]->val_str(str); 
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &my_empty_string;
  make_scrambled_password(tmp_value, res->c_ptr());
  str->set(tmp_value, SCRAMBLED_PASSWORD_CHAR_LENGTH, res->charset());
  return str;
}

char *Item_func_password::alloc(THD *thd, const char *password)
{
  char *buff= (char *) thd->alloc(SCRAMBLED_PASSWORD_CHAR_LENGTH+1);
  if (buff)
    make_scrambled_password(buff, password);
  return buff;
}

#define bin_to_ascii(c) ((c)>=38?((c)-38+'a'):(c)>=12?((c)-12+'A'):(c)+'.')

String *Item_func_encrypt::val_str(String *str)
{
  assert(fixed == 1);
  String *res  =args[0]->val_str(str);

#ifdef HAVE_CRYPT
  char salt[3],*salt_ptr;
  if ((null_value=args[0]->null_value))
    return 0;
  if (res->length() == 0)
    return &my_empty_string;

  if (arg_count == 1)
  {					// generate random salt
    time_t timestamp=current_thd->query_start();
    salt[0] = bin_to_ascii( (ulong) timestamp & 0x3f);
    salt[1] = bin_to_ascii(( (ulong) timestamp >> 5) & 0x3f);
    salt[2] = 0;
    salt_ptr=salt;
  }
  else
  {					// obtain salt from the first two bytes
    String *salt_str=args[1]->val_str(&tmp_value);
    if ((null_value= (args[1]->null_value || salt_str->length() < 2)))
      return 0;
    salt_ptr= salt_str->c_ptr();
  }
  pthread_mutex_lock(&LOCK_crypt);
  char *tmp= crypt(res->c_ptr(),salt_ptr);
  if (!tmp)
  {
    pthread_mutex_unlock(&LOCK_crypt);
    null_value= 1;
    return 0;
  }
  str->set(tmp, (uint) strlen(tmp), &my_charset_bin);
  str->copy();
  pthread_mutex_unlock(&LOCK_crypt);
  return str;
#else
  null_value=1;
  return 0;
#endif	/* HAVE_CRYPT */
}

void Item_func_encode::fix_length_and_dec()
{
  max_length=args[0]->max_length;
  maybe_null=args[0]->maybe_null || args[1]->maybe_null;
  collation.set(&my_charset_bin);
}

String *Item_func_encode::val_str(String *str)
{
  String *res;
  char pw_buff[80];
  String tmp_pw_value(pw_buff, sizeof(pw_buff), system_charset_info);
  String *password;
  assert(fixed == 1);

  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }

  if (!(password=args[1]->val_str(& tmp_pw_value)))
  {
    null_value=1;
    return 0;
  }

  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  SQL_CRYPT sql_crypt(password->ptr());
  sql_crypt.init();
  sql_crypt.encode((char*) res->ptr(),res->length());
  res->set_charset(&my_charset_bin);
  return res;
}

String *Item_func_decode::val_str(String *str)
{
  String *res;
  char pw_buff[80];
  String tmp_pw_value(pw_buff, sizeof(pw_buff), system_charset_info);
  String *password;
  assert(fixed == 1);

  if (!(res=args[0]->val_str(str)))
  {
    null_value=1; /* purecov: inspected */
    return 0; /* purecov: inspected */
  }

  if (!(password=args[1]->val_str(& tmp_pw_value)))
  {
    null_value=1;
    return 0;
  }

  null_value=0;
  res=copy_if_not_alloced(str,res,res->length());
  SQL_CRYPT sql_crypt(password->ptr());
  sql_crypt.init();
  sql_crypt.decode((char*) res->ptr(),res->length());
  return res;
}


Item *Item_func_sysconst::safe_charset_converter(CHARSET_INFO *tocs)
{
  Item_string *conv;
  uint conv_errors;
  String tmp, cstr, *ostr= val_str(&tmp);
  cstr.copy(ostr->ptr(), ostr->length(), ostr->charset(), tocs, &conv_errors);
  if (conv_errors ||
      !(conv= new Item_static_string_func(fully_qualified_func_name(),
                                          cstr.ptr(), cstr.length(),
                                          cstr.charset(),
                                          collation.derivation)))
  {
    return NULL;
  }
  conv->str_value.copy();
  conv->str_value.mark_as_const();
  return conv;
}


String *Item_func_database::val_str(String *str)
{
  assert(fixed == 1);
  THD *thd= current_thd;
  if (thd->db == NULL)
  {
    null_value= 1;
    return 0;
  }
  else
    str->copy(thd->db, thd->db_length, system_charset_info);
  return str;
}


/**
  @todo
  make USER() replicate properly (currently it is replicated to "")
*/
bool Item_func_user::init(const char *user, const char *host)
{
  assert(fixed == 1);

  // For system threads (e.g. replication SQL thread) user may be empty
  if (user)
  {
    CHARSET_INFO *cs= str_value.charset();
    uint res_length= (strlen(user)+strlen(host)+2) * cs->mbmaxlen;

    if (str_value.alloc(res_length))
    {
      null_value=1;
      return true;
    }

    res_length=cs->cset->snprintf(cs, (char*)str_value.ptr(), res_length,
                                  "%s@%s", user, host);
    str_value.length(res_length);
    str_value.mark_as_const();
  }
  return false;
}


bool Item_func_user::fix_fields(THD *thd, Item **ref)
{
  return (Item_func_sysconst::fix_fields(thd, ref) ||
          init(thd->main_security_ctx.user,
               thd->main_security_ctx.host_or_ip));
}


bool Item_func_current_user::fix_fields(THD *thd, Item **ref)
{
  if (Item_func_sysconst::fix_fields(thd, ref))
    return true;

  Security_context *ctx=
                         thd->security_ctx;
  return init(ctx->priv_user, ctx->priv_host);
}


/**
  Change a number to format '3,333,333,333.000'.

  This should be 'internationalized' sometimes.
*/

const int FORMAT_MAX_DECIMALS= 30;

Item_func_format::Item_func_format(Item *org, Item *dec)
: Item_str_func(org, dec)
{
}

void Item_func_format::fix_length_and_dec()
{
  collation.set(default_charset());
  uint char_length= args[0]->max_length/args[0]->collation.collation->mbmaxlen;
  max_length= ((char_length + (char_length-args[0]->decimals)/3) *
               collation.collation->mbmaxlen);
}


/**
  @todo
  This needs to be fixed for multi-byte character set where numbers
  are stored in more than one byte
*/

String *Item_func_format::val_str(String *str)
{
  uint32_t length;
  uint32_t str_length;
  /* Number of decimal digits */
  int dec;
  /* Number of characters used to represent the decimals, including '.' */
  uint32_t dec_length;
  int diff;
  assert(fixed == 1);

  dec= (int) args[1]->val_int();
  if (args[1]->null_value)
  {
    null_value=1;
    return NULL;
  }

  dec= set_zone(dec, 0, FORMAT_MAX_DECIMALS);
  dec_length= dec ? dec+1 : 0;
  null_value=0;

  if (args[0]->result_type() == DECIMAL_RESULT ||
      args[0]->result_type() == INT_RESULT)
  {
    my_decimal dec_val, rnd_dec, *res;
    res= args[0]->val_decimal(&dec_val);
    if ((null_value=args[0]->null_value))
      return 0; /* purecov: inspected */
    my_decimal_round(E_DEC_FATAL_ERROR, res, dec, false, &rnd_dec);
    my_decimal2string(E_DEC_FATAL_ERROR, &rnd_dec, 0, 0, 0, str);
    str_length= str->length();
    if (rnd_dec.sign())
      str_length--;
  }
  else
  {
    double nr= args[0]->val_real();
    if ((null_value=args[0]->null_value))
      return 0; /* purecov: inspected */
    nr= my_double_round(nr, (int64_t) dec, false, false);
    /* Here default_charset() is right as this is not an automatic conversion */
    str->set_real(nr, dec, default_charset());
    if (isnan(nr))
      return str;
    str_length=str->length();
    if (nr < 0)
      str_length--;				// Don't count sign
  }
  /* We need this test to handle 'nan' values */
  if (str_length >= dec_length+4)
  {
    char *tmp,*pos;
    length= str->length()+(diff=((int)(str_length- dec_length-1))/3);
    str= copy_if_not_alloced(&tmp_str,str,length);
    str->length(length);
    tmp= (char*) str->ptr()+length - dec_length-1;
    for (pos= (char*) str->ptr()+length-1; pos != tmp; pos--)
      pos[0]= pos[-diff];
    while (diff)
    {
      *pos= *(pos - diff);
      pos--;
      *pos= *(pos - diff);
      pos--;
      *pos= *(pos - diff);
      pos--;
      pos[0]=',';
      pos--;
      diff--;
    }
  }
  return str;
}


void Item_func_format::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("format("));
  args[0]->print(str, query_type);
  str->append(',');
  args[1]->print(str, query_type);
  str->append(')');
}

void Item_func_elt::fix_length_and_dec()
{
  max_length=0;
  decimals=0;

  if (agg_arg_charsets(collation, args+1, arg_count-1, MY_COLL_ALLOW_CONV, 1))
    return;

  for (uint i= 1 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
  maybe_null=1;					// NULL if wrong first arg
}


double Item_func_elt::val_real()
{
  assert(fixed == 1);
  uint tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return 0.0;
  double result= args[tmp]->val_real();
  null_value= args[tmp]->null_value;
  return result;
}


int64_t Item_func_elt::val_int()
{
  assert(fixed == 1);
  uint tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return 0;

  int64_t result= args[tmp]->val_int();
  null_value= args[tmp]->null_value;
  return result;
}


String *Item_func_elt::val_str(String *str)
{
  assert(fixed == 1);
  uint tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return NULL;

  String *result= args[tmp]->val_str(str);
  if (result)
    result->set_charset(collation.collation);
  null_value= args[tmp]->null_value;
  return result;
}


void Item_func_make_set::split_sum_func(THD *thd, Item **ref_pointer_array,
					List<Item> &fields)
{
  item->split_sum_func2(thd, ref_pointer_array, fields, &item, true);
  Item_str_func::split_sum_func(thd, ref_pointer_array, fields);
}


void Item_func_make_set::fix_length_and_dec()
{
  max_length=arg_count-1;

  if (agg_arg_charsets(collation, args, arg_count, MY_COLL_ALLOW_CONV, 1))
    return;
  
  for (uint i=0 ; i < arg_count ; i++)
    max_length+=args[i]->max_length;

  used_tables_cache|=	  item->used_tables();
  not_null_tables_cache&= item->not_null_tables();
  const_item_cache&=	  item->const_item();
  with_sum_func= with_sum_func || item->with_sum_func;
}


void Item_func_make_set::update_used_tables()
{
  Item_func::update_used_tables();
  item->update_used_tables();
  used_tables_cache|=item->used_tables();
  const_item_cache&=item->const_item();
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
	    if (tmp_str.copy(*res))		// Don't use 'str'
	      return &my_empty_string;
	    result= &tmp_str;
	  }
	}
	else
	{
	  if (result != &tmp_str)
	  {					// Copy data to tmp_str
	    if (tmp_str.alloc(result->length()+res->length()+1) ||
		tmp_str.copy(*result))
	      return &my_empty_string;
	    result= &tmp_str;
	  }
	  if (tmp_str.append(STRING_WITH_LEN(","), &my_charset_bin) || tmp_str.append(*res))
	    return &my_empty_string;
	}
      }
    }
  }
  return result;
}


Item *Item_func_make_set::transform(Item_transformer transformer, uchar *arg)
{
  Item *new_item= item->transform(transformer, arg);
  if (!new_item)
    return 0;

  /*
    THD::change_item_tree() should be called only if the tree was
    really transformed, i.e. when a new item has been created.
    Otherwise we'll be allocating a lot of unnecessary memory for
    change records at each execution.
  */
  if (item != new_item)
    current_thd->change_item_tree(&item, new_item);
  return Item_str_func::transform(transformer, arg);
}


void Item_func_make_set::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("make_set("));
  item->print(str, query_type);
  if (arg_count)
  {
    str->append(',');
    print_args(str, 0, query_type);
  }
  str->append(')');
}


String *Item_func_char::val_str(String *str)
{
  assert(fixed == 1);
  str->length(0);
  str->set_charset(collation.collation);
  for (uint i=0 ; i < arg_count ; i++)
  {
    int32_t num=(int32_t) args[i]->val_int();
    if (!args[i]->null_value)
    {
      char char_num= (char) num;
      if (num&0xFF000000L) {
        str->append((char)(num>>24));
        goto b2;
      } else if (num&0xFF0000L) {
    b2:        str->append((char)(num>>16));
        goto b1;
      } else if (num&0xFF00L) {
    b1:        str->append((char)(num>>8));
      }
      str->append(&char_num, 1);
    }
  }
  str->realloc(str->length());			// Add end 0 (for Purify)
  return check_well_formed_result(str);
}


inline String* alloc_buffer(String *res,String *str,String *tmp_value,
			    ulong length)
{
  if (res->alloced_length() < length)
  {
    if (str->alloced_length() >= length)
    {
      (void) str->copy(*res);
      str->length(length);
      return str;
    }
    if (tmp_value->alloc(length))
      return 0;
    (void) tmp_value->copy(*res);
    tmp_value->length(length);
    return tmp_value;
  }
  res->length(length);
  return res;
}


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
  uint length,tot_length;
  char *to;
  /* must be int64_t to avoid truncation */
  int64_t count= args[1]->val_int();
  String *res= args[0]->val_str(str);

  if (args[0]->null_value || args[1]->null_value)
    goto err;				// string and/or delim are null
  null_value= 0;

  if (count <= 0 && (count == 0 || !args[1]->unsigned_flag))
    return &my_empty_string;

  /* Assumes that the maximum length of a String is < INT32_MAX. */
  /* Bounds check on count:  If this is triggered, we will error. */
  if ((uint64_t) count > INT32_MAX)
    count= INT32_MAX;
  if (count == 1)			// To avoid reallocs
    return res;
  length=res->length();
  // Safe length check
  if (length > current_thd->variables.max_allowed_packet / (uint) count)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
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
  uint32_t res_byte_length,res_char_length,pad_char_length,pad_byte_length;
  char *to;
  const char *ptr_pad;
  /* must be int64_t to avoid truncation */
  int64_t count= args[1]->val_int();
  int64_t byte_count;
  String *res= args[0]->val_str(str);
  String *rpad= args[2]->val_str(&rpad_str);

  if (!res || args[1]->null_value || !rpad || 
      ((count < 0) && !args[1]->unsigned_flag))
    goto err;
  null_value=0;
  /* Assumes that the maximum length of a String is < INT32_MAX. */
  /* Set here so that rest of code sees out-of-bound value as such. */
  if ((uint64_t) count > INT32_MAX)
    count= INT32_MAX;
  if (count <= (res_char_length= res->numchars()))
  {						// String to pad is big enough
    res->length(res->charpos((int) count));	// Shorten result if longer
    return (res);
  }
  pad_char_length= rpad->numchars();

  byte_count= count * collation.collation->mbmaxlen;
  if ((uint64_t) byte_count > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }
  if (args[2]->null_value || !pad_char_length)
    goto err;
  res_byte_length= res->length();	/* Must be done before alloc_buffer */
  if (!(res= alloc_buffer(res,str,&tmp_value, (ulong) byte_count)))
    goto err;

  to= (char*) res->ptr()+res_byte_length;
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
  return (res);

 err:
  null_value=1;
  return 0;
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
  
  if ((uint64_t) byte_count > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }

  if (args[2]->null_value || !pad_char_length ||
      str->alloc((uint32_t) byte_count))
    goto err;
  
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


String *Item_func_conv::val_str(String *str)
{
  assert(fixed == 1);
  String *res= args[0]->val_str(str);
  char *endptr,ans[65],*ptr;
  int64_t dec;
  int from_base= (int) args[1]->val_int();
  int to_base= (int) args[2]->val_int();
  int err;

  if (args[0]->null_value || args[1]->null_value || args[2]->null_value ||
      abs(to_base) > 36 || abs(to_base) < 2 ||
      abs(from_base) > 36 || abs(from_base) < 2 || !(res->length()))
  {
    null_value= 1;
    return NULL;
  }
  null_value= 0;
  unsigned_flag= !(from_base < 0);

  if (from_base < 0)
    dec= my_strntoll(res->charset(), res->ptr(), res->length(),
                     -from_base, &endptr, &err);
  else
    dec= (int64_t) my_strntoull(res->charset(), res->ptr(), res->length(),
                                 from_base, &endptr, &err);

  ptr= int64_t2str(dec, ans, to_base);
  if (str->copy(ans, (uint32_t) (ptr-ans), default_charset()))
    return &my_empty_string;
  return str;
}


String *Item_func_conv_charset::val_str(String *str)
{
  assert(fixed == 1);
  if (use_cached_value)
    return null_value ? 0 : &str_value;
  String *arg= args[0]->val_str(str);
  uint dummy_errors;
  if (!arg)
  {
    null_value=1;
    return 0;
  }
  null_value= str_value.copy(arg->ptr(),arg->length(),arg->charset(),
                             conv_charset, &dummy_errors);
  return null_value ? 0 : check_well_formed_result(&str_value);
}

void Item_func_conv_charset::fix_length_and_dec()
{
  collation.set(conv_charset, DERIVATION_IMPLICIT);
  max_length = args[0]->max_length*conv_charset->mbmaxlen;
}

void Item_func_conv_charset::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("convert("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" using "));
  str->append(conv_charset->csname);
  str->append(')');
}

String *Item_func_set_collation::val_str(String *str)
{
  assert(fixed == 1);
  str=args[0]->val_str(str);
  if ((null_value=args[0]->null_value))
    return 0;
  str->set_charset(collation.collation);
  return str;
}

void Item_func_set_collation::fix_length_and_dec()
{
  CHARSET_INFO *set_collation;
  const char *colname;
  String tmp, *str= args[1]->val_str(&tmp);
  colname= str->c_ptr();
  if (colname == binary_keyword)
    set_collation= get_charset_by_csname(args[0]->collation.collation->csname,
					 MY_CS_BINSORT,MYF(0));
  else
  {
    if (!(set_collation= get_charset_by_name(colname,MYF(0))))
    {
      my_error(ER_UNKNOWN_COLLATION, MYF(0), colname);
      return;
    }
  }

  if (!set_collation || 
      !my_charset_same(args[0]->collation.collation,set_collation))
  {
    my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0),
             colname, args[0]->collation.collation->csname);
    return;
  }
  collation.set(set_collation, DERIVATION_EXPLICIT,
                args[0]->collation.repertoire);
  max_length= args[0]->max_length;
}


bool Item_func_set_collation::eq(const Item *item, bool binary_cmp) const
{
  /* Assume we don't have rtti */
  if (this == item)
    return 1;
  if (item->type() != FUNC_ITEM)
    return 0;
  Item_func *item_func=(Item_func*) item;
  if (arg_count != item_func->arg_count ||
      functype() != item_func->functype())
    return 0;
  Item_func_set_collation *item_func_sc=(Item_func_set_collation*) item;
  if (collation.collation != item_func_sc->collation.collation)
    return 0;
  for (uint i=0; i < arg_count ; i++)
    if (!args[i]->eq(item_func_sc->args[i], binary_cmp))
      return 0;
  return 1;
}


void Item_func_set_collation::print(String *str, enum_query_type query_type)
{
  str->append('(');
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" collate "));
  assert(args[1]->basic_const_item() &&
              args[1]->type() == Item::STRING_ITEM);
  args[1]->str_value.print(str);
  str->append(')');
}

String *Item_func_charset::val_str(String *str)
{
  assert(fixed == 1);
  uint dummy_errors;

  CHARSET_INFO *cs= args[0]->collation.collation; 
  null_value= 0;
  str->copy(cs->csname, strlen(cs->csname),
	    &my_charset_latin1, collation.collation, &dummy_errors);
  return str;
}

String *Item_func_collation::val_str(String *str)
{
  assert(fixed == 1);
  uint dummy_errors;
  CHARSET_INFO *cs= args[0]->collation.collation; 

  null_value= 0;
  str->copy(cs->name, strlen(cs->name),
	    &my_charset_latin1, collation.collation, &dummy_errors);
  return str;
}


void Item_func_weight_string::fix_length_and_dec()
{
  CHARSET_INFO *cs= args[0]->collation.collation;
  collation.set(&my_charset_bin, args[0]->collation.derivation);
  flags= my_strxfrm_flag_normalize(flags, cs->levels_for_order);
  max_length= cs->mbmaxlen * max(args[0]->max_length, nweights);
  maybe_null= 1;
}


/* Return a weight_string according to collation */
String *Item_func_weight_string::val_str(String *str)
{
  String *res;
  CHARSET_INFO *cs= args[0]->collation.collation;
  uint tmp_length, frm_length;
  assert(fixed == 1);

  if (args[0]->result_type() != STRING_RESULT ||
      !(res= args[0]->val_str(str)))
    goto nl;
  
  tmp_length= cs->coll->strnxfrmlen(cs, cs->mbmaxlen *
                                        max(res->length(), nweights));

  if (tmp_value.alloc(tmp_length))
    goto nl;

  frm_length= cs->coll->strnxfrm(cs,
                                 (uchar*) tmp_value.ptr(), tmp_length,
                                 nweights ? nweights : tmp_length,
                                 (const uchar*) res->ptr(), res->length(),
                                 flags);
  tmp_value.length(frm_length);
  null_value= 0;
  return &tmp_value;

nl:
  null_value= 1;
  return 0;
}


String *Item_func_hex::val_str(String *str)
{
  String *res;
  assert(fixed == 1);
  if (args[0]->result_type() != STRING_RESULT)
  {
    uint64_t dec;
    char ans[65],*ptr;
    /* Return hex of unsigned int64_t value */
    if (args[0]->result_type() == REAL_RESULT ||
        args[0]->result_type() == DECIMAL_RESULT)
    {
      double val= args[0]->val_real();
      if ((val <= (double) INT64_MIN) || 
          (val >= (double) (uint64_t) UINT64_MAX))
        dec=  ~(int64_t) 0;
      else
        dec= (uint64_t) (val + (val > 0 ? 0.5 : -0.5));
    }
    else
      dec= (uint64_t) args[0]->val_int();

    if ((null_value= args[0]->null_value))
      return 0;
    ptr= int64_t2str(dec,ans,16);
    if (str->copy(ans,(uint32_t) (ptr-ans),default_charset()))
      return &my_empty_string;			// End of memory
    return str;
  }

  /* Convert given string to a hex string, character by character */
  res= args[0]->val_str(str);
  if (!res || tmp_value.alloc(res->length()*2+1))
  {
    null_value=1;
    return 0;
  }
  null_value=0;
  tmp_value.length(res->length()*2);

  octet2hex((char*) tmp_value.ptr(), res->ptr(), res->length());
  return &tmp_value;
}

  /** Convert given hex string to a binary string. */

String *Item_func_unhex::val_str(String *str)
{
  const char *from, *end;
  char *to;
  String *res;
  uint length;
  assert(fixed == 1);

  res= args[0]->val_str(str);
  if (!res || tmp_value.alloc(length= (1+res->length())/2))
  {
    null_value=1;
    return 0;
  }

  from= res->ptr();
  null_value= 0;
  tmp_value.length(length);
  to= (char*) tmp_value.ptr();
  if (res->length() % 2)
  {
    int hex_char;
    *to++= hex_char= hexchar_to_int(*from++);
    if ((null_value= (hex_char == -1)))
      return 0;
  }
  for (end=res->ptr()+res->length(); from < end ; from+=2, to++)
  {
    int hex_char;
    *to= (hex_char= hexchar_to_int(from[0])) << 4;
    if ((null_value= (hex_char == -1)))
      return 0;
    *to|= hex_char= hexchar_to_int(from[1]);
    if ((null_value= (hex_char == -1)))
      return 0;
  }
  return &tmp_value;
}


void Item_func_binary::print(String *str, enum_query_type query_type)
{
  str->append(STRING_WITH_LEN("cast("));
  args[0]->print(str, query_type);
  str->append(STRING_WITH_LEN(" as binary)"));
}


String *Item_load_file::val_str(String *str)
{
  assert(fixed == 1);
  String *file_name;
  File file;
  struct stat stat_info;
  char path[FN_REFLEN];

  if (!(file_name= args[0]->val_str(str)))
    goto err;

  (void) fn_format(path, file_name->c_ptr(), mysql_real_data_home, "",
		   MY_RELATIVE_PATH | MY_UNPACK_FILENAME);

  /* Read only allowed from within dir specified by secure_file_priv */
  if (opt_secure_file_priv &&
      strncmp(opt_secure_file_priv, path, strlen(opt_secure_file_priv)))
    goto err;

  if (stat(path, &stat_info))
    goto err;

  if (!(stat_info.st_mode & S_IROTH))
  {
    /* my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), file_name->c_ptr()); */
    goto err;
  }
  if (stat_info.st_size > (long) current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_thd->variables.max_allowed_packet);
    goto err;
  }
  if (tmp_value.alloc(stat_info.st_size))
    goto err;
  if ((file = my_open(file_name->c_ptr(), O_RDONLY, MYF(0))) < 0)
    goto err;
  if (my_read(file, (uchar*) tmp_value.ptr(), stat_info.st_size, MYF(MY_NABP)))
  {
    my_close(file, MYF(0));
    goto err;
  }
  tmp_value.length(stat_info.st_size);
  my_close(file, MYF(0));
  null_value = 0;
  return(&tmp_value);

err:
  null_value = 1;
  return(0);
}


String* Item_func_export_set::val_str(String* str)
{
  assert(fixed == 1);
  uint64_t the_set = (uint64_t) args[0]->val_int();
  String yes_buf, *yes;
  yes = args[1]->val_str(&yes_buf);
  String no_buf, *no;
  no = args[2]->val_str(&no_buf);
  String *sep = NULL, sep_buf ;

  uint num_set_values = 64;
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
      /* errors is not checked - assume "," can always be converted */
      uint errors;
      sep_buf.copy(STRING_WITH_LEN(","), &my_charset_bin, collation.collation, &errors);
      sep = &sep_buf;
    }
    break;
  default:
    assert(0); // cannot happen
  }
  null_value=0;

  for (uint i = 0; i < num_set_values; i++, mask = (mask << 1))
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
  uint length=max(args[1]->max_length,args[2]->max_length);
  uint sep_length=(arg_count > 3 ? args[3]->max_length : 1);
  max_length=length*64+sep_length*63;

  if (agg_arg_charsets(collation, args+1, min(4,arg_count)-1,
                       MY_COLL_ALLOW_CONV, 1))
    return;
}


#define get_esc_bit(mask, num) (1 & (*((mask) + ((num) >> 3))) >> ((num) & 7))

/**
  QUOTE() function returns argument string in single quotes suitable for
  using in a SQL statement.

  Adds a \\ before all characters that needs to be escaped in a SQL string.
  We also escape '^Z' (END-OF-FILE in windows) to avoid probelms when
  running commands from a file in windows.

  This function is very useful when you want to generate SQL statements.

  @note
    QUOTE(NULL) returns the string 'NULL' (4 letters, without quotes).

  @retval
    str	   Quoted string
  @retval
    NULL	   Out of memory.
*/

String *Item_func_quote::val_str(String *str)
{
  assert(fixed == 1);
  /*
    Bit mask that has 1 for set for the position of the following characters:
    0, \, ' and ^Z
  */

  static uchar escmask[32]=
  {
    0x01, 0x00, 0x00, 0x04, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  char *from, *to, *end, *start;
  String *arg= args[0]->val_str(str);
  uint arg_length, new_length;
  if (!arg)					// Null argument
  {
    /* Return the string 'NULL' */
    str->copy(STRING_WITH_LEN("NULL"), collation.collation);
    null_value= 0;
    return str;
  }

  arg_length= arg->length();
  new_length= arg_length+2; /* for beginning and ending ' signs */

  for (from= (char*) arg->ptr(), end= from + arg_length; from < end; from++)
    new_length+= get_esc_bit(escmask, (uchar) *from);

  if (tmp_value.alloc(new_length))
    goto null;

  /*
    We replace characters from the end to the beginning
  */
  to= (char*) tmp_value.ptr() + new_length - 1;
  *to--= '\'';
  for (start= (char*) arg->ptr(),end= start + arg_length; end-- != start; to--)
  {
    /*
      We can't use the bitmask here as we want to replace \O and ^Z with 0
      and Z
    */
    switch (*end)  {
    case 0:
      *to--= '0';
      *to=   '\\';
      break;
    case '\032':
      *to--= 'Z';
      *to=   '\\';
      break;
    case '\'':
    case '\\':
      *to--= *end;
      *to=   '\\';
      break;
    default:
      *to= *end;
      break;
    }
  }
  *to= '\'';
  tmp_value.length(new_length);
  tmp_value.set_charset(collation.collation);
  null_value= 0;
  return &tmp_value;

null:
  null_value= 1;
  return 0;
}

int64_t Item_func_uncompressed_length::val_int()
{
  assert(fixed == 1);
  String *res= args[0]->val_str(&value);
  if (!res)
  {
    null_value=1;
    return 0; /* purecov: inspected */
  }
  null_value=0;
  if (res->is_empty()) return 0;

  /*
    res->ptr() using is safe because we have tested that string is not empty,
    res->c_ptr() is not used because:
      - we do not need \0 terminated string to get first 4 bytes
      - c_ptr() tests simbol after string end (uninitialiozed memory) which
        confuse valgrind
  */
  return uint4korr(res->ptr()) & 0x3FFFFFFF;
}

#ifdef HAVE_COMPRESS
#include "zlib.h"

String *Item_func_compress::val_str(String *str)
{
  int err= Z_OK, code;
  ulong new_size;
  String *res;
  Byte *body;
  char *tmp, *last_char;
  assert(fixed == 1);

  if (!(res= args[0]->val_str(str)))
  {
    null_value= 1;
    return 0;
  }
  null_value= 0;
  if (res->is_empty()) return res;

  /*
    Citation from zlib.h (comment for compress function):

    Compresses the source buffer into the destination buffer.  sourceLen is
    the byte length of the source buffer. Upon entry, destLen is the total
    size of the destination buffer, which must be at least 0.1% larger than
    sourceLen plus 12 bytes.
    We assume here that the buffer can't grow more than .25 %.
  */
  new_size= res->length() + res->length() / 5 + 12;

  // Check new_size overflow: new_size <= res->length()
  if (((uint32_t) (new_size+5) <= res->length()) || 
      buffer.realloc((uint32_t) new_size + 4 + 1))
  {
    null_value= 1;
    return 0;
  }

  body= ((Byte*)buffer.ptr()) + 4;

  // As far as we have checked res->is_empty() we can use ptr()
  if ((err= compress(body, &new_size,
		     (const Bytef*)res->ptr(), res->length())) != Z_OK)
  {
    code= err==Z_MEM_ERROR ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_BUF_ERROR;
    push_warning(current_thd,MYSQL_ERROR::WARN_LEVEL_ERROR,code,ER(code));
    null_value= 1;
    return 0;
  }

  tmp= (char*)buffer.ptr(); // int4store is a macro; avoid side effects
  int4store(tmp, res->length() & 0x3FFFFFFF);

  /* This is to ensure that things works for CHAR fields, which trim ' ': */
  last_char= ((char*)body)+new_size-1;
  if (*last_char == ' ')
  {
    *++last_char= '.';
    new_size++;
  }

  buffer.length((uint32_t)new_size + 4);
  return &buffer;
}


String *Item_func_uncompress::val_str(String *str)
{
  assert(fixed == 1);
  String *res= args[0]->val_str(str);
  ulong new_size;
  int err;
  uint code;

  if (!res)
    goto err;
  null_value= 0;
  if (res->is_empty())
    return res;

  /* If length is less than 4 bytes, data is corrupt */
  if (res->length() <= 4)
  {
    push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_ZLIB_Z_DATA_ERROR,
			ER(ER_ZLIB_Z_DATA_ERROR));
    goto err;
  }

  /* Size of uncompressed data is stored as first 4 bytes of field */
  new_size= uint4korr(res->ptr()) & 0x3FFFFFFF;
  if (new_size > current_thd->variables.max_allowed_packet)
  {
    push_warning_printf(current_thd,MYSQL_ERROR::WARN_LEVEL_ERROR,
			ER_TOO_BIG_FOR_UNCOMPRESS,
			ER(ER_TOO_BIG_FOR_UNCOMPRESS),
                        current_thd->variables.max_allowed_packet);
    goto err;
  }
  if (buffer.realloc((uint32_t)new_size))
    goto err;

  if ((err= uncompress((Byte*)buffer.ptr(), &new_size,
		       ((const Bytef*)res->ptr())+4,res->length())) == Z_OK)
  {
    buffer.length((uint32_t) new_size);
    return &buffer;
  }

  code= ((err == Z_BUF_ERROR) ? ER_ZLIB_Z_BUF_ERROR :
	 ((err == Z_MEM_ERROR) ? ER_ZLIB_Z_MEM_ERROR : ER_ZLIB_Z_DATA_ERROR));
  push_warning(current_thd,MYSQL_ERROR::WARN_LEVEL_ERROR,code,ER(code));

err:
  null_value= 1;
  return 0;
}
#endif

/*
  UUID, as in
    DCE 1.1: Remote Procedure Call,
    Open Group Technical Standard Document Number C706, October 1997,
    (supersedes C309 DCE: Remote Procedure Call 8/1994,
    which was basis for ISO/IEC 11578:1996 specification)
*/

static struct rand_struct uuid_rand;
static uint nanoseq;
static uint64_t uuid_time=0;
static char clock_seq_and_node_str[]="-0000-000000000000";

/**
  number of 100-nanosecond intervals between
  1582-10-15 00:00:00.00 and 1970-01-01 00:00:00.00.
*/
#define UUID_TIME_OFFSET ((uint64_t) 141427 * 24 * 60 * 60 * 1000 * 10 )

#define UUID_VERSION      0x1000
#define UUID_VARIANT      0x8000

static void tohex(char *to, uint from, uint len)
{
  to+= len;
  while (len--)
  {
    *--to= _dig_vec_lower[from & 15];
    from >>= 4;
  }
}

static void set_clock_seq_str()
{
  uint16_t clock_seq= ((uint)(my_rnd(&uuid_rand)*16383)) | UUID_VARIANT;
  tohex(clock_seq_and_node_str+1, clock_seq, 4);
  nanoseq= 0;
}

String *Item_func_uuid::val_str(String *str)
{
  assert(fixed == 1);
  char *s;
  THD *thd= current_thd;

  pthread_mutex_lock(&LOCK_uuid_generator);
  if (! uuid_time) /* first UUID() call. initializing data */
  {
    ulong tmp=sql_rnd_with_mutex();
    uchar mac[6];
    int i;
    if (my_gethwaddr(mac))
    {
      /* purecov: begin inspected */
      /*
        generating random "hardware addr"
        and because specs explicitly specify that it should NOT correlate
        with a clock_seq value (initialized random below), we use a separate
        randominit() here
      */
      randominit(&uuid_rand, tmp + (ulong) thd, tmp + (ulong)global_query_id);
      for (i=0; i < (int)sizeof(mac); i++)
        mac[i]=(uchar)(my_rnd(&uuid_rand)*255);
      /* purecov: end */    
    }
    s=clock_seq_and_node_str+sizeof(clock_seq_and_node_str)-1;
    for (i=sizeof(mac)-1 ; i>=0 ; i--)
    {
      *--s=_dig_vec_lower[mac[i] & 15];
      *--s=_dig_vec_lower[mac[i] >> 4];
    }
    randominit(&uuid_rand, tmp + (ulong) server_start_time,
	       tmp + (ulong) thd->status_var.bytes_sent);
    set_clock_seq_str();
  }

  uint64_t tv=my_getsystime() + UUID_TIME_OFFSET + nanoseq;
  if (unlikely(tv < uuid_time))
    set_clock_seq_str();
  else if (unlikely(tv == uuid_time))
  {
    /* special protection from low-res system clocks */
    nanoseq++;
    tv++;
  }
  else
  {
    if (nanoseq)
    {
      tv-=nanoseq;
      nanoseq=0;
    }
    assert(tv > uuid_time);
  }
  uuid_time=tv;
  pthread_mutex_unlock(&LOCK_uuid_generator);

  uint32_t time_low=            (uint32_t) (tv & 0xFFFFFFFF);
  uint16_t time_mid=            (uint16_t) ((tv >> 32) & 0xFFFF);
  uint16_t time_hi_and_version= (uint16_t) ((tv >> 48) | UUID_VERSION);

  str->realloc(UUID_LENGTH+1);
  str->length(UUID_LENGTH);
  str->set_charset(system_charset_info);
  s=(char *) str->ptr();
  s[8]=s[13]='-';
  tohex(s, time_low, 8);
  tohex(s+9, time_mid, 4);
  tohex(s+14, time_hi_and_version, 4);
  strmov(s+18, clock_seq_and_node_str);
  return str;
}
