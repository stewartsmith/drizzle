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

# This file is public domain and comes with NO WARRANTY of any kind
pkglib_LTLIBRARIES =	libmystrings.la
libmystrings_la_LDFLAGS = -static
libmystrings_la_SOURCES = 

# Force C++ linking - dummy.cxx doesn't have to exist with EXTRA in the name
nodist_EXTRA_libmystrings_la_SOURCES = dummy.cxx
# This can't be listed here as $(top_builddir)/strings/libmystingslt.la
# or it breaks make's dependency track for -j builds
libmystrings_la_LIBADD = libmystringslt.la

noinst_LTLIBRARIES =	libmystringslt.la

# Exact one of ASSEMBLER_X
if ASSEMBLER_x86
ASRCS		= strings-x86.s longlong2str-x86.s my_strtoll10-x86.s
CSRCS		= longlong2str_asm.c 
else
if ASSEMBLER_sparc32
# These file MUST all be on the same line!! Otherwise automake
# generats a very broken makefile
ASRCS		= bmove_upp-sparc.s strappend-sparc.s strend-sparc.s strmake-sparc.s strnmov-sparc.s 
CSRCS 	= strcont.c strfill.c strcend.c is_prefix.c longlong2str.c strxmov.c my_strtoll10.c 
else
#no assembler
ASRCS		=
# These file MUST all be on the same line!! Otherwise automake
# generats a very broken makefile
CSRCS 	= strxmov.c bmove_upp.c strappend.c strcont.c strend.c strfill.c strcend.c is_prefix.c strmake.c strnmov.c longlong2str.c my_strtoll10.c
endif
endif

mystringsincludedir = ${includedir}/mystrings
dist_mystringsinclude_HEADERS = 	decimal.h \
					m_ctype.h \
					m_string.h \
					my_xml.h

noinst_HEADERS =	my_uctype.h


# C files common to all three cases are included directly below, so that
# ASRCS and CSRCS can be smaller
# LIBOBJS contains strstr strtoll strtoull strmov IFF the don't exist on
# the target system. This helps prevent empty object file problems.
libmystringslt_la_LIBADD = $(LIBOBJS)
libmystringslt_la_SOURCES = 	$(ASRCS) $(CSRCS) bmove512.c \
				bchange.c strxnmov.c int2str.c str2int.c \
                                llstr.c strnlen.c \
                                ctype.c ctype-simple.c ctype-mb.c \
                                ctype-big5.c ctype-cp932.c ctype-czech.c \
                                ctype-eucjpms.c ctype-euc_kr.c ctype-gb2312.c \
                                ctype-gbk.c ctype-sjis.c ctype-tis620.c \
                                ctype-ujis.c ctype-utf8.c ctype-ucs2.c \
                                ctype-uca.c ctype-win1250ch.c ctype-bin.c \
                                ctype-latin1.c xml.c \
                                decimal.c ctype-extra.c str_alloc.c \
                                dtoa.c my_strchr.c

# Default charset definitions
EXTRA_DIST = 	ctype-big5.c ctype-cp932.c ctype-czech.c ctype-eucjpms.c \
	       	ctype-euc_kr.c ctype-win1250ch.c \
		ctype-gb2312.c ctype-gbk.c ctype-sjis.c ctype-utf8.c \
		ctype-ucs2.c ctype-uca.c ctype-tis620.c ctype-ujis.c \
		xml.c decimal.c strto.c strings-x86.s \
		longlong2str.c longlong2str-x86.s longlong2str_asm.c \
		my_strtoll10.c my_strtoll10-x86.s \
		strxmov.c bmove_upp.c strappend.c strcont.c strend.c \
		strfill.c strcend.c is_prefix.c \
		strmake.c strnmov.c strnlen.c \
		bmove_upp-sparc.s strappend-sparc.s strend-sparc.s \
		strmake-sparc.s \
		strnmov-sparc.s strxmov-sparc.s \
		t_ctype.h my_strchr.c

FLAGS=$(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS)

