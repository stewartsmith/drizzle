/* Copyright (C) 2004-2006 MySQL AB

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <config.h>

#include <drizzled/type/time.h>

#include <drizzled/util/gmtime.h>

#include <drizzled/internal/m_string.h>
#include <drizzled/charset.h>
#include <drizzled/util/test.h>
#include <drizzled/definitions.h>
#include <drizzled/sql_string.h>

#include <cstdio>
#include <algorithm>

using namespace std;

namespace drizzled
{

static int check_time_range(type::Time *my_time, int *warning);

/* Windows version of localtime_r() is declared in my_ptrhead.h */

uint64_t log_10_int[20]=
{
  1, 10, 100, 1000, 10000UL, 100000UL, 1000000UL, 10000000UL,
  100000000ULL, 1000000000ULL, 10000000000ULL, 100000000000ULL,
  1000000000000ULL, 10000000000000ULL, 100000000000000ULL,
  1000000000000000ULL, 10000000000000000ULL, 100000000000000000ULL,
  1000000000000000000ULL, 10000000000000000000ULL
};


/* Position for YYYY-DD-MM HH-MM-DD.FFFFFF AM in default format */

static unsigned char internal_format_positions[]=
{0, 1, 2, 3, 4, 5, 6, (unsigned char) 255};

static char time_separator=':';

static uint32_t const days_at_timestart=719528;	/* daynr at 1970.01.01 */
unsigned char days_in_month[]= {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0};

/*
  Offset of system time zone from UTC in seconds used to speed up
  work of my_system_gmt_sec() function.
*/
static long my_time_zone=0;


/* Calc days in one year. works with 0 <= year <= 99 */

uint32_t calc_days_in_year(uint32_t year)
{
  return ((year & 3) == 0 && (year%100 || (year%400 == 0 && year)) ?
          366 : 365);
}


namespace type {
/**
  @brief Check datetime value for validity according to flags.

  @param[in]  ltime          Date to check.
  @param[in]  not_zero_date  ltime is not the zero date
  @param[in]  flags          flags to check
                             (see store() flags in drizzle_time.h)
  @param[out] was_cut        set to 2 if value was invalid according to flags.
                             (Feb 29 in non-leap etc.)  This remains unchanged
                             if value is not invalid.

  @details Here we assume that year and month is ok!
    If month is 0 we allow any date. (This only happens if we allow zero
    date parts in store())
    Disallow dates with zero year and non-zero month and/or day.

  @return
    0  OK
    1  error
*/

bool Time::check(bool not_zero_date, uint32_t flags, type::cut_t &was_cut) const
{
  if (not_zero_date)
  {
    if ((((flags & TIME_NO_ZERO_IN_DATE) || !(flags & TIME_FUZZY_DATE)) &&
         (month == 0 || day == 0)) ||
        (not (flags & TIME_INVALID_DATES) &&
         month && day > days_in_month[month-1] &&
         (month != 2 || calc_days_in_year(year) != 366 ||
          day != 29)))
    {
      was_cut= type::INVALID;
      return true;
    }
  }
  else if (flags & TIME_NO_ZERO_DATE)
  {
    /*
      We don't set &was_cut here to signal that the problem was a zero date
      and not an invalid date
    */
    return true;
  }
  return false;
}

/*
  Convert a timestamp string to a type::Time value.

  SYNOPSIS
    store()
    str                 String to parse
    length              Length of string
    l_time              Date is stored here
    flags               Bitmap of following items
                        TIME_FUZZY_DATE    Set if we should allow partial dates
                        TIME_DATETIME_ONLY Set if we only allow full datetimes.
                        TIME_NO_ZERO_IN_DATE	Don't allow partial dates
                        TIME_NO_ZERO_DATE	Don't allow 0000-00-00 date
                        TIME_INVALID_DATES	Allow 2000-02-31
    was_cut             0	Value OK
			1       If value was cut during conversion
			2	check(date,flags) considers date invalid

  DESCRIPTION
    At least the following formats are recogniced (based on number of digits)
    YYMMDD, YYYYMMDD, YYMMDDHHMMSS, YYYYMMDDHHMMSS
    YY-MM-DD, YYYY-MM-DD, YY-MM-DD HH.MM.SS
    YYYYMMDDTHHMMSS  where T is a the character T (ISO8601)
    Also dates where all parts are zero are allowed

    The second part may have an optional .###### fraction part.

  NOTES
   This function should work with a format position vector as long as the
   following things holds:
   - All date are kept together and all time parts are kept together
   - Date and time parts must be separated by blank
   - Second fractions must come after second part and be separated
     by a '.'.  (The second fractions are optional)
   - AM/PM must come after second fractions (or after seconds if no fractions)
   - Year must always been specified.
   - If time is before date, then we will use datetime format only if
     the argument consist of two parts, separated by space.
     Otherwise we will assume the argument is a date.
   - The hour part must be specified in hour-minute-second order.

  RETURN VALUES
    DRIZZLE_TIMESTAMP_NONE        String wasn't a timestamp, like
                                [DD [HH:[MM:[SS]]]].fraction.
                                l_time is not changed.
    DRIZZLE_TIMESTAMP_DATE        DATE string (YY MM and DD parts ok)
    DRIZZLE_TIMESTAMP_DATETIME    Full timestamp
    DRIZZLE_TIMESTAMP_ERROR       Timestamp with wrong values.
                                All elements in l_time is set to 0
*/

#define MAX_DATE_PARTS 8

type::timestamp_t Time::store(const char *str, uint32_t length, uint32_t flags, type::cut_t &was_cut)
{
  uint32_t field_length, year_length=4, digits, i, number_of_fields;
  uint32_t date[MAX_DATE_PARTS], date_len[MAX_DATE_PARTS];
  uint32_t add_hours= 0, start_loop;
  uint32_t not_zero_date, allow_space;
  bool is_internal_format;
  const char *pos, *last_field_pos=NULL;
  const char *end=str+length;
  const unsigned char *format_position;
  bool found_delimitier= 0, found_space= 0;
  uint32_t frac_pos, frac_len;

  was_cut= type::VALID;

  /* Skip space at start */
  for (; str != end && my_isspace(&my_charset_utf8_general_ci, *str) ; str++)
    ;

  if (str == end || ! my_isdigit(&my_charset_utf8_general_ci, *str))
  {
    was_cut= type::CUT;
    return(type::DRIZZLE_TIMESTAMP_NONE);
  }

  is_internal_format= 0;
  /* This has to be changed if want to activate different timestamp formats */
  format_position= internal_format_positions;

  /*
    Calculate number of digits in first part.
    If length= 8 or >= 14 then year is of format YYYY.
    (YYYY-MM-DD,  YYYYMMDD, YYYYYMMDDHHMMSS)
  */
  for (pos=str;
       pos != end && (my_isdigit(&my_charset_utf8_general_ci,*pos) || *pos == 'T');
       pos++)
    ;

  digits= (uint32_t) (pos-str);
  start_loop= 0;                                /* Start of scan loop */
  date_len[format_position[0]]= 0;              /* Length of year field */
  if (pos == end || *pos == '.')
  {
    /* Found date in internal format (only numbers like YYYYMMDD) */
    year_length= (digits == 4 || digits == 8 || digits >= 14) ? 4 : 2;
    field_length= year_length;
    is_internal_format= 1;
    format_position= internal_format_positions;
  }
  else
  {
    if (format_position[0] >= 3)                /* If year is after HHMMDD */
    {
      /*
        If year is not in first part then we have to determinate if we got
        a date field or a datetime field.
        We do this by checking if there is two numbers separated by
        space in the input.
      */
      while (pos < end && !my_isspace(&my_charset_utf8_general_ci, *pos))
        pos++;
      while (pos < end && !my_isdigit(&my_charset_utf8_general_ci, *pos))
        pos++;
      if (pos == end)
      {
        if (flags & TIME_DATETIME_ONLY)
        {
          was_cut= type::CUT;
          return(type::DRIZZLE_TIMESTAMP_NONE);   /* Can't be a full datetime */
        }
        /* Date field.  Set hour, minutes and seconds to 0 */
        date[0]= date[1]= date[2]= date[3]= date[4]= 0;
        start_loop= 5;                         /* Start with first date part */
      }
    }

    field_length= format_position[0] == 0 ? 4 : 2;
  }

  /*
    Only allow space in the first "part" of the datetime field and:
    - after days, part seconds
    - before and after AM/PM (handled by code later)

    2003-03-03 20:00:20 AM
    20:00:20.000000 AM 03-03-2000
  */
  i= max((uint32_t) format_position[0], (uint32_t) format_position[1]);
  set_if_bigger(i, (uint32_t) format_position[2]);
  allow_space= ((1 << i) | (1 << format_position[6]));
  allow_space&= (1 | 2 | 4 | 8);

  not_zero_date= 0;
  for (i = start_loop;
       i < MAX_DATE_PARTS-1 && str != end &&
         my_isdigit(&my_charset_utf8_general_ci,*str);
       i++)
  {
    const char *start= str;
    uint32_t tmp_value= (uint32_t) (unsigned char) (*str++ - '0');
    while (str != end && my_isdigit(&my_charset_utf8_general_ci,str[0]) &&
           (!is_internal_format || --field_length))
    {
      tmp_value=tmp_value*10 + (uint32_t) (unsigned char) (*str - '0');
      str++;
    }
    date_len[i]= (uint32_t) (str - start);
    if (tmp_value > 999999)                     /* Impossible date part */
    {
      was_cut= type::CUT;
      return(type::DRIZZLE_TIMESTAMP_NONE);
    }
    date[i]=tmp_value;
    not_zero_date|= tmp_value;

    /* Length of next field */
    field_length= format_position[i+1] == 0 ? 4 : 2;

    if ((last_field_pos= str) == end)
    {
      i++;                                      /* Register last found part */
      break;
    }
    /* Allow a 'T' after day to allow CCYYMMDDT type of fields */
    if (i == format_position[2] && *str == 'T')
    {
      str++;                                    /* ISO8601:  CCYYMMDDThhmmss */
      continue;
    }
    if (i == format_position[5])                /* Seconds */
    {
      if (*str == '.')                          /* Followed by part seconds */
      {
        str++;
        field_length= 6;                        /* 6 digits */
      }
      continue;
    }
    while (str != end &&
           (my_ispunct(&my_charset_utf8_general_ci,*str) ||
            my_isspace(&my_charset_utf8_general_ci,*str)))
    {
      if (my_isspace(&my_charset_utf8_general_ci,*str))
      {
        if (!(allow_space & (1 << i)))
        {
          was_cut= type::CUT;
          return(type::DRIZZLE_TIMESTAMP_NONE);
        }
        found_space= 1;
      }
      str++;
      found_delimitier= 1;                      /* Should be a 'normal' date */
    }
    /* Check if next position is AM/PM */
    if (i == format_position[6])                /* Seconds, time for AM/PM */
    {
      i++;                                      /* Skip AM/PM part */
      if (format_position[7] != 255)            /* If using AM/PM */
      {
        if (str+2 <= end && (str[1] == 'M' || str[1] == 'm'))
        {
          if (str[0] == 'p' || str[0] == 'P')
            add_hours= 12;
          else if (str[0] != 'a' || str[0] != 'A')
            continue;                           /* Not AM/PM */
          str+= 2;                              /* Skip AM/PM */
          /* Skip space after AM/PM */
          while (str != end && my_isspace(&my_charset_utf8_general_ci,*str))
            str++;
        }
      }
    }
    last_field_pos= str;
  }
  if (found_delimitier && !found_space && (flags & TIME_DATETIME_ONLY))
  {
    was_cut= type::CUT;
    return(type::DRIZZLE_TIMESTAMP_NONE);          /* Can't be a datetime */
  }

  str= last_field_pos;

  number_of_fields= i - start_loop;
  while (i < MAX_DATE_PARTS)
  {
    date_len[i]= 0;
    date[i++]= 0;
  }

  do 
  {
    if (not is_internal_format)
    {
      year_length= date_len[(uint32_t) format_position[0]];
      if (!year_length)                           /* Year must be specified */
      {
        was_cut= type::CUT;
        return(type::DRIZZLE_TIMESTAMP_NONE);
      }

      this->year=               date[(uint32_t) format_position[0]];
      this->month=              date[(uint32_t) format_position[1]];
      this->day=                date[(uint32_t) format_position[2]];
      this->hour=               date[(uint32_t) format_position[3]];
      this->minute=             date[(uint32_t) format_position[4]];
      this->second=             date[(uint32_t) format_position[5]];

      frac_pos= (uint32_t) format_position[6];
      frac_len= date_len[frac_pos];
      if (frac_len < 6)
        date[frac_pos]*= (uint32_t) log_10_int[6 - frac_len];
      this->second_part= date[frac_pos];

      if (format_position[7] != (unsigned char) 255)
      {
        if (this->hour > 12)
        {
          was_cut= type::CUT;
          break;
        }
        this->hour= this->hour%12 + add_hours;
      }
    }
    else
    {
      this->year=       date[0];
      this->month=      date[1];
      this->day=        date[2];
      this->hour=       date[3];
      this->minute=     date[4];
      this->second=     date[5];
      if (date_len[6] < 6)
        date[6]*= (uint32_t) log_10_int[6 - date_len[6]];
      this->second_part=date[6];
    }
    this->neg= 0;

    if (year_length == 2 && not_zero_date)
      this->year+= (this->year < YY_PART_YEAR ? 2000 : 1900);

    if (number_of_fields < 3 ||
        this->year > 9999 || this->month > 12 ||
        this->day > 31 || this->hour > 23 ||
        this->minute > 59 || this->second > 59)
    {
      /* Only give warning for a zero date if there is some garbage after */
      if (!not_zero_date)                         /* If zero date */
      {
        for (; str != end ; str++)
        {
          if (!my_isspace(&my_charset_utf8_general_ci, *str))
          {
            not_zero_date= 1;                     /* Give warning */
            break;
          }
        }
      }
      was_cut= test(not_zero_date) ? type::CUT : type::VALID;
      break;
    }

    if (check(not_zero_date != 0, flags, was_cut))
    {
      break;
    }

    this->time_type= (number_of_fields <= 3 ?
                      type::DRIZZLE_TIMESTAMP_DATE : type::DRIZZLE_TIMESTAMP_DATETIME);

    for (; str != end ; str++)
    {
      if (!my_isspace(&my_charset_utf8_general_ci,*str))
      {
        was_cut= type::CUT;
        break;
      }
    }

    return(time_type= (number_of_fields <= 3 ? type::DRIZZLE_TIMESTAMP_DATE : type::DRIZZLE_TIMESTAMP_DATETIME));
  } while (0);

  reset();

  return type::DRIZZLE_TIMESTAMP_ERROR;
}

type::timestamp_t Time::store(const char *str, uint32_t length, uint32_t flags)
{
  type::cut_t was_cut;
  return store(str, length, flags, was_cut);
}

/*
 Convert a time string to a type::Time struct.

  SYNOPSIS
   str_to_time()
   str                  A string in full TIMESTAMP format or
                        [-] DAYS [H]H:MM:SS, [H]H:MM:SS, [M]M:SS, [H]HMMSS,
                        [M]MSS or [S]S
                        There may be an optional [.second_part] after seconds
   length               Length of str
   l_time               Store result here
   warning              Set DRIZZLE_TIME_WARN_TRUNCATED flag if the input string
                        was cut during conversion, and/or
                        DRIZZLE_TIME_WARN_OUT_OF_RANGE flag, if the value is
                        out of range.

   NOTES
     Because of the extra days argument, this function can only
     work with times where the time arguments are in the above order.

   RETURN
     0  ok
     1  error
*/

bool Time::store(const char *str, uint32_t length, int &warning, type::timestamp_t arg)
{
  uint32_t date[5];
  uint64_t value;
  const char *end=str+length, *end_of_days;
  bool found_days,found_hours;
  uint32_t state;

  assert(arg == DRIZZLE_TIMESTAMP_TIME);

  this->neg=0;
  warning= 0;
  for (; str != end && my_isspace(&my_charset_utf8_general_ci,*str) ; str++)
    length--;
  if (str != end && *str == '-')
  {
    this->neg=1;
    str++;
    length--;
  }
  if (str == end)
    return true;

  /* Check first if this is a full TIMESTAMP */
  if (length >= 12)
  {                                             /* Probably full timestamp */
    type::cut_t was_cut;
    type::timestamp_t res= this->store(str, length, (TIME_FUZZY_DATE | TIME_DATETIME_ONLY), was_cut);
    if ((int) res >= (int) type::DRIZZLE_TIMESTAMP_ERROR)
    {
      if (was_cut != type::VALID)
        warning|= DRIZZLE_TIME_WARN_TRUNCATED;

      return res == type::DRIZZLE_TIMESTAMP_ERROR;
    }
  }

  /* Not a timestamp. Try to get this as a DAYS_TO_SECOND string */
  for (value=0; str != end && my_isdigit(&my_charset_utf8_general_ci,*str) ; str++)
    value=value*10L + (long) (*str - '0');

  /* Skip all space after 'days' */
  end_of_days= str;
  for (; str != end && my_isspace(&my_charset_utf8_general_ci, str[0]) ; str++)
    ;

  found_days=found_hours=0;
  if ((uint32_t) (end-str) > 1 && str != end_of_days &&
      my_isdigit(&my_charset_utf8_general_ci, *str))
  {                                             /* Found days part */
    date[0]= (uint32_t) value;
    state= 1;                                   /* Assume next is hours */
    found_days= 1;
  }
  else if ((end-str) > 1 &&  *str == time_separator &&
           my_isdigit(&my_charset_utf8_general_ci, str[1]))
  {
    date[0]= 0;                                 /* Assume we found hours */
    date[1]= (uint32_t) value;
    state=2;
    found_hours=1;
    str++;                                      /* skip ':' */
  }
  else
  {
    /* String given as one number; assume HHMMSS format */
    date[0]= 0;
    date[1]= (uint32_t) (value/10000);
    date[2]= (uint32_t) (value/100 % 100);
    date[3]= (uint32_t) (value % 100);
    state=4;
    goto fractional;
  }

  /* Read hours, minutes and seconds */
  for (;;)
  {
    for (value=0; str != end && my_isdigit(&my_charset_utf8_general_ci,*str) ; str++)
      value=value*10L + (long) (*str - '0');
    date[state++]= (uint32_t) value;
    if (state == 4 || (end-str) < 2 || *str != time_separator ||
        !my_isdigit(&my_charset_utf8_general_ci,str[1]))
      break;
    str++;                                      /* Skip time_separator (':') */
  }

  if (state != 4)
  {                                             /* Not HH:MM:SS */
    /* Fix the date to assume that seconds was given */
    if (!found_hours && !found_days)
    {
      internal::bmove_upp((unsigned char*) (date+4), (unsigned char*) (date+state),
                sizeof(long)*(state-1));
      memset(date, 0, sizeof(long)*(4-state));
    }
    else
      memset(date+state, 0, sizeof(long)*(4-state));
  }

fractional:
  /* Get fractional second part */
  if ((end-str) >= 2 && *str == '.' && my_isdigit(&my_charset_utf8_general_ci,str[1]))
  {
    int field_length= 5;
    str++; value=(uint32_t) (unsigned char) (*str - '0');
    while (++str != end && my_isdigit(&my_charset_utf8_general_ci, *str))
    {
      if (field_length-- > 0)
        value= value*10 + (uint32_t) (unsigned char) (*str - '0');
    }
    if (field_length > 0)
    {
      value*= (long) log_10_int[field_length];
    }
    else if (field_length < 0)
    {
      warning|= DRIZZLE_TIME_WARN_TRUNCATED;
    }

    date[4]= (uint32_t) value;
  }
  else
  {
    date[4]=0;
  }

  /* Check for exponent part: E<gigit> | E<sign><digit> */
  /* (may occur as result of %g formatting of time value) */
  if ((end - str) > 1 &&
      (*str == 'e' || *str == 'E') &&
      (my_isdigit(&my_charset_utf8_general_ci, str[1]) ||
       ((str[1] == '-' || str[1] == '+') &&
        (end - str) > 2 &&
        my_isdigit(&my_charset_utf8_general_ci, str[2]))))
    return 1;

  if (internal_format_positions[7] != 255)
  {
    /* Read a possible AM/PM */
    while (str != end && my_isspace(&my_charset_utf8_general_ci, *str))
      str++;
    if (str+2 <= end && (str[1] == 'M' || str[1] == 'm'))
    {
      if (str[0] == 'p' || str[0] == 'P')
      {
        str+= 2;
        date[1]= date[1]%12 + 12;
      }
      else if (str[0] == 'a' || str[0] == 'A')
        str+=2;
    }
  }

  /* Integer overflow checks */
  if (date[0] > UINT_MAX || date[1] > UINT_MAX ||
      date[2] > UINT_MAX || date[3] > UINT_MAX ||
      date[4] > UINT_MAX)
    return 1;

  this->year=         0;                      /* For protocol::store_time */
  this->month=        0;
  this->day=          date[0];
  this->hour=         date[1];
  this->minute=       date[2];
  this->second=       date[3];
  this->second_part=  date[4];
  this->time_type= type::DRIZZLE_TIMESTAMP_TIME;

  /* Check if the value is valid and fits into type::Time range */
  if (check_time_range(this, &warning))
  {
    return 1;
  }

  /* Check if there is garbage at end of the type::Time specification */
  if (str != end)
  {
    do
    {
      if (!my_isspace(&my_charset_utf8_general_ci,*str))
      {
        warning|= DRIZZLE_TIME_WARN_TRUNCATED;
        break;
      }
    } while (++str != end);
  }
  return 0;
}

} // namespace type



/*
  Check 'time' value to lie in the type::Time range

  SYNOPSIS:
    check_time_range()
    time     pointer to type::Time value
    warning  set DRIZZLE_TIME_WARN_OUT_OF_RANGE flag if the value is out of range

  DESCRIPTION
  If the time value lies outside of the range [-838:59:59, 838:59:59],
  set it to the closest endpoint of the range and set
  DRIZZLE_TIME_WARN_OUT_OF_RANGE flag in the 'warning' variable.

  RETURN
    0        time value is valid, but was possibly truncated
    1        time value is invalid
*/

static int check_time_range(type::Time *my_time, int *warning)
{
  int64_t hour;

  if (my_time->minute >= 60 || my_time->second >= 60)
    return 1;

  hour= my_time->hour + (24*my_time->day);
  if (hour <= TIME_MAX_HOUR &&
      (hour != TIME_MAX_HOUR || my_time->minute != TIME_MAX_MINUTE ||
       my_time->second != TIME_MAX_SECOND || !my_time->second_part))
    return 0;

  my_time->day= 0;
  my_time->hour= TIME_MAX_HOUR;
  my_time->minute= TIME_MAX_MINUTE;
  my_time->second= TIME_MAX_SECOND;
  my_time->second_part= 0;
  *warning|= DRIZZLE_TIME_WARN_OUT_OF_RANGE;
  return 0;
}


/*
  Prepare offset of system time zone from UTC for my_system_gmt_sec() func.

  SYNOPSIS
    init_time()
*/
void init_time(void)
{
  time_t seconds;
  struct tm *l_time,tm_tmp;
  type::Time my_time;
  type::Time::epoch_t epoch;
  bool not_used;

  seconds= (time_t) time((time_t*) 0);
  localtime_r(&seconds, &tm_tmp);
  l_time= &tm_tmp;
  my_time_zone=		3600;		/* Comp. for -3600 in my_gmt_sec */
  my_time.year=		(uint32_t) l_time->tm_year+1900;
  my_time.month=	(uint32_t) l_time->tm_mon+1;
  my_time.day=		(uint32_t) l_time->tm_mday;
  my_time.hour=		(uint32_t) l_time->tm_hour;
  my_time.minute=	(uint32_t) l_time->tm_min;
  my_time.second=	(uint32_t) l_time->tm_sec;
  my_time.time_type=	type::DRIZZLE_TIMESTAMP_NONE;
  my_time.second_part=  0;
  my_time.neg=          false;
  my_time.convert(epoch, &my_time_zone, &not_used); /* Init my_time_zone */
}


/*
  Handle 2 digit year conversions

  SYNOPSIS
  year_2000_handling()
  year     2 digit year

  RETURN
    Year between 1970-2069
*/

uint32_t year_2000_handling(uint32_t year)
{
  if ((year=year+1900) < 1900+YY_PART_YEAR)
    year+=100;
  return year;
}


/*
  Calculate nr of day since year 0 in new date-system (from 1615)

  SYNOPSIS
    calc_daynr()
    year		 Year (exact 4 digit year, no year conversions)
    month		 Month
    day			 Day

  NOTES: 0000-00-00 is a valid date, and will return 0

  RETURN
    Days since 0000-00-00
*/

long calc_daynr(uint32_t year,uint32_t month,uint32_t day)
{
  long delsum;
  int temp;

  if (year == 0 && month == 0 && day == 0)
    return 0;				/* Skip errors */
  delsum= (long) (365L * year+ 31*(month-1) +day);
  if (month <= 2)
      year--;
  else
    delsum-= (long) (month*4+23)/10;
  temp=(int) ((year/100+1)*3)/4;
  return(delsum+(int) year/4-temp);
} /* calc_daynr */


namespace type {
/*
  Convert time in type::Time representation in system time zone to its
  time_t form (number of seconds in UTC since begginning of Unix Epoch).

  SYNOPSIS
    my_system_gmt_sec()
      t               - time value to be converted
      my_timezone     - pointer to long where offset of system time zone
                        from UTC will be stored for caching
      in_dst_time_gap - set to true if time falls into spring time-gap

  NOTES
    The idea is to cache the time zone offset from UTC (including daylight
    saving time) for the next call to make things faster. But currently we
    just calculate this offset during startup (by calling init_time()
    function) and use it all the time.
    Time value provided should be legal time value (e.g. '2003-01-01 25:00:00'
    is not allowed).

  RETURN VALUE
    Time in UTC seconds since Unix Epoch representation.
*/
void Time::convert(epoch_t &epoch, long *my_timezone, bool *in_dst_time_gap, bool skip_timezone) const
{
  uint32_t loop;
  int shift= 0;
  type::Time tmp_time;
  type::Time *t= &tmp_time;
  struct tm *l_time,tm_tmp;
  long diff, current_timezone;

  /*
    Use temp variable to avoid trashing input data, which could happen in
    case of shift required for boundary dates processing.
  */
  tmp_time= *this;

  if (not t->isValidEpoch())
  {
    epoch= 0;
    return;
  }

  /*
    Calculate the gmt time based on current time and timezone
    The -1 on the end is to ensure that if have a date that exists twice
    (like 2002-10-27 02:00:0 MET), we will find the initial date.

    By doing -3600 we will have to call localtime_r() several times, but
    I couldn't come up with a better way to get a repeatable result :(

    We can't use mktime() as it's buggy on many platforms and not thread safe.

    Note: this code assumes that our time_t estimation is not too far away
    from real value (we assume that localtime_r(epoch) will return something
    within 24 hrs from t) which is probably true for all current time zones.

    Note2: For the dates, which have time_t representation close to
    MAX_INT32 (efficient time_t limit for supported platforms), we should
    do a small trick to avoid overflow. That is, convert the date, which is
    two days earlier, and then add these days to the final value.

    The same trick is done for the values close to 0 in time_t
    representation for platfroms with unsigned time_t (QNX).

    To be more verbose, here is a sample (extracted from the code below):
    (calc_daynr(2038, 1, 19) - (long) days_at_timestart)*86400L + 4*3600L
    would return -2147480896 because of the long type overflow. In result
    we would get 1901 year in localtime_r(), which is an obvious error.

    Alike problem raises with the dates close to Epoch. E.g.
    (calc_daynr(1969, 12, 31) - (long) days_at_timestart)*86400L + 23*3600L
    will give -3600.

    On some platforms, (E.g. on QNX) time_t is unsigned and localtime(-3600)
    wil give us a date around 2106 year. Which is no good.

    Theoreticaly, there could be problems with the latter conversion:
    there are at least two timezones, which had time switches near 1 Jan
    of 1970 (because of political reasons). These are America/Hermosillo and
    America/Mazatlan time zones. They changed their offset on
    1970-01-01 08:00:00 UTC from UTC-8 to UTC-7. For these zones
    the code below will give incorrect results for dates close to
    1970-01-01, in the case OS takes into account these historical switches.
    Luckily, it seems that we support only one platform with unsigned
    time_t. It's QNX. And QNX does not support historical timezone data at all.
    E.g. there are no /usr/share/zoneinfo/ files or any other mean to supply
    historical information for localtime_r() etc. That is, the problem is not
    relevant to QNX.

    We are safe with shifts close to MAX_INT32, as there are no known
    time switches on Jan 2038 yet :)
  */
#ifdef TIME_T_UNSIGNED
  {
    /*
      We can get 0 in time_t representaion only on 1969, 31 of Dec or on
      1970, 1 of Jan. For both dates we use shift, which is added
      to t->day in order to step out a bit from the border.
      This is required for platforms, where time_t is unsigned.
      As far as I know, among the platforms we support it's only QNX.
      Note: the order of below if-statements is significant.
    */

    if ((t->year == TIMESTAMP_MIN_YEAR + 1) && (t->month == 1)
        && (t->day <= 10))
    {
      t->day+= 2;
      shift= -2;
    }

    if ((t->year == TIMESTAMP_MIN_YEAR) && (t->month == 12)
        && (t->day == 31))
    {
      t->year++;
      t->month= 1;
      t->day= 2;
      shift= -2;
    }
  }
#endif

  epoch= (type::Time::epoch_t) (((calc_daynr((uint32_t) t->year, (uint32_t) t->month, (uint32_t) t->day) -
                   (long) days_at_timestart)*86400L + (long) t->hour*3600L +
                  (long) (t->minute*60 + t->second)) + (time_t) my_time_zone -
                 3600);

  current_timezone= my_time_zone;
  if (skip_timezone)
  {
    util::gmtime(epoch, &tm_tmp);
  }
  else
  {
    util::localtime(epoch, &tm_tmp);
  }

  l_time= &tm_tmp;
  for (loop=0;
       loop < 2 &&
	 (t->hour != (uint32_t) l_time->tm_hour ||
	  t->minute != (uint32_t) l_time->tm_min ||
          t->second != (uint32_t) l_time->tm_sec);
       loop++)
  {					/* One check should be enough ? */
    /* Get difference in days */
    int days= t->day - l_time->tm_mday;
    if (days < -1)
      days= 1;					/* Month has wrapped */
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour)) +
          (long) (60*((int) t->minute - (int) l_time->tm_min)) +
          (long) ((int) t->second - (int) l_time->tm_sec));
    current_timezone+= diff+3600;		/* Compensate for -3600 above */
    epoch+= (time_t) diff;
    if (skip_timezone)
    {
      util::gmtime(epoch, &tm_tmp);
    }
    else
    {
      util::localtime(epoch, &tm_tmp);
    }
    l_time=&tm_tmp;
  }
  /*
    Fix that if we are in the non existing daylight saving time hour
    we move the start of the next real hour.

    This code doesn't handle such exotical thing as time-gaps whose length
    is more than one hour or non-integer (latter can theoretically happen
    if one of seconds will be removed due leap correction, or because of
    general time correction like it happened for Africa/Monrovia time zone
    in year 1972).
  */
  if (loop == 2 && t->hour != (uint32_t) l_time->tm_hour)
  {
    int days= t->day - l_time->tm_mday;
    if (days < -1)
      days=1;					/* Month has wrapped */
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour))+
	  (long) (60*((int) t->minute - (int) l_time->tm_min)) +
          (long) ((int) t->second - (int) l_time->tm_sec));
    if (diff == 3600)
      epoch+=3600 - t->minute*60 - t->second;	/* Move to next hour */
    else if (diff == -3600)
      epoch-=t->minute*60 + t->second;		/* Move to previous hour */

    *in_dst_time_gap= true;
  }
  *my_timezone= current_timezone;


  /* shift back, if we were dealing with boundary dates */
  epoch+= shift*86400L;

  /*
    This is possible for dates, which slightly exceed boundaries.
    Conversion will pass ok for them, but we don't allow them.
    First check will pass for platforms with signed time_t.
    instruction above (epoch+= shift*86400L) could exceed
    MAX_INT32 (== TIMESTAMP_MAX_VALUE) and overflow will happen.
    So, epoch < TIMESTAMP_MIN_VALUE will be triggered.
  */
  if (epoch < TIMESTAMP_MIN_VALUE)
  {
    epoch= 0;
  }
} /* my_system_gmt_sec */


void Time::store(const struct tm &from)
{
  _is_local_time= false;
  neg= 0;
  second_part= 0;
  year=	(int32_t) ((from.tm_year+1900) % 10000);
  month= (int32_t) from.tm_mon+1;
  day= (int32_t) from.tm_mday;
  hour=	(int32_t) from.tm_hour;
  minute= (int32_t) from.tm_min;
  second= (int32_t) from.tm_sec;

  time_type= DRIZZLE_TIMESTAMP_DATETIME;
}

void Time::store(const struct timeval &from)
{
  store(from.tv_sec, (usec_t)from.tv_usec);
  time_type= type::DRIZZLE_TIMESTAMP_DATETIME;
}


void Time::store(const type::Time::epoch_t &from, bool use_localtime)
{
  store(from, 0, use_localtime);
}

void Time::store(const type::Time::epoch_t &from_arg, const usec_t &from_fractional_seconds, bool use_localtime)
{
  epoch_t from= from_arg;

  if (use_localtime)
  {
    util::localtime(from, *this);
    _is_local_time= true;
  }
  else
  {
    util::gmtime(from, *this);
  }

  // Since time_t/epoch_t doesn't have fractional seconds, we have to
  // collect them outside of the gmtime function.
  second_part= from_fractional_seconds;
  time_type= DRIZZLE_TIMESTAMP_DATETIME;
}

// Only implemented for one case, extend as needed.
void Time::truncate(const timestamp_t arg)
{
  assert(arg == type::DRIZZLE_TIMESTAMP_TIME);
  year= month= day= 0;

  time_type= arg;
}

void Time::convert(String &str, timestamp_t arg)
{
  str.alloc(MAX_STRING_LENGTH);
  size_t length= MAX_STRING_LENGTH;

  convert(str.c_ptr(), length, arg);

  str.length(length);
  str.set_charset(&my_charset_bin);
}

void Time::convert(char *str, size_t &to_length, timestamp_t arg)
{
  int32_t length= 0;
  switch (arg) {
  case DRIZZLE_TIMESTAMP_DATETIME:
    length= snprintf(str, to_length,
                     "%04" PRIu32 "-%02" PRIu32 "-%02" PRIu32
                     " %02" PRIu32 ":%02" PRIu32 ":%02" PRIu32 ".%06" PRIu32,
                     year,
                     month,
                     day,
                     hour,
                     minute,
                     second,
                     second_part);
    break;

  case DRIZZLE_TIMESTAMP_DATE:
    length= snprintf(str, to_length, "%04u-%02u-%02u",
                     year,
                     month,
                     day);
    break;

  case DRIZZLE_TIMESTAMP_TIME:
    {
      uint32_t extra_hours= 0;

      length= snprintf(str, to_length,
                       "%s%02u:%02u:%02u",
                       (neg ? "-" : ""),
                       extra_hours+ hour,
                       minute,
                       second);
    }
    break;

  case DRIZZLE_TIMESTAMP_NONE:
  case DRIZZLE_TIMESTAMP_ERROR:
    assert(0);
    break;
  }

  if (length < 0)
  {
    to_length= 0;
    return;
  }

  to_length= length;
}

}

/*
  Convert datetime value specified as number to broken-down TIME
  representation and form value of DATETIME type as side-effect.

  SYNOPSIS
    number_to_datetime()
      nr         - datetime value as number
      time_res   - pointer for structure for broken-down representation
      flags      - flags to use in validating date, as in store()
      was_cut    0      Value ok
                 1      If value was cut during conversion
                 2      check(date,flags) considers date invalid

  DESCRIPTION
    Convert a datetime value of formats YYMMDD, YYYYMMDD, YYMMDDHHMSS,
    YYYYMMDDHHMMSS to broken-down type::Time representation. Return value in
    YYYYMMDDHHMMSS format as side-effect.

    This function also checks if datetime value fits in DATETIME range.

  RETURN VALUE
    -1              Timestamp with wrong values
    anything else   DATETIME as integer in YYYYMMDDHHMMSS format
    Datetime value in YYYYMMDDHHMMSS format.
*/

static int64_t number_to_datetime(int64_t nr, type::Time *time_res,
                                  uint32_t flags, type::cut_t &was_cut)
{
  long part1,part2;

  was_cut= type::VALID;
  time_res->reset();
  time_res->time_type=type::DRIZZLE_TIMESTAMP_DATE;

  if (nr == 0LL || nr >= 10000101000000LL)
  {
    time_res->time_type= type::DRIZZLE_TIMESTAMP_DATETIME;
    goto ok;
  }
  if (nr < 101)
    goto err;
  if (nr <= (YY_PART_YEAR-1)*10000L+1231L)
  {
    nr= (nr+20000000L)*1000000L;                 /* YYMMDD, year: 2000-2069 */
    goto ok;
  }
  if (nr < (YY_PART_YEAR)*10000L+101L)
    goto err;
  if (nr <= 991231L)
  {
    nr= (nr+19000000L)*1000000L;                 /* YYMMDD, year: 1970-1999 */
    goto ok;
  }
  if (nr < 10000101L)
    goto err;
  if (nr <= 99991231L)
  {
    nr= nr*1000000L;
    goto ok;
  }
  if (nr < 101000000L)
    goto err;

  time_res->time_type= type::DRIZZLE_TIMESTAMP_DATETIME;

  if (nr <= (YY_PART_YEAR-1) * 10000000000LL + 1231235959LL)
  {
    nr= nr + 20000000000000LL;                   /* YYMMDDHHMMSS, 2000-2069 */
    goto ok;
  }
  if (nr <  YY_PART_YEAR * 10000000000LL + 101000000LL)
    goto err;
  if (nr <= 991231235959LL)
    nr= nr + 19000000000000LL;		/* YYMMDDHHMMSS, 1970-1999 */

 ok:
  part1=(long) (nr / 1000000LL);
  part2=(long) (nr - (int64_t) part1 * 1000000LL);
  time_res->year=  (int) (part1/10000L);  part1%=10000L;
  time_res->month= (int) part1 / 100;
  time_res->day=   (int) part1 % 100;
  time_res->hour=  (int) (part2/10000L);  part2%=10000L;
  time_res->minute=(int) part2 / 100;
  time_res->second=(int) part2 % 100;

  if (time_res->year <= 9999 && time_res->month <= 12 &&
      time_res->day <= 31 && time_res->hour <= 23 &&
      time_res->minute <= 59 && time_res->second <= 59 &&
      not time_res->check((nr != 0), flags, was_cut))
  {
    return nr;
  }

  /* Don't want to have was_cut get set if NO_ZERO_DATE was violated. */
  if (!nr && (flags & TIME_NO_ZERO_DATE))
    return -1LL;

 err:
  was_cut= type::CUT;
  return -1LL;
}


namespace type {

void Time::convert(datetime_t &ret, int64_t nr, uint32_t flags)
{
  type::cut_t was_cut;
  ret= number_to_datetime(nr, this, flags, was_cut);
}

void Time::convert(datetime_t &ret, int64_t nr, uint32_t flags, type::cut_t &was_cut)
{
  ret= number_to_datetime(nr, this, flags, was_cut);
}

/*
  Convert struct type::Time (date and time split into year/month/day/hour/...
  to a number in format YYYYMMDDHHMMSS (DATETIME),
  YYYYMMDD (DATE)  or HHMMSS (TIME).
*/


void Time::convert(datetime_t &datetime, timestamp_t arg)
{
  switch (arg)
  {
    // Convert to YYYYMMDDHHMMSS format
  case type::DRIZZLE_TIMESTAMP_DATETIME:
    datetime= ((int64_t) (year * 10000UL + month * 100UL + day) * 1000000ULL +
               (int64_t) (hour * 10000UL + minute * 100UL + second));
    break;

    // Convert to YYYYMMDD
  case type::DRIZZLE_TIMESTAMP_DATE:
    datetime= (year * 10000UL + month * 100UL + day);
    break;

    // Convert to HHMMSS
  case type::DRIZZLE_TIMESTAMP_TIME:
    datetime= (hour * 10000UL + minute * 100UL + second);
    break;

  case type::DRIZZLE_TIMESTAMP_NONE:
  case type::DRIZZLE_TIMESTAMP_ERROR:
    datetime= 0;
  }
}

} // namespace type

} /* namespace drizzled */
