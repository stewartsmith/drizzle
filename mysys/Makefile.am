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

MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)

noinst_LTLIBRARIES =	libmysys.la

mysysincludedir = ${includedir}/mysys
dist_mysysinclude_HEADERS = 	my_alloc.h \
				my_getopt.h \
				my_list.h \
				my_sys.h \
				my_pthread.h \
				typelib.h

noinst_HEADERS =	mysys_priv.h \
			my_bit.h \
			my_bitmap.h \
			my_dir.h \
			my_static.h \
			my_handler.h \
			my_handler_errors.h \
			my_nosys.h \
			mysys_err.h \
			my_tree.h \
			queues.h \
			hash.h \
			sha1.h \
			thr_alarm.h \
			thr_lock.h \
			base64.h

if BUILD_FAST_MUTEX
thr_mutex_source=thr_mutex.c
endif
if BUILD_THR_RWLOCK
thr_rwlock_source=thr_rwlock.c
endif

libmysys_la_SOURCES = my_init.c my_getwd.c mf_getdate.c \
			mf_path.c mf_loadpath.c my_file.c \
			my_open.c my_create.c my_dup.c my_seek.c my_read.c \
			my_write.c \
			mf_iocache.c mf_iocache2.c mf_cache.c mf_tempfile.c \
			mf_tempdir.c my_handler.c \
			my_malloc.c my_realloc.c my_once.c mulalloc.c \
			my_alloc.c \
			my_fopen.c my_fstream.c my_getsystime.c \
			my_error.c errors.c my_filename.c my_messnc.c \
			mf_format.c mf_same.c mf_dirname.c mf_fn_ext.c \
			my_symlink.c my_symlink2.c \
			mf_pack.c mf_unixpath.c mf_arr_appstr.c \
			mf_wcomp.c my_gethwaddr.c \
			mf_qsort.c mf_qsort2.c mf_sort.c \
			ptr_cmp.c mf_radix.c queues.c \
			tree.c list.c hash.c array.c typelib.c \
			my_copy.c my_lib.c \
			my_delete.c my_rename.c my_redel.c \
			my_quick.c my_static.c \
			my_sync.c my_getopt.c my_mkdir.c \
			default_modify.c default.c \
                        my_compress.c checksum.c \
			my_sleep.c \
			charset.c charset-def.c my_bitmap.c my_bit.c \
			sha1.c \
			my_access.c base64.c \
			thr_alarm.c thr_lock.c my_thr_init.c \
                        ${thr_mutex_source} ${thr_rwlock_source}

DEFS =			-DDEFAULT_BASEDIR=\"$(prefix)\" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DDEFAULT_HOME_ENV=DRIZZLE_HOME \
			-DDEFAULT_GROUP_SUFFIX_ENV=DRIZZLE_GROUP_SUFFIX \
			-DDEFAULT_SYSCONFDIR="\"$(sysconfdir)\"" \
                        @DEFS@

