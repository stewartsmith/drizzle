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

#include <drizzled/field/datetime.h>

/****************************************************************************
** datetime type
** In string context: YYYY-MM-DD HH:MM:DD
** In number context: YYYYMMDDHHMMDD
** Stored as a 8 byte unsigned int. Should sometimes be change to a 6 byte int.
****************************************************************************/

int Field_datetime::store(const char *from,
                          uint len,
                          CHARSET_INFO *cs __attribute__((unused)))
{
  DRIZZLE_TIME time_tmp;
  int error;
  uint64_t tmp= 0;
  enum enum_drizzle_timestamp_type func_res;
  THD *thd= table ? table->in_use : current_thd;

  func_res= str_to_datetime(from, len, &time_tmp,
                            (TIME_FUZZY_DATE |
                             (thd->variables.sql_mode &
                              (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE |
                               MODE_INVALID_DATES))),
                            &error);
  if ((int) func_res > (int) DRIZZLE_TIMESTAMP_ERROR)
    tmp= TIME_to_uint64_t_datetime(&time_tmp);
  else
    error= 1;                                 // Fix if invalid zero date

  if (error)
    set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                         ER_WARN_DATA_OUT_OF_RANGE,
                         from, len, DRIZZLE_TIMESTAMP_DATETIME, 1);

#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
  {
    int8store(ptr,tmp);
  }
  else
#endif
    int64_tstore(ptr,tmp);
  return error;
}


int Field_datetime::store(double nr)
{
  int error= 0;
  if (nr < 0.0 || nr > 99991231235959.0)
  {
    set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN, 
                         ER_WARN_DATA_OUT_OF_RANGE,
                         nr, DRIZZLE_TIMESTAMP_DATETIME);
    nr= 0.0;
    error= 1;
  }
  error|= Field_datetime::store((int64_t) rint(nr), false);
  return error;
}


int Field_datetime::store(int64_t nr,
                          bool unsigned_val __attribute__((unused)))
{
  DRIZZLE_TIME not_used;
  int error;
  int64_t initial_nr= nr;
  THD *thd= table ? table->in_use : current_thd;

  nr= number_to_datetime(nr, &not_used, (TIME_FUZZY_DATE |
                                         (thd->variables.sql_mode &
                                          (MODE_NO_ZERO_IN_DATE |
                                           MODE_NO_ZERO_DATE |
                                           MODE_INVALID_DATES))), &error);

  if (nr == -1LL)
  {
    nr= 0;
    error= 2;
  }

  if (error)
    set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN,
                         error == 2 ? ER_WARN_DATA_OUT_OF_RANGE :
                         ER_WARN_DATA_TRUNCATED, initial_nr,
                         DRIZZLE_TIMESTAMP_DATETIME, 1);

#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
  {
    int8store(ptr,nr);
  }
  else
#endif
    int64_tstore(ptr,nr);
  return error;
}


int Field_datetime::store_time(DRIZZLE_TIME *ltime,timestamp_type time_type)
{
  int64_t tmp;
  int error= 0;
  /*
    We don't perform range checking here since values stored in TIME
    structure always fit into DATETIME range.
  */
  if (time_type == DRIZZLE_TIMESTAMP_DATE ||
      time_type == DRIZZLE_TIMESTAMP_DATETIME)
  {
    tmp=((ltime->year*10000L+ltime->month*100+ltime->day)*1000000LL+
	 (ltime->hour*10000L+ltime->minute*100+ltime->second));
    if (check_date(ltime, tmp != 0,
                   (TIME_FUZZY_DATE |
                    (current_thd->variables.sql_mode &
                     (MODE_NO_ZERO_IN_DATE | MODE_NO_ZERO_DATE |
                      MODE_INVALID_DATES))), &error))
    {
      char buff[MAX_DATE_STRING_REP_LENGTH];
      String str(buff, sizeof(buff), &my_charset_latin1);
      make_datetime((DATE_TIME_FORMAT *) 0, ltime, &str);
      set_datetime_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED,
                           str.ptr(), str.length(), DRIZZLE_TIMESTAMP_DATETIME,1);
    }
  }
  else
  {
    tmp=0;
    error= 1;
    set_warning(MYSQL_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED, 1);
  }
#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
  {
    int8store(ptr,tmp);
  }
  else
#endif
    int64_tstore(ptr,tmp);
  return error;
}

bool Field_datetime::send_binary(Protocol *protocol)
{
  DRIZZLE_TIME tm;
  Field_datetime::get_date(&tm, TIME_FUZZY_DATE);
  return protocol->store(&tm);
}


double Field_datetime::val_real(void)
{
  return (double) Field_datetime::val_int();
}

int64_t Field_datetime::val_int(void)
{
  int64_t j;
#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
    j=sint8korr(ptr);
  else
#endif
    int64_tget(j,ptr);
  return j;
}


String *Field_datetime::val_str(String *val_buffer,
				String *val_ptr __attribute__((unused)))
{
  val_buffer->alloc(field_length);
  val_buffer->length(field_length);
  uint64_t tmp;
  long part1,part2;
  char *pos;
  int part3;

#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
    tmp=sint8korr(ptr);
  else
#endif
    int64_tget(tmp,ptr);

  /*
    Avoid problem with slow int64_t arithmetic and sprintf
  */

  part1=(long) (tmp/1000000LL);
  part2=(long) (tmp - (uint64_t) part1*1000000LL);

  pos=(char*) val_buffer->ptr() + MAX_DATETIME_WIDTH;
  *pos--=0;
  *pos--= (char) ('0'+(char) (part2%10)); part2/=10;
  *pos--= (char) ('0'+(char) (part2%10)); part3= (int) (part2 / 10);
  *pos--= ':';
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= ':';
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= (char) ('0'+(char) part3);
  *pos--= ' ';
  *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
  *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
  *pos--= '-';
  *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
  *pos--= (char) ('0'+(char) (part1%10)); part3= (int) (part1/10);
  *pos--= '-';
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
  *pos=(char) ('0'+(char) part3);
  return val_buffer;
}

bool Field_datetime::get_date(DRIZZLE_TIME *ltime, uint fuzzydate)
{
  int64_t tmp=Field_datetime::val_int();
  uint32_t part1,part2;
  part1=(uint32_t) (tmp/1000000LL);
  part2=(uint32_t) (tmp - (uint64_t) part1*1000000LL);

  ltime->time_type=	DRIZZLE_TIMESTAMP_DATETIME;
  ltime->neg=		0;
  ltime->second_part=	0;
  ltime->second=	(int) (part2%100);
  ltime->minute=	(int) (part2/100%100);
  ltime->hour=		(int) (part2/10000);
  ltime->day=		(int) (part1%100);
  ltime->month= 	(int) (part1/100%100);
  ltime->year= 		(int) (part1/10000);
  return (!(fuzzydate & TIME_FUZZY_DATE) && (!ltime->month || !ltime->day)) ? 1 : 0;
}

bool Field_datetime::get_time(DRIZZLE_TIME *ltime)
{
  return Field_datetime::get_date(ltime,0);
}

int Field_datetime::cmp(const uchar *a_ptr, const uchar *b_ptr)
{
  int64_t a,b;
#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
  {
    a=sint8korr(a_ptr);
    b=sint8korr(b_ptr);
  }
  else
#endif
  {
    int64_tget(a,a_ptr);
    int64_tget(b,b_ptr);
  }
  return ((uint64_t) a < (uint64_t) b) ? -1 :
    ((uint64_t) a > (uint64_t) b) ? 1 : 0;
}

void Field_datetime::sort_string(uchar *to,uint length __attribute__((unused)))
{
#ifdef WORDS_BIGENDIAN
  if (!table || !table->s->db_low_byte_first)
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


void Field_datetime::sql_type(String &res) const
{
  res.set_ascii(STRING_WITH_LEN("datetime"));
}

