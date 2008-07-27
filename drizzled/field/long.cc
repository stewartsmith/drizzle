/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include <drizzled/field/long.h>

/****************************************************************************
** long int
****************************************************************************/

int Field_long::store(const char *from,uint len,CHARSET_INFO *cs)
{
  long store_tmp;
  int error;
  int64_t rnd;
  
  error= get_int(cs, from, len, &rnd, UINT32_MAX, INT32_MIN, INT32_MAX);
  store_tmp= unsigned_flag ? (long) (uint64_t) rnd : (long) rnd;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int4store(ptr, store_tmp);
  }
  else
#endif
    longstore(ptr, store_tmp);
  return error;
}


int Field_long::store(double nr)
{
  int error= 0;
  int32_t res;
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      res=0;
      error= 1;
    }
    else if (nr > (double) UINT32_MAX)
    {
      res= INT32_MAX;
      set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      res=(int32_t) (ulong) nr;
  }
  else
  {
    if (nr < (double) INT32_MIN)
    {
      res=(int32_t) INT32_MIN;
      error= 1;
    }
    else if (nr > (double) INT32_MAX)
    {
      res=(int32_t) INT32_MAX;
      error= 1;
    }
    else
      res=(int32_t) (int64_t) nr;
  }
  if (error)
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);

#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int4store(ptr,res);
  }
  else
#endif
    longstore(ptr,res);
  return error;
}


int Field_long::store(int64_t nr, bool unsigned_val)
{
  int error= 0;
  int32_t res;

  if (unsigned_flag)
  {
    if (nr < 0 && !unsigned_val)
    {
      res=0;
      error= 1;
    }
    else if ((uint64_t) nr >= (1LL << 32))
    {
      res=(int32_t) (uint32_t) ~0L;
      error= 1;
    }
    else
      res=(int32_t) (uint32_t) nr;
  }
  else
  {
    if (nr < 0 && unsigned_val)
      nr= ((int64_t) INT32_MAX) + 1;           // Generate overflow
    if (nr < (int64_t) INT32_MIN) 
    {
      res=(int32_t) INT32_MIN;
      error= 1;
    }
    else if (nr > (int64_t) INT32_MAX)
    {
      res=(int32_t) INT32_MAX;
      error= 1;
    }
    else
      res=(int32_t) nr;
  }
  if (error)
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);

#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int4store(ptr,res);
  }
  else
#endif
    longstore(ptr,res);
  return error;
}


double Field_long::val_real(void)
{
  int32_t j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint4korr(ptr);
  else
#endif
    longget(j,ptr);
  return unsigned_flag ? (double) (uint32_t) j : (double) j;
}

int64_t Field_long::val_int(void)
{
  int32_t j;
  /* See the comment in Field_long::store(long long) */
  assert(table->in_use == current_thd);
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint4korr(ptr);
  else
#endif
    longget(j,ptr);
  return unsigned_flag ? (int64_t) (uint32_t) j : (int64_t) j;
}

String *Field_long::val_str(String *val_buffer,
			    String *val_ptr __attribute__((unused)))
{
  CHARSET_INFO *cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,12*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();
  int32_t j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint4korr(ptr);
  else
#endif
    longget(j,ptr);

  if (unsigned_flag)
    length=cs->cset->long10_to_str(cs,to,mlength, 10,(long) (uint32_t)j);
  else
    length=cs->cset->long10_to_str(cs,to,mlength,-10,(long) j);
  val_buffer->length(length);
  if (zerofill)
    prepend_zeros(val_buffer);
  return val_buffer;
}


bool Field_long::send_binary(Protocol *protocol)
{
  return protocol->store_long(Field_long::val_int());
}

int Field_long::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  int32_t a,b;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    a=sint4korr(a_ptr);
    b=sint4korr(b_ptr);
  }
  else
#endif
  {
    longget(a,a_ptr);
    longget(b,b_ptr);
  }
  if (unsigned_flag)
    return ((uint32_t) a < (uint32_t) b) ? -1 : ((uint32_t) a > (uint32_t) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_long::sort_string(uchar *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->s->db_low_byte_first)
  {
    if (unsigned_flag)
      to[0] = ptr[0];
    else
      to[0] = (char) (ptr[0] ^ 128);		/* Revers signbit */
    to[1]   = ptr[1];
    to[2]   = ptr[2];
    to[3]   = ptr[3];
  }
  else
#endif
  {
    if (unsigned_flag)
      to[0] = ptr[3];
    else
      to[0] = (char) (ptr[3] ^ 128);		/* Revers signbit */
    to[1]   = ptr[2];
    to[2]   = ptr[1];
    to[3]   = ptr[0];
  }
}


void Field_long::sql_type(String &res) const
{
  CHARSET_INFO *cs=res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(),
			  "int(%d)",(int) field_length));
  add_zerofill_and_unsigned(res);
}

