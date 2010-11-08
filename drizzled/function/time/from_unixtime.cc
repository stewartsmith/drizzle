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

#include "config.h"
#include <boost/lexical_cast.hpp>
#include "drizzled/function/time/from_unixtime.h"
#include "drizzled/session.h"
#include "drizzled/temporal.h"
#include "drizzled/time_functions.h"

#include <sstream>
#include <string>

namespace drizzled
{

void Item_func_from_unixtime::fix_length_and_dec()
{
  session= current_session;
  collation.set(&my_charset_bin);
  decimals= DATETIME_DEC;
  max_length=DateTime::MAX_STRING_LENGTH*MY_CHARSET_BIN_MB_MAXLEN;
  maybe_null= 1;
}

String *Item_func_from_unixtime::val_str(String *str)
{
  DRIZZLE_TIME time_tmp;

  assert(fixed == 1);

  if (get_date(&time_tmp, 0))
    return 0;

  if (str->alloc(MAX_DATE_STRING_REP_LENGTH))
  {
    null_value= 1;
    return 0;
  }

  make_datetime(&time_tmp, str);

  return str;
}

int64_t Item_func_from_unixtime::val_int()
{
  DRIZZLE_TIME time_tmp;

  assert(fixed == 1);

  if (get_date(&time_tmp, 0))
    return 0;

  return (int64_t) TIME_to_uint64_t_datetime(&time_tmp);
}

bool Item_func_from_unixtime::get_date(DRIZZLE_TIME *ltime, uint32_t)
{
  uint64_t tmp= (uint64_t)(args[0]->val_int());
  /*
    "tmp > TIMESTAMP_MAX_VALUE" check also covers case of negative
    from_unixtime() argument since tmp is unsigned.
  */
  if ((null_value= (args[0]->null_value || tmp > TIMESTAMP_MAX_VALUE)))
    return 1;

  Timestamp temporal;
  if (! temporal.from_time_t((time_t) tmp))
  {
    null_value= true;
    std::string tmp_string(boost::lexical_cast<std::string>(tmp));
    my_error(ER_INVALID_UNIX_TIMESTAMP_VALUE, MYF(0), tmp_string.c_str());
    return 0;
  }
  
  memset(ltime, 0, sizeof(*ltime));

  ltime->year= temporal.years();
  ltime->month= temporal.months();
  ltime->day= temporal.days();
  ltime->hour= temporal.hours();
  ltime->minute= temporal.minutes();
  ltime->second= temporal.seconds();
  ltime->time_type= DRIZZLE_TIMESTAMP_DATETIME;

  return 0;
}

} /* namespace drizzled */
