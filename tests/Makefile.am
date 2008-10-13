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

## Process this file with automake to create Makefile.in

SUBDIRS = 

benchdir_root=		$(prefix)
testdir =	        $(benchdir_root)/mysql-test
EXTRA_SCRIPTS = 	valgrind.supp $(PRESCRIPTS)
EXTRA_DIST = 		$(EXTRA_SCRIPTS) suite
GENSCRIPTS =		install_test_db dtr test-run
PRESCRIPTS =		test-run.pl stress-test.pl
noinst_SCRIPTS =	$(GENSCRIPTS) $(PRESCRIPTS)
CLEANFILES = 		$(GENSCRIPTS) mtr
noinst_PROGRAMS = 	resolve_stack_dump

LDADD=			$(top_builddir)/mysys/libmysys.la \
                        $(top_builddir)/mystrings/libmystrings.la \
			$(LIBINTL)



dist-hook:
	mkdir -p \
		$(distdir)/t \
                $(distdir)/extra/binlog_tests \
                $(distdir)/extra/rpl_tests \
		$(distdir)/r \
		$(distdir)/include \
		$(distdir)/std_data \
		$(distdir)/std_data/parts \
		$(distdir)/lib \
		$(distdir)/lib/My
	-$(INSTALL_DATA) $(srcdir)/t/*.def $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.test $(distdir)/t
	-$(INSTALL_DATA) $(srcdir)/t/*.imtest $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/t/*.sql $(distdir)/t
	-$(INSTALL_DATA) $(srcdir)/t/*.disabled $(distdir)/t
	-$(INSTALL_DATA) $(srcdir)/t/*.opt $(srcdir)/t/*.slave-mi $(distdir)/t
	-$(INSTALL_SCRIPT) $(srcdir)/t/*.sh $(distdir)/t
	$(INSTALL_DATA) $(srcdir)/extra/binlog_tests/*.test $(distdir)/extra/binlog_tests
	$(INSTALL_DATA) $(srcdir)/extra/rpl_tests/*.test $(distdir)/extra/rpl_tests
	-$(INSTALL_DATA) $(srcdir)/extra/binlog_tests/*.opt $(distdir)/extra/binlog_tests
	-$(INSTALL_DATA) $(srcdir)/extra/rpl_tests/*.opt $(distdir)/extra/rpl_tests
	$(INSTALL_DATA) $(srcdir)/include/*.inc $(distdir)/include
	$(INSTALL_DATA) $(srcdir)/include/*.sql $(distdir)/include
	$(INSTALL_DATA) $(srcdir)/include/*.test $(distdir)/include
	$(INSTALL_DATA) $(srcdir)/r/*.result $(srcdir)/r/*.require $(distdir)/r
	$(INSTALL_DATA) $(srcdir)/std_data/Moscow_leap $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/Index.xml $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.dat $(srcdir)/std_data/*.000001 $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/des_key_file $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.pem $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.frm $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.MY* $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/*.cnf $(distdir)/std_data
	$(INSTALL_DATA) $(srcdir)/std_data/parts/part_* $(distdir)/std_data/parts
	$(INSTALL_DATA) $(srcdir)/lib/*.pl $(distdir)/lib
	$(INSTALL_DATA) $(srcdir)/lib/My/*.pm $(distdir)/lib/My

# dtr - a shortcut for executing test-run.pl
dtr:
	$(RM) -f mtr dtr
	$(LN_S) test-run.pl mtr
	$(LN_S) test-run.pl dtr

# test-run - a shortcut for executing test-run.pl
test-run:
	$(RM) -f test-run
	$(LN_S) test-run.pl test-run


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
	  ./test-run $(force) --mysqld=--binlog-format=row

test-ns:
	cd tests ; \
	  ./test-run $(force) $(mem) --mysqld=--binlog-format=mixed

test-binlog-statement:
	cd tests ; \
	  ./test-run $(force) --mysqld=--binlog-format=statement

test: test-drizzle

# Please keep the list of tests in alphabetical order for ease of
# maintenance and verification

test-drizzle:
	  $(PERL) -I$(top_srcdir)/tests/lib \
		$(top_srcdir)/tests/test-run.pl --fast --reorder --force \
        1st \
        alter_table \
        bench_count_distinct \
	bulk_replace \
	comment_column2 \
	comment_table \
	comments \
	consistent_snapshot \
        count_distinct \
        count_distinct2 \
        count_distinct3 \
	create_select_tmp \
        ctype_filename \
        delete \
        distinct \
	drizzleslap \
        endspace \
        flush2 \
        func_equal \
        func_group_innodb \
        func_isnull \
        func_like \
	func_math \
	func_compress \
	func_sapdb \
	func_time \
	func_weight_string \
        greedy_optimizer \
        group_min_max_innodb \
        heap_auto_increment \
        heap_btree \
        heap_var \
        information_schema \
        innodb-lock \
        innodb-semi-consistent \
        innodb_timeout_rollback \
        insert \
        join \
        join_crash \
        join_nested \
        join_outer \
        join_outer_innodb \
        key_diff \
        key_primary \
        keywords \
        lock \
        limit \
        lock_tables_lost_commit \
	order_by \
	order_fill_sortbuf \
	overflow \
	pool_of_threads \
        rollback \
        select \
        subselect \
        subselect3 \
        subselect_innodb \
        sum_distinct \
        tablelock \
	type_blob \
	type_date \
	type_datetime \
	type_float \
        type_newdecimal \
	type_ranges \
	type_time \
	type_timestamp \
	type_uint \
	type_varchar \
        unsafe_binlog_innodb \
        update \
        variables-notembedded
	  $(PERL) -I$(top_srcdir)/tests/lib \
		$(top_srcdir)/tests/test-run.pl --fast --reorder --force \
		--suite=vcol
