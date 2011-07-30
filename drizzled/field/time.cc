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

#include <config.h>
#include <boost/lexical_cast.hpp>
#include <drizzled/field/time.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/temporal.h>

#include <arpa/inet.h>
#include <cmath>
#include <sstream>

namespace drizzled {
namespace field {

/**
  time_t type

  ** In string context: HH:MM:SS
  ** In number context: HHMMSS

 */
  Time::Time(unsigned char *ptr_arg,
             uint32_t,
             unsigned char *null_ptr_arg,
             unsigned char null_bit_arg,
             const char *field_name_arg) :
    Field_str(ptr_arg,
              DateTime::MAX_STRING_LENGTH - 1 /* no \0 */,
              null_ptr_arg,
              null_bit_arg,
              field_name_arg,
              &my_charset_bin)
{
}

Time::Time(bool maybe_null_arg,
           const char *field_name_arg) :
  Field_str((unsigned char*) NULL,
            DateTime::MAX_STRING_LENGTH - 1 /* no \0 */,
            maybe_null_arg ? (unsigned char*) "": 0,
            0,
            field_name_arg,
            &my_charset_bin)
{
}

int Time::store(const char *from,
                uint32_t len,
                const charset_info_st * const )
{
  drizzled::Time temporal;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if (not temporal.from_string(from, (size_t) len))
  {
    std::string tmp(boost::lexical_cast<std::string>(from));
    my_error(ER_INVALID_TIME_VALUE, MYF(0), tmp.c_str());
    return 1;
  }

  pack_time(temporal);

  return 0;
}

int Time::store(double from)
{ 
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  do
  {
    int64_t tmp;

    if (from > (double)TIME_MAX_VALUE)
    { 
      tmp= TIME_MAX_VALUE;
      break;
    }
    else if (from < (double) - TIME_MAX_VALUE)
    { 
      tmp= -TIME_MAX_VALUE;
      break;
    }
    else
    { 
      tmp=(long) floor(fabs(from));                 // Remove fractions

      if (from < 0)
        tmp= -tmp;

      if (tmp % 100 > 59 || tmp/100 % 100 > 59)
      { 
        break;
      }
    }

    return store(tmp, false);

  } while (0);

  std::string tmp(boost::lexical_cast<std::string>(from));
  my_error(ER_INVALID_TIME_VALUE, MYF(0), tmp.c_str());

  return 1;
}

int Time::store(int64_t from, bool)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  /* 
   * Try to create a DateTime from the supplied integer.  Throw an error
   * if unable to create a valid DateTime.  
   */
  drizzled::Time temporal;
  if (not temporal.from_time_t(from))
  {
    /* Convert the integer to a string using boost::lexical_cast */
    std::string tmp(boost::lexical_cast<std::string>(from));
    my_error(ER_INVALID_TIME_VALUE, MYF(0), tmp.c_str());
    return 2;
  }

  pack_time(temporal);

  return 0;
}

void Time::pack_time(drizzled::Time &temporal)
{
  int32_t tmp;
  temporal.to_int32_t(&tmp);
  tmp= htonl(tmp);
  memcpy(ptr, &tmp, sizeof(int32_t));
}

void Time::unpack_time(drizzled::Time &temporal) const
{
  int32_t tmp;

  memcpy(&tmp, ptr, sizeof(int32_t));
  tmp= htonl(tmp);

  temporal.from_int32_t(tmp);
}

void Time::unpack_time(int32_t &destination, const unsigned char *source) const
{
  memcpy(&destination, source, sizeof(int32_t));
  destination= htonl(destination);
}

double Time::val_real(void) const
{
  return (double) Time::val_int();
}

int64_t Time::val_int(void) const
{
  ASSERT_COLUMN_MARKED_FOR_READ;

  drizzled::Time temporal;
  unpack_time(temporal);

  /* We must convert into a "timestamp-formatted integer" ... */
  uint64_t result;
  temporal.to_uint64_t(result);
  return result;
}

String *Time::val_str(String *val_buffer, String *) const
{
  char *to;
  int to_len= field_length + 1;

  val_buffer->alloc(to_len);
  to= (char *) val_buffer->ptr();

  val_buffer->set_charset(&my_charset_bin);	/* Safety */

  drizzled::Time temporal;
  unpack_time(temporal);

  int rlen;
  rlen= temporal.to_string(to, to_len);
  assert(rlen < to_len);

  val_buffer->length(rlen);
  return val_buffer;
}

bool Time::get_date(type::Time &ltime, uint32_t) const
{
  ltime.reset();

  drizzled::Time temporal;
  unpack_time(temporal);

  ltime.time_type= type::DRIZZLE_TIMESTAMP_DATETIME;
  ltime.year= temporal.years();
  ltime.month= temporal.months();
  ltime.day= temporal.days();
  ltime.hour= temporal.hours();
  ltime.minute= temporal.minutes();
  ltime.second= temporal.seconds();

  return 0;
}

bool Time::get_time(type::Time &ltime) const
{
  return Time::get_date(ltime, 0);
}

int Time::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  int32_t a,b;

  unpack_time(a, a_ptr);
  unpack_time(b, b_ptr);

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
  }
  else
#endif
  {
    to[0] = ptr[3];
    to[1] = ptr[2];
    to[2] = ptr[1];
    to[3] = ptr[0];
  }
}

long Time::get_timestamp(bool *null_value) const
{
  if ((*null_value= is_null()))
    return 0;

  uint64_t tmp;
  return unpack_num(tmp);
}

size_t Time::max_string_length()
{
  return sizeof(int64_t);
}

} /* namespace field */
} /* namespace drizzled */
