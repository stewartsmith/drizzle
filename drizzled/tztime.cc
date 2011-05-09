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
#include <cstdio>
#include <drizzled/tztime.h>
#include <drizzled/gettext.h>
#include <drizzled/session.h>
#include <drizzled/time_functions.h>
#include <drizzled/charset.h>

namespace drizzled {

/**
 * String with names of SYSTEM time zone.
 */
static const String tz_SYSTEM_name("SYSTEM", 6, &my_charset_utf8_general_ci);


/**
 * Instance of this class represents local time zone used on this system
 * (specified by TZ environment variable or via any other system mechanism).
 * It uses system functions (localtime_r, my_system_gmt_sec) for conversion
 * and is always available. Because of this it is used by default - if there
 * were no explicit time zone specified. On the other hand because of this
 * conversion methods provided by this class is significantly slower and
 * possibly less multi-threaded-friendly than corresponding Time_zone_db
 * methods so the latter should be preffered there it is possible.
 */
class Time_zone_system : public Time_zone
{
public:
  virtual void gmt_sec_to_TIME(type::Time &tmp, type::Time::epoch_t t) const;
};


/**
 * @brief
 * Converts time from UTC seconds since Epoch (type::Time::epoch_t) representation
 * to system local time zone broken-down representation.
 *
 * @param    tmp   pointer to type::Time structure to fill-in
 * @param    t     type::Time::epoch_t value to be converted
 *
 * Note: We assume that value passed to this function will fit into type::Time::epoch_t range
 * supported by localtime_r. This conversion is putting restriction on
 * TIMESTAMP range in MySQL. If we can get rid of SYSTEM time zone at least
 * for interaction with client then we can extend TIMESTAMP range down to
 * the 1902 easily.
 */
void
Time_zone_system::gmt_sec_to_TIME(type::Time &tmp, type::Time::epoch_t t) const
{
  tmp.store(t);
}


static Time_zone_system tz_SYSTEM;

Time_zone *my_tz_SYSTEM= &tz_SYSTEM;

} /* namespace drizzled */
