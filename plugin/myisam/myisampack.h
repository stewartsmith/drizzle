/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#pragma once

/*
  Storing of values in high byte first order.

  integer keys and file pointers are stored with high byte first to get
  better compression
*/

/* these two are for uniformity */
#define mi_sint1korr(A) ((int8_t)(*A))

#define mi_sint2korr(A) ((int16_t) (((int16_t) (((const unsigned char*) (A))[1])) +\
                                  ((int16_t) ((int16_t) ((const char*) (A))[0]) << 8)))
#define mi_sint3korr(A) ((int32_t) (((((const unsigned char*) (A))[0]) & 128) ? \
                                  (((uint32_t) 255L << 24) | \
                                   (((uint32_t) ((const unsigned char*) (A))[0]) << 16) |\
                                   (((uint32_t) ((const unsigned char*) (A))[1]) << 8) | \
                                   ((uint32_t) ((const unsigned char*) (A))[2])) : \
                                  (((uint32_t) ((const unsigned char*) (A))[0]) << 16) |\
                                  (((uint32_t) ((const unsigned char*) (A))[1]) << 8) | \
                                  ((uint32_t) ((const unsigned char*) (A))[2])))
#define mi_sint4korr(A) ((int32_t) (((int32_t) (((const unsigned char*) (A))[3])) +\
                                  ((int32_t) (((const unsigned char*) (A))[2]) << 8) +\
                                  ((int32_t) (((const unsigned char*) (A))[1]) << 16) +\
                                  ((int32_t) ((int16_t) ((const char*) (A))[0]) << 24)))
#define mi_sint8korr(A) ((int64_t) mi_uint8korr(A))
#define mi_uint2korr(A) ((uint16_t) (((uint16_t) (((const unsigned char*) (A))[1])) +\
                                   ((uint16_t) (((const unsigned char*) (A))[0]) << 8)))
#define mi_uint3korr(A) ((uint32_t) (((uint32_t) (((const unsigned char*) (A))[2])) +\
                                   (((uint32_t) (((const unsigned char*) (A))[1])) << 8) +\
                                   (((uint32_t) (((const unsigned char*) (A))[0])) << 16)))
#define mi_uint4korr(A) ((uint32_t) (((uint32_t) (((const unsigned char*) (A))[3])) +\
                                   (((uint32_t) (((const unsigned char*) (A))[2])) << 8) +\
                                   (((uint32_t) (((const unsigned char*) (A))[1])) << 16) +\
                                   (((uint32_t) (((const unsigned char*) (A))[0])) << 24)))
#define mi_uint5korr(A) ((uint64_t)(((uint32_t) (((const unsigned char*) (A))[4])) +\
                                    (((uint32_t) (((const unsigned char*) (A))[3])) << 8) +\
                                    (((uint32_t) (((const unsigned char*) (A))[2])) << 16) +\
                                    (((uint32_t) (((const unsigned char*) (A))[1])) << 24)) +\
                                    (((uint64_t) (((const unsigned char*) (A))[0])) << 32))
#define mi_uint6korr(A) ((uint64_t)(((uint32_t) (((const unsigned char*) (A))[5])) +\
                                    (((uint32_t) (((const unsigned char*) (A))[4])) << 8) +\
                                    (((uint32_t) (((const unsigned char*) (A))[3])) << 16) +\
                                    (((uint32_t) (((const unsigned char*) (A))[2])) << 24)) +\
                        (((uint64_t) (((uint32_t) (((const unsigned char*) (A))[1])) +\
                                    (((uint32_t) (((const unsigned char*) (A))[0]) << 8)))) <<\
                                     32))
#define mi_uint7korr(A) ((uint64_t)(((uint32_t) (((const unsigned char*) (A))[6])) +\
                                    (((uint32_t) (((const unsigned char*) (A))[5])) << 8) +\
                                    (((uint32_t) (((const unsigned char*) (A))[4])) << 16) +\
                                    (((uint32_t) (((const unsigned char*) (A))[3])) << 24)) +\
                        (((uint64_t) (((uint32_t) (((const unsigned char*) (A))[2])) +\
                                    (((uint32_t) (((const unsigned char*) (A))[1])) << 8) +\
                                    (((uint32_t) (((const unsigned char*) (A))[0])) << 16))) <<\
                                     32))
#define mi_uint8korr(A) ((uint64_t)(((uint32_t) (((const unsigned char*) (A))[7])) +\
                                    (((uint32_t) (((const unsigned char*) (A))[6])) << 8) +\
                                    (((uint32_t) (((const unsigned char*) (A))[5])) << 16) +\
                                    (((uint32_t) (((const unsigned char*) (A))[4])) << 24)) +\
                        (((uint64_t) (((uint32_t) (((const unsigned char*) (A))[3])) +\
                                    (((uint32_t) (((const unsigned char*) (A))[2])) << 8) +\
                                    (((uint32_t) (((const unsigned char*) (A))[1])) << 16) +\
                                    (((uint32_t) (((const unsigned char*) (A))[0])) << 24))) <<\
                                    32))

/* This one is for uniformity */
#define mi_int1store(T,A) *((unsigned char*)(T))= (unsigned char) (A)

#define mi_int2store(T,A)   { uint32_t def_temp= (uint32_t) (A) ;\
                              ((unsigned char*) (T))[1]= (unsigned char) (def_temp);\
                              ((unsigned char*) (T))[0]= (unsigned char) (def_temp >> 8); }
#define mi_int3store(T,A)   { /*lint -save -e734 */\
                              uint32_t def_temp= (uint32_t) (A);\
                              ((unsigned char*) (T))[2]= (unsigned char) (def_temp);\
                              ((unsigned char*) (T))[1]= (unsigned char) (def_temp >> 8);\
                              ((unsigned char*) (T))[0]= (unsigned char) (def_temp >> 16);\
                              /*lint -restore */}
#define mi_int4store(T,A)   { uint32_t def_temp= (uint32_t) (A);\
                              ((unsigned char*) (T))[3]= (unsigned char) (def_temp);\
                              ((unsigned char*) (T))[2]= (unsigned char) (def_temp >> 8);\
                              ((unsigned char*) (T))[1]= (unsigned char) (def_temp >> 16);\
                              ((unsigned char*) (T))[0]= (unsigned char) (def_temp >> 24); }
#define mi_int5store(T,A)   { uint32_t def_temp= (uint32_t) (A),\
                              def_temp2= (uint32_t) ((A) >> 32);\
                              ((unsigned char*) (T))[4]= (unsigned char) (def_temp);\
                              ((unsigned char*) (T))[3]= (unsigned char) (def_temp >> 8);\
                              ((unsigned char*) (T))[2]= (unsigned char) (def_temp >> 16);\
                              ((unsigned char*) (T))[1]= (unsigned char) (def_temp >> 24);\
                              ((unsigned char*) (T))[0]= (unsigned char) (def_temp2); }
#define mi_int6store(T,A)   { uint32_t def_temp= (uint32_t) (A),\
                              def_temp2= (uint32_t) ((A) >> 32);\
                              ((unsigned char*) (T))[5]= (unsigned char) (def_temp);\
                              ((unsigned char*) (T))[4]= (unsigned char) (def_temp >> 8);\
                              ((unsigned char*) (T))[3]= (unsigned char) (def_temp >> 16);\
                              ((unsigned char*) (T))[2]= (unsigned char) (def_temp >> 24);\
                              ((unsigned char*) (T))[1]= (unsigned char) (def_temp2);\
                              ((unsigned char*) (T))[0]= (unsigned char) (def_temp2 >> 8); }
#define mi_int7store(T,A)   { uint32_t def_temp= (uint32_t) (A),\
                              def_temp2= (uint32_t) ((A) >> 32);\
                              ((unsigned char*) (T))[6]= (unsigned char) (def_temp);\
                              ((unsigned char*) (T))[5]= (unsigned char) (def_temp >> 8);\
                              ((unsigned char*) (T))[4]= (unsigned char) (def_temp >> 16);\
                              ((unsigned char*) (T))[3]= (unsigned char) (def_temp >> 24);\
                              ((unsigned char*) (T))[2]= (unsigned char) (def_temp2);\
                              ((unsigned char*) (T))[1]= (unsigned char) (def_temp2 >> 8);\
                              ((unsigned char*) (T))[0]= (unsigned char) (def_temp2 >> 16); }
#define mi_int8store(T,A)   { uint32_t def_temp3= (uint32_t) (A),\
                              def_temp4= (uint32_t) ((A) >> 32);\
                              mi_int4store((unsigned char*) (T) + 0, def_temp4);\
                              mi_int4store((unsigned char*) (T) + 4, def_temp3); }

#ifdef WORDS_BIGENDIAN

#define mi_float4get(V,M)   { float def_temp;\
                              ((unsigned char*) &def_temp)[0]= ((const unsigned char*) (M))[0];\
                              ((unsigned char*) &def_temp)[1]= ((const unsigned char*) (M))[1];\
                              ((unsigned char*) &def_temp)[2]= ((const unsigned char*) (M))[2];\
                              ((unsigned char*) &def_temp)[3]= ((const unsigned char*) (M))[3];\
                              (V)= def_temp; }

#define mi_float8get(V,M)   { double def_temp;\
                              ((unsigned char*) &def_temp)[0]= ((unsigned char*) (M))[0];\
                              ((unsigned char*) &def_temp)[1]= ((unsigned char*) (M))[1];\
                              ((unsigned char*) &def_temp)[2]= ((unsigned char*) (M))[2];\
                              ((unsigned char*) &def_temp)[3]= ((unsigned char*) (M))[3];\
                              ((unsigned char*) &def_temp)[4]= ((unsigned char*) (M))[4];\
                              ((unsigned char*) &def_temp)[5]= ((unsigned char*) (M))[5];\
                              ((unsigned char*) &def_temp)[6]= ((unsigned char*) (M))[6];\
                              ((unsigned char*) &def_temp)[7]= ((unsigned char*) (M))[7]; \
                              (V)= def_temp; }
#else

#define mi_float4get(V,M)   { float def_temp;\
                              ((unsigned char*) &def_temp)[0]= ((unsigned char*) (M))[3];\
                              ((unsigned char*) &def_temp)[1]= ((unsigned char*) (M))[2];\
                              ((unsigned char*) &def_temp)[2]= ((unsigned char*) (M))[1];\
                              ((unsigned char*) &def_temp)[3]= ((unsigned char*) (M))[0];\
                              (V)= def_temp; }

#if defined(__FLOAT_WORD_ORDER) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN)

#define mi_float8get(V,M)   { double def_temp;\
                              ((unsigned char*) &def_temp)[0]= ((unsigned char*) (M))[3];\
                              ((unsigned char*) &def_temp)[1]= ((unsigned char*) (M))[2];\
                              ((unsigned char*) &def_temp)[2]= ((unsigned char*) (M))[1];\
                              ((unsigned char*) &def_temp)[3]= ((unsigned char*) (M))[0];\
                              ((unsigned char*) &def_temp)[4]= ((unsigned char*) (M))[7];\
                              ((unsigned char*) &def_temp)[5]= ((unsigned char*) (M))[6];\
                              ((unsigned char*) &def_temp)[6]= ((unsigned char*) (M))[5];\
                              ((unsigned char*) &def_temp)[7]= ((unsigned char*) (M))[4];\
                              (V)= def_temp; }

#else

#define mi_float8get(V,M)   { double def_temp;\
                              ((unsigned char*) &def_temp)[0]= ((unsigned char*) (M))[7];\
                              ((unsigned char*) &def_temp)[1]= ((unsigned char*) (M))[6];\
                              ((unsigned char*) &def_temp)[2]= ((unsigned char*) (M))[5];\
                              ((unsigned char*) &def_temp)[3]= ((unsigned char*) (M))[4];\
                              ((unsigned char*) &def_temp)[4]= ((unsigned char*) (M))[3];\
                              ((unsigned char*) &def_temp)[5]= ((unsigned char*) (M))[2];\
                              ((unsigned char*) &def_temp)[6]= ((unsigned char*) (M))[1];\
                              ((unsigned char*) &def_temp)[7]= ((unsigned char*) (M))[0];\
                              (V)= def_temp; }
#endif /* __FLOAT_WORD_ORDER */
#endif /* WORDS_BIGENDIAN */

/* Fix to avoid warnings when sizeof(ha_rows) == sizeof(long) */
#define mi_rowstore(T,A)    mi_int8store(T, A)
#define mi_rowkorr(T)       mi_uint8korr(T)

#define mi_sizestore(T,A)   mi_int8store(T, A)
#define mi_sizekorr(T)      mi_uint8korr(T)

