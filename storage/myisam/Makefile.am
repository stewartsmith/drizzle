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
			$(LTLIBZ) \
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

libmyisamutil_la_SOURCES = mi_open.c mi_extra.c mi_info.c mi_rkey.c \
			mi_rnext.c mi_rnext_same.c \
			mi_search.c mi_page.c mi_key.c mi_locking.c \
			mi_rrnd.c mi_scan.c mi_cache.c \
			mi_statrec.c mi_dynrec.c \
			mi_update.c mi_write.c mi_unique.c \
			mi_delete.c \
			my_pread.c \
			mf_keycache.c mf_keycaches.c \
			mi_rprev.c mi_rfirst.c mi_rlast.c mi_rsame.c \
			mi_rsamepos.c mi_panic.c mi_close.c mi_create.c\
			mi_range.c mi_checksum.c \
			mi_changed.c mi_static.c mi_delete_all.c \
			mi_delete_table.c mi_rename.c  mi_check.c \
			mi_keycache.c mi_preload.c \
			my_handler.c \
			my_handler_errors.c \
			sort.c 
libmyisamha_la_SOURCES = ha_myisam.cc
libmyisamnone_la_SOURCES = mi_reportnone.c
libmyisam_la_SOURCES = 
libmyisam_la_LIBADD = libmyisamha.la libmyisamutil.la
myisamchk_SOURCES = myisamchk.cc 
