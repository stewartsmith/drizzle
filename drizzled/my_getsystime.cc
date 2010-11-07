/* Copyright (C) 2004 MySQL AB

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

/* get time since epoc in 100 nanosec units */
/* thus to get the current time we should use the system function
   with the highest possible resolution */

/*
   TODO: in functions my_micro_time() and my_micro_time_and_time() there
   exists some common code that should be merged into a function.
*/

#include "config.h"
#include "drizzled/drizzle_time.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

namespace drizzled
{

uint64_t my_getsystime()
{
#ifdef HAVE_CLOCK_GETTIME
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return (uint64_t)tp.tv_sec*10000000+(uint64_t)tp.tv_nsec/100;
#else
  /* TODO: check for other possibilities for hi-res timestamping */
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return (uint64_t)tv.tv_sec*10000000+(uint64_t)tv.tv_usec*10;
#endif
}

/*
  Return time in micro seconds

  SYNOPSIS
    my_micro_time()

  NOTES
    This function is to be used to measure performance in micro seconds.
    As it's not defined whats the start time for the clock, this function
    us only useful to measure time between two moments.

    For windows platforms we need the frequency value of the CUP. This is
    initalized in my_init.c through QueryPerformanceFrequency().

    If Windows platform doesn't support QueryPerformanceFrequency() we will
    obtain the time via GetClockCount, which only supports milliseconds.

  RETURN
    Value in microseconds from some undefined point in time
*/

uint64_t my_micro_time()
{
#if defined(HAVE_GETHRTIME)
  return gethrtime()/1000;
#else
  uint64_t newtime;
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0)
  {}
  newtime= (uint64_t)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;
#endif  /* defined(HAVE_GETHRTIME) */
}


/*
  Return time in seconds and timer in microseconds (not different start!)

  SYNOPSIS
    my_micro_time_and_time()
    time_arg		Will be set to seconds since epoch (00:00:00 UTC,
                        January 1, 1970)

  NOTES
    This function is to be useful when we need both the time and microtime.
    For example in MySQL this is used to get the query time start of a query
    and to measure the time of a query (for the slow query log)

  IMPLEMENTATION
    Value of time is as in time() call.
    Value of microtime is same as my_micro_time(), which may be totally
    unrealated to time()

  RETURN
    Value in microseconds from some undefined point in time
*/


uint64_t my_micro_time_and_time(time_t *time_arg)
{
  uint64_t newtime;
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0) {}
  *time_arg= t.tv_sec;
  newtime= (uint64_t)t.tv_sec * 1000000 + t.tv_usec;

  return newtime;
}

} /* namespace drizzled */
