# Copyright (C) 2005-2006 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#called from the top level Makefile


MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =		$(pkglibdir)/plugin

LDADD = $(LIBINTL)

DEFS =                  @DEFS@ -DHAVE_DRIZZLE_CONFIG_H

noinst_HEADERS =	ha_archive.h azio.h
noinst_PROGRAMS	=	archive_test archive_reader archive_performance concurrency_test

EXTRA_LTLIBRARIES =	ha_archive.la
pkglib_LTLIBRARIES =	@plugin_archive_shared_target@
ha_archive_la_LDFLAGS =	-module -avoid-version -rpath $(pkgplugindir) $(LDADD)
ha_archive_la_CXXFLAGS=	$(AM_CXXFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN
ha_archive_la_CFLAGS =	$(AM_CFLAGS) -DDRIZZLE_DYNAMIC_PLUGIN 
ha_archive_la_SOURCES =	ha_archive.cc azio.cc


EXTRA_LIBRARIES =	libarchive.a
noinst_LIBRARIES =	@plugin_archive_static_target@
libarchive_a_CXXFLAGS =	$(AM_CXXFLAGS) 
libarchive_a_CFLAGS =	$(AM_CFLAGS) 
libarchive_a_SOURCES =	ha_archive.cc azio.cc

archive_performance_SOURCES =	archive_performance.c azio.cc
archive_performance_CFLAGS =	$(AM_CFLAGS) 
archive_performance_LDADD =	$(top_builddir)/mysys/libmysys.la \
			$(top_builddir)/mystrings/libmystrings.la \
			$(LDADD) \
			-lz
archive_performance_LDFLAGS =

archive_test_SOURCES =	archive_test.c azio.cc
archive_test_CFLAGS =	$(AM_CFLAGS) 
archive_test_LDADD =	$(top_builddir)/mysys/libmysys.la \
			$(top_builddir)/mystrings/libmystrings.la \
			$(LDADD) \
			-lz
archive_test_LDFLAGS =

archive_reader_SOURCES = archive_reader.c azio.cc
archive_reader_CFLAGS =	$(AM_CFLAGS) 
archive_reader_LDADD =	$(top_builddir)/mysys/libmysys.la \
			$(top_builddir)/mystrings/libmystrings.la \
			$(LDADD) \
			-lz
archive_reader_LDFLAGS =

concurrency_test_SOURCES = concurrency_test.c azio.cc
concurrency_test_CFLAGS =	$(AM_CFLAGS) 
concurrency_test_LDADD =	$(top_builddir)/mysys/libmysys.la \
			$(top_builddir)/mystrings/libmystrings.la \
			$(top_builddir)/libdrizzle/libdrizzle.la \
			$(LDADD) \
			-lz
concurrency_test_LDFLAGS =

valgrind-test: archive_test archive_performance
	libtool --mode=execute valgrind --leak-check=yes --show-reachable=yes  ./concurrency_test
	libtool --mode=execute valgrind --leak-check=yes --show-reachable=yes  ./archive_test
	libtool --mode=execute valgrind --leak-check=yes --show-reachable=yes  ./archive_performance

EXTRA_DIST =		plug.in
