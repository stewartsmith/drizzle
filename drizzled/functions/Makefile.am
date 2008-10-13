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

noinst_LTLIBRARIES = 	libfunctions.la

libfunctions_la_SOURCES = abs.cc \
			  acos.cc \
			  asin.cc \
			  atan.cc \
			  additive_op.cc \
			  connection_id.cc \
			  decimal_typecast.cc \
			  divide.cc \
			  exp.cc \
			  func.cc \
                          int.cc \
                          int_divide.cc \
			  ln.cc \
			  log.cc \
			  minus.cc \
			  mod.cc \
			  multiply.cc \
			  neg.cc \
			  num1.cc \
                          numhybrid.cc \
                          num_op.cc \
                          plus.cc \
                          pow.cc \
			  real.cc \
			  signed.cc \
			  sqrt.cc \
			  unsigned.cc

noinst_HEADERS = abs.h \
		 acos.h \
		 asin.h \
		 atan.h \
		 additive_op.h \
		 connection_id.h \
		 dec.h \
		 decimal_typecast.h \
		 divide.h \
		 exp.h \
		 func.h \
                 int.h \
                 int_divide.h \
		 ln.h \
		 log.h \
		 minus.h \
		 mod.h \
		 multiply.h \
		 neg.h \
		 num1.h \
                 numhybrid.h \
                 num_op.h \
                 plus.h \
                 pow.h \
		 real.h \
		 signed.h \
		 sqrt.h \
		 unsigned.h
	

DEFS= -DDRIZZLE_SERVER @DEFS@
