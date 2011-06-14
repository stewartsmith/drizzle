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
 * Represents a time value from the database.
 *
 * NOTE: All times in the database are based on UTC
 * (Universal Coordinated Time)!
 *
 */

#pragma once
#ifndef __DBTIME_H__
#define __DBTIME_H__

#include <time.h>

#include "CSDefs.h"
#include "CSObject.h"

class CSTime : public CSObject  {
public:
	CSTime(): iIsNull(true) { }
	CSTime(s_int year, s_int mon, s_int day, s_int hour, s_int min, s_int sec, s_int nsec);
	virtual ~CSTime() { }

	bool isNull();
	
	void setNull();

	/*
	 * Set the time. The value given is a local time
     * sec - seconds (0 - 60)
     * min - minutes (0 - 59)
     * hour - hours (0 - 23)
     * day - day of month (1 - 31)
	 * mon - month of year (1 - 12)
	 * year - where year >= 1970 (on UNIX)
	 */
	void setLocal(s_int year, s_int mon, s_int day, s_int hour, s_int min, s_int sec, s_int nsec);

	/* Get the local time. */
	void getLocal(s_int& year, s_int& mon, s_int& day, s_int& hour, s_int& min, s_int& sec, s_int& nsec);

	/* Set the s_int time. */
	void setUTC(s_int year, s_int mon, s_int day, s_int hour, s_int min, s_int sec, s_int nsec);

	/* Get the universal time. */
	void getUTC(s_int& year, s_int& mon, s_int& day, s_int& hour, s_int& min, s_int& sec, s_int& nsec);

	/*
	 * Returns the time as a string in the local time 
	 * (time zone adjusted).
	 */
	char *getCString();

	/*
	 * As above, but using the given format.
	 */
	char *getCString(const char *format);

	/* Set the time given a value in seconds and nanoseconds in UTC since 1970.
	 * Used by UNIX.
	 */
	void setUTC1970(time_t sec, s_int nsec);
	void getUTC1970(time_t& sec, s_int& nsec);

	/* Set the time given a 100 nanosecond value in UTC since 1601.
	 * Used by Windows.
	 */
	void setUTC1601(uint64_t nsec100);
	uint64_t getUTC1601();

	/* 
	 * Tests if the time is more than 'max_age' seconds in the past.
	 */
	bool olderThen(time_t max_age);

	static	uint64_t getTimeCurrentTicks();
private:
	bool	iIsNull;
	char	iCString[100];

	/* The time based on UTC (GMT): */
	s_int	iYear;
	s_int	iMonth;
	s_int	iDay;
	s_int	iHours;
	s_int	iMinutes;
	s_int	iSeconds;
	s_int	iNanoSeconds;		/* Plus this number of nano seconds. */

	uint64_t get1970as1601();
};

#endif
