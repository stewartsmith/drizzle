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

LDADD=			libmyisamutil.la \
			libmyisamnone.la \
			$(top_builddir)/mysys/libmysys.la \
			$(top_builddir)/mystrings/libmystrings.la \
			$(LIBZ) \
			$(LIBINTL)

EXTRA_DIST =		plug.in

noinst_LTLIBRARIES =	libmyisam.la libmyisamutil.la \
			libmyisamha.la libmyisamnone.la
bin_PROGRAMS =		myisamchk

noinst_HEADERS =	myisamdef.h \
			ha_myisam.h \
			keycache.h \
			my_handler.h \
			myisam.h \
			myisampack.h

libmyisamutil_la_SOURCES= \
			mf_keycache.cc \
			mf_keycaches.cc \
			mi_cache.cc \
			mi_changed.cc \
			mi_check.cc \
			mi_checksum.cc \
			mi_close.cc \
			mi_create.cc\
			mi_delete.cc \
			mi_delete_all.cc \
			mi_delete_table.cc \
			mi_dynrec.cc \
			mi_extra.cc \
			mi_info.cc \
			mi_key.cc \
			mi_keycache.cc \
			mi_locking.cc \
			mi_open.cc \
			mi_page.cc \
			mi_panic.cc \
			mi_preload.cc \
			mi_range.cc \
			mi_rename.cc \
			mi_rfirst.cc \
			mi_rkey.cc \
			mi_rlast.cc \
			mi_rnext.cc \
			mi_rnext_same.cc \
			mi_rprev.cc \
			mi_rrnd.cc \
			mi_rsame.cc \
			mi_rsamepos.cc \
			mi_scan.cc \
			mi_search.cc \
			mi_static.cc \
			mi_statrec.cc \
			mi_unique.cc \
			mi_update.cc \
			mi_write.cc \
			my_handler.cc \
			my_handler_errors.cc \
			my_pread.cc \
			sort.cc 

libmyisamha_la_SOURCES = ha_myisam.cc
libmyisamnone_la_SOURCES = mi_reportnone.cc
libmyisam_la_SOURCES = 
libmyisam_la_LIBADD = libmyisamha.la libmyisamutil.la
myisamchk_SOURCES = myisamchk.cc 
