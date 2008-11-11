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

AM_CPPFLAGS = ${GLOBAL_CPPFLAGS} ${PROTOBUF_CPPFLAGS}

libtimefunc_la_SOURCES = \
		  curtime.cc \
		  dayname.cc \
		  dayofmonth.cc \
		  dayofyear.cc \
		  hour.cc \
		  minute.cc \
		  month.cc \
		  quarter.cc \
		  period_add.cc \
		  period_diff.cc \
		  second.cc \
		  time_to_sec.cc \
		  to_days.cc \
		  unix_timestamp.cc \
		  week.cc \
		  week_mode.cc \
		  weekday.cc \
		  year.cc \
		  yearweek.cc

noinst_HEADERS = \
		 curtime.h \
		 dayname.h \
		 dayofmonth.h \
		 dayofyear.h \
		 hour.h \
		 minute.h \
		 month.h \
		 quarter.h \
		 period_add.h \
		 period_diff.h \
		 second.h \
		 str_timefunc.h \
		 time_to_sec.h \
		 to_days.h \
		 unix_timestamp.h \
		 week.h \
		 week_mode.h \
		 weekday.h \
		 year.h  \
		 yearweek.h

DEFS= -DDRIZZLE_SERVER @DEFS@
