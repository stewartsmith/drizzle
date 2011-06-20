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
#include <drizzled/field/int32.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>

#include <math.h>

#include <algorithm>

using namespace std;

namespace drizzled {
namespace field {

/****************************************************************************
 ** Int32
 ****************************************************************************/

  int Int32::store(const char *from,uint32_t len, const charset_info_st * const cs)
  {
    ASSERT_COLUMN_MARKED_FOR_WRITE;
    int64_t rnd;
    int error= get_int(cs, from, len, &rnd, UINT32_MAX, INT32_MIN, INT32_MAX);
    long store_tmp= (long) rnd;
    longstore(ptr, store_tmp);
    return error;
  }


  int Int32::store(double nr)
  {
    int error= 0;
    int32_t res;
    nr=rint(nr);

    ASSERT_COLUMN_MARKED_FOR_WRITE;

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

    if (error)
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);

    longstore(ptr,res);

    return error;
  }


  int Int32::store(int64_t nr, bool unsigned_val)
  {
    int error= 0;
    int32_t res;

    ASSERT_COLUMN_MARKED_FOR_WRITE;

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
    {
      res=(int32_t) nr;
    }

    if (error)
      set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_OUT_OF_RANGE, 1);

    longstore(ptr,res);

    return error;
  }


  double Int32::val_real(void) const
  {
    int32_t j;

    ASSERT_COLUMN_MARKED_FOR_READ;

    longget(j,ptr);

    return (double) j;
  }

  int64_t Int32::val_int(void) const
  {
    int32_t j;

    ASSERT_COLUMN_MARKED_FOR_READ;

    longget(j,ptr);

    return (int64_t) j;
  }

  String *Int32::val_str(String *val_buffer, String *) const
  {
    const charset_info_st * const cs= &my_charset_bin;
    uint32_t length;
    uint32_t mlength= max(field_length+1,12*cs->mbmaxlen);
    val_buffer->alloc(mlength);
    char *to=(char*) val_buffer->ptr();
    int32_t j;

    ASSERT_COLUMN_MARKED_FOR_READ;

    longget(j,ptr);

    length=cs->cset->long10_to_str(cs,to,mlength,-10,(long) j);
    val_buffer->length(length);

    return val_buffer;
  }

  int Int32::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
  {
    int32_t a,b;

    longget(a,a_ptr);
    longget(b,b_ptr);

    return (a < b) ? -1 : (a > b) ? 1 : 0;
  }

  void Int32::sort_string(unsigned char *to,uint32_t )
  {
#ifdef WORDS_BIGENDIAN
    {
      to[0] = (char) (ptr[0] ^ 128);		/* Revers signbit */
      to[1]   = ptr[1];
      to[2]   = ptr[2];
      to[3]   = ptr[3];
    }
#else
    {
      to[0] = (char) (ptr[3] ^ 128);		/* Revers signbit */
      to[1]   = ptr[2];
      to[2]   = ptr[1];
      to[3]   = ptr[0];
    }
#endif
  }


  unsigned char *Int32::pack(unsigned char* to, const unsigned char *from, uint32_t, bool)
  {
    int32_t val;
    longget(val, from);

    longstore(to, val);
    return to + sizeof(val);
  }


  const unsigned char *Int32::unpack(unsigned char* to, const unsigned char *from, uint32_t, bool)
  {
    int32_t val;
    longget(val, from);

    longstore(to, val);

    return from + sizeof(val);
  }

} /* namespace field */
} /* namespace drizzled */
