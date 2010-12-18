/* -*- mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#include "config.h"
#include <boost/lexical_cast.hpp>
#include <drizzled/field/time.h>
#include <drizzled/error.h>
#include <drizzled/tztime.h>
#include <drizzled/table.h>
#include <drizzled/session.h>

#include <math.h>

#include <sstream>

#include "drizzled/temporal.h"

namespace drizzled
{

namespace field
{

/**
  time_t type

  ** In string context: HH:MM:SS
  ** In number context: HHMMSS

 */
  Time::Time(unsigned char *ptr_arg,
             uint32_t,
             unsigned char *null_ptr_arg,
             unsigned char null_bit_arg,
             const char *field_name_arg,
             const CHARSET_INFO * const cs) :
    Field_str(ptr_arg,
              DateTime::MAX_STRING_LENGTH - 1 /* no \0 */,
              null_ptr_arg,
              null_bit_arg,
              field_name_arg,
              cs)
{
  /* For 4.0 MYD and 4.0 InnoDB compatibility */
  flags|= UNSIGNED_FLAG;
}

Time::Time(bool maybe_null_arg,
           const char *field_name_arg,
           const CHARSET_INFO * const cs) :
  Field_str((unsigned char*) NULL,
            DateTime::MAX_STRING_LENGTH - 1 /* no \0 */,
            maybe_null_arg ? (unsigned char*) "": 0,
            0,
            field_name_arg,
            cs)
{
  /* For 4.0 MYD and 4.0 InnoDB compatibility */
  flags|= UNSIGNED_FLAG;
}

int Time::store(const char *from,
                uint32_t len,
                const CHARSET_INFO * const )
{
  drizzled::Time temporal;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if (not temporal.from_string(from, (size_t) len))
  {
    my_error(ER_INVALID_UNIX_TIMESTAMP_VALUE, MYF(ME_FATALERROR), from);
    return 1;
  }

  uint64_t tmp;
  temporal.to_uint64_t(tmp);

  pack_num(tmp);

  return 0;
}

int Time::store(double from)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if (from < 0 || from > 99991231235959.0)
  {
    /* Convert the double to a string using stringstream */
    std::stringstream ss;
    std::string tmp;
    ss.precision(18); /* 18 places should be fine for error display of double input. */
    ss << from; 
    ss >> tmp;

    my_error(ER_INVALID_UNIX_TIMESTAMP_VALUE, MYF(ME_FATALERROR), tmp.c_str());
    return 2;
  }
  return Time::store((int64_t) rint(from), false);
}

int Time::store(int64_t from, bool)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  /* 
   * Try to create a DateTime from the supplied integer.  Throw an error
   * if unable to create a valid DateTime.  
   */
  Timestamp temporal;
  if (! temporal.from_int64_t(from))
  {
    /* Convert the integer to a string using boost::lexical_cast */
    std::string tmp(boost::lexical_cast<std::string>(from));

    my_error(ER_INVALID_UNIX_TIMESTAMP_VALUE, MYF(ME_FATALERROR), tmp.c_str());
    return 2;
  }

  uint64_t tmp;
  temporal.to_time_t((time_t*)&tmp);

  pack_num(tmp);

  return 0;
}

double Time::val_real(void)
{
  return (double) Time::val_int();
}

int64_t Time::val_int(void)
{
  uint64_t temp;

  ASSERT_COLUMN_MARKED_FOR_READ;

  unpack_num(temp);

  Timestamp temporal;
  (void) temporal.from_time_t((time_t) temp);

  /* We must convert into a "timestamp-formatted integer" ... */
  int64_t result;
  temporal.to_int64_t(&result);
  return result;
}

String *Time::val_str(String *val_buffer, String *)
{
  uint64_t temp= 0;
  char *to;
  int to_len= field_length + 1;

  val_buffer->alloc(to_len);
  to= (char *) val_buffer->ptr();

  unpack_num(temp);

  val_buffer->set_charset(&my_charset_bin);	/* Safety */

  Timestamp temporal;
  (void) temporal.from_time_t((time_t) temp);

  int rlen;
  rlen= temporal.to_string(to, to_len);
  assert(rlen < to_len);

  val_buffer->length(rlen);
  return val_buffer;
}

bool Time::get_date(DRIZZLE_TIME *ltime, uint32_t)
{
  uint64_t temp;

  unpack_num(temp);
  
  memset(ltime, 0, sizeof(*ltime));

  Timestamp temporal;
  (void) temporal.from_time_t((time_t) temp);

  /* @TODO Goodbye the below code when DRIZZLE_TIME is finally gone.. */

  ltime->time_type= DRIZZLE_TIMESTAMP_DATETIME;
  ltime->year= temporal.years();
  ltime->month= temporal.months();
  ltime->day= temporal.days();
  ltime->hour= temporal.hours();
  ltime->minute= temporal.minutes();
  ltime->second= temporal.seconds();

  return 0;
}

bool Time::get_time(DRIZZLE_TIME *ltime)
{
  return Time::get_date(ltime,0);
}

int Time::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  uint64_t a,b;

  unpack_num(a, a_ptr);
  unpack_num(b, b_ptr);

  return (a < b) ? -1 : (a > b) ? 1 : 0;
}


void Time::sort_string(unsigned char *to,uint32_t )
{
#ifdef WORDS_BIGENDIAN
  if (!getTable() || !getTable()->getShare()->db_low_byte_first)
  {
    to[0] = ptr[0];
    to[1] = ptr[1];
    to[2] = ptr[2];
    to[3] = ptr[3];
    to[4] = ptr[4];
    to[5] = ptr[5];
    to[6] = ptr[6];
    to[7] = ptr[7];
  }
  else
#endif
  {
    to[0] = ptr[7];
    to[1] = ptr[6];
    to[2] = ptr[5];
    to[3] = ptr[4];
    to[4] = ptr[3];
    to[5] = ptr[2];
    to[6] = ptr[1];
    to[7] = ptr[0];
  }
}

void Time::sql_type(String &res) const
{
  res.set_ascii(STRING_WITH_LEN("timestamp"));
}

long Time::get_timestamp(bool *null_value)
{
  if ((*null_value= is_null()))
    return 0;

  uint64_t tmp;
  return unpack_num(tmp);
}

size_t Time::max_string_length()
{
  return sizeof(uint64_t);
}

} /* namespace field */
} /* namespace drizzled */
