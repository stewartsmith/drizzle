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

#include <float.h>
#include <math.h>

#include <algorithm>

#include <drizzled/field/double.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/current_session.h>
#include <drizzled/internal/m_string.h>

using namespace std;

namespace drizzled
{

/****************************************************************************
  double precision floating point numbers
****************************************************************************/

int Field_double::store(const char *from,uint32_t len, const charset_info_st * const cs)
{
  int error;
  char *end;
  double nr= my_strntod(cs,(char*) from, len, &end, &error);

  ASSERT_COLUMN_MARKED_FOR_WRITE;
  if (error || (!len || (((uint32_t) (end-from) != len) && getTable()->in_use->count_cuted_fields)))
  {
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN,
                (error ? ER_WARN_DATA_OUT_OF_RANGE : ER_WARN_DATA_TRUNCATED), 1);
    error= error ? 1 : 2;
  }
  Field_double::store(nr);
  return error;
}


int Field_double::store(double nr)
{
  int error= truncate(&nr, DBL_MAX);

  ASSERT_COLUMN_MARKED_FOR_WRITE;

#ifdef WORDS_BIGENDIAN
  if (getTable()->getShare()->db_low_byte_first)
  {
    float8store(ptr,nr);
  }
  else
#endif
    doublestore(ptr,nr);
  return error;
}


int Field_double::store(int64_t nr, bool unsigned_val)
{
  return Field_double::store(unsigned_val ? uint64_t2double((uint64_t) nr) :
                             (double) nr);
}

double Field_double::val_real(void) const
{
  double j;

  ASSERT_COLUMN_MARKED_FOR_READ;

#ifdef WORDS_BIGENDIAN
  if (getTable()->getShare()->db_low_byte_first)
  {
    float8get(j,ptr);
  }
  else
#endif
    doubleget(j,ptr);
  return j;
}

int64_t Field_double::val_int(void) const
{
  double j;
  int64_t res;

  ASSERT_COLUMN_MARKED_FOR_READ;

#ifdef WORDS_BIGENDIAN
  if (getTable()->getShare()->db_low_byte_first)
  {
    float8get(j,ptr);
  }
  else
#endif
    doubleget(j,ptr);
  /* Check whether we fit into int64_t range */
  if (j <= (double) INT64_MIN)
  {
    res= (int64_t) INT64_MIN;
    goto warn;
  }
  if (j >= (double) (uint64_t) INT64_MAX)
  {
    res= (int64_t) INT64_MAX;
    goto warn;
  }
  return (int64_t) rint(j);

warn:
  {
    char buf[DOUBLE_TO_STRING_CONVERSION_BUFFER_SIZE];
    String tmp(buf, sizeof(buf), &my_charset_utf8_general_ci), *str;
    str= val_str(&tmp, &tmp);
    Session *session= getTable() ? getTable()->in_use : current_session;
    push_warning_printf(session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                        ER_TRUNCATED_WRONG_VALUE,
                        ER(ER_TRUNCATED_WRONG_VALUE), "INTEGER",
                        str->c_ptr());
  }
  return res;
}


String *Field_double::val_str(String *val_buffer, String *) const
{
  double nr;

  ASSERT_COLUMN_MARKED_FOR_READ;

#ifdef WORDS_BIGENDIAN
  if (getTable()->getShare()->db_low_byte_first)
  {
    float8get(nr,ptr);
  }
  else
#endif
    doubleget(nr,ptr);

  uint32_t to_length= max(field_length, (uint32_t)DOUBLE_TO_STRING_CONVERSION_BUFFER_SIZE);
  val_buffer->alloc(to_length);
  char *to=(char*) val_buffer->ptr();
  size_t len;

  if (dec >= NOT_FIXED_DEC)
    len= internal::my_gcvt(nr, internal::MY_GCVT_ARG_DOUBLE, to_length - 1, to, NULL);
  else
    len= internal::my_fcvt(nr, dec, to, NULL);

  val_buffer->length((uint32_t) len);

  return val_buffer;
}

int Field_double::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  double a,b;
#ifdef WORDS_BIGENDIAN
  if (getTable()->getShare()->db_low_byte_first)
  {
    float8get(a,a_ptr);
    float8get(b,b_ptr);
  }
  else
#endif
  {
    doubleget(a, a_ptr);
    doubleget(b, b_ptr);
  }
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}


/* The following should work for IEEE */

void Field_double::sort_string(unsigned char *to,uint32_t )
{
  double nr;
#ifdef WORDS_BIGENDIAN
  if (getTable()->getShare()->db_low_byte_first)
  {
    float8get(nr,ptr);
  }
  else
#endif
    doubleget(nr,ptr);
  change_double_for_sort(nr, to);
}

} /* namespace drizzled */
