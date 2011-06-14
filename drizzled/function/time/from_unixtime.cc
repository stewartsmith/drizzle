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
#include <boost/lexical_cast.hpp>
#include <drizzled/function/time/from_unixtime.h>
#include <drizzled/current_session.h>
#include <drizzled/session.h>
#include <drizzled/temporal.h>
#include <drizzled/time_functions.h>
#include <drizzled/field.h>

#include <sstream>
#include <string>

namespace drizzled
{

void Item_func_from_unixtime::fix_length_and_dec()
{
  session= current_session;
  collation.set(&my_charset_bin);
  decimals= DATETIME_DEC;
  max_length=type::Time::MAX_STRING_LENGTH*MY_CHARSET_BIN_MB_MAXLEN;
  maybe_null= 1;
}

String *Item_func_from_unixtime::val_str(String *str)
{
  type::Time time_tmp;

  assert(fixed == 1);

  if (get_date(time_tmp, 0))
    return 0;

  str->alloc(type::Time::MAX_STRING_LENGTH);

  time_tmp.convert(*str);

  return str;
}

int64_t Item_func_from_unixtime::val_int()
{
  type::Time time_tmp;

  assert(fixed == 1);

  if (get_date(time_tmp, 0))
    return 0;

  int64_t ret;
  time_tmp.convert(ret);

  return (int64_t) ret;
}

bool Item_func_from_unixtime::get_date(type::Time &ltime, uint32_t)
{
  uint64_t tmp= 0;
  type::Time::usec_t fractional_tmp= 0;

  switch (args[0]->result_type()) {
  case REAL_RESULT:
  case ROW_RESULT:
  case DECIMAL_RESULT:
  case STRING_RESULT:
    {
      double double_tmp= args[0]->val_real();

      tmp= (uint64_t)(double_tmp);
      fractional_tmp=  (type::Time::usec_t)((uint64_t)((double_tmp - tmp) * type::Time::FRACTIONAL_DIGITS) % type::Time::FRACTIONAL_DIGITS);

      break;
    }

  case INT_RESULT:
    tmp= (uint64_t)(args[0]->val_int());
    break;
  }

  /*
    "tmp > TIMESTAMP_MAX_VALUE" check also covers case of negative
    from_unixtime() argument since tmp is unsigned.
  */
  if ((null_value= (args[0]->null_value || tmp > TIMESTAMP_MAX_VALUE)))
    return 1;

  ltime.reset();
  ltime.store(tmp, fractional_tmp);

  return 0;
}

} /* namespace drizzled */
