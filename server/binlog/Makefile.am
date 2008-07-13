# Copyright (C) 2000-2006 MySQL AB
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

# This file is public domain and comes with NO WARRANTY of any kind

AM_CPPFLAGS =			-I$(top_builddir)/include \
				-I$(top_srcdir)/include \
				-I$(top_srcdir)/server

LDADD=				$(top_builddir)/libdrizzle/libdrizzle.la -lz


bin_PROGRAMS =			drizzlebinlog 

drizzlebinlog_SOURCES =		drizzlebinlog.cc \
				$(top_srcdir)/mysys/mf_tempdir.c \
				$(top_srcdir)/mysys/my_new.cc \
				$(top_srcdir)/mysys/my_bit.c \
				$(top_srcdir)/mysys/my_bitmap.c \
				$(top_srcdir)/mysys/base64.c 
