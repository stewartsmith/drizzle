#  Copyright (C) 2008 Sun Microsystems
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

noinst_LTLIBRARIES = 	libtimefunc.la

AM_CPPFLAGS = ${GLOBAL_CPPFLAGS} ${PROTOBUF_CFLAGS}

libtimefunc_la_SOURCES = \
		add_time.cc \
		curdate.cc \
		curtime.cc \
		date.cc \
		date_add_interval.cc \
		date_format.cc \
		dayname.cc \
		dayofmonth.cc \
		extract.cc \
		dayofyear.cc \
		from_days.cc \
		from_unixtime.cc \
		get_format.cc \
		get_interval_value.cc \
		hour.cc \
		last_day.cc \
		makedate.cc \
		make_datetime_with_warn.cc \
		make_datetime.cc \
		maketime.cc \
		make_time_with_warn.cc \
		microsecond.cc \
		minute.cc \
		month.cc \
		now.cc \
		quarter.cc \
		period_add.cc \
		period_diff.cc \
		sec_to_time.cc \
		second.cc \
		str_to_date.cc \
		sysdate_local.cc \
		timediff.cc \
		timestamp_diff.cc \
		time_to_sec.cc \
		to_days.cc \
		typecast.cc \
		unix_timestamp.cc \
		week.cc \
		week_mode.cc \
		weekday.cc \
		year.cc \
		yearweek.cc

functionsincludedir = ${includedir}/drizzled/functions/time
nobase_dist_functionsinclude_HEADERS = \
		add_time.h \
		curdate.h \
		curtime.h \
		date.h \
		date_add_interval.h \
		date_format.h \
		dayname.h \
		dayofmonth.h \
		dayofyear.h \
		extract.h \
		from_days.h \
		from_unixtime.h \
		get_format.h \
		get_interval_value.h \
		hour.h \
		last_day.h \
		makedate.h \
		make_datetime_with_warn.h \
		make_datetime.h \
		maketime.h \
		make_time_with_warn.h \
		microsecond.h \
		minute.h \
		month.h \
		now.h \
		quarter.h \
		period_add.h \
		period_diff.h \
		sec_to_time.h \
		second.h \
		str_to_date.h \
		sysdate_local.h \
		str_timefunc.h \
		timediff.h \
		timestamp_diff.h \
		time_to_sec.h \
		to_days.h \
		typecast.h \
		unix_timestamp.h \
		week.h \
		week_mode.h \
		weekday.h \
		year.h  \
		yearweek.h

DEFS= -DDRIZZLE_SERVER @DEFS@
