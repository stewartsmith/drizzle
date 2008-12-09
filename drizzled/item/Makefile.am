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

noinst_LTLIBRARIES= 	libitem.la

libitem_la_SOURCES= \
		bin_string.cc \
		cache_row.cc \
		cmpfunc.cc \
		create.cc \
		decimal.cc \
		empty_string.cc \
		field.cc \
		hex_string.cc \
		ifloat.cc \
		istring.cc \
		ident.cc \
		int.cc \
		null.cc \
		num.cc \
		param.cc \
		ref.cc \
		row.cc \
		subselect.cc \
		sum.cc \
		type_holder.cc \
		uint.cc

itemincludedir = ${includedir}/drizzled/item
nobase_dist_iteminclude_HEADERS = \
		basic_constant.h \
		bin_string.h \
		blob.h \
		cache_row.h \
		cmpfunc.h \
		create.h \
		decimal.h \
		empty_string.h \
		field.h \
		hex_string.h \
		ifloat.h \
		istring.h \
		func.h \
		ident.h \
		int.h \
		null.h \
		num.h \
		param.h \
		ref.h \
		return_date_time.h \
		return_int.h \
		row.h \
		strfunc.h \
		subselect.h \
		sum.h \
		timefunc.h \
		type_holder.h \
		uint.h


DEFS= -DDRIZZLE_SERVER @DEFS@
