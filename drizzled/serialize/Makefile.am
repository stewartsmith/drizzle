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

LDADD= libserialize.la \
	${LIBPROTOBUF} \
	${LIBUUID} \
	${LIBINTL} \
	${top_builddir}/gnulib/libgnu.la

AM_CXXFLAGS += ${PROTOSKIP_WARNINGS}

TESTS_ENVIRONMENT = BINDIR=${top_builddir}/drizzled/serialize sh
TESTS = check_length.sh

noinst_PROGRAMS = \
	       table_reader \
	       table_writer \
	       replication_event_reader \
	       replication_event_writer \
               master_list_reader \
               master_list_writer \
               binlog_reader \
               binlog_writer \
	       schema_reader

check_PROGRAMS = length

noinst_LTLIBRARIES = libserialize.la

nodist_libserialize_la_SOURCES = \
		binary_log.pb.cc \
		master_list.pb.cc \
		replication_event.pb.cc \
		schema.pb.cc \
		table.pb.cc

nodist_noinst_HEADERS= \
		binary_log.pb.h \
		master_list.pb.h \
		replication_event.pb.h \
		schema.pb.h \
		table.pb.h

noinst_HEADERS= \
		binary_log.h \
		binlog_encoding.h \
		ioutil.h \
		serialize.h


binlog_reader_SOURCES = binlog_reader.cc binary_log.cc

binlog_writer_SOURCES = binlog_writer.cc binary_log.cc

schema_reader_SOURCES =  schema_reader.cc

table_reader_SOURCES =  table_reader.cc

table_writer_SOURCES =  table_writer.cc

master_list_writer_SOURCES =  master_list_writer.cc

master_list_reader_SOURCES =  master_list_reader.cc

replication_event_writer_SOURCES =  replication_event_writer.cc

replication_event_reader_SOURCES =  replication_event_reader.cc

length_SOURCES = length.cc

EXTRA_DIST = schema.proto table.proto binary_log.proto master_list.proto replication_event.proto check_length.sh
BUILT_SOURCES = schema.pb.h \
		table.pb.h \
		binary_log.pb.h \
		replication_event.pb.h \
		master_list.pb.h

CLEANFILES =  binary_log.pb.h \
              binary_log.pb.cc \
              schema.pb.h \
              schema.pb.cc \
              table.pb.h \
              table.pb.cc \
	      replication_event.pb.h \
              replication_event.pb.cc \
              master_list.pb.h \
              master_list.pb.cc

SUFFIXES = .proto .pb.cc .pb.h
.proto.pb.cc:
	$(PROTOC) --proto_path=.:${top_srcdir}/drizzled/serialize \
		--cpp_out=${top_builddir}/drizzled/serialize $<
.proto.pb.h:
	$(PROTOC) --proto_path=.:${top_srcdir}/drizzled/serialize \
		--cpp_out=${top_builddir}/drizzled/serialize $<
