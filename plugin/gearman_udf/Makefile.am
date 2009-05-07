# Copyright (C) 2009 Eric Day
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

EXTRA_LTLIBRARIES = libgearman_udf.la
pkgplugin_LTLIBRARIES =	@plugin_gearman_shared_target@
libgearman_udf_la_LDFLAGS = -module -avoid-version -rpath $(pkgplugindir)
libgearman_udf_la_LIBADD = $(LIBGEARMAN)
libgearman_udf_la_CPPFLAGS= $(AM_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
libgearman_udf_la_SOURCES = gearman_udf.cc function_map.cc function_map.h gman_servers_set.cc gman_sservers_set.h gman_do.cc gman_do.h

EXTRA_LIBRARIES = libgearman_udf.a
noinst_LIBRARIES = @plugin_gearman_static_target@
libgearman_udf_a_SOURCES= $(libgearman_udf_la_SOURCES)
