# Copyright (C) 2000-2003, 2005, 2006 MySQL AB
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

AM_CPPFLAGS =		-I$(top_builddir)/include -I$(top_srcdir)/include
pkglib_LTLIBRARIES =	libvio.la
libvio_la_SOURCES = 
libvio_la_LIBADD =      libviolt.la

noinst_LTLIBRARIES =    libviolt.la

noinst_HEADERS =	vio_priv.h

libviolt_la_SOURCES =	vio.c viosocket.c
