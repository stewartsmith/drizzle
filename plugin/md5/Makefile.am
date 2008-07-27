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

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =		$(pkglibdir)/plugin

AM_CPPFLAGS = ${GLOBAL_CPPFLAGS} $(openssl_includes) -I${top_srcdir}/drizzled

EXTRA_LTLIBRARIES =	libmd5udf.la
pkgplugin_LTLIBRARIES =	@plugin_md5_shared_target@
libmd5udf_la_LDFLAGS =	-module -rpath $(pkgplugindir) -lssl
libmd5udf_la_CXXFLAGS=	-DMYSQL_DYNAMIC_PLUGIN
libmd5udf_la_SOURCES =	md5udf.cc


EXTRA_LIBRARIES =	libmd5udf.a
noinst_LIBRARIES =	@plugin_md5_static_target@
libmd5udf_a_SOURCES=	$(libmd5udf_la_SOURCES)
