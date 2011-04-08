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
#include <drizzled/session.h>

namespace drizzled {

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
