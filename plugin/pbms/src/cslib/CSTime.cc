/* Copyright (C) 2008 PrimeBase Technologies GmbH, Germany
 *
 * PrimeBase Media Stream for MySQL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Original author: Paul McCullagh (H&G2JCtL)
 * Continued development: Barry Leslie
 *
 * 2007-05-20
 *
 *
 */

#include "CSConfig.h"

#include <string.h>

#ifdef OS_WINDOWS
#include <Windows.h>
extern int gettimeofday(struct timeval *tv, struct timezone *tz);
#else
#include <sys/time.h>
#endif

#include "CSTime.h"
#include "CSGlobal.h"
#include "CSStrUtil.h"

/*
 * timegm() is a none portable function so....
 * This is an implimentation of timegm() based on one found
 * here: http://www.opensync.org/changeset/1769
 *
 * Note to self: There is a better way to do this.
 * Since this is just used internally and tm_isdst is always 0
 * we only need to calculate the timezone offset to GM time once.
 */
static time_t my_timegm(struct tm *my_time)
{
	time_t local_secs, gm_secs;
	struct tm gm__rec, *gm_time;

	// Interpret 't' as the local time and convert it to seconds since the Epoch
	local_secs = mktime(my_time);
	if (local_secs == -1) {
		my_time->tm_hour--;
		local_secs = mktime (my_time);
		if (local_secs == -1)
			return -1; 
		local_secs += 3600;
	}
	
	// Get the gmtime based on the local seconds since the Epoch
	gm_time = gmtime_r(&local_secs, &gm__rec);
	gm_time->tm_isdst = 0;
	
	// Interpret gmtime as the local time and convert it to seconds since the Epoch
	gm_secs = mktime (gm_time);
	if (gm_secs == -1) {
		gm_time->tm_hour--;
		gm_secs = mktime (gm_time);
		if (gm_secs == -1)
			return -1; 
		gm_secs += 3600;
	}
	
	// Return the local time adjusted by the difference from GM time.
	return (local_secs - (gm_secs - local_secs));
}

CSTime::CSTime(s_int year, s_int mon, s_int day, s_int hour, s_int min, s_int sec, s_int nsec)
{
	setLocal(year, mon, day, hour, min, sec, nsec);
}

bool CSTime::isNull()
{
	return iIsNull;
}

void CSTime::setNull()
{
	iIsNull = true;
	iSeconds = 0;
	iNanoSeconds = 0;
}

void CSTime::setLocal(s_int year, s_int mon, s_int day, s_int hour, s_int min, s_int sec, s_int nsec)
{
	struct tm	ltime;
	time_t		secs;
	
	memset(&ltime, 0, sizeof(ltime));

	ltime.tm_sec = sec;
	ltime.tm_min = min;
	ltime.tm_hour = hour;
	ltime.tm_mday = day;
	ltime.tm_mon = mon - 1;
	ltime.tm_year = year - 1900;
	secs = mktime(&ltime);
	setUTC1970(secs, nsec);
}

void CSTime::getLocal(s_int& year, s_int& mon, s_int& day, s_int& hour, s_int& min, s_int& sec, s_int& nsec)
{
	struct tm	ltime;
	time_t		secs;

	memset(&ltime, 0, sizeof(ltime));
	
	getUTC1970(secs, nsec);
	localtime_r(&secs, &ltime);
	sec = ltime.tm_sec;
	min = ltime.tm_min;
	hour = ltime.tm_hour;
	day = ltime.tm_mday;
	mon = ltime.tm_mon + 1;
	year = ltime.tm_year + 1900;
}

void CSTime::setUTC(s_int year, s_int mon, s_int day, s_int hour, s_int min, s_int sec, s_int nsec)
{
	iNanoSeconds = nsec;
	iSeconds = sec;
	iMinutes = min;
	iHours = hour;
	iDay = day;
	iMonth = mon;
	iYear = year;
	iIsNull = false;
}

void CSTime::getUTC(s_int& year, s_int& mon, s_int& day, s_int& hour, s_int& min, s_int& sec, s_int& nsec)
{
	nsec = iNanoSeconds;
	sec = iSeconds;
	min = iMinutes;
	hour = iHours;
	day = iDay;
	mon = iMonth;
	year = iYear;
}

char *CSTime::getCString(const char *format)
{
	if (iIsNull)
		strcpy(iCString, "NULL");
	else {
		struct tm	ltime;
		time_t		secs;
		s_int		nsec;

		memset(&ltime, 0, sizeof(ltime));
	
		getUTC1970(secs, nsec);
		localtime_r(&secs, &ltime);
		strftime(iCString, 100, format, &ltime);
	}
	return iCString;
}

char *CSTime::getCString()
{
	return getCString("%Y-%m-%d %H:%M:%S");
}

void CSTime::setUTC1970(time_t sec, s_int nsec)
{
	struct tm	ltime;

	memset(&ltime, 0, sizeof(ltime));

	gmtime_r(&sec, &ltime);
	setUTC(ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec, nsec);
}

bool CSTime::olderThen(time_t max_age)
{
	time_t secs, now;
	s_int nsec;
	
	getUTC1970(secs, nsec);
	
	now = time(NULL);
	
	return ((now - secs) > max_age);
}

void CSTime::getUTC1970(time_t& sec, s_int& nsec)
{
#ifdef OS_WINDOWS
	uint64_t nsec100;

	/* Get the number of seconds since 1 Jan 1970 */
	nsec100 = getUTC1601();
	nsec100 = nsec100 - get1970as1601();
	sec = (time_t) (nsec100 / 10000000);
#else
	struct tm	ltime;

	memset(&ltime, 0, sizeof(ltime));

	ltime.tm_sec = iSeconds;
	ltime.tm_min = iMinutes;
	ltime.tm_hour = iHours;
	ltime.tm_mday = iDay;
	ltime.tm_mon = iMonth - 1;
	ltime.tm_year = iYear - 1900;
	sec = my_timegm(&ltime);
#endif
	nsec = iNanoSeconds;
}

#ifdef OS_WINDOWS

void CSTime::setUTC1601(uint64_t nsec100)
{
	FILETIME		ftime;
	SYSTEMTIME		stime;

	/* The input is actually a FILETIME value: */
	memcpy(&ftime, &nsec100, sizeof(ftime));

	if (!FileTimeToSystemTime(&ftime, &stime))
		CSException::throwLastError(CS_CONTEXT);
	setUTC((s_int) stime.wYear, (s_int) stime.wMonth, (s_int) stime.wDay,
		(s_int) stime.wHour, (s_int) stime.wMinute, (s_int) stime.wSecond, (s_int) stime.wMilliseconds * 1000);
}

uint64_t CSTime::getUTC1601()
{
	FILETIME	ftime;
	SYSTEMTIME	stime;
	uint64_t		nsec100;

	stime.wYear = iYear;
	stime.wMonth = iMonth;
	stime.wDay = iDay;
	stime.wHour = iHours;
	stime.wMinute = iMinutes;
	stime.wSecond = iSeconds;
	stime.wMilliseconds = iNanoSeconds / 1000;
	if (!SystemTimeToFileTime(&stime, &ftime))
		CSException::throwLastError(CS_CONTEXT);
	memcpy(&nsec100, &ftime, sizeof(nsec100));
	return nsec100;
}

uint64_t CSTime::get1970as1601()
{
	FILETIME	ftime;
	SYSTEMTIME	stime;
	uint64_t		nsec100;

	stime.wYear = 1970;
	stime.wMonth = 1;
	stime.wDay = 1;
	stime.wHour = 0;
	stime.wMinute = 0;
	stime.wSecond = 0;
	stime.wMilliseconds = 0;
	if (!SystemTimeToFileTime(&stime, &ftime))
		CSException::throwLastError(CS_CONTEXT);
	memcpy(&nsec100, &ftime, sizeof(nsec100));
	return nsec100;
}

#endif

uint64_t CSTime::getTimeCurrentTicks()
{
	struct timeval	now;

	/* Get the current time in microseconds: */
	gettimeofday(&now, NULL);
	return (uint64_t) now.tv_sec * (uint64_t) 1000000 + (uint64_t) now.tv_usec;
}



