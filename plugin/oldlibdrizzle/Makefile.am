# Copyright (C) 2008 Sun Microsystems, Inc.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 GNU General Public License as
# published by the Free Software Foundation.
#
# There are special exceptions to the terms and conditions of the GPL as it
# is applied to this software. View the full text of the exception in file
# EXCEPTIONS-CLIENT in the directory of this software distribution.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


noinst_HEADERS =\
		get_password.h \
		drizzle.h \
		drizzle_data.h \
		drizzle_field.h \
		drizzle_methods.h \
		drizzle_parameters.h \
		drizzle_res.h \
		drizzle_rows.h \
		libdrizzle.h \
		libdrizzle_priv.h \
		oldlibdrizzle.h \
		errmsg.h \
		net_serv.h \
		options.h \
		pack.h \
		password.h \
		vio.h

noinst_LTLIBRARIES= liboldlibdrizzle.la
liboldlibdrizzle_la_SOURCES= \
		client.cc \
		drizzle.cc \
		drizzle_data.cc \
		errmsg.cc \
		get_password.cc \
		libdrizzle.cc \
		oldlibdrizzle.cc \
		libdrizzle_priv.cc \
		net_serv.cc \
		pack.cc \
		password.cc \
		vio.cc \
		viosocket.cc

liboldlibdrizzle_la_LIBADD = $(LIBZ)

