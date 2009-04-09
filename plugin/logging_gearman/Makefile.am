# Copyright (C) 2009 Sun Microsystems
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

if BUILD_LOGGING_GEARMAN

EXTRA_LTLIBRARIES = liblogging_gearman.la
pkgplugin_LTLIBRARIES =	@plugin_logging_gearman_shared_target@
liblogging_gearman_la_LDFLAGS = -module -avoid-version -rpath $(pkgplugindir)
liblogging_gearman_la_LIBADD= ${LIBGEARMAN}
liblogging_gearman_la_CPPFLAGS = ${AM_CPPFLAGS} -DDRIZZLE_DYNAMIC_PLUGIN
liblogging_gearman_la_SOURCES = logging_gearman.cc

endif
