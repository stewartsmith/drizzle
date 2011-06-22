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
#include <boost/lexical_cast.hpp>

#include <drizzled/field/date.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/temporal.h>
#include <drizzled/session.h>
#include <drizzled/time_functions.h>
#include <drizzled/current_session.h>
#include <drizzled/system_variables.h>

#include <math.h>

#include <sstream>
#include <string>

namespace drizzled
{


/****************************************************************************
** Drizzle date type stored in 3 bytes
** In number context: YYYYMMDD
****************************************************************************/

/*
  Store string into a date field

  SYNOPSIS
    Field_date::store()
    from                Date string
    len                 Length of date field
    cs                  Character set (not used)

  RETURN
    0  ok
    1  Value was cut during conversion
    2  Wrong date string
    3  Datetime value that was cut (warning level NOTE)
       This is used by opt_range.cc:get_mm_leaf(). Note that there is a
       nearly-identical class Field_date doesn't ever return 3 from its
       store function.
*/
int Field_date::store(const char *from,
                         uint32_t len,
                         const charset_info_st * const )
{
  /* 
   * Try to create a DateTime from the supplied string.  Throw an error
   * if unable to create a valid DateTime.  A DateTime is used so that
   * automatic conversion from the higher-storage DateTime can be used
   * and matches on datetime format strings can occur.
   */
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  DateTime temporal;
  if (! temporal.from_string(from, (size_t) len))
  {
    my_error(ER_INVALID_DATE_VALUE, MYF(ME_FATALERROR), from);
    return 2;
  }
  /* Create the stored integer format. @TODO This should go away. Should be up to engine... */
  uint32_t int_value= (temporal.years() * 10000) + (temporal.months() * 100) + temporal.days();
  int4store(ptr, int_value);
  return 0;
}

int Field_date::store(double from)
{
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  if (from < 0.0 || from > 99991231235959.0)
  {
    /* Convert the double to a string using stringstream */
    std::stringstream ss;
    std::string tmp;
    ss.precision(18); /* 18 places should be fine for error display of double input. */
    ss << from; ss >> tmp;

    my_error(ER_INVALID_DATE_VALUE, MYF(ME_FATALERROR), tmp.c_str());
    return 2;
  }
  return Field_date::store((int64_t) rint(from), false);
}

int Field_date::store(int64_t from, bool)
{
  /* 
   * Try to create a DateTime from the supplied integer.  Throw an error
   * if unable to create a valid DateTime.  
   */
  ASSERT_COLUMN_MARKED_FOR_WRITE;
  DateTime temporal;
  if (! temporal.from_int64_t(from))
  {
    /* Convert the integer to a string using boost::lexical_cast */
    std::string tmp(boost::lexical_cast<std::string>(from)); 

    my_error(ER_INVALID_DATE_VALUE, MYF(ME_FATALERROR), tmp.c_str());
    return 2;
  }

  /* Create the stored integer format. @TODO This should go away. Should be up to engine... */
  uint32_t int_value= (temporal.years() * 10000) + (temporal.months() * 100) + temporal.days();
  int4store(ptr, int_value);

  return 0;
}

int Field_date::store_time(type::Time &ltime,
                           type::timestamp_t time_type)
{
  long tmp;
  int error= 0;
  if (time_type == type::DRIZZLE_TIMESTAMP_DATE || time_type == type::DRIZZLE_TIMESTAMP_DATETIME)
  {
    tmp= ltime.year*10000 + ltime.month*100 + ltime.day;

    Session *session= getTable() ? getTable()->in_use : current_session;
    type::cut_t cut_error= type::VALID;
    if (ltime.check(tmp != 0,
                     (TIME_FUZZY_DATE |
                      (session->variables.sql_mode & (MODE_NO_ZERO_DATE | MODE_INVALID_DATES))), cut_error))
    {
      char buff[type::Time::MAX_STRING_LENGTH];
      String str(buff, sizeof(buff), &my_charset_utf8_general_ci);
      ltime.convert(str, type::DRIZZLE_TIMESTAMP_DATE);
      set_datetime_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED,
                           str.ptr(), str.length(), type::DRIZZLE_TIMESTAMP_DATE, 1);
    }

    error= static_cast<int>(cut_error);

    if (not error && ltime.time_type != type::DRIZZLE_TIMESTAMP_DATE &&
        (ltime.hour || ltime.minute || ltime.second || ltime.second_part))
    {
      char buff[type::Time::MAX_STRING_LENGTH];
      String str(buff, sizeof(buff), &my_charset_utf8_general_ci);
      ltime.convert(str);
      set_datetime_warning(DRIZZLE_ERROR::WARN_LEVEL_NOTE,
                           ER_WARN_DATA_TRUNCATED,
                           str.ptr(), str.length(), type::DRIZZLE_TIMESTAMP_DATE, 1);
      error= 3;
    }
  }
  else
  {
    tmp=0;
    error= 1;
    set_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
  }

  int4store(ptr,tmp);

  return error;
}

double Field_date::val_real(void) const
{
  return (double) Field_date::val_int();
}

int64_t Field_date::val_int(void) const
{
  uint32_t j;

  ASSERT_COLUMN_MARKED_FOR_READ;

  j= uint4korr(ptr);

  return (int64_t) j;
}

String *Field_date::val_str(String *val_buffer, String *) const
{
  val_buffer->alloc(field_length);
  val_buffer->length(field_length);
  uint32_t tmp=(uint32_t) uint4korr(ptr);
  int32_t part;
  char *pos=(char*) val_buffer->ptr()+10;

  ASSERT_COLUMN_MARKED_FOR_READ;

  /* Open coded to get more speed */
  *pos--=0;					// End NULL
  part=(int32_t) (tmp % 100);
  *pos--= (char) ('0'+part%10);
  *pos--= (char) ('0'+part/10);
  *pos--= '-';
  part=(int32_t) (tmp/100%100);
  *pos--= (char) ('0'+part%10);
  *pos--= (char) ('0'+part/10);
  *pos--= '-';
  part=(int32_t) (tmp/10000);
  *pos--= (char) ('0'+part%10); part/=10;
  *pos--= (char) ('0'+part%10); part/=10;
  *pos--= (char) ('0'+part%10); part/=10;
  *pos=   (char) ('0'+part);
  return val_buffer;
}

bool Field_date::get_date(type::Time &ltime, uint32_t fuzzydate) const
{
  uint32_t tmp=(uint32_t) uint4korr(ptr);
  ltime.day=		(int) (tmp%100);
  ltime.month= 	(int) (tmp/100%100);
  ltime.year= 		(int) (tmp/10000);
  ltime.time_type= type::DRIZZLE_TIMESTAMP_DATE;
  ltime.hour= ltime.minute= ltime.second= ltime.second_part= ltime.neg= 0;

  return ((!(fuzzydate & TIME_FUZZY_DATE) && (!ltime.month || !ltime.day)) ?
          1 : 0);
}

bool Field_date::get_time(type::Time &ltime) const
{
  return Field_date::get_date(ltime ,0);
}

int Field_date::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  uint32_t a,b;
  a=(uint32_t) uint4korr(a_ptr);
  b=(uint32_t) uint4korr(b_ptr);
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_date::sort_string(unsigned char *to,uint32_t )
{
  to[0] = ptr[3];
  to[1] = ptr[2];
  to[2] = ptr[1];
  to[3] = ptr[0];
}

} /* namespace drizzled */
