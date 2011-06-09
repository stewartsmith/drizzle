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
#include <drizzled/base.h>
#include <plugin/myisam/my_handler.h>
#include <drizzled/internal/my_sys.h>

#include <cassert>
#include <algorithm>

using namespace drizzled;
using namespace std;

template<class T>
int CMP_NUM(const T& a, const T&b)
{
  return (a < b) ? -1 : (a == b) ? 0 : 1;
}


int ha_compare_text(const charset_info_st * const charset_info, unsigned char *a, uint32_t a_length,
		    unsigned char *b, uint32_t b_length, bool part_key,
		    bool skip_end_space)
{
  if (!part_key)
    return charset_info->coll->strnncollsp(charset_info, a, a_length,
                                           b, b_length, (bool)!skip_end_space);
  return charset_info->coll->strnncoll(charset_info, a, a_length,
                                       b, b_length, part_key);
}


static int compare_bin(unsigned char *a, uint32_t a_length, unsigned char *b, uint32_t b_length,
                       bool part_key, bool skip_end_space)
{
  uint32_t length= min(a_length,b_length);
  unsigned char *end= a+ length;
  int flag;

  while (a < end)
    if ((flag= (int) *a++ - (int) *b++))
      return flag;
  if (part_key && b_length < a_length)
    return 0;
  if (skip_end_space && a_length != b_length)
  {
    int swap= 1;
    /*
      We are using space compression. We have to check if longer key
      has next character < ' ', in which case it's less than the shorter
      key that has an implicite space afterwards.

      This code is identical to the one in
      strings/ctype-simple.c:my_strnncollsp_simple
    */
    if (a_length < b_length)
    {
      /* put shorter key in a */
      a_length= b_length;
      a= b;
      swap= -1;					/* swap sign of result */
    }
    for (end= a + a_length-length; a < end ; a++)
    {
      if (*a != ' ')
	return (*a < ' ') ? -swap : swap;
    }
    return 0;
  }
  return (int) (a_length-b_length);
}


/*
  Compare two keys

  SYNOPSIS
    ha_key_cmp()
    keyseg	Array of key segments of key to compare
    a		First key to compare, in format from _mi_pack_key()
		This is normally key specified by user
    b		Second key to compare.  This is always from a row
    key_length	Length of key to compare.  This can be shorter than
		a to just compare sub keys
    next_flag	How keys should be compared
		If bit SEARCH_FIND is not set the keys includes the row
		position and this should also be compared
    diff_pos    OUT Number of first keypart where values differ, counting
                from one.
    diff_pos[1] OUT  (b + diff_pos[1]) points to first value in tuple b
                      that is different from corresponding value in tuple a.

  EXAMPLES
   Example1: if the function is called for tuples
     ('aaa','bbb') and ('eee','fff'), then
     diff_pos[0] = 1 (as 'aaa' != 'eee')
     diff_pos[1] = 0 (offset from beggining of tuple b to 'eee' keypart).

   Example2: if the index function is called for tuples
     ('aaa','bbb') and ('aaa','fff'),
     diff_pos[0] = 2 (as 'aaa' != 'eee')
     diff_pos[1] = 3 (offset from beggining of tuple b to 'fff' keypart,
                      here we assume that first key part is CHAR(3) NOT NULL)

  NOTES
    Number-keys can't be splited

  RETURN VALUES
    <0	If a < b
    0	If a == b
    >0	If a > b
*/

#define FCMP(A,B) ((int) (A) - (int) (B))

int ha_key_cmp(register HA_KEYSEG *keyseg, register unsigned char *a,
	       register unsigned char *b, uint32_t key_length, uint32_t nextflag,
	       uint32_t *diff_pos)
{
  int flag;
  int32_t l_1,l_2;
  uint32_t u_1,u_2;
  double d_1,d_2;
  uint32_t next_key_length;
  unsigned char *orig_b= b;

  *diff_pos=0;
  for ( ; (int) key_length >0 ; key_length=next_key_length, keyseg++)
  {
    unsigned char *end;
    uint32_t piks=! (keyseg->flag & HA_NO_SORT);
    (*diff_pos)++;
    diff_pos[1]= (uint)(b - orig_b);

    /* Handle NULL part */
    if (keyseg->null_bit)
    {
      key_length--;
      if (*a != *b && piks)
      {
        flag = (int) *a - (int) *b;
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      }
      b++;
      if (!*a++)                                /* If key was NULL */
      {
        if (nextflag == (SEARCH_FIND | SEARCH_UPDATE))
          nextflag=SEARCH_SAME;                 /* Allow duplicate keys */
  	else if (nextflag & SEARCH_NULL_ARE_NOT_EQUAL)
	{
	  /*
	    This is only used from mi_check() to calculate cardinality.
	    It can't be used when searching for a key as this would cause
	    compare of (a,b) and (b,a) to return the same value.
	  */
	  return -1;
	}
        next_key_length=key_length;
        continue;                               /* To next key part */
      }
    }
    end= a+ min((uint32_t)keyseg->length,key_length);
    next_key_length=key_length-keyseg->length;

    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_TEXT:                       /* Ascii; Key is converted */
      if (keyseg->flag & HA_SPACE_PACK)
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if (piks &&
            (flag=ha_compare_text(keyseg->charset,a,a_length,b,b_length,
				  (bool) ((nextflag & SEARCH_PREFIX) &&
					     next_key_length <= 0),
				  (bool)!(nextflag & SEARCH_PREFIX))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      else
      {
	uint32_t length=(uint) (end-a), a_length=length, b_length=length;
        if (piks &&
            (flag= ha_compare_text(keyseg->charset, a, a_length, b, b_length,
				   (bool) ((nextflag & SEARCH_PREFIX) &&
					      next_key_length <= 0),
				   (bool)!(nextflag & SEARCH_PREFIX))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a=end;
        b+=length;
      }
      break;
    case HA_KEYTYPE_BINARY:
      if (keyseg->flag & HA_SPACE_PACK)
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if (piks &&
	    (flag=compare_bin(a,a_length,b,b_length,
                              (bool) ((nextflag & SEARCH_PREFIX) &&
                                         next_key_length <= 0),1)))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
      else
      {
        uint32_t length=keyseg->length;
        if (piks &&
	    (flag=compare_bin(a,length,b,length,
                              (bool) ((nextflag & SEARCH_PREFIX) &&
                                         next_key_length <= 0),0)))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=length;
        b+=length;
      }
      break;
    case HA_KEYTYPE_VARTEXT1:
    case HA_KEYTYPE_VARTEXT2:
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if (piks &&
	    (flag= ha_compare_text(keyseg->charset,a,a_length,b,b_length,
                                   (bool) ((nextflag & SEARCH_PREFIX) &&
                                              next_key_length <= 0),
				   (bool) ((nextflag & (SEARCH_FIND |
							   SEARCH_UPDATE)) ==
					      SEARCH_FIND &&
                                              ! (keyseg->flag &
                                                 HA_END_SPACE_ARE_EQUAL)))))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+= a_length;
        b+= b_length;
        break;
      }
    case HA_KEYTYPE_VARBINARY1:
    case HA_KEYTYPE_VARBINARY2:
      {
        int a_length,b_length,pack_length;
        get_key_length(a_length,a);
        get_key_pack_length(b_length,pack_length,b);
        next_key_length=key_length-b_length-pack_length;

        if (piks &&
	    (flag=compare_bin(a,a_length,b,b_length,
                              (bool) ((nextflag & SEARCH_PREFIX) &&
                                         next_key_length <= 0), 0)))
          return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
        a+=a_length;
        b+=b_length;
        break;
      }
    case HA_KEYTYPE_LONG_INT:
      l_1= mi_sint4korr(a);
      l_2= mi_sint4korr(b);
      if (piks && (flag = CMP_NUM(l_1,l_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(long int); */
      break;
    case HA_KEYTYPE_ULONG_INT:
      u_1= mi_sint4korr(a);
      u_2= mi_sint4korr(b);
      if (piks && (flag = CMP_NUM(u_1,u_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 4; /* sizeof(long int); */
      break;
    case HA_KEYTYPE_DOUBLE:
      mi_float8get(d_1,a);
      mi_float8get(d_2,b);
      /*
        The following may give a compiler warning about floating point
        comparison not being safe, but this is ok in this context as
        we are bascily doing sorting
      */
      if (piks && (flag = CMP_NUM(d_1,d_2)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 8;  /* sizeof(double); */
      break;
    case HA_KEYTYPE_LONGLONG:
    {
      int64_t ll_a,ll_b;
      ll_a= mi_sint8korr(a);
      ll_b= mi_sint8korr(b);
      if (piks && (flag = CMP_NUM(ll_a,ll_b)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 8;
      break;
    }
    case HA_KEYTYPE_ULONGLONG:
    {
      uint64_t ll_a,ll_b;
      ll_a= mi_uint8korr(a);
      ll_b= mi_uint8korr(b);
      if (piks && (flag = CMP_NUM(ll_a,ll_b)))
        return ((keyseg->flag & HA_REVERSE_SORT) ? -flag : flag);
      a=  end;
      b+= 8;
      break;
    }
    case HA_KEYTYPE_END:                        /* Ready */
      goto end;                                 /* diff_pos is incremented */
    }
  }
  (*diff_pos)++;
end:
  if (!(nextflag & SEARCH_FIND))
  {
    uint32_t i;
    if (nextflag & (SEARCH_NO_FIND | SEARCH_LAST)) /* Find record after key */
      return (nextflag & (SEARCH_BIGGER | SEARCH_LAST)) ? -1 : 1;
    flag=0;
    for (i=keyseg->length ; i-- > 0 ; )
    {
      if (*a++ != *b++)
      {
        flag= FCMP(a[-1],b[-1]);
        break;
      }
    }
    if (nextflag & SEARCH_SAME)
      return (flag);                            /* read same */
    if (nextflag & SEARCH_BIGGER)
      return (flag <= 0 ? -1 : 1);              /* read next */
    return (flag < 0 ? -1 : 1);                 /* read previous */
  }
  return 0;
} /* ha_key_cmp */


/*
  Find the first NULL value in index-suffix values tuple

  SYNOPSIS
    ha_find_null()
      keyseg     Array of keyparts for key suffix
      a          Key suffix value tuple

  DESCRIPTION
    Find the first NULL value in index-suffix values tuple.

  TODO
    Consider optimizing this function or its use so we don't search for
    NULL values in completely NOT NULL index suffixes.

  RETURN
    First key part that has NULL as value in values tuple, or the last key
    part (with keyseg->type==HA_TYPE_END) if values tuple doesn't contain
    NULLs.
*/

HA_KEYSEG *ha_find_null(HA_KEYSEG *keyseg, unsigned char *a)
{
  for (; (enum ha_base_keytype) keyseg->type != HA_KEYTYPE_END; keyseg++)
  {
    unsigned char *end;
    if (keyseg->null_bit)
    {
      if (!*a++)
        return keyseg;
    }
    end= a+ keyseg->length;

    switch ((enum ha_base_keytype) keyseg->type) {
    case HA_KEYTYPE_TEXT:
    case HA_KEYTYPE_BINARY:
      if (keyseg->flag & HA_SPACE_PACK)
      {
        int a_length;
        get_key_length(a_length, a);
        a += a_length;
        break;
      }
      else
        a= end;
      break;
    case HA_KEYTYPE_VARTEXT1:
    case HA_KEYTYPE_VARTEXT2:
    case HA_KEYTYPE_VARBINARY1:
    case HA_KEYTYPE_VARBINARY2:
      {
        int a_length;
        get_key_length(a_length, a);
        a+= a_length;
        break;
      }
    case HA_KEYTYPE_LONG_INT:
    case HA_KEYTYPE_ULONG_INT:
    case HA_KEYTYPE_LONGLONG:
    case HA_KEYTYPE_ULONGLONG:
    case HA_KEYTYPE_DOUBLE:
      a= end;
      break;
    case HA_KEYTYPE_END:
      /* keep compiler happy */
      assert(0);
      break;
    }
  }
  return keyseg;
}



