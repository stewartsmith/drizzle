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
#include <drizzled/current_session.h>
#include CSTDINT_H
#include <drizzled/sql_locale.h>
#include <drizzled/error.h>
#include <drizzled/function/time/str_to_date.h>
#include <drizzled/function/time/make_datetime.h>

/*
  Date formats corresponding to compound %r and %T conversion specifiers

  Note: We should init at least first element of "positions" array
        (first member) or hpux11 compiler will die horribly.
*/
static DATE_TIME_FORMAT time_ampm_format= {{0}, '\0', 0,
                                           {(char *)"%I:%i:%S %p", 11}};
static DATE_TIME_FORMAT time_24hrs_format= {{0}, '\0', 0,
                                            {(char *)"%H:%i:%S", 8}};

/**
  Extract datetime value to DRIZZLE_TIME struct from string value
  according to format string.

  @param format		date/time format specification
  @param val			String to decode
  @param length		Length of string
  @param l_time		Store result here
  @param cached_timestamp_type  It uses to get an appropriate warning
                                in the case when the value is truncated.
  @param sub_pattern_end    if non-zero then we are parsing string which
                            should correspond compound specifier (like %T or
                            %r) and this parameter is pointer to place where
                            pointer to end of string matching this specifier
                            should be stored.

  @note
    Possibility to parse strings matching to patterns equivalent to compound
    specifiers is mainly intended for use from inside of this function in
    order to understand %T and %r conversion specifiers, so number of
    conversion specifiers that can be used in such sub-patterns is limited.
    Also most of checks are skipped in this case.

  @note
    If one adds new format specifiers to this function he should also
    consider adding them to get_date_time_result_type() function.

  @retval
    0	ok
  @retval
    1	error
*/

static bool extract_date_time(DATE_TIME_FORMAT *format,
			      const char *val, uint32_t length, DRIZZLE_TIME *l_time,
                              enum enum_drizzle_timestamp_type cached_timestamp_type,
                              const char **sub_pattern_end,
                              const char *date_time_type)
{
  int weekday= 0, yearday= 0, daypart= 0;
  int week_number= -1;
  int error= 0;
  int  strict_week_number_year= -1;
  int frac_part;
  bool usa_time= 0;
  bool sunday_first_n_first_week_non_iso= false;
  bool strict_week_number= false;
  bool strict_week_number_year_type= false;
  const char *val_begin= val;
  const char *val_end= val + length;
  const char *ptr= format->format.str;
  const char *end= ptr + format->format.length;
  const CHARSET_INFO * const cs= &my_charset_bin;

  if (!sub_pattern_end)
    memset(l_time, 0, sizeof(*l_time));

  for (; ptr != end && val != val_end; ptr++)
  {
    /* Skip pre-space between each argument */
    while (val != val_end && my_isspace(cs, *val))
      val++;

    if (*ptr == '%' && ptr+1 != end)
    {
      int val_len;
      char *tmp;

      error= 0;

      val_len= (uint) (val_end - val);
      switch (*++ptr) {
	/* Year */
      case 'Y':
	tmp= (char*) val + cmin(4, val_len);
	l_time->year= (int) my_strtoll10(val, &tmp, &error);
        if ((int) (tmp-val) <= 2)
          l_time->year= year_2000_handling(l_time->year);
	val= tmp;
	break;
      case 'y':
	tmp= (char*) val + cmin(2, val_len);
	l_time->year= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
        l_time->year= year_2000_handling(l_time->year);
	break;

	/* Month */
      case 'm':
      case 'c':
	tmp= (char*) val + cmin(2, val_len);
	l_time->month= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;
      case 'M':
	if ((l_time->month= check_word(my_locale_en_US.month_names,
				       val, val_end, &val)) <= 0)
	  goto err;
	break;
      case 'b':
	if ((l_time->month= check_word(my_locale_en_US.ab_month_names,
				       val, val_end, &val)) <= 0)
	  goto err;
	break;
	/* Day */
      case 'd':
      case 'e':
	tmp= (char*) val + cmin(2, val_len);
	l_time->day= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;
      case 'D':
	tmp= (char*) val + cmin(2, val_len);
	l_time->day= (int) my_strtoll10(val, &tmp, &error);
	/* Skip 'st, 'nd, 'th .. */
	val= tmp + cmin((int) (val_end-tmp), 2);
	break;

	/* Hour */
      case 'h':
      case 'I':
      case 'l':
	usa_time= 1;
	/* fall through */
      case 'k':
      case 'H':
	tmp= (char*) val + cmin(2, val_len);
	l_time->hour= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

	/* Minute */
      case 'i':
	tmp= (char*) val + cmin(2, val_len);
	l_time->minute= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

	/* Second */
      case 's':
      case 'S':
	tmp= (char*) val + cmin(2, val_len);
	l_time->second= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

	/* Second part */
      case 'f':
	tmp= (char*) val_end;
	if (tmp - val > 6)
	  tmp= (char*) val + 6;
	l_time->second_part= (int) my_strtoll10(val, &tmp, &error);
	frac_part= 6 - (tmp - val);
	if (frac_part > 0)
	  l_time->second_part*= (ulong) log_10_int[frac_part];
	val= tmp;
	break;

	/* AM / PM */
      case 'p':
	if (val_len < 2 || ! usa_time)
	  goto err;
	if (!my_strnncoll(&my_charset_utf8_general_ci,
			  (const unsigned char *) val, 2,
			  (const unsigned char *) "PM", 2))
	  daypart= 12;
	else if (my_strnncoll(&my_charset_utf8_general_ci,
			      (const unsigned char *) val, 2,
			      (const unsigned char *) "AM", 2))
	  goto err;
	val+= 2;
	break;

	/* Exotic things */
      case 'W':
	if ((weekday= check_word(my_locale_en_US.day_names, val, val_end, &val)) <= 0)
	  goto err;
	break;
      case 'a':
	if ((weekday= check_word(my_locale_en_US.ab_day_names, val, val_end, &val)) <= 0)
	  goto err;
	break;
      case 'w':
	tmp= (char*) val + 1;
	if ((weekday= (int) my_strtoll10(val, &tmp, &error)) < 0 ||
	    weekday >= 7)
	  goto err;
        /* We should use the same 1 - 7 scale for %w as for %W */
        if (!weekday)
          weekday= 7;
	val= tmp;
	break;
      case 'j':
	tmp= (char*) val + cmin(val_len, 3);
	yearday= (int) my_strtoll10(val, &tmp, &error);
	val= tmp;
	break;

        /* Week numbers */
      case 'V':
      case 'U':
      case 'v':
      case 'u':
        sunday_first_n_first_week_non_iso= (*ptr=='U' || *ptr== 'V');
        strict_week_number= (*ptr=='V' || *ptr=='v');
	tmp= (char*) val + cmin(val_len, 2);
	if ((week_number= (int) my_strtoll10(val, &tmp, &error)) < 0 ||
            (strict_week_number && !week_number) ||
            week_number > 53)
          goto err;
	val= tmp;
	break;

        /* Year used with 'strict' %V and %v week numbers */
      case 'X':
      case 'x':
        strict_week_number_year_type= (*ptr=='X');
        tmp= (char*) val + cmin(4, val_len);
        strict_week_number_year= (int) my_strtoll10(val, &tmp, &error);
        val= tmp;
        break;

        /* Time in AM/PM notation */
      case 'r':
        /*
          We can't just set error here, as we don't want to generate two
          warnings in case of errors
        */
        if (extract_date_time(&time_ampm_format, val,
                              (uint)(val_end - val), l_time,
                              cached_timestamp_type, &val, "time"))
          return(1);
        break;

        /* Time in 24-hour notation */
      case 'T':
        if (extract_date_time(&time_24hrs_format, val,
                              (uint)(val_end - val), l_time,
                              cached_timestamp_type, &val, "time"))
          return(1);
        break;

        /* Conversion specifiers that match classes of characters */
      case '.':
	while (my_ispunct(cs, *val) && val != val_end)
	  val++;
	break;
      case '@':
	while (my_isalpha(cs, *val) && val != val_end)
	  val++;
	break;
      case '#':
	while (my_isdigit(cs, *val) && val != val_end)
	  val++;
	break;
      default:
	goto err;
      }
      if (error)				// Error from my_strtoll10
	goto err;
    }
    else if (!my_isspace(cs, *ptr))
    {
      if (*val != *ptr)
	goto err;
      val++;
    }
  }
  if (usa_time)
  {
    if (l_time->hour > 12 || l_time->hour < 1)
      goto err;
    l_time->hour= l_time->hour%12+daypart;
  }

  /*
    If we are recursively called for parsing string matching compound
    specifiers we are already done.
  */
  if (sub_pattern_end)
  {
    *sub_pattern_end= val;
    return(0);
  }

  if (yearday > 0)
  {
    uint32_t days;
    days= calc_daynr(l_time->year,1,1) +  yearday - 1;
    if (days <= 0 || days > MAX_DAY_NUMBER)
      goto err;
    get_date_from_daynr(days,&l_time->year,&l_time->month,&l_time->day);
  }

  if (week_number >= 0 && weekday)
  {
    int days;
    uint32_t weekday_b;

    /*
      %V,%v require %X,%x resprectively,
      %U,%u should be used with %Y and not %X or %x
    */
    if ((strict_week_number &&
        (strict_week_number_year < 0 || (strict_week_number_year_type != sunday_first_n_first_week_non_iso))) ||
        (!strict_week_number && strict_week_number_year >= 0))
      goto err;

    /* Number of days since year 0 till 1st Jan of this year */
    days= calc_daynr((strict_week_number ? strict_week_number_year :
                                           l_time->year),
                     1, 1);
    /* Which day of week is 1st Jan of this year */
    weekday_b= calc_weekday(days, sunday_first_n_first_week_non_iso);

    /*
      Below we are going to sum:
      1) number of days since year 0 till 1st day of 1st week of this year
      2) number of days between 1st week and our week
      3) and position of our day in the week
    */
    if (sunday_first_n_first_week_non_iso)
    {
      days+= ((weekday_b == 0) ? 0 : 7) - weekday_b +
             (week_number - 1) * 7 +
             weekday % 7;
    }
    else
    {
      days+= ((weekday_b <= 3) ? 0 : 7) - weekday_b +
             (week_number - 1) * 7 +
             (weekday - 1);
    }

    if (days <= 0 || days > MAX_DAY_NUMBER)
      goto err;
    get_date_from_daynr(days,&l_time->year,&l_time->month,&l_time->day);
  }

  if (l_time->month > 12 || l_time->day > 31 || l_time->hour > 23 ||
      l_time->minute > 59 || l_time->second > 59)
    goto err;

  if (val != val_end)
  {
    do
    {
      if (!my_isspace(&my_charset_utf8_general_ci,*val))
      {
	make_truncated_value_warning(current_session, DRIZZLE_ERROR::WARN_LEVEL_WARN,
                                     val_begin, length,
				     cached_timestamp_type, NULL);
	break;
      }
    } while (++val != val_end);
  }
  return(0);

err:
  {
    char buff[128];
    size_t len= cmin(length, sizeof(buff)-1);
    strncpy(buff, val_begin, len);
    buff[len]= '\0';
    push_warning_printf(current_session, DRIZZLE_ERROR::WARN_LEVEL_ERROR,
                        ER_WRONG_VALUE_FOR_TYPE, ER(ER_WRONG_VALUE_FOR_TYPE),
                        date_time_type, buff, "str_to_date");
  }
  return(1);
}




/**
  Get type of datetime value (DATE/TIME/...) which will be produced
  according to format string.

  @param format   format string
  @param length   length of format string

  @note
    We don't process day format's characters('D', 'd', 'e') because day
    may be a member of all date/time types.

  @note
    Format specifiers supported by this function should be in sync with
    specifiers supported by extract_date_time() function.

  @return
    One of date_time_format_types values:
    - DATE_TIME_MICROSECOND
    - DATE_TIME
    - DATE_ONLY
    - TIME_MICROSECOND
    - TIME_ONLY
*/

static date_time_format_types
get_date_time_result_type(const char *format, uint32_t length)
{
  const char *time_part_frms= "HISThiklrs";
  const char *date_part_frms= "MVUXYWabcjmvuxyw";
  bool date_part_used= 0, time_part_used= 0, frac_second_used= 0;

  const char *val= format;
  const char *end= format + length;

  for (; val != end && val != end; val++)
  {
    if (*val == '%' && val+1 != end)
    {
      val++;
      if (*val == 'f')
        frac_second_used= time_part_used= 1;
      else if (!time_part_used && strchr(time_part_frms, *val))
	time_part_used= 1;
      else if (!date_part_used && strchr(date_part_frms, *val))
	date_part_used= 1;
      if (date_part_used && frac_second_used)
      {
        /*
          frac_second_used implies time_part_used, and thus we already
          have all types of date-time components and can end our search.
        */
	return DATE_TIME_MICROSECOND;
    }
  }
  }

  /* We don't have all three types of date-time components */
  if (frac_second_used)
    return TIME_MICROSECOND;
  if (time_part_used)
  {
    if (date_part_used)
      return DATE_TIME;
    return TIME_ONLY;
  }
  return DATE_ONLY;
}


void Item_func_str_to_date::fix_length_and_dec()
{
  maybe_null= 1;
  decimals=0;
  cached_field_type= DRIZZLE_TYPE_DATETIME;
  max_length= MAX_DATETIME_FULL_WIDTH*MY_CHARSET_BIN_MB_MAXLEN;
  cached_timestamp_type= DRIZZLE_TIMESTAMP_NONE;
  if ((const_item= args[1]->const_item()))
  {
    char format_buff[64];
    String format_str(format_buff, sizeof(format_buff), &my_charset_bin);
    String *format= args[1]->val_str(&format_str);
    if (!args[1]->null_value)
    {
      cached_format_type= get_date_time_result_type(format->ptr(),
                                                    format->length());
      switch (cached_format_type) {
      case DATE_ONLY:
        cached_timestamp_type= DRIZZLE_TIMESTAMP_DATE;
        cached_field_type= DRIZZLE_TYPE_DATE;
        max_length= MAX_DATE_WIDTH * MY_CHARSET_BIN_MB_MAXLEN;
        break;
      case TIME_ONLY:
      case TIME_MICROSECOND:
        cached_timestamp_type= DRIZZLE_TIMESTAMP_TIME;
        cached_field_type= DRIZZLE_TYPE_TIME;
        max_length= MAX_TIME_WIDTH * MY_CHARSET_BIN_MB_MAXLEN;
        break;
      default:
        cached_timestamp_type= DRIZZLE_TIMESTAMP_DATETIME;
        cached_field_type= DRIZZLE_TYPE_DATETIME;
        break;
      }
    }
  }
}


bool Item_func_str_to_date::get_date(DRIZZLE_TIME *ltime, uint32_t fuzzy_date)
{
  DATE_TIME_FORMAT date_time_format;
  char val_buff[64], format_buff[64];
  String val_string(val_buff, sizeof(val_buff), &my_charset_bin), *val;
  String format_str(format_buff, sizeof(format_buff), &my_charset_bin), *format;

  val=    args[0]->val_str(&val_string);
  format= args[1]->val_str(&format_str);
  if (args[0]->null_value || args[1]->null_value)
    goto null_date;

  null_value= 0;
  memset(ltime, 0, sizeof(*ltime));
  date_time_format.format.str=    (char*) format->ptr();
  date_time_format.format.length= format->length();
  if (extract_date_time(&date_time_format, val->ptr(), val->length(),
			ltime, cached_timestamp_type, 0, "datetime") ||
      ((fuzzy_date & TIME_NO_ZERO_DATE) &&
       (ltime->year == 0 || ltime->month == 0 || ltime->day == 0)))
    goto null_date;
  if (cached_timestamp_type == DRIZZLE_TIMESTAMP_TIME && ltime->day)
  {
    /*
      Day part for time type can be nonzero value and so
      we should add hours from day part to hour part to
      keep valid time value.
    */
    ltime->hour+= ltime->day*24;
    ltime->day= 0;
  }
  return 0;

null_date:
  return (null_value=1);
}


String *Item_func_str_to_date::val_str(String *str)
{
  assert(fixed == 1);
  DRIZZLE_TIME ltime;

  if (Item_func_str_to_date::get_date(&ltime, TIME_FUZZY_DATE))
    return 0;

  if (!make_datetime((const_item ? cached_format_type :
		     (ltime.second_part ? DATE_TIME_MICROSECOND : DATE_TIME)),
		     &ltime, str))
    return str;
  return 0;
}
