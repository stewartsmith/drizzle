/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <config.h>
#include <drizzled/function/time/date_format.h>
#include <drizzled/session.h>
#include <drizzled/time_functions.h>
#include <drizzled/internal/m_string.h>
#include <drizzled/typelib.h>
#include <drizzled/system_variables.h>

#include <cstdio>
#include <algorithm>

using namespace std;

namespace drizzled {

/**
  Create a formated date/time value in a string.
*/

static bool make_date_time(Session &session,
                           String *format, type::Time *l_time,
                           type::timestamp_t type, String *str)
{
  char intbuff[15];
  uint32_t hours_i;
  uint32_t weekday;
  ulong length;
  const char *ptr, *end;
  MY_LOCALE *locale= session.variables.lc_time_names;

  str->length(0);

  if (l_time->neg)
    str->append('-');

  end= (ptr= format->c_ptr()) + format->length();
  for (; ptr != end ; ptr++)
  {
    if (*ptr != '%' || ptr+1 == end)
      str->append(*ptr);
    else
    {
      switch (*++ptr) {
      case 'M':
        if (!l_time->month)
          return 1;
        str->append(locale->month_names->type_names[l_time->month-1],
                    strlen(locale->month_names->type_names[l_time->month-1]),
                    system_charset_info);
        break;
      case 'b':
        if (!l_time->month)
          return 1;
        str->append(locale->ab_month_names->type_names[l_time->month-1],
                    strlen(locale->ab_month_names->type_names[l_time->month-1]),
                    system_charset_info);
        break;
      case 'W':
        if (type == type::DRIZZLE_TIMESTAMP_TIME)
          return 1;
        weekday= calc_weekday(calc_daynr(l_time->year,l_time->month,
                              l_time->day),0);
        str->append(locale->day_names->type_names[weekday],
                    strlen(locale->day_names->type_names[weekday]),
                    system_charset_info);
        break;
      case 'a':
        if (type == type::DRIZZLE_TIMESTAMP_TIME)
          return 1;
        weekday=calc_weekday(calc_daynr(l_time->year,l_time->month,
                             l_time->day),0);
        str->append(locale->ab_day_names->type_names[weekday],
                    strlen(locale->ab_day_names->type_names[weekday]),
                    system_charset_info);
        break;
      case 'D':
	if (type == type::DRIZZLE_TIMESTAMP_TIME)
	  return 1;
	length= internal::int10_to_str(l_time->day, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	if (l_time->day >= 10 &&  l_time->day <= 19)
	  str->append(STRING_WITH_LEN("th"));
	else
	{
	  switch (l_time->day %10) {
	  case 1:
	    str->append(STRING_WITH_LEN("st"));
	    break;
	  case 2:
	    str->append(STRING_WITH_LEN("nd"));
	    break;
	  case 3:
	    str->append(STRING_WITH_LEN("rd"));
	    break;
	  default:
	    str->append(STRING_WITH_LEN("th"));
	    break;
	  }
	}
	break;
      case 'Y':
	length= internal::int10_to_str(l_time->year, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 4, '0');
	break;
      case 'y':
	length= internal::int10_to_str(l_time->year%100, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'm':
	length= internal::int10_to_str(l_time->month, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'c':
	length= internal::int10_to_str(l_time->month, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'd':
	length= internal::int10_to_str(l_time->day, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'e':
	length= internal::int10_to_str(l_time->day, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'f':
	length= internal::int10_to_str(l_time->second_part, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 6, '0');
	break;
      case 'H':
	length= internal::int10_to_str(l_time->hour, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'h':
      case 'I':
	hours_i= (l_time->hour%24 + 11)%12+1;
	length= internal::int10_to_str(hours_i, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'i':					/* minutes */
	length= internal::int10_to_str(l_time->minute, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'j':
	if (type == type::DRIZZLE_TIMESTAMP_TIME)
	  return 1;
	length= internal::int10_to_str(calc_daynr(l_time->year,l_time->month,
					l_time->day) -
		     calc_daynr(l_time->year,1,1) + 1, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 3, '0');
	break;
      case 'k':
	length= internal::int10_to_str(l_time->hour, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'l':
	hours_i= (l_time->hour%24 + 11)%12+1;
	length= internal::int10_to_str(hours_i, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;
      case 'p':
	hours_i= l_time->hour%24;
	str->append(hours_i < 12 ? "AM" : "PM",2);
	break;
      case 'r':
	length= snprintf(intbuff, sizeof(intbuff), 
		    ((l_time->hour % 24) < 12) ?
                    "%02d:%02d:%02d AM" : "%02d:%02d:%02d PM",
		    (l_time->hour+11)%12+1,
		    l_time->minute,
		    l_time->second);
	str->append(intbuff, length);
	break;
      case 'S':
      case 's':
	length= internal::int10_to_str(l_time->second, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
	break;
      case 'T':
	length= snprintf(intbuff, sizeof(intbuff), 
		    "%02d:%02d:%02d",
		    l_time->hour,
		    l_time->minute,
		    l_time->second);
	str->append(intbuff, length);
	break;
      case 'U':
      case 'u':
      {
	uint32_t year;
	if (type == type::DRIZZLE_TIMESTAMP_TIME)
	  return 1;
	length= internal::int10_to_str(calc_week(l_time,
				       (*ptr) == 'U' ?
				       WEEK_FIRST_WEEKDAY : WEEK_MONDAY_FIRST,
				       &year),
			     intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
      }
      break;
      case 'v':
      case 'V':
      {
	uint32_t year;
	if (type == type::DRIZZLE_TIMESTAMP_TIME)
	  return 1;
	length= internal::int10_to_str(calc_week(l_time,
				       ((*ptr) == 'V' ?
					(WEEK_YEAR | WEEK_FIRST_WEEKDAY) :
					(WEEK_YEAR | WEEK_MONDAY_FIRST)),
				       &year),
			     intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 2, '0');
      }
      break;
      case 'x':
      case 'X':
      {
	uint32_t year;
	if (type == type::DRIZZLE_TIMESTAMP_TIME)
	  return 1;
	(void) calc_week(l_time,
			 ((*ptr) == 'X' ?
			  WEEK_YEAR | WEEK_FIRST_WEEKDAY :
			  WEEK_YEAR | WEEK_MONDAY_FIRST),
			 &year);
	length= internal::int10_to_str(year, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 4, '0');
      }
      break;
      case 'w':
	if (type == type::DRIZZLE_TIMESTAMP_TIME)
	  return 1;
	weekday=calc_weekday(calc_daynr(l_time->year,l_time->month,
					l_time->day),1);
	length= internal::int10_to_str(weekday, intbuff, 10) - intbuff;
	str->append_with_prefill(intbuff, length, 1, '0');
	break;

      default:
	str->append(*ptr);
	break;
      }
    }
  }
  return 0;
}

void Item_func_date_format::fix_length_and_dec()
{
  Item *arg1= args[1];

  decimals=0;
  const charset_info_st * const cs= getSession().variables.getCollation();
  collation.set(cs, arg1->collation.derivation);
  if (arg1->type() == STRING_ITEM)
  {                                             // Optimize the normal case
    fixed_length= 1;
    max_length= format_length(&arg1->str_value) *
                collation.collation->mbmaxlen;
  }
  else
  {
    fixed_length= 0;
    max_length= min(arg1->max_length,(uint32_t) MAX_BLOB_WIDTH) * 10 *
                   collation.collation->mbmaxlen;
    set_if_smaller(max_length,MAX_BLOB_WIDTH);
  }
  maybe_null= 1;                                 // If wrong date
}

bool Item_func_date_format::eq(const Item *item, bool binary_cmp) const
{
  Item_func_date_format *item_func;

  if (item->type() != FUNC_ITEM)
    return 0;
  if (func_name() != ((Item_func*) item)->func_name())
    return 0;
  if (this == item)
    return 1;
  item_func= (Item_func_date_format*) item;
  if (!args[0]->eq(item_func->args[0], binary_cmp))
    return 0;
  /*
    We must compare format string case sensitive.
    This needed because format modifiers with different case,
    for example %m and %M, have different meaning.
  */
  if (!args[1]->eq(item_func->args[1], 1))
    return 0;
  return 1;
}

uint32_t Item_func_date_format::format_length(const String *format)
{
  uint32_t size=0;
  const char *ptr=format->ptr();
  const char *end=ptr+format->length();

  for (; ptr != end ; ptr++)
  {
    if (*ptr != '%' || ptr == end-1)
      size++;
    else
    {
      switch(*++ptr) {
      case 'M': /* month, textual */
      case 'W': /* day (of the week), textual */
	size += 64; /* large for UTF8 locale data */
	break;
      case 'D': /* day (of the month), numeric plus english suffix */
      case 'Y': /* year, numeric, 4 digits */
      case 'x': /* Year, used with 'v' */
      case 'X': /* Year, used with 'v, where week starts with Monday' */
	size += 4;
	break;
      case 'a': /* locale's abbreviated weekday name (Sun..Sat) */
      case 'b': /* locale's abbreviated month name (Jan.Dec) */
	size += 32; /* large for UTF8 locale data */
	break;
      case 'j': /* day of year (001..366) */
	size += 3;
	break;
      case 'U': /* week (00..52) */
      case 'u': /* week (00..52), where week starts with Monday */
      case 'V': /* week 1..53 used with 'x' */
      case 'v': /* week 1..53 used with 'x', where week starts with Monday */
      case 'y': /* year, numeric, 2 digits */
      case 'm': /* month, numeric */
      case 'd': /* day (of the month), numeric */
      case 'h': /* hour (01..12) */
      case 'I': /* --||-- */
      case 'i': /* minutes, numeric */
      case 'l': /* hour ( 1..12) */
      case 'p': /* locale's AM or PM */
      case 'S': /* second (00..61) */
      case 's': /* seconds, numeric */
      case 'c': /* month (0..12) */
      case 'e': /* day (0..31) */
	size += 2;
	break;
      case 'k': /* hour ( 0..23) */
      case 'H': /* hour (00..23; value > 23 OK, padding always 2-digit) */
	size += 7; /* docs allow > 23, range depends on sizeof(unsigned int) */
	break;
      case 'r': /* time, 12-hour (hh:mm:ss [AP]M) */
	size += 11;
	break;
      case 'T': /* time, 24-hour (hh:mm:ss) */
	size += 8;
	break;
      case 'f': /* microseconds */
	size += 6;
	break;
      case 'w': /* day (of the week), numeric */
      case '%':
      default:
	size++;
	break;
      }
    }
  }
  return size;
}


String *Item_func_date_format::val_str(String *str)
{
  String *format;
  type::Time l_time;
  uint32_t size;
  assert(fixed == 1);

  if (!is_time_format)
  {
    if (get_arg0_date(l_time, TIME_FUZZY_DATE))
      return 0;
  }
  else
  {
    String *res;
    if (!(res=args[0]->val_str(str)) ||
        (str_to_time_with_warn(&getSession(), res->ptr(), res->length(), &l_time)))
      goto null_date;

    l_time.year=l_time.month=l_time.day=0;
    null_value=0;
  }

  if (!(format = args[1]->val_str(str)) || !format->length())
    goto null_date;

  if (fixed_length)
    size= max_length;
  else
    size= format_length(format);

  if (size < type::Time::MAX_STRING_LENGTH)
    size= type::Time::MAX_STRING_LENGTH;

  if (format == str)
    str= &value;				// Save result here

  str->alloc(size);

  /* Create the result string */
  str->set_charset(collation.collation);
  if (not make_date_time(getSession(),
                         format, &l_time,
                         is_time_format ? type::DRIZZLE_TIMESTAMP_TIME :
                         type::DRIZZLE_TIMESTAMP_DATE,
                         str))
    return str;

null_date:
  null_value=1;

  return 0;
}

} /* namespace drizzled */
