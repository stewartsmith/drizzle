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

mysysincludedir= ${includedir}/mysys
dist_mysysinclude_HEADERS= \
		definitions.h \
		iocache.h \
		my_alloc.h \
		my_getopt.h \
		my_list.h \
		my_pthread.h \
		my_sys.h \
		typelib.h

noinst_HEADERS=	\
		aio_result.h \
		base64.h \
		drizzle_time.h \
		hash.h \
		my_bit.h \
		my_bitmap.h \
		my_dir.h \
		my_static.h \
		my_time.h \
		my_tree.h \
		mysys_err.h \
		mysys_priv.h \
		queues.h \
		sha1.h \
		thr_alarm.h \
		thr_lock.h

if BUILD_FAST_MUTEX
thr_mutex_source= thr_mutex.cc
endif

libmysys_la_SOURCES= \
		array.cc \
		base64.cc \
		charset-def.cc \
		charset.cc \
		checksum.cc \
		default.cc \
		default_modify.cc \
		errors.cc \
		hash.cc \
		list.cc \
		mf_arr_appstr.cc \
		mf_cache.cc \
		mf_dirname.cc \
		mf_fn_ext.cc \
		mf_format.cc \
		mf_getdate.cc \
		mf_iocache.cc \
		mf_iocache2.cc \
		mf_loadpath.cc \
		mf_pack.cc \
		mf_path.cc \
		mf_qsort.cc \
		mf_qsort2.cc \
		mf_radix.cc \
		mf_same.cc \
		mf_sort.cc \
		mf_tempfile.cc \
		mf_unixpath.cc \
		mf_wcomp.cc \
		mulalloc.cc \
		my_access.cc \
		my_alloc.cc \
		my_bit.cc \
		my_bitmap.cc \
		my_copy.cc \
		my_create.cc \
		my_delete.cc \
		my_dup.cc \
		my_error.cc \
		my_file.cc \
		my_filename.cc \
		my_fopen.cc \
		my_getopt.cc \
		my_getsystime.cc \
		my_init.cc \
		my_lib.cc \
		my_messnc.cc \
		my_mkdir.cc \
		my_open.cc \
		my_read.cc \
		my_redel.cc \
		my_rename.cc \
		my_sleep.cc \
		my_static.cc \
		my_symlink.cc \
		my_symlink2.cc \
		my_sync.cc \
		my_thr_init.cc \
		my_time.cc \
		my_write.cc \
		ptr_cmp.cc \
		queues.cc \
		sha1.cc \
		thr_alarm.cc \
		thr_lock.cc \
		tree.cc \
		typelib.cc \
		${thr_mutex_source}

DEFS=		-DDEFAULT_BASEDIR=\"$(prefix)\" \
		-DDATADIR="\"$(MYSQLDATAdir)\"" \
		-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
		-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
		-DDEFAULT_HOME_ENV=DRIZZLE_HOME \
		-DDEFAULT_GROUP_SUFFIX_ENV=DRIZZLE_GROUP_SUFFIX \
		-DDEFAULT_SYSCONFDIR="\"$(sysconfdir)\"" \
		@DEFS@

