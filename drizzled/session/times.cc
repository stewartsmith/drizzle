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
#include <drizzled/statistics_variables.h>

namespace drizzled {

type::Time::epoch_t Session::getCurrentTimestamp(bool actual) const
{
  return ((actual ? boost::posix_time::microsec_clock::universal_time() : times._end_timer) - times._epoch).total_microseconds();
}

type::Time::epoch_t Session::getCurrentTimestampEpoch() const
{
	return ((times._user_time.is_not_a_date_time() ? times._start_timer : times._user_time) - times._epoch).total_seconds();
}

type::Time::epoch_t Session::getCurrentTimestampEpoch(type::Time::usec_t &fraction_arg) const
{
  if (not times._user_time.is_not_a_date_time())
  {
    fraction_arg= 0;
    return (times._user_time - times._epoch).total_seconds();
  }

  fraction_arg= times._start_timer.time_of_day().fractional_seconds() % 1000000;
  return (times._start_timer - times._epoch).total_seconds();
}

uint64_t Session::getElapsedTime() const
{
  return (times._end_timer - times._start_timer).total_microseconds();
}

void Session::resetUserTime()
{
  times._user_time= boost::posix_time::not_a_date_time;
}

boost::posix_time::ptime Session::start_timer() const
{
  return times._start_timer;
}

void Session::set_time()
{
  times._end_timer= times._start_timer= boost::posix_time::microsec_clock::universal_time();
  times.utime_after_lock= (times._start_timer - times._epoch).total_microseconds();
}

void Session::set_time_after_lock()
{
  times.utime_after_lock= (boost::posix_time::microsec_clock::universal_time() - times._epoch).total_microseconds();
}

type::Time::epoch_t Session::query_start()
{
  return getCurrentTimestampEpoch();
}

void Session::set_time(time_t t) // This is done by a sys_var, as long as user_time is set, we will use that for all references to time
{
  times._user_time= boost::posix_time::from_time_t(t);
}

uint64_t Session::getConnectMicroseconds() const
{
  return (times._connect_time - times._epoch).total_microseconds();
}

uint64_t Session::getConnectSeconds() const
{
  return (times._connect_time - times._epoch).total_seconds();
}

void Session::set_end_timer()
{
  times._end_timer= boost::posix_time::microsec_clock::universal_time();
  status_var.execution_time_nsec+= (times._end_timer - times._start_timer).total_microseconds();
}

}
