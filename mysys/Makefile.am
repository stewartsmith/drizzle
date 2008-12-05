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
dist_mysysinclude_HEADERS = 	iocache.h \
                                my_alloc.h \
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
			my_nosys.h \
			mysys_err.h \
			my_tree.h \
			queues.h \
			hash.h \
			sha1.h \
			thr_alarm.h \
			my_time.h \
			drizzle_time.h \
			thr_lock.h \
			base64.h \
			aio_result.h

if BUILD_FAST_MUTEX
thr_mutex_source=thr_mutex.cc
endif

libmysys_la_SOURCES = my_init.cc mf_getdate.cc \
			mf_path.cc mf_loadpath.cc my_file.cc \
			my_open.cc my_create.cc my_dup.cc my_seek.cc my_read.cc \
			my_write.cc \
			mf_iocache.cc mf_iocache2.cc mf_cache.cc mf_tempfile.cc \
			mf_tempdir.cc \
			my_malloc.cc my_realloc.cc my_once.cc mulalloc.cc \
			my_alloc.cc \
			my_fopen.cc my_fstream.cc my_getsystime.cc \
			my_error.cc errors.cc my_filename.cc my_messnc.cc \
			mf_format.cc mf_same.cc mf_dirname.cc mf_fn_ext.cc \
			my_symlink.cc my_symlink2.cc \
			mf_pack.cc mf_unixpath.cc mf_arr_appstr.cc \
			mf_wcomp.cc \
			mf_qsort.cc mf_qsort2.cc mf_sort.cc \
			ptr_cmp.cc mf_radix.cc queues.cc \
			tree.cc list.cc array.cc typelib.cc \
			my_copy.cc my_lib.cc hash.cc \
			my_delete.cc my_rename.cc my_redel.cc \
			my_quick.cc my_static.cc \
			my_time.cc \
			my_sync.cc my_getopt.cc my_mkdir.cc \
			default_modify.cc default.cc \
                        checksum.cc \
			my_sleep.cc \
			charset.cc charset-def.cc my_bitmap.cc my_bit.cc \
			sha1.cc \
			my_access.cc base64.cc \
			thr_alarm.cc thr_lock.cc my_thr_init.cc \
                        ${thr_mutex_source}

DEFS =			-DDEFAULT_BASEDIR=\"$(prefix)\" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DDEFAULT_HOME_ENV=DRIZZLE_HOME \
			-DDEFAULT_GROUP_SUFFIX_ENV=DRIZZLE_GROUP_SUFFIX \
			-DDEFAULT_SYSCONFDIR="\"$(sysconfdir)\"" \
                        @DEFS@

