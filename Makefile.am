#  Copyright (C) 2009 Sun Microsystems
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; version 2 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

ACLOCAL_AMFLAGS = -I m4 --force

# Process this file with automake to create Makefile.in
if BUILD_GETTEXT
  po=po
endif

SUBDIRS = ${po} \
	  gnulib \
	  drizzled/message \
	  . \
	  drizzled \
	  tests \
	  support-files

BUILT_SOURCES=		drizzled/configmake.h
EXTRA_DIST=		config/config.rpath m4/gnulib-cache.m4 \
			config/plugin.ac config/plugin.am \
			config/autorun.sh

CLEANFILES=		${BUILT_SOURCES}
DISTCLEANFILES=		ac_available_languages_fragment
MAINTAINERCLEANFILES=	autom4te.cache

CLIENT_LDADD=		mysys/libmysys.la \
			mystrings/libmystrings.la \
			$(LIBDRIZZLE) $(LIBINTL) $(LIBZ) \
			client/libgetpassword.la

noinst_LTLIBRARIES= \
		client/libgetpassword.la \
		mysys/libmysys.la \
		mystrings/libmystrings.la

noinst_PROGRAMS=\
		client/drizzletest \
		extra/my_print_defaults

bin_PROGRAMS= \
		client/drizzle \
		client/drizzlecheck \
		client/drizzledump \
		client/drizzleimport \
		client/drizzleslap

man_MANS=\
		client/drizzle.1 \
		client/drizzlecheck.1 \
		client/drizzled.8 \
		client/drizzledump.1 \
		client/drizzleimport.1 \
		client/drizzleslap.1

client_libgetpassword_la_SOURCES= client/get_password.cc

noinst_HEADERS=	\
		client/client_priv.h \
		client/errname.h \
		client/get_password.h \
		client/my_readline.h \
		drizzled/configmake.h \
		mystrings/decimal.h \
		mystrings/m_ctype.h \
		mystrings/m_string.h \
		mystrings/my_uctype.h \
		mystrings/t_ctype.h \
		mystrings/utf8.h \
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
		mystrings/my_strtoll10.cc

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

client_drizzle_SOURCES=		client/drizzle.cc client/readline.cc
client_drizzle_LDADD=		${CLIENT_LDADD} ${READLINE_LIBS}

client_drizzlecheck_SOURCES =	client/drizzlecheck.cc
client_drizzlecheck_LDADD=	${CLIENT_LDADD}

client_drizzledump_SOURCES=	client/drizzledump.cc
client_drizzledump_LDADD=	${CLIENT_LDADD}

client_drizzleimport_SOURCES=	client/drizzleimport.cc
client_drizzleimport_LDADD=	${CLIENT_LDADD}

client_drizzleslap_SOURCES=	client/drizzleslap.cc
client_drizzleslap_LDADD=	${CLIENT_LDADD}

client_drizzletest_SOURCES=	client/drizzletest.cc
client_drizzletest_LDADD=	${CLIENT_LDADD} ${LIBPCRE}

EXTRA_LTLIBRARIES=
pkgplugin_LTLIBRARIES=
include config/plugin.am

extra_my_print_defaults_SOURCES= extra/my_print_defaults.cc
extra_my_print_defaults_LDADD= \
		$(top_builddir)/mysys/libmysys.la \
		$(top_builddir)/mystrings/libmystrings.la \
		$(LIBINTL)

EXTRA_DIST+=	${man_MANS}

.PHONY: test \
    indent

# Target 'test' will run the regression test suite using the built server.
#
# If you are running in a shared environment, users can avoid clashing
# port numbers by setting individual small numbers 1-100 to the
# environment variable MTR_BUILD_THREAD. The script "test-run"
# will then calculate the various port numbers it needs from this,
# making sure each user use different ports.

test: test-drizzle

test-all:
	cd tests ; \
	$(MAKE) $(AM_MAKEFLAGS) test-all

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
