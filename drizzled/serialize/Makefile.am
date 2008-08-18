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

LDADD = libserialize.la -lprotobuf

libserialize_la_CXXFLAGS =

noinst_LTLIBRARIES = libserialize.la

libserialize_la_SOURCES = \
			  table.pb.cc \
			  schema.pb.cc 

noinst_HEADERS = \
		 table.pb.h \
		 schema.pb.h \
		 serialize.h

schema.pb.cc: schema.proto
	protoc --cpp_out=${top_builddir}/drizzled/serialize schema.proto

table.pb.cc: table.proto
	protoc --cpp_out=${top_builddir}/drizzled/serialize table.proto
	
DEFS= @DEFS@

bin_PROGRAMS = \
	       table_reader \
	       table_writer \
	       schema_reader

schema_reader_SOURCES =  schema_reader.cc

table_reader_SOURCES =  table_reader.cc
table_reader_CXXFLAGS =

table_writer_SOURCES =  table_writer.cc
table_writer_CXXFLAGS =

EXTRA_DIST = schema.proto table.proto
BUILT_SOURCES = schema.pb.h schema.pb.cc table.pb.h table.pb.cc
