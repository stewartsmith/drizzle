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

#Makefile.am example for a Hello World
MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =		$(pkglibdir)/plugin

EXTRA_LTLIBRARIES =	libhello_world.la
pkgplugin_LTLIBRARIES =	@plugin_hello_world_shared_target@
libhello_world_la_LDFLAGS =	-module -rpath $(pkgplugindir)
libhello_world_la_CPPFLAGS=	$(AM_CPPFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
libhello_world_la_SOURCES =	hello_world.cc


EXTRA_LIBRARIES =	libhello_world.a
noinst_LIBRARIES =	@plugin_hello_world_static_target@
libhello_world_a_CPPFLAGS =	$(AM_CPPFLAGS)
libhello_world_a_SOURCES=	hello_world.cc
