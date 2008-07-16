# Copyright (C) 2000-2002, 2005-2006 MySQL AB
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

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
AM_CPPFLAGS =           -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/server \
                        -I$(srcdir)
WRAPLIBS=

LDADD =			libheap.la \
			$(top_builddir)/mysys/libmysyslt.la \
			$(top_builddir)/strings/libmystringslt.la

DEFS =                  @DEFS@
noinst_LTLIBRARIES  =	libheap.la
noinst_PROGRAMS	=	hp_test1 hp_test2
noinst_HEADERS =	heapdef.h ha_heap.h
libheap_la_SOURCES =	hp_open.c hp_extra.c hp_close.c hp_panic.c hp_info.c \
			hp_rrnd.c hp_scan.c hp_update.c hp_write.c hp_delete.c \
			hp_rsame.c hp_create.c hp_rename.c hp_rfirst.c \
			hp_rnext.c hp_rlast.c hp_rprev.c hp_clear.c \
			hp_rkey.c hp_block.c \
			ha_heap.cc \
			hp_hash.c _check.c _rectest.c hp_static.c


EXTRA_DIST =		plug.in

