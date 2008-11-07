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

#include <drizzled/server_includes.h>
#include <mysys/sha1.h>
#include <zlib.h>
#include <drizzled/query_id.h>
#include <uuid/uuid.h>
#include <drizzled/data_home.h>
#include <drizzled/error.h>

// For soundex_map
#include <mysys/my_static.h>
#include CMATH_H

#if defined(CMATH_NAMESPACE)
using namespace CMATH_NAMESPACE;
#endif

using namespace std;

String my_empty_string("",default_charset_info);

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
  uint32_t remove_length;

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
  uint32_t remove_length;

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
  uint32_t remove_length;

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


Item *Item_func_sysconst::safe_charset_converter(const CHARSET_INFO * const tocs)
{
  Item_string *conv;
  uint32_t conv_errors;
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
  Session *session= current_session;
  if (session->db == NULL)
  {
    null_value= 1;
    return 0;
  }
  else
    str->copy(session->db, session->db_length, system_charset_info);
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
    const CHARSET_INFO * const cs= str_value.charset();
    uint32_t res_length= (strlen(user)+strlen(host)+2) * cs->mbmaxlen;

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


bool Item_func_user::fix_fields(Session *session, Item **ref)
{
  return (Item_func_sysconst::fix_fields(session, ref) ||
          init(session->main_security_ctx.user,
               session->main_security_ctx.ip));
}


bool Item_func_current_user::fix_fields(Session *session, Item **ref)
{
  if (Item_func_sysconst::fix_fields(session, ref))
    return true;

  Security_context *ctx=
                         session->security_ctx;
  return init(ctx->user, ctx->ip);
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
  uint32_t char_length= args[0]->max_length/args[0]->collation.collation->mbmaxlen;
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

  for (uint32_t i= 1 ; i < arg_count ; i++)
  {
    set_if_bigger(max_length,args[i]->max_length);
    set_if_bigger(decimals,args[i]->decimals);
  }
  maybe_null=1;					// NULL if wrong first arg
}


double Item_func_elt::val_real()
{
  assert(fixed == 1);
  uint32_t tmp;
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
  uint32_t tmp;
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
  uint32_t tmp;
  null_value=1;
  if ((tmp=(uint) args[0]->val_int()) == 0 || tmp >= arg_count)
    return NULL;

  String *result= args[tmp]->val_str(str);
  if (result)
    result->set_charset(collation.collation);
  null_value= args[tmp]->null_value;
  return result;
}


void Item_func_make_set::split_sum_func(Session *session, Item **ref_pointer_array,
					List<Item> &fields)
{
  item->split_sum_func2(session, ref_pointer_array, fields, &item, true);
  Item_str_func::split_sum_func(session, ref_pointer_array, fields);
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


Item *Item_func_make_set::transform(Item_transformer transformer, unsigned char *arg)
{
  Item *new_item= item->transform(transformer, arg);
  if (!new_item)
    return 0;

  /*
    Session::change_item_tree() should be called only if the tree was
    really transformed, i.e. when a new item has been created.
    Otherwise we'll be allocating a lot of unnecessary memory for
    change records at each execution.
  */
  if (item != new_item)
    current_session->change_item_tree(&item, new_item);
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
  for (uint32_t i=0 ; i < arg_count ; i++)
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
  uint32_t length,tot_length;
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
  if (length > current_session->variables.max_allowed_packet / (uint) count)
  {
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_session->variables.max_allowed_packet);
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
  if ((uint64_t) byte_count > current_session->variables.max_allowed_packet)
  {
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_session->variables.max_allowed_packet);
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
  
  if ((uint64_t) byte_count > current_session->variables.max_allowed_packet)
  {
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_session->variables.max_allowed_packet);
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
  uint32_t dummy_errors;
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
  const CHARSET_INFO *set_collation;
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
  for (uint32_t i=0; i < arg_count ; i++)
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
  uint32_t dummy_errors;

  const CHARSET_INFO * const cs= args[0]->collation.collation; 
  null_value= 0;
  str->copy(cs->csname, strlen(cs->csname),
	    &my_charset_utf8_general_ci, collation.collation, &dummy_errors);
  return str;
}

String *Item_func_collation::val_str(String *str)
{
  assert(fixed == 1);
  uint32_t dummy_errors;
  const CHARSET_INFO * const cs= args[0]->collation.collation; 

  null_value= 0;
  str->copy(cs->name, strlen(cs->name),
	    &my_charset_utf8_general_ci, collation.collation, &dummy_errors);
  return str;
}


void Item_func_weight_string::fix_length_and_dec()
{
  const CHARSET_INFO * const cs= args[0]->collation.collation;
  collation.set(&my_charset_bin, args[0]->collation.derivation);
  flags= my_strxfrm_flag_normalize(flags, cs->levels_for_order);
  max_length= cs->mbmaxlen * cmax(args[0]->max_length, nweights);
  maybe_null= 1;
}


/* Return a weight_string according to collation */
String *Item_func_weight_string::val_str(String *str)
{
  String *res;
  const CHARSET_INFO * const cs= args[0]->collation.collation;
  uint32_t tmp_length, frm_length;
  assert(fixed == 1);

  if (args[0]->result_type() != STRING_RESULT ||
      !(res= args[0]->val_str(str)))
    goto nl;
  
  tmp_length= cs->coll->strnxfrmlen(cs, cs->mbmaxlen *
                                        cmax(res->length(), nweights));

  if (tmp_value.alloc(tmp_length))
    goto nl;

  frm_length= cs->coll->strnxfrm(cs,
                                 (unsigned char*) tmp_value.ptr(), tmp_length,
                                 nweights ? nweights : tmp_length,
                                 (const unsigned char*) res->ptr(), res->length(),
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
  uint32_t length;
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

  (void) fn_format(path, file_name->c_ptr(), drizzle_real_data_home, "",
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
  if (stat_info.st_size > (long) current_session->variables.max_allowed_packet)
  {
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
			ER_WARN_ALLOWED_PACKET_OVERFLOWED,
			ER(ER_WARN_ALLOWED_PACKET_OVERFLOWED),
			func_name(), current_session->variables.max_allowed_packet);
    goto err;
  }
  if (tmp_value.alloc(stat_info.st_size))
    goto err;
  if ((file = my_open(file_name->c_ptr(), O_RDONLY, MYF(0))) < 0)
    goto err;
  if (my_read(file, (unsigned char*) tmp_value.ptr(), stat_info.st_size, MYF(MY_NABP)))
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
      /* errors is not checked - assume "," can always be converted */
      uint32_t errors;
      sep_buf.copy(STRING_WITH_LEN(","), &my_charset_bin, collation.collation, &errors);
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
  uint32_t length=cmax(args[1]->max_length,args[2]->max_length);
  uint32_t sep_length=(arg_count > 3 ? args[3]->max_length : 1);
  max_length=length*64+sep_length*63;

  if (agg_arg_charsets(collation, args+1, cmin((uint)4,arg_count)-1,
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

  static unsigned char escmask[32]=
  {
    0x01, 0x00, 0x00, 0x04, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };

  char *from, *to, *end, *start;
  String *arg= args[0]->val_str(str);
  uint32_t arg_length, new_length;
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
    new_length+= get_esc_bit(escmask, (unsigned char) *from);

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



String *Item_func_uuid::val_str(String *str)
{
  uuid_t uu;
  char *uuid_string;

  /* 36 characters for uuid string +1 for NULL */
  str->realloc(36+1);
  str->length(36);
  str->set_charset(system_charset_info);
  uuid_string= (char *) str->ptr();
  uuid_generate_time(uu);
  uuid_unparse(uu, uuid_string);

  return str;
}
