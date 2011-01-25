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


#include "config.h"
#include <cstdio>
#include "drizzled/tzfile.h"
#include "drizzled/tztime.h"
#include "drizzled/gettext.h"
#include "drizzled/session.h"
#include "drizzled/time_functions.h"

namespace drizzled
{

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
  Time_zone_system() {}                       /* Remove gcc warning */
  virtual type::Time::epoch_t TIME_to_gmt_sec(const type::Time &t,
                                              bool *in_dst_time_gap) const;
  virtual void gmt_sec_to_TIME(type::Time &tmp, type::Time::epoch_t t) const;
  virtual const String * get_name() const;
};


/**
 * @brief
 * Converts local time in system time zone in type::Time representation
 * to its type::Time::epoch_t representation.
 *
 * @details
 * This method uses system function (localtime_r()) for conversion
 * local time in system time zone in type::Time structure to its type::Time::epoch_t
 * representation. Unlike the same function for Time_zone_db class
 * it it won't handle unnormalized input properly. Still it will
 * return lowest possible type::Time::epoch_t in case of ambiguity or if we
 * provide time corresponding to the time-gap.
 *
 * You should call init_time() function before using this function.
 *
 * @param   t               pointer to type::Time structure with local time in
 *                          broken-down representation.
 * @param   in_dst_time_gap pointer to bool which is set to true if datetime
 *                          value passed doesn't really exist (i.e. falls into
 *                          spring time-gap) and is not touched otherwise.
 *
 * @return
 * Corresponding type::Time::epoch_t value or 0 in case of error
 */
type::Time::epoch_t
Time_zone_system::TIME_to_gmt_sec(const type::Time &t, bool *in_dst_time_gap) const
{
  long not_used;
  type::Time::epoch_t tmp;
  t.convert(tmp, &not_used, in_dst_time_gap);
  return tmp;
}


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


/**
 * @brief
 * Get name of time zone
 *
 * @return
 * Name of time zone as String
 */
const String *
Time_zone_system::get_name() const
{
  return &tz_SYSTEM_name;
}

static Time_zone_system tz_SYSTEM;

Time_zone *my_tz_SYSTEM= &tz_SYSTEM;


/**
 * @brief
 * Initialize time zone support infrastructure.
 *
 * @details
 * This function will init memory structures needed for time zone support,
 * it will register mandatory SYSTEM time zone in them. It will try to open
 * mysql.time_zone* tables and load information about default time zone and
 * information which further will be shared among all time zones loaded.
 * If system tables with time zone descriptions don't exist it won't fail
 * (unless default_tzname is time zone from tables). If bootstrap parameter
 * is true then this routine assumes that we are in bootstrap mode and won't
 * load time zone descriptions unless someone specifies default time zone
 * which is supposedly stored in those tables.
 * It'll also set default time zone if it is specified.
 *
 * @param   session            current thread object
 * @param   default_tzname     default time zone or 0 if none.
 * @param   bootstrap          indicates whenever we are in bootstrap mode
 *
 * @return
 *  0 - ok
 *  1 - Error
 */
bool
my_tz_init(Session *session, const char *default_tzname)
{
  if (default_tzname)
  {
    String tmp_tzname2(default_tzname, &my_charset_utf8_general_ci);
    /*
      Time zone tables may be open here, and my_tz_find() may open
      most of them once more, but this is OK for system tables open
      for READ.
    */
    if (!(global_system_variables.time_zone= my_tz_find(session, &tmp_tzname2)))
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Fatal error: Illegal or unknown default time zone '%s'"),
                    default_tzname);
      return true;
    }
  }

  return false;
}

/**
 * @brief
 * Get Time_zone object for specified time zone.
 *
 * @todo
 * Not implemented yet. This needs to hook into some sort of OS system call.
 */
Time_zone *
my_tz_find(Session *,
           const String *)
{
  return NULL;
}

} /* namespace drizzled */
