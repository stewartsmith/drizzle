# Copyright (C) 2000-2006 MySQL AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA

srcdir=			${top_srcdir}/tests
benchdir_root=		$(prefix)
testdir =	        $(benchdir_root)/mysql-test
EXTRA_SCRIPTS = 	valgrind.supp $(PRESCRIPTS)
GENSCRIPTS =		install_test_db dtr test-run
PRESCRIPTS =		test-run.pl stress-test.pl
noinst_SCRIPTS =	$(GENSCRIPTS) $(PRESCRIPTS)
CLEANFILES = 		$(GENSCRIPTS) mtr
DISTCLEANFILES = 	var
noinst_PROGRAMS = 	resolve_stack_dump
resolve_stack_dump_SOURCES = resolve_stack_dump.cc

LDADD=			$(top_builddir)/mysys/libmysys.la \
                        $(top_builddir)/mystrings/libmystrings.la \
			$(LIBINTL)

TEST_RUN= $(PERL) -I$(top_srcdir)/tests/lib \
		${top_srcdir}/tests/test-run.pl \
		--vardir=${top_builddir}/tests/var \
		--testdir=${top_srcdir}/tests \
		--mtr-build-thread=$$$$

EXTRA_DIST = \
	$(EXTRA_SCRIPTS) \
	suite \
	${srcdir}/t/*.test \
	${srcdir}/t/*.opt \
	${srcdir}/extra/rpl_tests/*.test \
	${srcdir}/extra/binlog_tests/*.test \
	${srcdir}/include/*.inc \
	${srcdir}/include/*.sql \
	${srcdir}/include/*.test \
	${srcdir}/r/*.result \
	${srcdir}/r/*.require \
	${srcdir}/std_data/Moscow_leap \
	${srcdir}/std_data/Index.xml \
	${srcdir}/std_data/*.dat \
	${srcdir}/std_data/*.000001 \
	${srcdir}/std_data/des_key_file \
	${srcdir}/std_data/*.pem \
	${srcdir}/std_data/*.frm \
	${srcdir}/std_data/*.MY* \
	${srcdir}/std_data/*.cnf \
	${srcdir}/std_data/parts/part_* \
	${srcdir}/lib/*.pl \
	${srcdir}/lib/My/*.pm

check: test-drizzle

# dtr - a shortcut for executing test-run.pl
dtr:
	$(RM) -f mtr dtr
	$(LN_S) ${top_srcdir}/tests/test-run.pl mtr
	$(LN_S) ${top_srcdir}/tests/test-run.pl dtr
	$(MKDIR_P) ${top_builddir}/tests/var

# test-run - a shortcut for executing test-run.pl
test-run:
	$(RM) -f test-run
	$(LN_S) ${top_srcdir}/tests/test-run.pl test-run


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
    test-embedded \
    test-fast \
    test-fast-cursor \
    test-fast-view \
    test-full-qa

# Target 'test' will run the regression test suite using the built server.
#
# If you are running in a shared environment, users can avoid clashing
# port numbers by setting individual small numbers 1-100 to the
# environment variable MTR_BUILD_THREAD. The script "test-run"
# will then calculate the various port numbers it needs from this,
# making sure each user use different ports.

test-nr:
	cd tests ; \
	  $(TEST_RUN) $(force) --mysqld=--binlog-format=row

test-ns:
	cd tests ; \
	  $(TEST_RUN) $(force) $(mem) --mysqld=--binlog-format=mixed

test-binlog-statement:
	cd tests ; \
	  $(TEST_RUN) $(force) --mysqld=--binlog-format=statement

test: test-drizzle

# Please keep the list of tests in alphabetical order for ease of
# maintenance and verification

test-drizzle:
	  $(TEST_RUN) --fast --reorder --force --suite=main,vcol
