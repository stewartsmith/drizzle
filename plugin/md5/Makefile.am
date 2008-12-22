# Copyright (C) 2006 MySQL AB
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

AM_CPPFLAGS = ${GLOBAL_CPPFLAGS} $(SSL_CFLAGS)

if BUILD_MD5_UDF
EXTRA_LTLIBRARIES=	libmd5udf.la
pkgplugin_LTLIBRARIES=	$(plugin_md5_static_target)
libmd5udf_la_LDFLAGS=	-module -avoid-version -rpath $(pkgplugindir)
libmd5udf_la_LIBADD=	$(CRYPTO_LIBS)
libmd5udf_la_SOURCES=	md5udf.cc
endif
