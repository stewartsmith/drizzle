/* -*- mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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
#include <drizzled/field/microtime.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/current_session.h>
#include <drizzled/temporal.h>
#include <cmath>
#include <sstream>
#include <boost/date_time/posix_time/posix_time.hpp>


namespace drizzled {
namespace field {

static boost::posix_time::ptime _epoch(boost::gregorian::date(1970, 1, 1));

Microtime::Microtime(unsigned char *ptr_arg,
                     unsigned char *null_ptr_arg,
                     unsigned char null_bit_arg,
                     enum utype unireg_check_arg,
                     const char *field_name_arg,
                     drizzled::TableShare *share) :
  Epoch(ptr_arg,
        null_ptr_arg,
        null_bit_arg,
        unireg_check_arg,
        field_name_arg,
        share)
{
}

Microtime::Microtime(bool maybe_null_arg,
                     const char *field_name_arg) :
  Epoch(maybe_null_arg,
        field_name_arg)
{
}

int Microtime::store(const char *from,
                     uint32_t len,
                     const charset_info_st * const )
{
  MicroTimestamp temporal;

  ASSERT_COLUMN_MARKED_FOR_WRITE;

  if (not temporal.from_string(from, (size_t) len))
  {
    my_error(ER_INVALID_TIMESTAMP_VALUE, MYF(ME_FATALERROR), from);
    return 1;
  }

  struct timeval tmp;
  temporal.to_timeval(tmp);

  uint64_t tmp_seconds= tmp.tv_sec;
  uint32_t tmp_micro= tmp.tv_usec;

  pack_num(tmp_seconds);
  pack_num(tmp_micro, ptr +8);

  return 0;
}

int Microtime::store_time(type::Time &ltime, type::timestamp_t )
{
  long my_timezone;
  bool in_dst_time_gap;

  type::Time::epoch_t time_tmp;
  ltime.convert(time_tmp, &my_timezone, &in_dst_time_gap, true);
  uint64_t tmp_seconds= time_tmp;
  uint32_t tmp_micro= ltime.second_part;

  pack_num(tmp_seconds);
  pack_num(tmp_micro, ptr +8);

  return 0;
}

int Microtime::store(double from)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  uint64_t from_tmp= (uint64_t)from;
  type::Time::usec_t fractional_seconds= (type::Time::usec_t)((from - from_tmp) * type::Time::FRACTIONAL_DIGITS) % type::Time::FRACTIONAL_DIGITS;

  MicroTimestamp temporal;
  if (not temporal.from_int64_t(from_tmp))
  {
    /* Convert the integer to a string using boost::lexical_cast */
    std::string tmp(boost::lexical_cast<std::string>(from));

    my_error(ER_INVALID_TIMESTAMP_VALUE, MYF(ME_FATALERROR), tmp.c_str());
    return 2;
  }

  time_t tmp;
  temporal.to_time_t(tmp);

  uint64_t tmp_micro= tmp;
  pack_num(tmp_micro);
  pack_num(fractional_seconds, ptr +8);

  return 0;
}

int Microtime::store(int64_t from, bool)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;

  MicroTimestamp temporal;
  if (not temporal.from_int64_t(from))
  {
    /* Convert the integer to a string using boost::lexical_cast */
    std::string tmp(boost::lexical_cast<std::string>(from));

    my_error(ER_INVALID_TIMESTAMP_VALUE, MYF(ME_FATALERROR), tmp.c_str());
    return 2;
  }

  time_t tmp;
  temporal.to_time_t(tmp);

  uint64_t tmp_micro= tmp;
  pack_num(tmp_micro);
  pack_num(static_cast<uint32_t>(0), ptr +8);

  return 0;
}

double Microtime::val_real(void) const
{
  uint64_t temp;
  type::Time::usec_t micro_temp;

  ASSERT_COLUMN_MARKED_FOR_READ;

  unpack_num(temp);
  unpack_num(micro_temp, ptr +8);

  Timestamp temporal;
  (void) temporal.from_time_t((time_t) temp);

  /* We must convert into a "timestamp-formatted integer" ... */
  int64_t result;
  temporal.to_int64_t(&result);

  result+= micro_temp % type::Time::FRACTIONAL_DIGITS;

  return result;
}

type::Decimal *Microtime::val_decimal(type::Decimal *decimal_value) const
{
  type::Time ltime;

  get_date(ltime, 0);

  return date2_class_decimal(&ltime, decimal_value);
}

int64_t Microtime::val_int(void) const
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

String *Microtime::val_str(String *val_buffer, String *) const
{
  uint64_t temp= 0;
  type::Time::usec_t micro_temp= 0;

  unpack_num(temp);
  unpack_num(micro_temp, ptr +8);

  type::Time tmp_time;
  tmp_time.store(temp, micro_temp);

  tmp_time.convert(*val_buffer);


  return val_buffer;
}

bool Microtime::get_date(type::Time &ltime, uint32_t) const
{
  uint64_t temp;
  uint32_t micro_temp= 0;

  unpack_num(temp);
  unpack_num(micro_temp, ptr +8);
  
  ltime.reset();

  ltime.store(temp, micro_temp);

  return false;
}

bool Microtime::get_time(type::Time &ltime) const
{
  return Microtime::get_date(ltime, 0);
}

int Microtime::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  uint64_t a,b;
  uint32_t a_micro, b_micro;

  unpack_num(a, a_ptr);
  unpack_num(a_micro, a_ptr +8);

  unpack_num(b, b_ptr);
  unpack_num(b_micro, b_ptr +8);

  if (a == b)
    return (a_micro < b_micro) ? -1 : (a_micro > b_micro) ? 1 : 0;

  return (a < b) ? -1 : (a > b) ? 1 : 0;
}


void Microtime::sort_string(unsigned char *to,uint32_t )
{
#ifdef WORDS_BIGENDIAN
  if ((not getTable()) or (not getTable()->getShare()->db_low_byte_first))
  {
    std::reverse_copy(to, to+pack_length(), ptr);
    std::reverse_copy(to +8, to+pack_length(), ptr +8);
  }
  else
#endif
  {
    memcpy(to, ptr, pack_length());
  }
}

void Microtime::set_time()
{
  Session *session= getTable() ? getTable()->in_use : current_session;

  type::Time::usec_t fractional_seconds= 0;
  uint64_t epoch_seconds= session->times.getCurrentTimestampEpoch(fractional_seconds);

  set_notnull();
  pack_num(epoch_seconds);
  pack_num(fractional_seconds, ptr +8);
}

long Microtime::get_timestamp(bool *null_value) const
{
  if ((*null_value= is_null()))
    return 0;

  uint64_t tmp;
  return unpack_num(tmp);
}

} /* namespace field */
} /* namespace drizzled */
