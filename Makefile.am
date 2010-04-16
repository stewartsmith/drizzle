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

# includes append to these:
SUFFIXES =
TESTS = 
check_PROGRAMS =
noinst_HEADERS =
nobase_nodist_include_HEADERS =
nobase_dist_include_HEADERS =
lib_LTLIBRARIES = 
noinst_LTLIBRARIES =
noinst_PROGRAMS =

# Process this file with automake to create Makefile.in
if BUILD_GETTEXT
  po=po
endif

VERSION=$(PANDORA_RELEASE_VERSION)

SUBDIRS = ${po} \
	  gnulib \
	  .

DIST_SUBDIRS = ${SUBDIRS} tests

BUILT_SOURCES=		drizzled/configmake.h
EXTRA_DIST= \
	    Doxyfile \
	    config/config.rpath \
	    config/make-lint.py \
	    config/pandora_vc_revinfo \
	    extra/clean_source.sh \
	    extra/cpplint.py \
	    extra/run_cpplint.sh  \
	    m4/gnulib-cache.m4


CLEANFILES=		${BUILT_SOURCES}
DISTCLEANFILES=		ac_available_languages_fragment \
			.plugin.scan \
			config/bzr_revinfo \
			config/top.h

MAINTAINERCLEANFILES=	autom4te.cache

EXTRA_LTLIBRARIES=
pkgplugin_LTLIBRARIES=
include config/pandora-plugin.am

EXTRA_DIST+=	${man_MANS}

.PHONY: \
	indent

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
          echo '#undef VERSION'; \
          echo '#define VERSION "${PANDORA_RELEASE_VERSION}"'; \
	} | sed '/""/d' > $@-t
	@if diff $@-t $@ >/dev/null 2>&1 ; then \
	  rm @-t ; \
	else \
	  mv $@-t $@ ; \
	fi

clean-local:
	find . -name '*.gcno' | xargs rm -f 

include config/lint-source.am

include drizzled/include.am
include drizzled/internal/include.am
include drizzled/algorithm/include.am
include drizzled/util/include.am
include drizzled/message/include.am
include client/include.am
include extra/include.am
include support-files/include.am
include tests/include.am

merge-clean:
	find ./ | $(GREP) \.orig | xargs rm -f
	find ./ | $(GREP) \.rej | xargs rm -f
	find ./ | $(GREP) \~$$ | xargs rm -f
	bzr unknowns

