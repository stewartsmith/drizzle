//
// time.c
//
// Time routines
//
// Copyright (C) 2002 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// 1. Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.  
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.  
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
// SUCH DAMAGE.
// 

#include <config.h>

#include <drizzled/type/time.h>
#include <drizzled/util/gmtime.h>

namespace drizzled {
namespace util {

#define YEAR0                   1900
#define EPOCH_YR                1970
#define SECS_DAY                (24L * 60L * 60L)
#define LEAPYEAR(year)          (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define YEARSIZE(year)          (LEAPYEAR(year) ? 366 : 365)
#define FIRSTSUNDAY(timp)       (((timp)->tm_yday - (timp)->tm_wday + 420) % 7)
#define FIRSTDAYOF(timp)        (((timp)->tm_wday - (timp)->tm_yday + 420) % 7)

#define TIME_MAX                INT64_MIN

const int _ytab[2][12] = 
{
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

struct tm *gmtime(const type::epoch_t &timer, struct tm *tmbuf)
{
  uint64_t dayclock, dayno;
  int year = EPOCH_YR;

  if (timer < 0)
    return NULL;

  dayclock = (uint64_t) timer % SECS_DAY;
  dayno = (uint64_t) timer / SECS_DAY;

  tmbuf->tm_sec = dayclock % 60;
  tmbuf->tm_min = (dayclock % 3600) / 60;
  tmbuf->tm_hour = dayclock / 3600;
  tmbuf->tm_wday = (dayno + 4) % 7; // Day 0 was a thursday
  while (dayno >= (uint64_t) YEARSIZE(year)) 
  {
    dayno -= YEARSIZE(year);
    year++;
  }
  tmbuf->tm_year = year - YEAR0;
  tmbuf->tm_yday = dayno;
  tmbuf->tm_mon = 0;
  while (dayno >= (uint64_t) _ytab[LEAPYEAR(year)][tmbuf->tm_mon]) 
  {
    dayno -= _ytab[LEAPYEAR(year)][tmbuf->tm_mon];
    tmbuf->tm_mon++;
  }
  tmbuf->tm_mday = dayno + 1;
  tmbuf->tm_isdst = 0;

  return tmbuf;
}

void gmtime(const type::epoch_t &timer, type::Time &tmbuf)
{
  uint64_t dayclock, dayno;
  int32_t year= EPOCH_YR;

  if (timer < 0)
    return;

  tmbuf.reset();

  dayclock= (uint64_t) timer % SECS_DAY;
  dayno= (uint64_t) timer / SECS_DAY;

  tmbuf.second= dayclock % 60;
  tmbuf.minute= (dayclock % 3600) / 60;
  tmbuf.hour= dayclock / 3600;
  while (dayno >= (uint64_t) YEARSIZE(year)) 
  {
    dayno -= YEARSIZE(year);
    year++;
  }
  tmbuf.year= year;
  while (dayno >= (uint64_t) _ytab[LEAPYEAR(year)][tmbuf.month]) 
  {
    dayno -= _ytab[LEAPYEAR(year)][tmbuf.month];
    tmbuf.month++;
  }
  tmbuf.month++;
  tmbuf.day= dayno +1;
  tmbuf.time_type= type::DRIZZLE_TIMESTAMP_DATETIME;
}

} /* namespace util */
} /* namespace drizzled */
