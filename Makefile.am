#  Copyright (C) 2009 Sun Microsystems, Inc.
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
dist_include_HEADERS =
nobase_nodist_pkginclude_HEADERS =
nobase_dist_pkginclude_HEADERS =
dist_pkginclude_HEADERS =
sbin_PROGRAMS =
lib_LTLIBRARIES =
noinst_LTLIBRARIES =
extradir=${libdir}
extra_LTLIBRARIES =
noinst_PROGRAMS =
man_MANS =
bin_PROGRAMS =

VERSION=$(PANDORA_RELEASE_VERSION)

SUBDIRS = .
if BUILD_PO
SUBDIRS += po
endif

DIST_SUBDIRS = ${SUBDIRS} tests

BUILT_SOURCES=
BUILT_MAINT_SRC=
EXTRA_DIST= \
	    .quickly \
	    ChangeLog \
	    COPYING.Boost \
	    COPYING.BSD \
	    DRIZZLE.FAQ \
	    EXCEPTIONS-CLIENT \
	    config/autorun.sh \
	    config/config.rpath \
	    config/drizzle.ver \
	    config/link-warning.h \
	    config/pandora_vc_revinfo \
	    config/pre_hook.sh \
	    po/Makefile.in.in \
	    ${top_srcdir}/m4/pandora*m4


CLEANFILES=		${BUILT_SOURCES}
DISTCLEANFILES=		ac_available_languages_fragment \
			.plugin.scan \
			config/bzr_revinfo \
			config/top.h \
			intltool-extract \
			intltool-merge \
			intltool-update

MAINTAINERCLEANFILES= \
		      autom4te.cache \
		      ChangeLog

EXTRA_LTLIBRARIES=
pkgplugin_LTLIBRARIES=
include config/pandora-plugin.am

EXTRA_DIST+=	${man_MANS}

.PHONY: \
	indent


indent:
	for f in `find ${top_srcdir} -type f | grep -v innobase |\
                  ${EGREP} '\.(cc|c|h)$$' ` ; do \
            uncrustify -f $$f -c ${top_srcdir}/config/uncrustify.cfg \
               -o indentoutput.tmp && mv indentoutput.tmp "$$f" ;\
        done

BUILT_SOURCES+= drizzled/configmake.h
nobase_nodist_pkginclude_HEADERS+= drizzled/configmake.h
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
	-rm -rf docs/api docs/dev docs/_build docs/doctrees

include config/lcov.am

include drizzled/message/include.am
include drizzled/include.am
include drizzled/internal/include.am
include drizzled/algorithm/include.am
include drizzled/util/include.am
include client/include.am
include support-files/include.am
include tests/include.am
include unittests/include.am
include libdrizzle-1.0/libdrizzle/include.am
include libdrizzle-2.0/libdrizzle/include.am
include win32/include.am
include docs/include.am

TESTS += ${check_PROGRAMS}

merge-clean:
	find ./ | $(GREP) \.orig | xargs rm -f
	find ./ | $(GREP) \.moved | xargs rm -f
	find ./ | $(GREP) \.rej | xargs rm -f
	find ./ | $(GREP) \~$$ | xargs rm -f
	bzr unknowns

install-exec-hook:
	cd $(DESTDIR)$(sbindir) && \
	mv -f drizzled$(EXEEXT) drizzled7$(EXEEXT) && \
	$(LN_S) drizzled7$(EXEEXT) drizzled$(EXEEXT)

install-data-hook:
	cd $(DESTDIR)$(man8dir) && \
	mv -f drizzled.8 drizzled7.8 && \
	$(LN_S) drizzled7.8 drizzled.8

uninstall-hook:
	rm $(DESTDIR)$(man8dir)/drizzled7.8
	rm $(DESTDIR)$(sbindir)/drizzled7$(EXEEXT)
