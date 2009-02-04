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




libdrizzleclientincludedir = ${includedir}/libdrizzleclient
dist_libdrizzleclientinclude_HEADERS =	\
					get_password.h \
					drizzle.h \
					drizzle_data.h \
					drizzle_field.h \
					drizzle_methods.h \
					drizzle_options.h \
					drizzle_parameters.h \
					drizzle_res.h \
					drizzle_rows.h \
					libdrizzle.h \
					errmsg.h \
					net_serv.h \
					pack.h \
					password.h \
					vio.h

lib_LTLIBRARIES = libdrizzleclient.la
libdrizzleclient_la_SOURCES = client.c \
			drizzle.c \
			drizzle_data.c \
			errmsg.c \
			get_password.c \
			libdrizzle.c \
			libdrizzle_priv.c \
			local_infile.c \
			net_serv.c \
			pack.c \
			password.c \
			vio.c \
			viosocket.c

libdrizzleclient_la_LDFLAGS = -version-info @SHARED_LIB_VERSION@ @LD_VERSION_SCRIPT@
libdrizzleclient_la_LIBADD = $(LTLIBZ)

noinst_HEADERS =	libdrizzle_priv.h \
			local_infile.h

EXTRA_DIST =		libdrizzleclient.ver
CLEANFILES =		$(target_libadd) $(SHLIBOBJS) \
			$(target) $(BUILT_SOURCES)

