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

EXTRA_LTLIBRARIES=		libcompressionudf.la
noinst_HEADERS=			compress.h uncompress.h uncompressed_length.h
pkgplugin_LTLIBRARIES=		@plugin_compression_shared_target@
libcompressionudf_la_LDFLAGS=	-module -avoid-version -rpath $(pkgplugindir)\
				${LTLIBZ}
libcompressionudf_la_CPPFLAGS=	$(AM_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
libcompressionudf_la_SOURCES=	compress.cc \
				compressionudf.cc \
				uncompress.cc \
				uncompressed_length.cc


EXTRA_LIBRARIES=		libcompressionudf.a
noinst_LIBRARIES=		@plugin_compression_static_target@
libcompressionudf_a_SOURCES=	$(libcompressionudf_la_SOURCES)
