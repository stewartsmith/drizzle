#  Copyright (C) 2008 Sun Microsystems
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

noinst_HEADERS = convert.h \
		 math.h \
                 test.h

# Trick automake into producing CXXCOMPILE var. Won't be needed once we
# have real CXX files in here
lib_LTLIBRARIES = libutil.la
libutil_la_SOURCES = math.cc

if GCC_PCH
BUILT_SOURCES = convert.h.gch \
                test.h.gch

# Headers are always going to have unused macros... the prototection macro.
%.h.gch: %.h
	$(CXXCOMPILE) -Wno-unused-macros -include config.h -c $<
endif 
