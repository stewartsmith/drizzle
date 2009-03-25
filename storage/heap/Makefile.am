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

LDADD =			libheap.la \
			$(top_builddir)/mysys/libmysys.la \
			$(top_builddir)/strings/libmystrings.la

noinst_LTLIBRARIES  =	libheap.la
noinst_HEADERS =	heapdef.h ha_heap.h heap.h
libheap_la_SOURCES= \
			ha_heap.cc \
			hp_block.cc \
			hp_clear.cc \
			hp_close.cc \
			hp_create.cc \
			hp_delete.cc \
			hp_dspace.cc \
			hp_extra.cc \
			hp_hash.cc \
			hp_info.cc \
			hp_open.cc \
			hp_panic.cc \
			hp_record.cc \
			hp_rectest.cc \
			hp_rename.cc \
			hp_rfirst.cc \
			hp_rkey.cc \
			hp_rlast.cc \
			hp_rnext.cc \
			hp_rprev.cc \
			hp_rrnd.cc \
			hp_rsame.cc \
			hp_scan.cc \
			hp_static.cc \
			hp_update.cc \
			hp_write.cc


EXTRA_DIST =		plug.in

