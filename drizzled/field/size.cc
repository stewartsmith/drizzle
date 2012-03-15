/* - mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
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


#include <config.h>
#include <drizzled/field/size.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/internal/my_sys.h>

#include <math.h>

#include <algorithm>

using namespace std;

namespace drizzled
{

namespace field
{

/****************************************************************************
  Field type Size int (8 bytes)
 ****************************************************************************/

Size::Size(unsigned char *ptr_arg, uint32_t len_arg,
           unsigned char *null_ptr_arg,
           unsigned char null_bit_arg,
           enum utype unireg_check_arg,
           const char *field_name_arg) :
  Field_num(ptr_arg,
            len_arg,
            null_ptr_arg,
            null_bit_arg,
            unireg_check_arg,
            field_name_arg,
            0, false, true)
{
  flags|= UNSIGNED_FLAG;
}

Size::Size(uint32_t len_arg,bool maybe_null_arg,
           const char *field_name_arg,
           bool unsigned_arg) :
  Field_num((unsigned char*) 0,
            len_arg, maybe_null_arg ? (unsigned char*) "": 0,
            0,
            NONE,
            field_name_arg,
            0,
            0,
            unsigned_arg)
{
  flags|= UNSIGNED_FLAG;
  assert(unsigned_arg);
}

int Size::store(const char *from,uint32_t len, const charset_info_st * const cs)
{
  int error= 0;
  char *end;
  uint64_t tmp;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  tmp= cs->cset->strntoull10rnd(cs, from, len, false, &end,&error);
  if (error == ERANGE)
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    error= 1;
  }
  else if (getTable()->in_use->count_cuted_fields && check_int(cs, from, len, end, error))
  {
    error= 1;
  }
  else
  {
    error= 0;
  }

  int64_tstore(ptr,tmp);

  return error;
}


int Size::store(double nr)
{
  int error= 0;
  int64_t res;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  nr= rint(nr);

  if (nr <= (double) INT64_MIN)
  {
    res= INT64_MIN;
    error= (nr < (double) INT64_MIN);
  }
  else if (nr >= (double) (uint64_t) INT64_MAX)
  {
    res= INT64_MAX;
    error= (nr > (double) INT64_MAX);
  }
  else
  {
    res=(int64_t) nr;
  }

  if (error)
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    return 0;
  }

  int64_tstore(ptr, res);

  return error;
}


int Size::store(int64_t nr, bool arg)
{
  int error= 0;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if (not arg and nr < 0)
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);
    return 0;
  }

  int64_tstore(ptr,nr);

  return error;
}


double Size::val_real(void) const
{
  int64_t j;

  ASSERT_COLUMN_MARKED_FOR_READ;

  int64_tget(j,ptr);

  return (double) j;
}


int64_t Size::val_int(void) const
{
  int64_t j;

  ASSERT_COLUMN_MARKED_FOR_READ;

  int64_tget(j,ptr);

  return j;
}


String *Size::val_str(String *val_buffer, String *) const
{
  const charset_info_st * const cs= &my_charset_bin;
  uint32_t length;
  uint32_t mlength= max(field_length+1,22*cs->mbmaxlen);
  val_buffer->alloc(mlength);
  char *to=(char*) val_buffer->ptr();
  int64_t j;

  ASSERT_COLUMN_MARKED_FOR_READ;

  int64_tget(j,ptr);

  length=(uint32_t) (cs->cset->int64_t10_to_str)(cs,to,mlength, -10, j);
  val_buffer->length(length);

  return val_buffer;
}

int Size::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  int64_t a,b;

  int64_tget(a,a_ptr);
  int64_tget(b,b_ptr);

  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Size::sort_string(unsigned char *to,uint32_t )
{
#ifdef WORDS_BIGENDIAN
  {
    to[0] = (char) (ptr[0] ^ 128);		/* Revers signbit */
    to[1]   = ptr[1];
    to[2]   = ptr[2];
    to[3]   = ptr[3];
    to[4]   = ptr[4];
    to[5]   = ptr[5];
    to[6]   = ptr[6];
    to[7]   = ptr[7];
  }
#else
  {
    to[0] = (char) (ptr[7] ^ 128);		/* Revers signbit */
    to[1]   = ptr[6];
    to[2]   = ptr[5];
    to[3]   = ptr[4];
    to[4]   = ptr[3];
    to[5]   = ptr[2];
    to[6]   = ptr[1];
    to[7]   = ptr[0];
  }
#endif
}

unsigned char *Size::pack(unsigned char* to, const unsigned char *from, uint32_t, bool)
{
  int64_t val;

  int64_tget(val, from);
  int64_tstore(to, val);

  return to + sizeof(val);
}


const unsigned char *Size::unpack(unsigned char* to, const unsigned char *from, uint32_t, bool)
{
  int64_t val;

  int64_tget(val, from);
  int64_tstore(to, val);

  return from + sizeof(val);
}

} /* namespace field */
} /* namespace drizzled */
