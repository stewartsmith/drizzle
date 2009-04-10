# Copyright (C) 2008 Sun Microsystems
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


noinst_LTLIBRARIES= libprotobuf_replicator.la
libprotobuf_replicator_la_SOURCES= protobuf_replicator.cc
libprotobuf_replicator_la_LIBADD= $(top_builddir)/drizzled/message/libdrizzledmessage.la
libprotobuf_replicator_la_CXXFLAGS= ${AM_CXXFLAGS} ${PROTOSKIP_WARNINGS}
