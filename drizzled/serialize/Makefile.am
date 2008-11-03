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

#called from the top level Makefile

LDADD = libserialize.la ${PROTOBUF_LIBS}

AM_CPPFLAGS = ${PROTOBUF_CPPFLAGS}
libserialize_la_CXXFLAGS =

TESTS_ENVIRONMENT = sh
TESTS = check_length.sh

%.pb.h %.pb.cc : %.proto
	$(PROTOC) --cpp_out=${top_builddir}/drizzled/serialize $<

bin_PROGRAMS = \
	       table_reader \
	       table_writer \
               master_list_reader \
               master_list_writer \
               binlog_reader \
               binlog_writer \
	       schema_reader

check_PROGRAMS = length

noinst_LTLIBRARIES = libserialize.la

libserialize_la_SOURCES = \
			  table.pb.cc \
			  master_list.pb.cc \
			  binary_log.pb.cc \
			  schema.pb.cc

noinst_HEADERS = table.pb.h \
		 master_list.pb.h \
		 schema.pb.h \
		 binary_log.pb.h \
		 binlog_encoding.h \
		 serialize.h

DEFS= @DEFS@

binlog_reader_SOURCES = binlog_reader.cc binary_log.cc
binlog_reader_CXXFLAGS = 

binlog_writer_SOURCES = binlog_writer.cc binary_log.cc
binlog_writer_CXXFLAGS = 

schema_reader_SOURCES =  schema_reader.cc
schema_reader_CXXFLAGS =

table_reader_SOURCES =  table_reader.cc
table_reader_CXXFLAGS =

table_writer_SOURCES =  table_writer.cc
table_writer_CXXFLAGS =

master_list_writer_SOURCES =  master_list_writer.cc
master_list_writer_CXXFLAGS =

master_list_reader_SOURCES =  master_list_reader.cc
master_list_reader_CXXFLAGS =

length_SOURCES = length.cc
length_CXXFLAGS = -fexceptions

EXTRA_DIST = schema.proto table.proto binary_log.proto master_list.proto
BUILT_SOURCES = schema.pb.h \
		table.pb.h \
		binary_log.pb.h \
		master_list.pb.h

CLEANFILES =  binary_log.pb.h \
              binary_log.pb.cc \
              schema.pb.h \
              schema.pb.cc \
              table.pb.h \
              table.pb.cc \
              master_list.pb.h \
              master_list.pb.cc
