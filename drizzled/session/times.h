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

#include <boost/date_time.hpp>
#include <drizzled/type/time.h>

namespace drizzled {
namespace session {

class Times
{
public:
	Times()
    : _epoch(boost::gregorian::date(1970, 1 ,1))
	{
    
    _connect_time = boost::posix_time::microsec_clock::universal_time();
		utime_after_lock = 0;
	}

  type::Time::epoch_t getCurrentTimestamp(bool actual= true) const;
  type::Time::epoch_t getCurrentTimestampEpoch() const;
  type::Time::epoch_t getCurrentTimestampEpoch(type::Time::usec_t& fraction_arg) const;

  boost::posix_time::ptime _epoch;
  boost::posix_time::ptime _connect_time;
  boost::posix_time::ptime _end_timer;
  boost::posix_time::ptime _user_time;
  boost::posix_time::ptime _start_timer;
	uint64_t utime_after_lock;
};

}
}
