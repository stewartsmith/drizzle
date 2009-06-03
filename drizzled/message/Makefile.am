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

LDADD= libdrizzledmessage.la \
	${LIBPROTOBUF} \
	${LIBUUID} \
	${LIBINTL} \
	${top_builddir}/gnulib/libgnu.la

AM_CXXFLAGS += ${PROTOSKIP_WARNINGS}
TESTS_ENVIRONMENT = BINDIR=${top_builddir}/drizzled/message sh
TESTS = check_length.sh

noinst_PROGRAMS = \
		table_reader \
		table_writer \
		transaction_reader \
		command_reader \
		transaction_writer \
		master_list_reader \
		master_list_writer \
		binlog_reader \
		binlog_writer \
		schema_reader

check_PROGRAMS = length

noinst_LTLIBRARIES = libdrizzledmessage.la

nodist_libdrizzledmessage_la_SOURCES = \
		binary_log.pb.cc \
		master_list.pb.cc \
		transaction.pb.cc \
		schema.pb.cc \
		table.pb.cc

nodist_noinst_HEADERS= \
		binary_log.pb.h \
		master_list.pb.h \
		transaction.pb.h \
		schema.pb.h \
		table.pb.h

noinst_HEADERS= \
		binary_log.h \
		binlog_encoding.h \
		ioutil.h \
		all.h


binlog_reader_SOURCES = binlog_reader.cc binary_log.cc

binlog_writer_SOURCES = binlog_writer.cc binary_log.cc

schema_reader_SOURCES =  schema_reader.cc

table_reader_SOURCES =  table_reader.cc

table_writer_SOURCES =  table_writer.cc

master_list_writer_SOURCES =  master_list_writer.cc

master_list_reader_SOURCES =  master_list_reader.cc

transaction_writer_SOURCES =  transaction_writer.cc

transaction_reader_SOURCES =  transaction_reader.cc

command_reader_SOURCES =  command_reader.cc

length_SOURCES = length.cc

EXTRA_DIST = \
		schema.proto \
		table.proto \
		binary_log.proto \
		master_list.proto \
		transaction.proto \
		check_length.sh

BUILT_SOURCES = \
		schema.pb.h \
		table.pb.h \
		binary_log.pb.h \
		transaction.pb.h \
		master_list.pb.h

CLEANFILES = \
		binary_log.pb.h \
		binary_log.pb.cc \
		schema.pb.h \
		schema.pb.cc \
		table.pb.h \
		table.pb.cc \
		transaction.pb.h \
		transaction.pb.cc \
		master_list.pb.h \
		master_list.pb.cc

SUFFIXES = .proto .pb.cc .pb.h
.proto.pb.cc:
	$(PROTOC) --proto_path=.:${top_srcdir}/drizzled/message \
		--cpp_out=${top_builddir}/drizzled/message $<
.proto.pb.h:
	$(PROTOC) --proto_path=.:${top_srcdir}/drizzled/message \
		--cpp_out=${top_builddir}/drizzled/message $<
