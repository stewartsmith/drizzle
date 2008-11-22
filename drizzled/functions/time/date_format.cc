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
#include <drizzled/functions/time/date_format.h>

void Item_func_date_format::fix_length_and_dec()
{
  Session* session= current_session;
  /*
    Must use this_item() in case it's a local SP variable
    (for ->max_length and ->str_value)
  */
  Item *arg1= args[1]->this_item();

  decimals=0;
  const CHARSET_INFO * const cs= session->variables.collation_connection;
  uint32_t repertoire= arg1->collation.repertoire;
  if (!session->variables.lc_time_names->is_ascii)
    repertoire|= MY_REPERTOIRE_EXTENDED;
  collation.set(cs, arg1->collation.derivation, repertoire);
  if (arg1->type() == STRING_ITEM)
  {                                             // Optimize the normal case
    fixed_length=1;
    max_length= format_length(&arg1->str_value) *
                collation.collation->mbmaxlen;
  }
  else
  {
    fixed_length=0;
    max_length=cmin(arg1->max_length,(uint32_t) MAX_BLOB_WIDTH) * 10 *
                   collation.collation->mbmaxlen;
    set_if_smaller(max_length,MAX_BLOB_WIDTH);
  }
  maybe_null=1;                                 // If wrong date
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
  DRIZZLE_TIME l_time;
  uint32_t size;
  assert(fixed == 1);

  if (!is_time_format)
  {
    if (get_arg0_date(&l_time, TIME_FUZZY_DATE))
      return 0;
  }
  else
  {
    String *res;
    if (!(res=args[0]->val_str(str)) ||
	(str_to_time_with_warn(res->ptr(), res->length(), &l_time)))
      goto null_date;

    l_time.year=l_time.month=l_time.day=0;
    null_value=0;
  }

  if (!(format = args[1]->val_str(str)) || !format->length())
    goto null_date;

  if (fixed_length)
    size=max_length;
  else
    size=format_length(format);

  if (size < MAX_DATE_STRING_REP_LENGTH)
    size= MAX_DATE_STRING_REP_LENGTH;

  if (format == str)
    str= &value;				// Save result here
  if (str->alloc(size))
    goto null_date;

  DATE_TIME_FORMAT date_time_format;
  date_time_format.format.str=    (char*) format->ptr();
  date_time_format.format.length= format->length(); 

  /* Create the result string */
  str->set_charset(collation.collation);
  if (!make_date_time(&date_time_format, &l_time,
                      is_time_format ? DRIZZLE_TIMESTAMP_TIME :
                                       DRIZZLE_TIMESTAMP_DATE,
                      str))
    return str;

null_date:
  null_value=1;
  return 0;
}

