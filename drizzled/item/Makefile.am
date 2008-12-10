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
		cache.cc \
		cache_decimal.cc \
		cache_int.cc \
		cache_real.cc \
		cache_row.cc \
		cache_str.cc \
		cmpfunc.cc \
		copy_string.cc \
		create.cc \
		decimal.cc \
		direct_ref.cc \
		empty_string.cc \
		field.cc \
		hex_string.cc \
		ifloat.cc \
		insert_value.cc \
		int_with_ref.cc \
		istring.cc \
		ident.cc \
		int.cc \
		null.cc \
		num.cc \
		outer_ref.cc \
		param.cc \
		ref.cc \
		ref_null_helper.cc \
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
		cache.h \
		cache_decimal.h \
		cache_int.h \
		cache_real.h \
		cache_row.h \
		cache_str.h \
		cmpfunc.h \
		copy_string.h \
		create.h \
		decimal.h \
		direct_ref.h \
		empty_string.h \
		field.h \
		hex_string.h \
		ifloat.h \
		insert_value.h \
		int_with_ref.h \
		istring.h \
		func.h \
		ident.h \
		int.h \
		null.h \
		num.h \
		outer_ref.h \
		param.h \
		ref.h \
		ref_null_helper.h \
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
