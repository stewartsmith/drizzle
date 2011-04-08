/* Drizzle
 * Copyright (C) 2011 Olaf van der Spek
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <drizzled/session/times.h>
#include <drizzled/session.h>

namespace drizzled {

type::Time::epoch_t Session::getCurrentTimestamp(bool actual) const
{
  return ((actual ? boost::posix_time::microsec_clock::universal_time() : _end_timer) - _epoch).total_microseconds();
}

type::Time::epoch_t Session::getCurrentTimestampEpoch() const
{
	return ((_user_time.is_not_a_date_time() ? _start_timer : _user_time) - _epoch).total_seconds();
}

type::Time::epoch_t Session::getCurrentTimestampEpoch(type::Time::usec_t &fraction_arg) const
{
  if (not _user_time.is_not_a_date_time())
  {
    fraction_arg= 0;
    return (_user_time - _epoch).total_seconds();
  }

  fraction_arg= _start_timer.time_of_day().fractional_seconds() % 1000000;
  return (_start_timer - _epoch).total_seconds();
}

uint64_t Session::getElapsedTime() const
{
  return (_end_timer - _start_timer).total_microseconds();
}

void Session::resetUserTime()
{
  _user_time= boost::posix_time::not_a_date_time;
}

boost::posix_time::ptime Session::start_timer() const
{
  return _start_timer;
}

void Session::set_time()
{
  _end_timer= _start_timer= boost::posix_time::microsec_clock::universal_time();
  utime_after_lock= (_start_timer - _epoch).total_microseconds();
}

void Session::set_time_after_lock()
{
  utime_after_lock= (boost::posix_time::microsec_clock::universal_time() - _epoch).total_microseconds();
}

}
