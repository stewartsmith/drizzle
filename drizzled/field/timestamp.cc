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


#include <drizzled/server_includes.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/error.h>
#include <drizzled/tztime.h>
#include <drizzled/table.h>
#include <drizzled/session.h>
#include CMATH_H

#if defined(CMATH_NAMESPACE)
using namespace CMATH_NAMESPACE;
#endif

/**
  TIMESTAMP type holds datetime values in range from 1970-01-01 00:00:01 UTC to
  2038-01-01 00:00:00 UTC stored as number of seconds since Unix
  Epoch in UTC.

  Up to one of timestamps columns in the table can be automatically
  set on row update and/or have NOW() as default value.
  TABLE::timestamp_field points to Field object for such timestamp with
  auto-set-on-update. TABLE::time_stamp holds offset in record + 1 for this
  field, and is used by handler code which performs updates required.

  Actually SQL-99 says that we should allow niladic functions (like NOW())
  as defaults for any field. Current limitations (only NOW() and only
  for one TIMESTAMP field) are because of restricted binary .frm format
  and should go away in the future.

  Also because of this limitation of binary .frm format we use 5 different
  unireg_check values with TIMESTAMP field to distinguish various cases of
  DEFAULT or ON UPDATE values. These values are:

  TIMESTAMP_OLD_FIELD - old timestamp, if there was not any fields with
    auto-set-on-update (or now() as default) in this table before, then this
    field has NOW() as default and is updated when row changes, else it is
    field which has 0 as default value and is not automatically updated.
  TIMESTAMP_DN_FIELD - field with NOW() as default but not set on update
    automatically (TIMESTAMP DEFAULT NOW())
  TIMESTAMP_UN_FIELD - field which is set on update automatically but has not
    NOW() as default (but it may has 0 or some other const timestamp as
    default) (TIMESTAMP ON UPDATE NOW()).
  TIMESTAMP_DNUN_FIELD - field which has now() as default and is auto-set on
    update. (TIMESTAMP DEFAULT NOW() ON UPDATE NOW())
  NONE - field which is not auto-set on update with some other than NOW()
    default value (TIMESTAMP DEFAULT 0).

  Note that TIMESTAMP_OLD_FIELDs are never created explicitly now, they are
  left only for preserving ability to read old tables. Such fields replaced
  with their newer analogs in CREATE TABLE and in SHOW CREATE TABLE. This is
  because we want to prefer NONE unireg_check before TIMESTAMP_OLD_FIELD for
  "TIMESTAMP DEFAULT 'Const'" field. (Old timestamps allowed such
  specification too but ignored default value for first timestamp, which of
  course is non-standard.) In most cases user won't notice any change, only
  exception is different behavior of old/new timestamps during ALTER TABLE.
 */

Field_timestamp::Field_timestamp(unsigned char *ptr_arg,
                                 uint32_t ,
                                 unsigned char *null_ptr_arg, unsigned char null_bit_arg,
                                 enum utype unireg_check_arg,
                                 const char *field_name_arg,
                                 TABLE_SHARE *share,
                                 const CHARSET_INFO * const cs)
  :Field_str(ptr_arg, MAX_DATETIME_WIDTH, null_ptr_arg, null_bit_arg,
	     unireg_check_arg, field_name_arg, cs)
{
  /* For 4.0 MYD and 4.0 InnoDB compatibility */
  flags|= UNSIGNED_FLAG;
  if (!share->timestamp_field && unireg_check != NONE)
  {
    /* This timestamp has auto-update */
    share->timestamp_field= this;
    flags|= TIMESTAMP_FLAG;
    if (unireg_check != TIMESTAMP_DN_FIELD)
      flags|= ON_UPDATE_NOW_FLAG;
  }
}


Field_timestamp::Field_timestamp(bool maybe_null_arg,
                                 const char *field_name_arg,
                                 const CHARSET_INFO * const cs)
  :Field_str((unsigned char*) 0, MAX_DATETIME_WIDTH,
             maybe_null_arg ? (unsigned char*) "": 0, 0,
	     NONE, field_name_arg, cs)
{
  /* For 4.0 MYD and 4.0 InnoDB compatibility */
  flags|= UNSIGNED_FLAG;
    if (unireg_check != TIMESTAMP_DN_FIELD)
      flags|= ON_UPDATE_NOW_FLAG;
}


/**
  Get auto-set type for TIMESTAMP field.

  Returns value indicating during which operations this TIMESTAMP field
  should be auto-set to current timestamp.
*/
timestamp_auto_set_type Field_timestamp::get_auto_set_type() const
{
  switch (unireg_check)
  {
  case TIMESTAMP_DN_FIELD:
    return TIMESTAMP_AUTO_SET_ON_INSERT;
  case TIMESTAMP_UN_FIELD:
    return TIMESTAMP_AUTO_SET_ON_UPDATE;
  case TIMESTAMP_OLD_FIELD:
    /*
      Although we can have several such columns in legacy tables this
      function should be called only for first of them (i.e. the one
      having auto-set property).
    */
    assert(table->timestamp_field == this);
    /* Fall-through */
  case TIMESTAMP_DNUN_FIELD:
    return TIMESTAMP_AUTO_SET_ON_BOTH;
  default:
    /*
      Normally this function should not be called for TIMESTAMPs without
      auto-set property.
    */
    assert(0);
    return TIMESTAMP_NO_AUTO_SET;
  }
}


int Field_timestamp::store(const char *from,
                           uint32_t len,
                           const CHARSET_INFO * const )
{
  DRIZZLE_TIME l_time;
  time_t tmp= 0;
  int error;
  bool have_smth_to_conv;
  bool in_dst_time_gap;
  Session *session= table ? table->in_use : current_session;

  /* We don't want to store invalid or fuzzy datetime values in TIMESTAMP */
  have_smth_to_conv= (str_to_datetime(from, len, &l_time, 1, &error) >
                      DRIZZLE_TIMESTAMP_ERROR);

  if (error || !have_smth_to_conv)
  {
    error= 1;
    set_datetime_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN, ER_WARN_DATA_TRUNCATED,
                         from, len, DRIZZLE_TIMESTAMP_DATETIME, 1);
  }

  /* Only convert a correct date (not a zero date) */
  if (have_smth_to_conv && l_time.month)
  {
    if (!(tmp= TIME_to_timestamp(session, &l_time, &in_dst_time_gap)))
    {
      set_datetime_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN,
                           ER_WARN_DATA_OUT_OF_RANGE,
                           from, len, DRIZZLE_TIMESTAMP_DATETIME, !error);
      error= 1;
    }
    else if (in_dst_time_gap)
    {
      set_datetime_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN,
                           ER_WARN_INVALID_TIMESTAMP,
                           from, len, DRIZZLE_TIMESTAMP_DATETIME, !error);
      error= 1;
    }
  }
  store_timestamp(tmp);
  return error;
}


int Field_timestamp::store(double nr)
{
  int error= 0;
  if (nr < 0 || nr > 99991231235959.0)
  {
    set_datetime_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN,
                         ER_WARN_DATA_OUT_OF_RANGE,
                         nr, DRIZZLE_TIMESTAMP_DATETIME);
    nr= 0;					// Avoid overflow on buff
    error= 1;
  }
  error|= Field_timestamp::store((int64_t) rint(nr), false);
  return error;
}


int Field_timestamp::store(int64_t nr,
                           bool )
{
  DRIZZLE_TIME l_time;
  time_t timestamp= 0;
  int error;
  bool in_dst_time_gap;
  Session *session= table ? table->in_use : current_session;

  /* We don't want to store invalid or fuzzy datetime values in TIMESTAMP */
  int64_t tmp= number_to_datetime(nr, &l_time, (session->variables.sql_mode &
                                                 MODE_NO_ZERO_DATE), &error);
  if (tmp == INT64_C(-1))
  {
    error= 2;
  }

  if (!error && tmp)
  {
    if (!(timestamp= TIME_to_timestamp(session, &l_time, &in_dst_time_gap)))
    {
      set_datetime_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN,
                           ER_WARN_DATA_OUT_OF_RANGE,
                           nr, DRIZZLE_TIMESTAMP_DATETIME, 1);
      error= 1;
    }
    if (in_dst_time_gap)
    {
      set_datetime_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN,
                           ER_WARN_INVALID_TIMESTAMP,
                           nr, DRIZZLE_TIMESTAMP_DATETIME, 1);
      error= 1;
    }
  } else if (error)
    set_datetime_warning(DRIZZLE_ERROR::WARN_LEVEL_WARN,
                         ER_WARN_DATA_TRUNCATED,
                         nr, DRIZZLE_TIMESTAMP_DATETIME, 1);

  store_timestamp(timestamp);
  return error;
}

double Field_timestamp::val_real(void)
{
  return (double) Field_timestamp::val_int();
}

int64_t Field_timestamp::val_int(void)
{
  uint32_t temp;
  DRIZZLE_TIME time_tmp;
  Session  *session= table ? table->in_use : current_session;

#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
    temp=uint4korr(ptr);
  else
#endif
    longget(temp,ptr);

  if (temp == 0L)				// No time
    return(0);					/* purecov: inspected */

  session->variables.time_zone->gmt_sec_to_TIME(&time_tmp, (time_t)temp);

  return time_tmp.year * INT64_C(10000000000) +
         time_tmp.month * INT64_C(100000000) +
         time_tmp.day * 1000000 + time_tmp.hour * 10000 +
         time_tmp.minute * 100 + time_tmp.second;
}


String *Field_timestamp::val_str(String *val_buffer, String *val_ptr)
{
  uint32_t temp, temp2;
  DRIZZLE_TIME time_tmp;
  Session *session= table ? table->in_use : current_session;
  char *to;

  val_buffer->alloc(field_length+1);
  to= (char*) val_buffer->ptr();
  val_buffer->length(field_length);

#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
    temp=uint4korr(ptr);
  else
#endif
    longget(temp,ptr);

  if (temp == 0L)
  {				      /* Zero time is "000000" */
    val_ptr->set(STRING_WITH_LEN("0000-00-00 00:00:00"), &my_charset_bin);
    return val_ptr;
  }
  val_buffer->set_charset(&my_charset_bin);	// Safety

  session->variables.time_zone->gmt_sec_to_TIME(&time_tmp,(time_t)temp);

  temp= time_tmp.year % 100;
  if (temp < YY_PART_YEAR - 1)
  {
    *to++= '2';
    *to++= '0';
  }
  else
  {
    *to++= '1';
    *to++= '9';
  }
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= '-';
  temp=time_tmp.month;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= '-';
  temp=time_tmp.day;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= ' ';
  temp=time_tmp.hour;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= ':';
  temp=time_tmp.minute;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to++= ':';
  temp=time_tmp.second;
  temp2=temp/10; temp=temp-temp2*10;
  *to++= (char) ('0'+(char) (temp2));
  *to++= (char) ('0'+(char) (temp));
  *to= 0;
  return val_buffer;
}


bool Field_timestamp::get_date(DRIZZLE_TIME *ltime, uint32_t fuzzydate)
{
  long temp;
  Session *session= table ? table->in_use : current_session;
#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
    temp=uint4korr(ptr);
  else
#endif
    longget(temp,ptr);
  if (temp == 0L)
  {				      /* Zero time is "000000" */
    if (fuzzydate & TIME_NO_ZERO_DATE)
      return 1;
    memset(ltime, 0, sizeof(*ltime));
  }
  else
  {
    session->variables.time_zone->gmt_sec_to_TIME(ltime, (time_t)temp);
  }
  return 0;
}

bool Field_timestamp::get_time(DRIZZLE_TIME *ltime)
{
  return Field_timestamp::get_date(ltime,0);
}


bool Field_timestamp::send_binary(Protocol *protocol)
{
  DRIZZLE_TIME tm;
  Field_timestamp::get_date(&tm, 0);
  return protocol->store(&tm);
}


int Field_timestamp::cmp(const unsigned char *a_ptr, const unsigned char *b_ptr)
{
  int32_t a,b;
#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
  {
    a=sint4korr(a_ptr);
    b=sint4korr(b_ptr);
  }
  else
#endif
  {
  longget(a,a_ptr);
  longget(b,b_ptr);
  }
  return ((uint32_t) a < (uint32_t) b) ? -1 : ((uint32_t) a > (uint32_t) b) ? 1 : 0;
}


void Field_timestamp::sort_string(unsigned char *to,uint32_t )
{
#ifdef WORDS_BIGENDIAN
  if (!table || !table->s->db_low_byte_first)
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


void Field_timestamp::sql_type(String &res) const
{
  res.set_ascii(STRING_WITH_LEN("timestamp"));
}


void Field_timestamp::set_time()
{
  Session *session= table ? table->in_use : current_session;
  long tmp= (long) session->query_start();
  set_notnull();
  store_timestamp(tmp);
}


void Field_timestamp::set_default()
{
  if (table->timestamp_field == this &&
      unireg_check != TIMESTAMP_UN_FIELD)
    set_time();
  else
    Field::set_default();
}

long Field_timestamp::get_timestamp(bool *null_value)
{
  if ((*null_value= is_null()))
    return 0;
#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
    return sint4korr(ptr);
#endif
  long tmp;
  longget(tmp,ptr);
  return tmp;
}


void Field_timestamp::store_timestamp(time_t timestamp)
{
#ifdef WORDS_BIGENDIAN
  if (table && table->s->db_low_byte_first)
  {
    int4store(ptr,timestamp);
  }
  else
#endif
    longstore(ptr,(uint32_t) timestamp);
}

