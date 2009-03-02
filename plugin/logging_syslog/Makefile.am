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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


noinst_HEADERS= names.h
EXTRA_LTLIBRARIES = liblogging_syslog.la
pkgplugin_LTLIBRARIES =	@plugin_logging_syslog_shared_target@
liblogging_syslog_la_LDFLAGS = -module -avoid-version -rpath $(pkgplugindir)
liblogging_syslog_la_CPPFLAGS = $(AM_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
liblogging_syslog_la_SOURCES = logging_syslog.cc

EXTRA_LIBRARIES = liblogging_syslog.a
noinst_LIBRARIES = @plugin_logging_syslog_static_target@
liblogging_syslog_a_SOURCES = $(liblogging_syslog_la_SOURCES)
