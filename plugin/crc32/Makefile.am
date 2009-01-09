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

AM_CPPFLAGS = 		${GLOBAL_CPPFLAGS} -I${top_srcdir}/drizzled
EXTRA_LTLIBRARIES =	libcrc32udf.la
pkgplugin_LTLIBRARIES =	@plugin_crc32_shared_target@
libcrc32udf_la_LDFLAGS =	-module -avoid-version -rpath $(pkgplugindir)
libcrc32udf_la_LIBADD =		$(LTLIBZ)
libcrc32udf_la_CXXFLAGS=	$(AM_CXXFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
libcrc32udf_la_SOURCES =	crc32udf.cc


EXTRA_LIBRARIES =	libcrc32udf.a
noinst_LIBRARIES =	@plugin_crc32_static_target@
libcrc32udf_a_SOURCES=	$(libcrc32udf_la_SOURCES)
