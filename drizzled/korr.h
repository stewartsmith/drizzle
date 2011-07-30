/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once


/*
 * Define-functions for reading and storing in machine independent format
 * (low byte first)
 *
 * No one seems to know what "korr" means in this context. A global search
 * and replace would be fine if someone can come up with a better description
 */

/* Optimized store functions for Intel x86 */
#if defined(__i386__)
#define sint2korr(A)	(*((int16_t *) (A)))
#define sint3korr(A)	((int32_t) ((((unsigned char) (A)[2]) & 128) ?  \
                                    (((uint32_t) 255L << 24) |          \
                                     (((uint32_t) (unsigned char) (A)[2]) << 16) | \
                                     (((uint32_t) (unsigned char) (A)[1]) << 8) | \
                                     ((uint32_t) (unsigned char) (A)[0])) : \
                                    (((uint32_t) (unsigned char) (A)[2]) << 16) | \
                                    (((uint32_t) (unsigned char) (A)[1]) << 8) | \
                                    ((uint32_t) (unsigned char) (A)[0])))
#define sint4korr(A)	(*((long *) (A)))
#define uint2korr(A)	(*((uint16_t *) (A)))
#if defined(HAVE_VALGRIND)
#define uint3korr(A)	(uint32_t) (((uint32_t) ((unsigned char) (A)[0])) +\
				  (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				  (((uint32_t) ((unsigned char) (A)[2])) << 16))
#else
/*
   ATTENTION !

    Please, note, uint3korr reads 4 bytes (not 3) !
    It means, that you have to provide enough allocated space !
*/
#define uint3korr(A)	(long) (*((unsigned int *) (A)) & 0xFFFFFF)
#endif /* HAVE_VALGRIND */
#define uint4korr(A)	(*((uint32_t *) (A)))
#define uint8korr(A)	(*((uint64_t *) (A)))
#define sint8korr(A)	(*((int64_t *) (A)))
#define int2store(T,A)	*((uint16_t*) (T))= (uint16_t) (A)
#define int3store(T,A)  do { *(T)=  (unsigned char) ((A));\
                            *(T+1)=(unsigned char) (((uint32_t) (A) >> 8));\
                            *(T+2)=(unsigned char) (((A) >> 16)); } while (0)
#define int4store(T,A)	*((long *) (T))= (long) (A)
#define int8store(T,A)	*((uint64_t *) (T))= (uint64_t) (A)

typedef union {
  double v;
  long m[2];
} doubleget_union;
#define doubleget(V,M)	\
do { doubleget_union _tmp; \
     _tmp.m[0] = *((long*)(M)); \
     _tmp.m[1] = *(((long*) (M))+1); \
     (V) = _tmp.v; } while(0)
#define doublestore(T,V) do { *((long *) T) = ((doubleget_union *)&V)->m[0]; \
			     *(((long *) T)+1) = ((doubleget_union *)&V)->m[1]; \
                         } while (0)
#define float8get(V,M)   doubleget((V),(M))
#define floatstore(T,V)  memcpy((T), (&V), sizeof(float))
#define float8store(V,M) doublestore((V),(M))
#else

/*
  We're here if it's not a IA-32 architecture (Win32 and UNIX IA-32 defines
  were done before)
*/
#define sint2korr(A)	(int16_t) (((int16_t) ((unsigned char) (A)[0])) +\
				 ((int16_t) ((int16_t) (A)[1]) << 8))
#define sint4korr(A)	(int32_t) (((int32_t) ((unsigned char) (A)[0])) +\
				(((int32_t) ((unsigned char) (A)[1]) << 8)) +\
				(((int32_t) ((unsigned char) (A)[2]) << 16)) +\
				(((int32_t) ((int16_t) (A)[3]) << 24)))
#define sint8korr(A)	(int64_t) uint8korr(A)
#define uint2korr(A)	(uint16_t) (((uint16_t) ((unsigned char) (A)[0])) +\
				  ((uint16_t) ((unsigned char) (A)[1]) << 8))
#define uint3korr(A)	(uint32_t) (((uint32_t) ((unsigned char) (A)[0])) +\
				  (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				  (((uint32_t) ((unsigned char) (A)[2])) << 16))
#define uint4korr(A)	(uint32_t) (((uint32_t) ((unsigned char) (A)[0])) +\
				  (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				  (((uint32_t) ((unsigned char) (A)[2])) << 16) +\
				  (((uint32_t) ((unsigned char) (A)[3])) << 24))
#define uint8korr(A)	((uint64_t)(((uint32_t) ((unsigned char) (A)[0])) +\
				    (((uint32_t) ((unsigned char) (A)[1])) << 8) +\
				    (((uint32_t) ((unsigned char) (A)[2])) << 16) +\
				    (((uint32_t) ((unsigned char) (A)[3])) << 24)) +\
			(((uint64_t) (((uint32_t) ((unsigned char) (A)[4])) +\
				    (((uint32_t) ((unsigned char) (A)[5])) << 8) +\
				    (((uint32_t) ((unsigned char) (A)[6])) << 16) +\
				    (((uint32_t) ((unsigned char) (A)[7])) << 24))) <<\
				    32))
#define int2store(T,A)       do { uint32_t def_temp= (uint32_t) (A) ;\
                                  *((unsigned char*) (T))=  (unsigned char)(def_temp); \
                                   *((unsigned char*) (T)+1)=(unsigned char)((def_temp >> 8)); \
                             } while(0)
#define int3store(T,A)       do { /*lint -save -e734 */\
                                  *((unsigned char*)(T))=(unsigned char) ((A));\
                                  *((unsigned char*) (T)+1)=(unsigned char) (((A) >> 8));\
                                  *((unsigned char*)(T)+2)=(unsigned char) (((A) >> 16)); \
                                  /*lint -restore */} while(0)
#define int4store(T,A)       do { *((char *)(T))=(char) ((A));\
                                  *(((char *)(T))+1)=(char) (((A) >> 8));\
                                  *(((char *)(T))+2)=(char) (((A) >> 16));\
                                  *(((char *)(T))+3)=(char) (((A) >> 24)); } while(0)
#define int8store(T,A)       do { uint32_t def_temp= (uint32_t) (A), def_temp2= (uint32_t) ((A) >> 32); \
                                  int4store((T),def_temp); \
                                  int4store((T+4),def_temp2); } while(0)
#ifdef WORDS_BIGENDIAN
#define float4get(V,M)   do { float def_temp;\
                              ((unsigned char*) &def_temp)[0]=(M)[3];\
                              ((unsigned char*) &def_temp)[1]=(M)[2];\
                              ((unsigned char*) &def_temp)[2]=(M)[1];\
                              ((unsigned char*) &def_temp)[3]=(M)[0];\
                              (V)=def_temp; } while(0)
#define float8store(T,V) do { *(T)= ((unsigned char *) &V)[7];\
                              *((T)+1)=(char) ((unsigned char *) &V)[6];\
                              *((T)+2)=(char) ((unsigned char *) &V)[5];\
                              *((T)+3)=(char) ((unsigned char *) &V)[4];\
                              *((T)+4)=(char) ((unsigned char *) &V)[3];\
                              *((T)+5)=(char) ((unsigned char *) &V)[2];\
                              *((T)+6)=(char) ((unsigned char *) &V)[1];\
                              *((T)+7)=(char) ((unsigned char *) &V)[0]; } while(0)

#define float8get(V,M)   do { double def_temp;\
                              ((unsigned char*) &def_temp)[0]=(M)[7];\
                              ((unsigned char*) &def_temp)[1]=(M)[6];\
                              ((unsigned char*) &def_temp)[2]=(M)[5];\
                              ((unsigned char*) &def_temp)[3]=(M)[4];\
                              ((unsigned char*) &def_temp)[4]=(M)[3];\
                              ((unsigned char*) &def_temp)[5]=(M)[2];\
                              ((unsigned char*) &def_temp)[6]=(M)[1];\
                              ((unsigned char*) &def_temp)[7]=(M)[0];\
                              (V) = def_temp; } while(0)
#else
#define float4get(V,M)   memcpy(&V, (M), sizeof(float))

#if defined(__FLOAT_WORD_ORDER) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
#define doublestore(T,V) do { *(((char*)T)+0)=(char) ((unsigned char *) &V)[4];\
                              *(((char*)T)+1)=(char) ((unsigned char *) &V)[5];\
                              *(((char*)T)+2)=(char) ((unsigned char *) &V)[6];\
                              *(((char*)T)+3)=(char) ((unsigned char *) &V)[7];\
                              *(((char*)T)+4)=(char) ((unsigned char *) &V)[0];\
                              *(((char*)T)+5)=(char) ((unsigned char *) &V)[1];\
                              *(((char*)T)+6)=(char) ((unsigned char *) &V)[2];\
                              *(((char*)T)+7)=(char) ((unsigned char *) &V)[3]; }\
                         while(0)
#define doubleget(V,M)   do { double def_temp;\
                              ((unsigned char*) &def_temp)[0]=(M)[4];\
                              ((unsigned char*) &def_temp)[1]=(M)[5];\
                              ((unsigned char*) &def_temp)[2]=(M)[6];\
                              ((unsigned char*) &def_temp)[3]=(M)[7];\
                              ((unsigned char*) &def_temp)[4]=(M)[0];\
                              ((unsigned char*) &def_temp)[5]=(M)[1];\
                              ((unsigned char*) &def_temp)[6]=(M)[2];\
                              ((unsigned char*) &def_temp)[7]=(M)[3];\
                              (V) = def_temp; } while(0)
#endif /* __FLOAT_WORD_ORDER */

#define float8get(V,M)   doubleget((V),(M))
#define float8store(V,M) doublestore((V),(M))
#endif /* WORDS_BIGENDIAN */

#endif /* __i386__ */

/*
  Define-funktions for reading and storing in machine format from/to
  short/long to/from some place in memory V should be a (not
  register) variable, M is a pointer to byte
*/

#ifdef WORDS_BIGENDIAN

#define shortget(V,M)   do { V = (short) (((short) ((unsigned char) (M)[1]))+\
                                 ((short) ((short) (M)[0]) << 8)); } while(0)
#define longget(V,M)    do { int32_t def_temp;\
                             ((unsigned char*) &def_temp)[0]=(M)[0];\
                             ((unsigned char*) &def_temp)[1]=(M)[1];\
                             ((unsigned char*) &def_temp)[2]=(M)[2];\
                             ((unsigned char*) &def_temp)[3]=(M)[3];\
                             (V)=def_temp; } while(0)
#define shortstore(T,A) do { uint32_t def_temp=(uint32_t) (A) ;\
                             *(((char*)T)+1)=(char)(def_temp); \
                             *(((char*)T)+0)=(char)(def_temp >> 8); } while(0)
#define longstore(T,A)  do { *(((char*)T)+3)=((A));\
                             *(((char*)T)+2)=(((A) >> 8));\
                             *(((char*)T)+1)=(((A) >> 16));\
                             *(((char*)T)+0)=(((A) >> 24)); } while(0)

#define floatstore(T, V)   memcpy((T), (&V), sizeof(float))
#define doubleget(V, M)	  memcpy(&V, (M), sizeof(double))
#define doublestore(T, V)  memcpy((T), &V, sizeof(double))
#define int64_tget(V, M)   memcpy(&V, (M), sizeof(uint64_t))
#define int64_tstore(T, V) memcpy((T), &V, sizeof(uint64_t))

#else

#define shortget(V,M)	do { V = sint2korr(M); } while(0)
#define longget(V,M)	do { V = sint4korr(M); } while(0)
#define shortstore(T,V) int2store(T,V)
#define longstore(T,V)	int4store(T,V)
#ifndef floatstore
#define floatstore(T,V)   memcpy((T), (&V), sizeof(float))
#endif
#ifndef doubleget
#define doubleget(V, M)	  memcpy(&V, (M), sizeof(double))
#define doublestore(T,V)  memcpy((T), &V, sizeof(double))
#endif /* doubleget */
#define int64_tget(V,M)   memcpy(&V, (M), sizeof(uint64_t))
#define int64_tstore(T,V) memcpy((T), &V, sizeof(uint64_t))

#endif /* WORDS_BIGENDIAN */

