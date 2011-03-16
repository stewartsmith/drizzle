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
 * Original author: Paul McCullagh
 * Continued development: Barry Leslie
 *
 * 2007-05-25
 *
 * H&G2JCtL
 *
 * MySQL interface.
 *
 */


#ifdef DRIZZLED
#include <config.h>
#include <drizzled/common.h>
#include <drizzled/data_home.h>
#include <drizzled/current_session.h>
#include <drizzled/plugin.h>
#include <drizzled/session.h>
#else
#include "cslib/CSConfig.h"
#endif

#include "cslib/CSGlobal.h"
#include "cslib/CSException.h"
#include "defs_ms.h"
#include "mysql_ms.h"

/* Note: 'new' used here is NOT CSObject::new which is a DEBUG define*/
#ifdef new
#undef new
#endif

void *ms_my_get_thread()
{
	THD *thd = current_thd;

	return (void *) thd;
}

#ifdef DRIZZLED
const char *ms_my_get_mysql_home_path()
{
	return drizzled::getDataHomeCatalog().file_string().c_str();
}

bool ms_is_autocommit()
{
	return (session_test_options(current_thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) == 0;
}

#else
const char *ms_my_get_mysql_home_path()
{
	return mysql_real_data_home;
}

bool ms_is_autocommit()
{
	return (thd_test_options(current_thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) == 0;
}
#endif

/* YYYYMMDDHHMMSS */
uint64_t	ms_my_1970_to_mysql_time(time_t t)
{
	struct tm	details;
	uint64_t		sec;
	uint64_t		min;
	uint64_t		hour;
	uint64_t		day;
	uint64_t		mon;
	uint64_t		year;

	gmtime_r(&t, &details);
	sec = (uint64_t) details.tm_sec;
	min = (uint64_t) details.tm_min * 100LL;
	hour = (uint64_t) details.tm_hour * 10000LL;
	day = (uint64_t) details.tm_mday * 1000000LL;
	mon = (uint64_t) (details.tm_mon+1) * 100000000LL;
	year = (uint64_t) (details.tm_year+1900) * 10000000000LL;
	
	return year + mon + day + hour + min + sec;
}


