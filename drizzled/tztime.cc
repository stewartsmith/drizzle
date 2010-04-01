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

/* Structure describing local time type (e.g. Moscow summer time (MSD)) */
typedef struct ttinfo
{
  long tt_gmtoff; // Offset from UTC in seconds
  uint32_t tt_isdst;   // Is daylight saving time or not. Used to set tm_isdst
  uint32_t tt_abbrind; // Index of start of abbreviation for this time type.
  /*
    We don't use tt_ttisstd and tt_ttisgmt members of original elsie-code
    struct since we don't support POSIX-style TZ descriptions in variables.
  */
} TRAN_TYPE_INFO;

/* Structure describing leap-second corrections. */
typedef struct lsinfo
{
  time_t ls_trans; // Transition time
  long      ls_corr;  // Correction to apply
} LS_INFO;

/*
  Structure with information describing ranges of time_t shifted to local
  time (time_t + offset). Used for local DRIZZLE_TIME -> time_t conversion.
  See comments for TIME_to_gmt_sec() for more info.
*/
typedef struct revtinfo
{
  long rt_offset; // Offset of local time from UTC in seconds
  uint32_t rt_type;    // Type of period 0 - Normal period. 1 - Spring time-gap
} REVT_INFO;


/*
  Structure which fully describes time zone which is
  described in our db or in zoneinfo files.
*/
typedef struct st_time_zone_info
{
  uint32_t leapcnt;  // Number of leap-second corrections
  uint32_t timecnt;  // Number of transitions between time types
  uint32_t typecnt;  // Number of local time types
  uint32_t charcnt;  // Number of characters used for abbreviations
  uint32_t revcnt;   // Number of transition descr. for TIME->time_t conversion
  /* The following are dynamical arrays are allocated in memory::Root */
  time_t *ats;       // Times of transitions between time types
  unsigned char	*types; // Local time types for transitions
  TRAN_TYPE_INFO *ttis; // Local time types descriptions
  /* Storage for local time types abbreviations. They are stored as ASCIIZ */
  char *chars;
  /*
    Leap seconds corrections descriptions, this array is shared by
    all time zones who use leap seconds.
  */
  LS_INFO *lsis;
  /*
    Starting points and descriptions of shifted time_t (time_t + offset)
    ranges on which shifted time_t -> time_t mapping is linear or undefined.
    Used for tm -> time_t conversion.
  */
  time_t *revts;
  REVT_INFO *revtis;
  /*
    Time type which is used for times smaller than first transition or if
    there are no transitions at all.
  */
  TRAN_TYPE_INFO *fallback_tti;

} TIME_ZONE_INFO;


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

/*
  Converts time from time_t representation (seconds in UTC since Epoch)
  to broken down representation using given local time zone offset.

  SYNOPSIS
    sec_to_TIME()
      tmp    - pointer to structure for broken down representation
      t      - time_t value to be converted
      offset - local time zone offset

  DESCRIPTION
    Convert time_t with offset to DRIZZLE_TIME struct. Differs from timesub
    (from elsie code) because doesn't contain any leap correction and
    TM_GMTOFF and is_dst setting and contains some MySQL specific
    initialization. Funny but with removing of these we almost have
    glibc's offtime function.
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


/*
  Find time range wich contains given time_t value

  SYNOPSIS
    find_time_range()
      t                - time_t value for which we looking for range
      range_boundaries - sorted array of range starts.
      higher_bound     - number of ranges

  DESCRIPTION
    Performs binary search for range which contains given time_t value.
    It has sense if number of ranges is greater than zero and time_t value
    is greater or equal than beginning of first range. It also assumes that
    t belongs to some range specified.

    With this localtime_r on real data may takes less time than with linear
    search (I've seen 30% speed up).

  RETURN VALUE
    Index of range to which t belongs
*/
static uint
find_time_range(time_t t, const time_t *range_boundaries,
                uint32_t higher_bound)
{
  uint32_t i, lower_bound= 0;

  /*
    Function will work without this assertion but result would be meaningless.
  */
  assert(higher_bound > 0 && t >= range_boundaries[0]);

  /*
    Do binary search for minimal interval which contain t. We preserve:
    range_boundaries[lower_bound] <= t < range_boundaries[higher_bound]
    invariant and decrease this higher_bound - lower_bound gap twice
    times on each step.
  */

  while (higher_bound - lower_bound > 1)
  {
    i= (lower_bound + higher_bound) >> 1;
    if (range_boundaries[i] <= t)
      lower_bound= i;
    else
      higher_bound= i;
  }
  return lower_bound;
}

/*
  Find local time transition for given time_t.

  SYNOPSIS
    find_transition_type()
      t   - time_t value to be converted
      sp  - pointer to struct with time zone description

  RETURN VALUE
    Pointer to structure in time zone description describing
    local time type for given time_t.
*/
static
const TRAN_TYPE_INFO *
find_transition_type(time_t t, const TIME_ZONE_INFO *sp)
{
  if (unlikely(sp->timecnt == 0 || t < sp->ats[0]))
  {
    /*
      If we have not any transitions or t is before first transition let
      us use fallback time type.
    */
    return sp->fallback_tti;
  }

  /*
    Do binary search for minimal interval between transitions which
    contain t. With this localtime_r on real data may takes less
    time than with linear search (I've seen 30% speed up).
  */
  return &(sp->ttis[sp->types[find_time_range(t, sp->ats, sp->timecnt)]]);
}


/*
  Converts time in time_t representation (seconds in UTC since Epoch) to
  broken down DRIZZLE_TIME representation in local time zone.

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp          - pointer to structure for broken down represenatation
      sec_in_utc   - time_t value to be converted
      sp           - pointer to struct with time zone description

  TODO
    We can improve this function by creating joined array of transitions and
    leap corrections. This will require adding extra field to TRAN_TYPE_INFO
    for storing number of "extra" seconds to minute occured due to correction
    (60th and 61st second, look how we calculate them as "hit" in this
    function).
    Under realistic assumptions about frequency of transitions the same array
    can be used for DRIZZLE_TIME -> time_t conversion. For this we need to
    implement tweaked binary search which will take into account that some
    DRIZZLE_TIME has two matching time_t ranges and some of them have none.
*/
static void
gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t sec_in_utc, const TIME_ZONE_INFO *sp)
{
  const TRAN_TYPE_INFO *ttisp;
  const LS_INFO *lp;
  long  corr= 0;
  int   hit= 0;
  int   i;

  /*
    Find proper transition (and its local time type) for our sec_in_utc value.
    Funny but again by separating this step in function we receive code
    which very close to glibc's code. No wonder since they obviously use
    the same base and all steps are sensible.
  */
  ttisp= find_transition_type(sec_in_utc, sp);

  /*
    Let us find leap correction for our sec_in_utc value and number of extra
    secs to add to this minute.
    This loop is rarely used because most users will use time zones without
    leap seconds, and even in case when we have such time zone there won't
    be many iterations (we have about 22 corrections at this moment (2004)).
  */
  for ( i= sp->leapcnt; i-- > 0; )
  {
    lp= &sp->lsis[i];
    if (sec_in_utc >= lp->ls_trans)
    {
      if (sec_in_utc == lp->ls_trans)
      {
        hit= ((i == 0 && lp->ls_corr > 0) ||
              lp->ls_corr > sp->lsis[i - 1].ls_corr);
        if (hit)
        {
          while (i > 0 &&
                 sp->lsis[i].ls_trans == sp->lsis[i - 1].ls_trans + 1 &&
                 sp->lsis[i].ls_corr == sp->lsis[i - 1].ls_corr + 1)
          {
            hit++;
            i--;
          }
        }
      }
      corr= lp->ls_corr;
      break;
    }
  }

  sec_to_TIME(tmp, sec_in_utc, ttisp->tt_gmtoff - corr);

  tmp->second+= hit;
}


/*
  Converts local time in broken down representation to local
  time zone analog of time_t represenation.

  SYNOPSIS
    sec_since_epoch()
      year, mon, mday, hour, min, sec - broken down representation.

  DESCRIPTION
    Converts time in broken down representation to time_t representation
    ignoring time zone. Note that we cannot convert back some valid _local_
    times near ends of time_t range because of time_t overflow. But we
    ignore this fact now since MySQL will never pass such argument.

  RETURN VALUE
    Seconds since epoch time representation.
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
  Converts local time in broken down DRIZZLE_TIME representation to time_t
  representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to structure for broken down represenatation
      sp              - pointer to struct with time zone description
      in_dst_time_gap - pointer to bool which is set to true if datetime
                        value passed doesn't really exist (i.e. falls into
                        spring time-gap) and is not touched otherwise.

  DESCRIPTION
    This is mktime analog for MySQL. It is essentially different
    from mktime (or hypotetical my_mktime) because:
    - It has no idea about tm_isdst member so if it
      has two answers it will give the smaller one
    - If we are in spring time gap then it will return
      beginning of the gap
    - It can give wrong results near the ends of time_t due to
      overflows, but we are safe since in MySQL we will never
      call this function for such dates (its restriction for year
      between 1970 and 2038 gives us several days of reserve).
    - By default it doesn't support un-normalized input. But if
      sec_since_epoch() function supports un-normalized dates
      then this function should handle un-normalized input right,
      altough it won't normalize structure TIME.

    Traditional approach to problem of conversion from broken down
    representation to time_t is iterative. Both elsie's and glibc
    implementation try to guess what time_t value should correspond to
    this broken-down value. They perform localtime_r function on their
    guessed value and then calculate the difference and try to improve
    their guess. Elsie's code guesses time_t value in bit by bit manner,
    Glibc's code tries to add difference between broken-down value
    corresponding to guess and target broken-down value to current guess.
    It also uses caching of last found correction... So Glibc's approach
    is essentially faster but introduces some undetermenism (in case if
    is_dst member of broken-down representation (tm struct) is not known
    and we have two possible answers).

    We use completely different approach. It is better since it is both
    faster than iterative implementations and fully determenistic. If you
    look at time_t to DRIZZLE_TIME conversion then you'll find that it consist
    of two steps:
    The first is calculating shifted time_t value and the second - TIME
    calculation from shifted time_t value (well it is a bit simplified
    picture). The part in which we are interested in is time_t -> shifted
    time_t conversion. It is piecewise linear function which is defined
    by combination of transition times as break points and times offset
    as changing function parameter. The possible inverse function for this
    converison would be ambiguos but with MySQL's restrictions we can use
    some function which is the same as inverse function on unambigiuos
    ranges and coincides with one of branches of inverse function in
    other ranges. Thus we just need to build table which will determine
    this shifted time_t -> time_t conversion similar to existing
    (time_t -> shifted time_t table). We do this in
    prepare_tz_info function.

  TODO
    If we can even more improve this function. For doing this we will need to
    build joined map of transitions and leap corrections for gmt_sec_to_TIME()
    function (similar to revts/revtis). Under realistic assumptions about
    frequency of transitions we can use the same array for TIME_to_gmt_sec().
    We need to implement special version of binary search for this. Such step
    will be beneficial to CPU cache since we will decrease data-set used for
    conversion twice.

  RETURN VALUE
    Seconds in UTC since Epoch.
    0 in case of error.
*/
static time_t
TIME_to_gmt_sec(const DRIZZLE_TIME *t, const TIME_ZONE_INFO *sp,
                bool *in_dst_time_gap)
{
  time_t local_t;
  uint32_t saved_seconds;
  uint32_t i;
  int shift= 0;

  if (!validate_timestamp_range(t))
    return(0);


  /* We need this for correct leap seconds handling */
  if (t->second < SECS_PER_MIN)
    saved_seconds= 0;
  else
    saved_seconds= t->second;

  /*
    NOTE: to convert full time_t range we do a shift of the
    boundary dates here to avoid overflow of time_t.
    We use alike approach in my_system_gmt_sec().

    However in that function we also have to take into account
    overflow near 0 on some platforms. That's because my_system_gmt_sec
    uses localtime_r(), which doesn't work with negative values correctly
    on platforms with unsigned time_t (QNX). Here we don't use localtime()
    => we negative values of local_t are ok.
  */

  if ((t->year == TIMESTAMP_MAX_YEAR) && (t->month == 1) && t->day > 4)
  {
    /*
      We will pass (t->day - shift) to sec_since_epoch(), and
      want this value to be a positive number, so we shift
      only dates > 4.01.2038 (to avoid owerflow).
    */
    shift= 2;
  }


  local_t= sec_since_epoch(t->year, t->month, (t->day - shift),
                           t->hour, t->minute,
                           saved_seconds ? 0 : t->second);

  /* We have at least one range */
  assert(sp->revcnt >= 1);

  if (local_t < sp->revts[0] || local_t > sp->revts[sp->revcnt])
  {
    /*
      This means that source time can't be represented as time_t due to
      limited time_t range.
    */
    return(0);
  }

  /* binary search for our range */
  i= find_time_range(local_t, sp->revts, sp->revcnt);

  /*
    As there are no offset switches at the end of TIMESTAMP range,
    we could simply check for overflow here (and don't need to bother
    about DST gaps etc)
  */
  if (shift)
  {
    if (local_t > (time_t) (TIMESTAMP_MAX_VALUE - shift * SECS_PER_DAY +
                            sp->revtis[i].rt_offset - saved_seconds))
    {
      return(0);                           /* time_t overflow */
    }
    local_t+= shift * SECS_PER_DAY;
  }

  if (sp->revtis[i].rt_type)
  {
    /*
      Oops! We are in spring time gap.
      May be we should return error here?
      Now we are returning time_t value corresponding to the
      beginning of the gap.
    */
    *in_dst_time_gap= 1;
    local_t= sp->revts[i] - sp->revtis[i].rt_offset + saved_seconds;
  }
  else
    local_t= local_t - sp->revtis[i].rt_offset + saved_seconds;

  /* check for TIMESTAMP_MAX_VALUE was already done above */
  if (local_t < TIMESTAMP_MIN_VALUE)
    local_t= 0;

  return(local_t);
}


/*
  End of elsie derived code.
*/
#endif /* !defined(TZINFO2SQL) */


/*
  String with names of SYSTEM time zone.
*/
static const String tz_SYSTEM_name("SYSTEM", 6, &my_charset_utf8_general_ci);


/*
  Instance of this class represents local time zone used on this system
  (specified by TZ environment variable or via any other system mechanism).
  It uses system functions (localtime_r, my_system_gmt_sec) for conversion
  and is always available. Because of this it is used by default - if there
  were no explicit time zone specified. On the other hand because of this
  conversion methods provided by this class is significantly slower and
  possibly less multi-threaded-friendly than corresponding Time_zone_db
  methods so the latter should be preffered there it is possible.
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


/*
  Converts local time in system time zone in DRIZZLE_TIME representation
  to its time_t representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to DRIZZLE_TIME structure with local time in
                        broken-down representation.
      in_dst_time_gap - pointer to bool which is set to true if datetime
                        value passed doesn't really exist (i.e. falls into
                        spring time-gap) and is not touched otherwise.

  DESCRIPTION
    This method uses system function (localtime_r()) for conversion
    local time in system time zone in DRIZZLE_TIME structure to its time_t
    representation. Unlike the same function for Time_zone_db class
    it it won't handle unnormalized input properly. Still it will
    return lowest possible time_t in case of ambiguity or if we
    provide time corresponding to the time-gap.

    You should call init_time() function before using this function.

  RETURN VALUE
    Corresponding time_t value or 0 in case of error
*/
time_t
Time_zone_system::TIME_to_gmt_sec(const DRIZZLE_TIME *t, bool *in_dst_time_gap) const
{
  long not_used;
  return my_system_gmt_sec(t, &not_used, in_dst_time_gap);
}


/*
  Converts time from UTC seconds since Epoch (time_t) representation
  to system local time zone broken-down representation.

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp - pointer to DRIZZLE_TIME structure to fill-in
      t   - time_t value to be converted

  NOTE
    We assume that value passed to this function will fit into time_t range
    supported by localtime_r. This conversion is putting restriction on
    TIMESTAMP range in MySQL. If we can get rid of SYSTEM time zone at least
    for interaction with client then we can extend TIMESTAMP range down to
    the 1902 easily.
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


/*
  Get name of time zone

  SYNOPSIS
    get_name()

  RETURN VALUE
    Name of time zone as String
*/
const String *
Time_zone_system::get_name() const
{
  return &tz_SYSTEM_name;
}


/*
  Instance of this class represents UTC time zone. It uses system gmtime_r
  function for conversions and is always available. It is used only for
  time_t -> DRIZZLE_TIME conversions in various UTC_...  functions, it is not
  intended for DRIZZLE_TIME -> time_t conversions and shouldn't be exposed to user.
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


/*
  Convert UTC time from DRIZZLE_TIME representation to its time_t representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to DRIZZLE_TIME structure with local time
                        in broken-down representation.
      in_dst_time_gap - pointer to bool which is set to true if datetime
                        value passed doesn't really exist (i.e. falls into
                        spring time-gap) and is not touched otherwise.

  DESCRIPTION
    Since Time_zone_utc is used only internally for time_t -> TIME
    conversions, this function of Time_zone interface is not implemented for
    this class and should not be called.

  RETURN VALUE
    0
*/
time_t
Time_zone_utc::TIME_to_gmt_sec(const DRIZZLE_TIME *,
                               bool *) const
{
  /* Should be never called */
  assert(0);
  return 0;
}


/*
  Converts time from UTC seconds since Epoch (time_t) representation
  to broken-down representation (also in UTC).

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp - pointer to DRIZZLE_TIME structure to fill-in
      t   - time_t value to be converted

  NOTE
    See note for apropriate Time_zone_system method.
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


/*
  Get name of time zone

  SYNOPSIS
    get_name()

  DESCRIPTION
    Since Time_zone_utc is used only internally by SQL's UTC_* functions it
    is not accessible directly, and hence this function of Time_zone
    interface is not implemented for this class and should not be called.

  RETURN VALUE
    0
*/
const String *
Time_zone_utc::get_name() const
{
  /* Should be never called */
  assert(0);
  return 0;
}


/*
  Instance of this class represents some time zone which is
  described in mysql.time_zone family of tables.
*/
class Time_zone_db : public Time_zone
{
public:
  Time_zone_db(TIME_ZONE_INFO *tz_info_arg, const String * tz_name_arg);
  virtual time_t TIME_to_gmt_sec(const DRIZZLE_TIME *t,
                                    bool *in_dst_time_gap) const;
  virtual void gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const;
  virtual const String * get_name() const;
private:
  TIME_ZONE_INFO *tz_info;
  const String *tz_name;
};


/*
  Initializes object representing time zone described by mysql.time_zone
  tables.

  SYNOPSIS
    Time_zone_db()
      tz_info_arg - pointer to TIME_ZONE_INFO structure which is filled
                    according to db or other time zone description
                    (for example by my_tz_init()).
                    Several Time_zone_db instances can share one
                    TIME_ZONE_INFO structure.
      tz_name_arg - name of time zone.
*/
Time_zone_db::Time_zone_db(TIME_ZONE_INFO *tz_info_arg,
                           const String *tz_name_arg):
  tz_info(tz_info_arg), tz_name(tz_name_arg)
{
}


/*
  Converts local time in time zone described from TIME
  representation to its time_t representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to DRIZZLE_TIME structure with local time
                        in broken-down representation.
      in_dst_time_gap - pointer to bool which is set to true if datetime
                        value passed doesn't really exist (i.e. falls into
                        spring time-gap) and is not touched otherwise.

  DESCRIPTION
    Please see ::TIME_to_gmt_sec for function description and
    parameter restrictions.

  RETURN VALUE
    Corresponding time_t value or 0 in case of error
*/
time_t
Time_zone_db::TIME_to_gmt_sec(const DRIZZLE_TIME *t, bool *in_dst_time_gap) const
{
  return ::drizzled::TIME_to_gmt_sec(t, tz_info, in_dst_time_gap);
}


/*
  Converts time from UTC seconds since Epoch (time_t) representation
  to local time zone described in broken-down representation.

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp - pointer to DRIZZLE_TIME structure to fill-in
      t   - time_t value to be converted
*/
void
Time_zone_db::gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const
{
  ::drizzled::gmt_sec_to_TIME(tmp, t, tz_info);
}


/*
  Get name of time zone

  SYNOPSIS
    get_name()

  RETURN VALUE
    Name of time zone as ASCIIZ-string
*/
const String *
Time_zone_db::get_name() const
{
  return tz_name;
}


/*
  Instance of this class represents time zone which
  was specified as offset from UTC.
*/
class Time_zone_offset : public Time_zone
{
public:
  Time_zone_offset(long tz_offset_arg);
  virtual time_t TIME_to_gmt_sec(const DRIZZLE_TIME *t,
                                    bool *in_dst_time_gap) const;
  virtual void   gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const;
  virtual const String * get_name() const;
  /*
    This have to be public because we want to be able to access it from
    my_offset_tzs_get_key() function
  */
  long offset;
private:
  /* Extra reserve because of snprintf */
  char name_buff[7+16];
  String name;
};


/*
  Initializes object representing time zone described by its offset from UTC.

  SYNOPSIS
    Time_zone_offset()
      tz_offset_arg - offset from UTC in seconds.
                      Positive for direction to east.
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


/*
  Converts local time in time zone described as offset from UTC
  from DRIZZLE_TIME representation to its time_t representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to DRIZZLE_TIME structure with local time
                        in broken-down representation.
      in_dst_time_gap - pointer to bool which should be set to true if
                        datetime  value passed doesn't really exist
                        (i.e. falls into spring time-gap) and is not
                        touched otherwise.
                        It is not really used in this class.

  RETURN VALUE
    Corresponding time_t value or 0 in case of error
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


/*
  Converts time from UTC seconds since Epoch (time_t) representation
  to local time zone described as offset from UTC and in broken-down
  representation.

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp - pointer to DRIZZLE_TIME structure to fill-in
      t   - time_t value to be converted
*/
void
Time_zone_offset::gmt_sec_to_TIME(DRIZZLE_TIME *tmp, time_t t) const
{
  sec_to_TIME(tmp, t, offset);
}


/*
  Get name of time zone

  SYNOPSIS
    get_name()

  RETURN VALUE
    Name of time zone as pointer to String object
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


/*
  Initialize time zone support infrastructure.

  SYNOPSIS
    my_tz_init()
      session            - current thread object
      default_tzname - default time zone or 0 if none.
      bootstrap      - indicates whenever we are in bootstrap mode

  DESCRIPTION
    This function will init memory structures needed for time zone support,
    it will register mandatory SYSTEM time zone in them. It will try to open
    mysql.time_zone* tables and load information about default time zone and
    information which further will be shared among all time zones loaded.
    If system tables with time zone descriptions don't exist it won't fail
    (unless default_tzname is time zone from tables). If bootstrap parameter
    is true then this routine assumes that we are in bootstrap mode and won't
    load time zone descriptions unless someone specifies default time zone
    which is supposedly stored in those tables.
    It'll also set default time zone if it is specified.

  RETURN VALUES
    0 - ok
    1 - Error
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

/*
  Get Time_zone object for specified time zone.

  Not implemented yet. This needs to hook into some sort of OS system call.

*/
Time_zone *
my_tz_find(Session *,
           const String *)
{
  return NULL;
}

} /* namespace drizzled */
