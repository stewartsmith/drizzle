/* Copyright (c) 2008 PrimeBase Technologies GmbH, Germany
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
#include <time.h>

#ifdef OS_WINDOWS
#include <Windows.h>
#endif

#include "CSTime.h"
#include "CSGlobal.h"
#include "CSStrUtil.h"

#ifdef OS_SOLARIS
/* This is an implimentation of timegm() for solaris
 * which originated here: http://www.opensync.org/changeset/1769
 */
time_t my_timegm(struct tm *t)
{
	time_t tl, tb;
	struct tm *tg;

	tl = mktime(t);
	if (tl == -1) {
		t->tm_hour--;
		tl = mktime (t);
		if (tl == -1)
			return -1; 
		tl += 3600;
	}
	tg = gmtime(&tl);
	tg->tm_isdst = 0;
	tb = mktime (tg);
	if (tb == -1) {
		tg->tm_hour--;
		tb = mktime (tg);
		if (tb == -1)
			return -1; 
			tb += 3600;
	}
	return (tl - (tb - tl));
}
#else
#define my_timegm(x) timegm(x)
#endif

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


