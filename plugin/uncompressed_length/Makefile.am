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

AM_CPPFLAGS = 		${GLOBAL_CPPFLAGS} -I${top_srcdir}/drizzled
EXTRA_LTLIBRARIES =	libuncompressed_lengthudf.la
pkgplugin_LTLIBRARIES =	@plugin_uncompressed_length_shared_target@
libuncompressed_lengthudf_la_LDFLAGS =	-module -avoid-version -rpath $(pkgplugindir)
libuncompressed_lengthudf_la_CXXFLAGS=	$(AM_CXXFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
libuncompressed_lengthudf_la_SOURCES =	uncompressed_length.cc


EXTRA_LIBRARIES =	libuncompressed_lengthudf.a
noinst_LIBRARIES =	@plugin_uncompressed_length_static_target@
libuncompressed_lengthudf_a_SOURCES=	$(libuncompressed_lengthudf_la_SOURCES)
