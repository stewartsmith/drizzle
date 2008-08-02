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

#include <drizzled/field/timetype.h>

/****************************************************************************
** time type
** In string context: HH:MM:SS
** In number context: HHMMSS
** Stored as a 3 byte unsigned int
****************************************************************************/

int Field_time::store(const char *from,
                      uint len,
                      CHARSET_INFO *cs __attribute__((unused)))
{
  DRIZZLE_TIME ltime;
  long tmp;
  int error= 0;
  int warning;

  if (str_to_time(from, len, &ltime, &warning))
  {
    tmp=0L;
    error= 2;
    set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED,
                         from, len, DRIZZLE_TIMESTAMP_TIME, 1);
  }
  else
  {
    if (warning & DRIZZLE_TIME_WARN_TRUNCATED)
    {
      set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                           ER_WARN_DATA_TRUNCATED,
                           from, len, DRIZZLE_TIMESTAMP_TIME, 1);
      error= 1;
    }
    if (warning & DRIZZLE_TIME_WARN_OUT_OF_RANGE)
    {
      set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN, 
                           ER_WARN_DATA_OUT_OF_RANGE,
                           from, len, DRIZZLE_TIMESTAMP_TIME, !error);
      error= 1;
    }
    if (ltime.month)
      ltime.day=0;
    tmp=(ltime.day*24L+ltime.hour)*10000L+(ltime.minute*100+ltime.second);
  }
  
  if (ltime.neg)
    tmp= -tmp;
  int3store(ptr,tmp);
  return error;
}


int Field_time::store_time(DRIZZLE_TIME *ltime,
                           timestamp_type time_type __attribute__((unused)))
{
  long tmp= ((ltime->month ? 0 : ltime->day * 24L) + ltime->hour) * 10000L +
            (ltime->minute * 100 + ltime->second);
  if (ltime->neg)
    tmp= -tmp;
  return Field_time::store((int64_t) tmp, false);
}


int Field_time::store(double nr)
{
  long tmp;
  int error= 0;
  if (nr > (double)TIME_MAX_VALUE)
  {
    tmp= TIME_MAX_VALUE;
    set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                         ER_WARN_DATA_OUT_OF_RANGE, nr, DRIZZLE_TIMESTAMP_TIME);
    error= 1;
  }
  else if (nr < (double)-TIME_MAX_VALUE)
  {
    tmp= -TIME_MAX_VALUE;
    set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN, 
                         ER_WARN_DATA_OUT_OF_RANGE, nr, DRIZZLE_TIMESTAMP_TIME);
    error= 1;
  }
  else
  {
    tmp=(long) floor(fabs(nr));			// Remove fractions
    if (nr < 0)
      tmp= -tmp;
    if (tmp % 100 > 59 || tmp/100 % 100 > 59)
    {
      tmp=0;
      set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN, 
                           ER_WARN_DATA_OUT_OF_RANGE, nr,
                           DRIZZLE_TIMESTAMP_TIME);
      error= 1;
    }
  }
  int3store(ptr,tmp);
  return error;
}


int Field_time::store(int64_t nr, bool unsigned_val)
{
  long tmp;
  int error= 0;
  if (nr < (int64_t) -TIME_MAX_VALUE && !unsigned_val)
  {
    tmp= -TIME_MAX_VALUE;
    set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN, 
                         ER_WARN_DATA_OUT_OF_RANGE, nr,
                         DRIZZLE_TIMESTAMP_TIME, 1);
    error= 1;
  }
  else if (nr > (int64_t) TIME_MAX_VALUE || (nr < 0 && unsigned_val))
  {
    tmp= TIME_MAX_VALUE;
    set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN, 
                         ER_WARN_DATA_OUT_OF_RANGE, nr,
                         DRIZZLE_TIMESTAMP_TIME, 1);
    error= 1;
  }
  else
  {
    tmp=(long) nr;
    if (tmp % 100 > 59 || tmp/100 % 100 > 59)
    {
      tmp=0;
      set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN, 
                           ER_WARN_DATA_OUT_OF_RANGE, nr,
                           DRIZZLE_TIMESTAMP_TIME, 1);
      error= 1;
    }
  }
  int3store(ptr,tmp);
  return error;
}


double Field_time::val_real(void)
{
  uint32_t j= (uint32_t) uint3korr(ptr);
  return (double) j;
}

int64_t Field_time::val_int(void)
{
  return (int64_t) sint3korr(ptr);
}


/**
  @note
  This function is multi-byte safe as the result string is always of type
  my_charset_bin
*/

String *Field_time::val_str(String *val_buffer,
			    String *val_ptr __attribute__((unused)))
{
  DRIZZLE_TIME ltime;
  val_buffer->alloc(MAX_DATE_STRING_REP_LENGTH);
  long tmp=(long) sint3korr(ptr);
  ltime.neg= 0;
  if (tmp < 0)
  {
    tmp= -tmp;
    ltime.neg= 1;
  }
  ltime.day= (uint) 0;
  ltime.hour= (uint) (tmp/10000);
  ltime.minute= (uint) (tmp/100 % 100);
  ltime.second= (uint) (tmp % 100);
  make_time((DATE_TIME_FORMAT*) 0, &ltime, val_buffer);
  return val_buffer;
}


/**
  @note
  Normally we would not consider 'time' as a valid date, but we allow
  get_date() here to be able to do things like
  DATE_FORMAT(time, "%l.%i %p")
*/
 
bool Field_time::get_date(DRIZZLE_TIME *ltime, uint fuzzydate)
{
  long tmp;
  THD *thd= table ? table->in_use : current_thd;
  if (!(fuzzydate & TIME_FUZZY_DATE))
  {
    push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_WARN_DATA_OUT_OF_RANGE,
                        ER(ER_WARN_DATA_OUT_OF_RANGE), field_name,
                        thd->row_count);
    return 1;
  }
  tmp=(long) sint3korr(ptr);
  ltime->neg=0;
  if (tmp < 0)
  {
    ltime->neg= 1;
    tmp=-tmp;
  }
  ltime->hour=tmp/10000;
  tmp-=ltime->hour*10000;
  ltime->minute=   tmp/100;
  ltime->second= tmp % 100;
  ltime->year= ltime->month= ltime->day= ltime->second_part= 0;
  return 0;
}


bool Field_time::get_time(DRIZZLE_TIME *ltime)
{
  long tmp=(long) sint3korr(ptr);
  ltime->neg=0;
  if (tmp < 0)
  {
    ltime->neg= 1;
    tmp=-tmp;
  }
  ltime->day= 0;
  ltime->hour=   (int) (tmp/10000);
  tmp-=ltime->hour*10000;
  ltime->minute= (int) tmp/100;
  ltime->second= (int) tmp % 100;
  ltime->second_part=0;
  ltime->time_type= DRIZZLE_TIMESTAMP_TIME;
  return 0;
}


bool Field_time::send_binary(Protocol *protocol)
{
  DRIZZLE_TIME tm;
  Field_time::get_time(&tm);
  tm.day= tm.hour/24;				// Move hours to days
  tm.hour-= tm.day*24;
  return protocol->store_time(&tm);
}


int Field_time::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  int32_t a,b;
  a=(int32_t) sint3korr(a_ptr);
  b=(int32_t) sint3korr(b_ptr);
  return (a < b) ? -1 : (a > b) ? 1 : 0;
}

void Field_time::sort_string(uchar *to,uint length __attribute__((unused)))
{
  to[0] = (uchar) (ptr[2] ^ 128);
  to[1] = ptr[1];
  to[2] = ptr[0];
}

void Field_time::sql_type(String &res) const
{
  res.set_ascii(STRING_WITH_LEN("time"));
}





