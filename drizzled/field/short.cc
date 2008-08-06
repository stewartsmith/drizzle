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

#include <drizzled/server_includes.h>
#include <drizzled/field/short.h>

/****************************************************************************
 Field type short int (2 byte)
****************************************************************************/

int Field_short::store(const char *from,uint len, const CHARSET_INFO * const cs)
{
  int store_tmp;
  int error;
  int64_t rnd;
  
  error= get_int(cs, from, len, &rnd, UINT16_MAX, INT16_MIN, INT16_MAX);
  store_tmp= unsigned_flag ? (int) (uint64_t) rnd : (int) rnd;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int2store(ptr, store_tmp);
  }
  else
#endif
    shortstore(ptr, (short) store_tmp);
  return error;
}


int Field_short::store(double nr)
{
  int error= 0;
  int16_t res;
  nr=rint(nr);
  if (unsigned_flag)
  {
    if (nr < 0)
    {
      res=0;
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if (nr > (double) UINT16_MAX)
    {
      res=(int16_t) UINT16_MAX;
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      res=(int16_t) (uint16_t) nr;
  }
  else
  {
    if (nr < (double) INT16_MIN)
    {
      res=INT16_MIN;
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if (nr > (double) INT16_MAX)
    {
      res=INT16_MAX;
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      res=(int16_t) (int) nr;
  }
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int2store(ptr,res);
  }
  else
#endif
    shortstore(ptr,res);
  return error;
}


int Field_short::store(int64_t nr, bool unsigned_val)
{
  int error= 0;
  int16_t res;

  if (unsigned_flag)
  {
    if (nr < 0L && !unsigned_val)
    {
      res=0;
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if ((uint64_t) nr > (uint64_t) UINT16_MAX)
    {
      res=(int16_t) UINT16_MAX;
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      res=(int16_t) (uint16_t) nr;
  }
  else
  {
    if (nr < 0 && unsigned_val)
      nr= UINT16_MAX+1;                         // Generate overflow

    if (nr < INT16_MIN)
    {
      res=INT16_MIN;
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else if (nr > (int64_t) INT16_MAX)
    {
      res=INT16_MAX;
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
      error= 1;
    }
    else
      res=(int16_t) nr;
  }
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    int2store(ptr,res);
  }
  else
#endif
    shortstore(ptr,res);
  return error;
}


double Field_short::val_real(void)
{
  short j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint2korr(ptr);
  else
#endif
    shortget(j,ptr);
  return unsigned_flag ? (double) (unsigned short) j : (double) j;
}

int64_t Field_short::val_int(void)
{
  short j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint2korr(ptr);
  else
#endif
    shortget(j,ptr);
  return unsigned_flag ? (int64_t) (unsigned short) j : (int64_t) j;
}


String *Field_short::val_str(String *val_buffer,
			     String *val_ptr __attribute__((unused)))
{
  const CHARSET_INFO * const cs= &my_charset_bin;
  uint length;
  uint mlength=max(field_length+1,7*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();
  short j;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
    j=sint2korr(ptr);
  else
#endif
    shortget(j,ptr);

  if (unsigned_flag)
    length=(uint) cs->cset->long10_to_str(cs, to, mlength, 10, 
					  (long) (uint16_t) j);
  else
    length=(uint) cs->cset->long10_to_str(cs, to, mlength,-10, (long) j);
  val_buffer->length(length);

  return val_buffer;
}


bool Field_short::send_binary(Protocol *protocol)
{
  return protocol->store_short(Field_short::val_int());
}


int Field_short::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  short a,b;
#ifdef WORDS_BIGENDIAN
  if (table->s->db_low_byte_first)
  {
    a=sint2korr(a_ptr);
    b=sint2korr(b_ptr);
  }
  else
#endif
  {
    shortget(a,a_ptr);
    shortget(b,b_ptr);
  }

  if (unsigned_flag)
    return ((unsigned short) a < (unsigned short) b) ? -1 :
    ((unsigned short) a > (unsigned short) b) ? 1 : 0;
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_short::sort_string(uchar *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table->s->db_low_byte_first)
  {
    if (unsigned_flag)
      to[0] = ptr[0];
    else
      to[0] = (char) (ptr[0] ^ 128);		/* Revers signbit */
    to[1]   = ptr[1];
  }
  else
#endif
  {
    if (unsigned_flag)
      to[0] = ptr[1];
    else
      to[0] = (char) (ptr[1] ^ 128);		/* Revers signbit */
    to[1]   = ptr[0];
  }
}

void Field_short::sql_type(String &res) const
{
  const CHARSET_INFO * const cs= res.charset();
  res.length(cs->cset->snprintf(cs,(char*) res.ptr(),res.alloced_length(), "smallint"));
  add_unsigned(res);
}
