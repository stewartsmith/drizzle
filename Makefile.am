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

ACLOCAL_AMFLAGS = -I m4

# Process this file with automake to create Makefile.in
if BUILD_GETTEXT
  po=po
endif

SUBDIRS = ${po} . \
	  mystrings \
	  mysys \
	  extra \
	  libdrizzle \
	  client \
	  storage \
	  plugin \
	  drizzled \
	  tests \
	  support-files

EXTRA_DIST = config/config.rpath
DISTCLEANFILES = ac_available_languages_fragment

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

distclean-local:
	@RM@ -r -f autom4te.cache

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
	$(MAKE) $(AM_MAKEFLAGS) test 

doxygen:
	doxygen Doxyfile

indent:
	for f in `find ${top_srcdir} -type f | grep -v innobase |\
                  ${EGREP} '\.(cc|c|h)$$' ` ; do \
            uncrustify -f $$f -c ${top_srcdir}/config/uncrustify.cfg \
               -o indentoutput.tmp && mv indentoutput.tmp "$$f" ;\
        done
