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
#include "drizzled/tzfile.h"
#include "drizzled/tztime.h"
#include "drizzled/gettext.h"
#include "drizzled/session.h"
#include "drizzled/time_functions.h"

namespace drizzled
{

#if !defined(TZINFO2SQL)

static const uint32_t mon_lengths[2][MONS_PER_YEAR]=
{
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const uint32_t mon_starts[2][MONS_PER_YEAR]=
{
  { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
  { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
};

static const uint32_t year_lengths[2]=
{
  DAYS_PER_NYEAR, DAYS_PER_LYEAR
};

static inline int leaps_thru_end_of(int year)
{
  return ((year) / 4 - (year) / 100 + (year) / 400);
}

static inline bool isleap(int year)
{
  return (((year) % 4) == 0 && (((year) % 100) != 0 || ((year) % 400) == 0));
}

/**
 * @brief
 * Converts time from time_t representation (seconds in UTC since Epoch)
 * to broken down representation using given local time zone offset.
 *
 * @details
 *  Convert time_t with offset to DRIZZLE_TIME struct. Differs from timesub
 *  (from elsie code) because doesn't contain any leap correction and
 *  TM_GMTOFF and is_dst setting and contains some MySQL specific
 *  initialization. Funny but with removing of these we almost have
 *  glibc's offtime function.
 *
 * @param tmp     pointer to structure for broken down representation
 * @param t       time_t value to be converted
 * @param offset  local time zone offset
 */
static void
sec_to_TIME(DRIZZLE_TIME * tmp, time_t t, long offset)
{
  long days;
  long rem;
  int y;
  int yleap;
  const uint32_t *ip;

  days= (long) (t / SECS_PER_DAY);
  rem=  (long) (t % SECS_PER_DAY);

  /*
    We do this as separate step after dividing t, because this
    allows us handle times near time_t bounds without overflows.
  */
  rem+= offset;
  while (rem < 0)
  {
    rem+= SECS_PER_DAY;
    days--;
  }
  while (rem >= SECS_PER_DAY)
  {
    rem -= SECS_PER_DAY;
    days++;
  }
  tmp->hour= (uint32_t)(rem / SECS_PER_HOUR);
  rem= rem % SECS_PER_HOUR;
  tmp->minute= (uint32_t)(rem / SECS_PER_MIN);
  /*
    A positive leap second requires a special
    representation.  This uses "... ??:59:60" et seq.
  */
  tmp->second= (uint32_t)(rem % SECS_PER_MIN);

  y= EPOCH_YEAR;
  while (days < 0 || days >= (long)year_lengths[yleap= isleap(y)])
  {
    int	newy;

    newy= y + days / DAYS_PER_NYEAR;
    if (days < 0)
      newy--;
    days-= (newy - y) * DAYS_PER_NYEAR +
           leaps_thru_end_of(newy - 1) -
           leaps_thru_end_of(y - 1);
    y= newy;
  }
  tmp->year= y;

  ip= mon_lengths[yleap];
  for (tmp->month= 0; days >= (long) ip[tmp->month]; tmp->month++)
    days= days - (long) ip[tmp->month];
  tmp->month++;
  tmp->day= (uint32_t)(days + 1);

  /* filling MySQL specific DRIZZLE_TIME members */
  tmp->neg= 0; tmp->second_part= 0;
  tmp->time_type= DRIZZLE_TIMESTAMP_DATETIME;
}



/**
 * @brief
 * Converts local time in broken down representation to local
 * time zone analog of time_t represenation.
 *
 * @details
 * Converts time in broken down representation to time_t representation
 * ignoring time zone. Note that we cannot convert back some valid _local_
 * times near ends of time_t range because of time_t overflow. But we
 * ignore this fact now since MySQL will never pass such argument.
 *
 * @return
 * Seconds since epoch time representation.
 */
static time_t
sec_since_epoch(int year, int mon, int mday, int hour, int min ,int sec)
{
  /* Guard against time_t overflow (on system with 32 bit time_t) */
  assert(!(year == TIMESTAMP_MAX_YEAR && mon == 1 && mday > 17));
#ifndef WE_WANT_TO_HANDLE_UNORMALIZED_DATES
  /*
    It turns out that only whenever month is normalized or unnormalized
    plays role.
  */
  assert(mon > 0 && mon < 13);
  long days= year * DAYS_PER_NYEAR - EPOCH_YEAR * DAYS_PER_NYEAR +
             leaps_thru_end_of(year - 1) -
             leaps_thru_end_of(EPOCH_YEAR - 1);
  days+= mon_starts[isleap(year)][mon - 1];
#else
  long norm_month= (mon - 1) % MONS_PER_YEAR;
  long a_year= year + (mon - 1)/MONS_PER_YEAR - (int)(norm_month < 0);
  long days= a_year * DAYS_PER_NYEAR - EPOCH_YEAR * DAYS_PER_NYEAR +
             leaps_thru_end_of(a_year - 1) -
             leaps_thru_end_of(EPOCH_YEAR - 1);
  days+= mon_starts[isleap(a_year)]
                    [norm_month + (norm_month < 0 ? MONS_PER_YEAR : 0)];
#endif
  days+= mday - 1;

  return ((days * HOURS_PER_DAY + hour) * MINS_PER_HOUR + min) *
         SECS_PER_MIN + sec;
}


/*
  End of elsie derived code.
*/
#endif /* !defined(TZINFO2SQL) */


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
  virtual time_t TIME_to_gmt_sec(const DRIZZLE_TIME *t,
                                    bool *in_dst_time_gap) const;
  virtual void gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const;
  virtual const String * get_name() const;
};


/**
 * @brief
 * Converts local time in system time zone in DRIZZLE_TIME representation
 * to its time_t representation.
 *
 * @details
 * This method uses system function (localtime_r()) for conversion
 * local time in system time zone in DRIZZLE_TIME structure to its time_t
 * representation. Unlike the same function for Time_zone_db class
 * it it won't handle unnormalized input properly. Still it will
 * return lowest possible time_t in case of ambiguity or if we
 * provide time corresponding to the time-gap.
 *
 * You should call init_time() function before using this function.
 *
 * @param   t               pointer to DRIZZLE_TIME structure with local time in
 *                          broken-down representation.
 * @param   in_dst_time_gap pointer to bool which is set to true if datetime
 *                          value passed doesn't really exist (i.e. falls into
 *                          spring time-gap) and is not touched otherwise.
 *
 * @return
 * Corresponding time_t value or 0 in case of error
 */
time_t
Time_zone_system::TIME_to_gmt_sec(const DRIZZLE_TIME *t, bool *in_dst_time_gap) const
{
  long not_used;
  return my_system_gmt_sec(t, &not_used, in_dst_time_gap);
}


/**
 * @brief
 * Converts time from UTC seconds since Epoch (time_t) representation
 * to system local time zone broken-down representation.
 *
 * @param    tmp   pointer to DRIZZLE_TIME structure to fill-in
 * @param    t     time_t value to be converted
 *
 * Note: We assume that value passed to this function will fit into time_t range
 * supported by localtime_r. This conversion is putting restriction on
 * TIMESTAMP range in MySQL. If we can get rid of SYSTEM time zone at least
 * for interaction with client then we can extend TIMESTAMP range down to
 * the 1902 easily.
 */
void
Time_zone_system::gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const
{
  struct tm tmp_tm;
  time_t tmp_t= (time_t)t;

  localtime_r(&tmp_t, &tmp_tm);
  localtime_to_TIME(tmp, &tmp_tm);
  tmp->time_type= DRIZZLE_TIMESTAMP_DATETIME;
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


/**
 * Instance of this class represents UTC time zone. It uses system gmtime_r
 * function for conversions and is always available. It is used only for
 * time_t -> DRIZZLE_TIME conversions in various UTC_...  functions, it is not
 * intended for DRIZZLE_TIME -> time_t conversions and shouldn't be exposed to user.
 */
class Time_zone_utc : public Time_zone
{
public:
  Time_zone_utc() {}                          /* Remove gcc warning */
  virtual time_t TIME_to_gmt_sec(const DRIZZLE_TIME *t,
                                    bool *in_dst_time_gap) const;
  virtual void gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const;
  virtual const String * get_name() const;
};


/**
 * @brief
 * Convert UTC time from DRIZZLE_TIME representation to its time_t representation.
 *
 * @details
 * Since Time_zone_utc is used only internally for time_t -> TIME
 * conversions, this function of Time_zone interface is not implemented for
 * this class and should not be called.
 *
 * @param  t               pointer to DRIZZLE_TIME structure with local time
 *                         in broken-down representation.
 * @param  in_dst_time_gap pointer to bool which is set to true if datetime
 *                         value passed doesn't really exist (i.e. falls into
 *                         spring time-gap) and is not touched otherwise.
 * @return
 * 0
 */
time_t
Time_zone_utc::TIME_to_gmt_sec(const DRIZZLE_TIME *,
                               bool *) const
{
  /* Should be never called */
  assert(0);
  return 0;
}


/**
 * @brief
 * Converts time from UTC seconds since Epoch (time_t) representation
 * to broken-down representation (also in UTC).
 *
 * @param   tmp  pointer to DRIZZLE_TIME structure to fill-in
 * @param   t    time_t value to be converted
 *
 * Note:
 * See note for apropriate Time_zone_system method.
 */
void
Time_zone_utc::gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const
{
  struct tm tmp_tm;
  time_t tmp_t= (time_t)t;
  gmtime_r(&tmp_t, &tmp_tm);
  localtime_to_TIME(tmp, &tmp_tm);
  tmp->time_type= DRIZZLE_TIMESTAMP_DATETIME;
}


/**
 * @brief
 * Get name of time zone
 *
 * @details
 * Since Time_zone_utc is used only internally by SQL's UTC_* functions it
 * is not accessible directly, and hence this function of Time_zone
 * interface is not implemented for this class and should not be called.
 */
const String *
Time_zone_utc::get_name() const
{
  /* Should be never called */
  assert(0);
  return 0;
}

/**
 * Instance of this class represents time zone which
 * was specified as offset from UTC.
 */
class Time_zone_offset : public Time_zone
{
public:
  Time_zone_offset(long tz_offset_arg);
  virtual time_t TIME_to_gmt_sec(const DRIZZLE_TIME *t,
                                    bool *in_dst_time_gap) const;
  virtual void   gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const;
  virtual const String * get_name() const;

  /**
   * This has to be public because we want to be able to access it from
   * my_offset_tzs_get_key() function
   */
  long offset;

private:
  /* Extra reserve because of snprintf */
  char name_buff[7+16];
  String name;
};


/**
 * @brief
 * Initializes object representing time zone described by its offset from UTC.
 *
 * @param  tz_offset_arg   offset from UTC in seconds.
 *                         Positive for direction to east.
 */
Time_zone_offset::Time_zone_offset(long tz_offset_arg):
  offset(tz_offset_arg)
{
  uint32_t hours= abs((int)(offset / SECS_PER_HOUR));
  uint32_t minutes= abs((int)(offset % SECS_PER_HOUR / SECS_PER_MIN));
  ulong length= snprintf(name_buff, sizeof(name_buff), "%s%02d:%02d",
                         (offset>=0) ? "+" : "-", hours, minutes);
  name.set(name_buff, length, &my_charset_utf8_general_ci);
}


/**
 * @brief
 * Converts local time in time zone described as offset from UTC
 * from DRIZZLE_TIME representation to its time_t representation.
 *
 * @param  t                 pointer to DRIZZLE_TIME structure with local time
 *                           in broken-down representation.
 * @param  in_dst_time_gap   pointer to bool which should be set to true if
 *                           datetime  value passed doesn't really exist
 *                           (i.e. falls into spring time-gap) and is not
 *                           touched otherwise.
 *                           It is not really used in this class.
 *
 * @return
 * Corresponding time_t value or 0 in case of error
 */
time_t
Time_zone_offset::TIME_to_gmt_sec(const DRIZZLE_TIME *t,
                                  bool *) const
{
  time_t local_t;
  int shift= 0;

  /*
    Check timestamp range.we have to do this as calling function relies on
    us to make all validation checks here.
  */
  if (!validate_timestamp_range(t))
    return 0;

  /*
    Do a temporary shift of the boundary dates to avoid
    overflow of time_t if the time value is near it's
    maximum range
  */
  if ((t->year == TIMESTAMP_MAX_YEAR) && (t->month == 1) && t->day > 4)
    shift= 2;

  local_t= sec_since_epoch(t->year, t->month, (t->day - shift),
                           t->hour, t->minute, t->second) -
           offset;

  if (shift)
  {
    /* Add back the shifted time */
    local_t+= shift * SECS_PER_DAY;
  }

  if (local_t >= TIMESTAMP_MIN_VALUE && local_t <= TIMESTAMP_MAX_VALUE)
    return local_t;

  /* range error*/
  return 0;
}


/**
 * @brief
 * Converts time from UTC seconds since Epoch (time_t) representation
 * to local time zone described as offset from UTC and in broken-down
 * representation.
 *
 * @param  tmp   pointer to DRIZZLE_TIME structure to fill-in
 * @param  t     time_t value to be converted
 */
void
Time_zone_offset::gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const
{
  sec_to_TIME(tmp, t, offset);
}


/**
 * @brief
 * Get name of time zone
 *
 * @return
 * Name of time zone as pointer to String object
 */
const String *
Time_zone_offset::get_name() const
{
  return &name;
}


static Time_zone_utc tz_UTC;
static Time_zone_system tz_SYSTEM;
static Time_zone_offset tz_OFFSET0(0);

Time_zone *my_tz_SYSTEM= &tz_SYSTEM;

class Tz_names_entry: public memory::SqlAlloc
{
public:
  String name;
  Time_zone *tz;
};


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
