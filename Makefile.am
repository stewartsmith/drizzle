# test
# # Copyright (C) 2000-2006 MySQL AB
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

ACLOCAL_AMFLAGS = -I m4 --force

# Process this file with automake to create Makefile.in
if BUILD_GETTEXT
  po=po
endif

SUBDIRS = ${po} \
	  gnulib \
	  . \
	  extra \
	  client \
	  drizzled/message \
	  storage \
	  plugin \
	  drizzled \
	  tests \
	  support-files

EXTRA_DIST = config/config.rpath m4/gnulib-cache.m4
CLEANFILES = drizzled/configmake.h
DISTCLEANFILES = ac_available_languages_fragment
MAINTAINERCLEANFILES = autom4te.cache

BUILT_SOURCES=		drizzled/configmake.h
noinst_LTLIBRARIES=	mysys/libmysys.la mystrings/libmystrings.la

noinst_HEADERS=	\
		drizzled/configmake.h \
		mystrings/decimal.h \
		mystrings/m_ctype.h \
		mystrings/m_string.h \
		mystrings/my_uctype.h \
		mystrings/t_ctype.h
		mystrings/utf8.h
		mysys/aio_result.h \
		mysys/base64.h \
		mysys/definitions.h \
		mysys/drizzle_time.h \
		mysys/hash.h \
		mysys/iocache.h \
		mysys/my_alloc.h \
		mysys/my_bit.h \
		mysys/my_bitmap.h \
		mysys/my_dir.h \
		mysys/my_getopt.h \
		mysys/my_pthread.h \
		mysys/my_static.h \
		mysys/my_sys.h \
		mysys/my_time.h \
		mysys/my_tree.h \
		mysys/mysys_err.h \
		mysys/mysys_priv.h \
		mysys/sha1.h \
		mysys/thr_lock.h \
		mysys/typelib.h

mystrings_libmystrings_la_SOURCES= \
		mystrings/bmove_upp.cc \
		mystrings/ctype-bin.cc \
		mystrings/ctype-extra.cc \
		mystrings/ctype-mb.cc \
		mystrings/ctype-simple.cc \
		mystrings/ctype-uca.cc \
		mystrings/ctype-utf8.cc \
		mystrings/ctype.cc \
		mystrings/decimal.cc \
		mystrings/dtoa.cc \
		mystrings/int2str.cc \
		mystrings/is_prefix.cc \
		mystrings/llstr.cc \
		mystrings/longlong2str.cc \
		mystrings/my_strtoll10.cc \
		mystrings/str2int.cc

mysys_libmysys_la_SOURCES= \
		mysys/array.cc \
		mysys/base64.cc \
		mysys/charset-def.cc \
		mysys/charset.cc \
		mysys/checksum.cc \
		mysys/default.cc \
		mysys/default_modify.cc \
		mysys/errors.cc \
		mysys/hash.cc \
		mysys/mf_arr_appstr.cc \
		mysys/mf_cache.cc \
		mysys/mf_dirname.cc \
		mysys/mf_fn_ext.cc \
		mysys/mf_format.cc \
		mysys/mf_getdate.cc \
		mysys/mf_iocache.cc \
		mysys/mf_iocache2.cc \
		mysys/mf_loadpath.cc \
		mysys/mf_pack.cc \
		mysys/mf_qsort.cc \
		mysys/mf_qsort2.cc \
		mysys/mf_radix.cc \
		mysys/mf_same.cc \
		mysys/mf_sort.cc \
		mysys/mf_tempfile.cc \
		mysys/mf_wcomp.cc \
		mysys/mulalloc.cc \
		mysys/my_access.cc \
		mysys/my_alloc.cc \
		mysys/my_bit.cc \
		mysys/my_bitmap.cc \
		mysys/my_copy.cc \
		mysys/my_create.cc \
		mysys/my_delete.cc \
		mysys/my_dup.cc \
		mysys/my_error.cc \
		mysys/my_file.cc \
		mysys/my_filename.cc \
		mysys/my_getopt.cc \
		mysys/my_getsystime.cc \
		mysys/my_init.cc \
		mysys/my_lib.cc \
		mysys/my_open.cc \
		mysys/my_read.cc \
		mysys/my_redel.cc \
		mysys/my_rename.cc \
		mysys/my_static.cc \
		mysys/my_symlink.cc \
		mysys/my_symlink2.cc \
		mysys/my_sync.cc \
		mysys/my_thr_init.cc \
		mysys/my_time.cc \
		mysys/my_write.cc \
		mysys/ptr_cmp.cc \
		mysys/sha1.cc \
		mysys/thr_lock.cc \
		mysys/tree.cc \
		mysys/typelib.cc

# Create empty datadir 
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(localstatedir) 
	@if test `id -u` = 0 ; then \
		if test -n "$(GROUPADD)" && test -n "$(USERADD)"; then \
			$(GROUPADD) $(DRIZZLED_USER) ;\
			$(USERADD) -g $(DRIZZLED_USER) $(DRIZZLED_USER) ;\
			chown $(DRIZZLED_USER) $(DESTDIR)$(localstatedir) ;\
		fi \
	fi


.PHONY: test \
    test-force \
    test-full \
    test-force-full \
    test-force-mem \
    test-pl \
    test-force-pl \
    test-full-pl \
    test-force-full-pl \
    test-force-pl-mem \
    test-ps test-nr \
    test-pr test-ns \
    test-binlog-statement \
    test-ext-funcs \
    test-ext-rpl \
    test-ext-jp \
    test-ext-stress \
    test-ext \
    test-fast \
    test-fast-cursor \
    test-fast-view \
    test-full-qa \
    indent

# Target 'test' will run the regression test suite using the built server.
#
# If you are running in a shared environment, users can avoid clashing
# port numbers by setting individual small numbers 1-100 to the
# environment variable MTR_BUILD_THREAD. The script "test-run"
# will then calculate the various port numbers it needs from this,
# making sure each user use different ports.

test-nr:
	cd tests ; \
	  ./test-run $(force) --mysqld=--binlog-format=row

test-ns:
	cd tests ; \
	  ./test-run $(force) $(mem) --mysqld=--binlog-format=mixed

test-binlog-statement:
	cd tests ; \
	  ./test-run $(force) --mysqld=--binlog-format=statement

test: test-drizzle

test-drizzle:
	cd tests ; \
	$(MAKE) $(AM_MAKEFLAGS) test-drizzle

test-big:
	cd tests ; \
	$(MAKE) $(AM_MAKEFLAGS) test-big

doxygen:
	doxygen Doxyfile

indent:
	for f in `find ${top_srcdir} -type f | grep -v innobase |\
                  ${EGREP} '\.(cc|c|h)$$' ` ; do \
            uncrustify -f $$f -c ${top_srcdir}/config/uncrustify.cfg \
               -o indentoutput.tmp && mv indentoutput.tmp "$$f" ;\
        done

drizzled/configmake.h: ${top_srcdir}/Makefile.in
	@echo "Making $@"
	@rm -f $@-t $@
	@{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  echo '#define PREFIX "$(prefix)"'; \
	  echo '#define EXEC_PREFIX "$(exec_prefix)"'; \
	  echo '#define BINDIR "$(bindir)"'; \
	  echo '#define SBINDIR "$(sbindir)"'; \
	  echo '#define LIBEXECDIR "$(libexecdir)"'; \
	  echo '#define DATAROOTDIR "$(datarootdir)"'; \
	  echo '#define DATADIR "$(datadir)"'; \
	  echo '#define SYSCONFDIR "$(sysconfdir)"'; \
	  echo '#define SHAREDSTATEDIR "$(sharedstatedir)"'; \
	  echo '#define LOCALSTATEDIR "$(localstatedir)"'; \
	  echo '#define INCLUDEDIR "$(includedir)"'; \
	  echo '#define OLDINCLUDEDIR "$(oldincludedir)"'; \
	  echo '#define DOCDIR "$(docdir)"'; \
	  echo '#define INFODIR "$(infodir)"'; \
	  echo '#define HTMLDIR "$(htmldir)"'; \
	  echo '#define DVIDIR "$(dvidir)"'; \
	  echo '#define PDFDIR "$(pdfdir)"'; \
	  echo '#define PSDIR "$(psdir)"'; \
	  echo '#define LIBDIR "$(libdir)"'; \
	  echo '#define LISPDIR "$(lispdir)"'; \
	  echo '#define LOCALEDIR "$(localedir)"'; \
	  echo '#define MANDIR "$(mandir)"'; \
	  echo '#define MANEXT "$(manext)"'; \
	  echo '#define PKGDATADIR "$(pkgdatadir)"'; \
	  echo '#define PKGINCLUDEDIR "$(pkgincludedir)"'; \
	  echo '#define PKGLIBDIR "$(pkglibdir)"'; \
	  echo '#define PKGLIBEXECDIR "$(pkglibexecdir)"'; \
	  echo '#define PKGPLUGINDIR "$(pkgplugindir)"'; \
	} | sed '/""/d' > $@-t
	@if diff $@-t $@ >/dev/null 2>&1 ; then \
	  rm @-t ; \
	else \
	  mv $@-t $@ ; \
	fi

if HAVE_LCOV

lcov: lcov-clean test lcov/index.html

lcov/drizzle.output: drizzled/drizzled
	mkdir -p lcov
	${LCOV} --directory ${top_srcdir}/lcov --capture --output-file lcov/drizzle.output

lcov/index.html: lcov/drizzle.output
	${GENHTML} -o lcov lcov/drizzle.output

lcov-clean:
	ln -fs pars/pars0lex.l ${top_srcdir}/storage/innobase/pars0lex.l
	ln -fs pars/lexyy.c ${top_srcdir}/storage/innobase/lexyy.c
	ln -fs pars/pars0grm.c ${top_srcdir}/storage/innobase/pars0grm.c
	ln -fs pars/pars0grm.y ${top_srcdir}/storage/innobase/pars0grm.y

	${LCOV} --directory ${top_srcdir} --zerocounters

endif
