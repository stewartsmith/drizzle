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

#include <drizzled/function/time/unix_timestamp.h>
#include <drizzled/field/epoch.h>
#include <drizzled/session.h>
#include <drizzled/session/times.h>
#include <drizzled/temporal.h>
#include <drizzled/item/field.h>

namespace drizzled {

int64_t Item_func_unix_timestamp::val_int()
{
  type::Time ltime;

  assert(fixed == 1);
  if (arg_count == 0)
    return (int64_t) getSession().times.getCurrentTimestampEpoch();

  if (args[0]->type() == FIELD_ITEM)
  {                                             // Optimize timestamp field
    Field *field=((Item_field*) args[0])->field;
    if (field->is_timestamp())
      return ((field::Epoch::pointer) field)->get_timestamp(&null_value);
  }

  if (get_arg0_date(ltime, 0))
  {
    /*
      We have to set null_value again because get_arg0_date will also set it
      to true if we have wrong datetime parameter (and we should return 0 in
      this case).
    */
    null_value= args[0]->null_value;
    return 0;
  }

  Timestamp temporal;

  temporal.set_years(ltime.year);
  temporal.set_months(ltime.month);
  temporal.set_days(ltime.day);
  temporal.set_hours(ltime.hour);
  temporal.set_minutes(ltime.minute);
  temporal.set_seconds(ltime.second);
  temporal.set_epoch_seconds();

  if (! temporal.is_valid())
  {
    null_value= true;
    char buff[DateTime::MAX_STRING_LENGTH];
    int buff_len;
    buff_len= temporal.to_string(buff, DateTime::MAX_STRING_LENGTH);
    assert((buff_len+1) < DateTime::MAX_STRING_LENGTH);
    my_error(ER_INVALID_UNIX_TIMESTAMP_VALUE, MYF(0), buff);
    return 0;
  }

  time_t tmp;
  temporal.to_time_t(tmp);

  return (int64_t) tmp;
}

} /* namespace drizzled */
