/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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
#include CSTDINT_H
#include <drizzled/function/time/add_time.h>
#include <drizzled/function/time/make_datetime.h>
#include <drizzled/function/time/make_datetime_with_warn.h>

void Item_func_add_time::fix_length_and_dec()
{
  enum_field_types arg0_field_type;
  decimals=0;
  max_length=MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  maybe_null= 1;

  /*
    The field type for the result of an Item_func_add_time function is defined
    as follows:

    - If first arg is a DRIZZLE_TYPE_DATETIME or DRIZZLE_TYPE_TIMESTAMP
      result is DRIZZLE_TYPE_DATETIME
    - If first arg is a DRIZZLE_TYPE_TIME result is DRIZZLE_TYPE_TIME
    - Otherwise the result is DRIZZLE_TYPE_VARCHAR
  */

  cached_field_type= DRIZZLE_TYPE_VARCHAR;
  arg0_field_type= args[0]->field_type();
  if (arg0_field_type == DRIZZLE_TYPE_DATE ||
      arg0_field_type == DRIZZLE_TYPE_DATETIME ||
      arg0_field_type == DRIZZLE_TYPE_TIMESTAMP)
    cached_field_type= DRIZZLE_TYPE_DATETIME;
  else if (arg0_field_type == DRIZZLE_TYPE_TIME)
    cached_field_type= DRIZZLE_TYPE_TIME;
}

/**
  ADDTIME(t,a) and SUBTIME(t,a) are time functions that calculate a
  time/datetime value

  t: time_or_datetime_expression
  a: time_expression

  Result: Time value or datetime value
*/

String *Item_func_add_time::val_str(String *str)
{
  assert(fixed == 1);
  DRIZZLE_TIME l_time1, l_time2, l_time3;
  bool is_time= 0;
  long days, microseconds;
  int64_t seconds;
  int l_sign= sign;

  null_value=0;
  if (is_date)                        // TIMESTAMP function
  {
    if (get_arg0_date(&l_time1, TIME_FUZZY_DATE) ||
        args[1]->get_time(&l_time2) ||
        l_time1.time_type == DRIZZLE_TIMESTAMP_TIME ||
        l_time2.time_type != DRIZZLE_TIMESTAMP_TIME)
      goto null_date;
  }
  else                                // ADDTIME function
  {
    if (args[0]->get_time(&l_time1) ||
        args[1]->get_time(&l_time2) ||
        l_time2.time_type == DRIZZLE_TIMESTAMP_DATETIME)
      goto null_date;
    is_time= (l_time1.time_type == DRIZZLE_TIMESTAMP_TIME);
  }
  if (l_time1.neg != l_time2.neg)
    l_sign= -l_sign;

  memset(&l_time3, 0, sizeof(l_time3));

  l_time3.neg= calc_time_diff(&l_time1, &l_time2, -l_sign,
			      &seconds, &microseconds);

  /*
    If first argument was negative and diff between arguments
    is non-zero we need to swap sign to get proper result.
  */
  if (l_time1.neg && (seconds || microseconds))
    l_time3.neg= 1-l_time3.neg;         // Swap sign of result

  if (!is_time && l_time3.neg)
    goto null_date;

  days= (long)(seconds/86400L);

  calc_time_from_sec(&l_time3, (long)(seconds%86400L), microseconds);

  if (!is_time)
  {
    get_date_from_daynr(days,&l_time3.year,&l_time3.month,&l_time3.day);
    if (l_time3.day &&
	!make_datetime(l_time1.second_part || l_time2.second_part ?
		       DATE_TIME_MICROSECOND : DATE_TIME,
		       &l_time3, str))
      return str;
    goto null_date;
  }

  l_time3.hour+= days*24;
  if (!make_datetime_with_warn(l_time1.second_part || l_time2.second_part ?
                               TIME_MICROSECOND : TIME_ONLY,
                               &l_time3, str))
    return str;

null_date:
  null_value=1;
  return 0;
}


void Item_func_add_time::print(String *str, enum_query_type query_type)
{
  if (is_date)
  {
    assert(sign > 0);
    str->append(STRING_WITH_LEN("timestamp("));
  }
  else
  {
    if (sign > 0)
      str->append(STRING_WITH_LEN("addtime("));
    else
      str->append(STRING_WITH_LEN("subtime("));
  }
  args[0]->print(str, query_type);
  str->append(',');
  args[1]->print(str, query_type);
  str->append(')');
}

