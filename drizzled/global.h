/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

/* This is the include file that should be included 'first' in every C file. */

#ifndef DRIZZLE_SERVER_GLOBAL_H
#define DRIZZLE_SERVER_GLOBAL_H

#if defined(i386) && !defined(__i386__)
#define __i386__
#endif

#include "config.h"

#if defined(__cplusplus)

# if defined(__GNUC) && defined(__EXCEPTIONS)
#  error "Please add -fno-exceptions to CXXFLAGS and reconfigure/recompile"
# endif

# include CSTDINT_H
# include CINTTYPES_H
# include CMATH_H
# include <cstdio>
# include <cstdlib>
# include <cstddef>
# include <cassert>
# include <cerrno>
#else
# include <stdint.h>
# include <inttypes.h>
# include <stdio.h>
# include <stdlib.h>
# include <stddef.h>
# include <math.h>
# include <errno.h>        /* Recommended by debian */
/*
  A lot of our programs uses asserts, so better to always include it
*/
# include <assert.h>
# include <stdbool.h>

#endif // __cplusplus

/*
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif */ /* TIME_WITH_SYS_TIME */

/*
  Temporary solution to solve bug#7156. Include "sys/types.h" before
  the thread headers, else the function madvise() will not be defined
*/
#if defined(HAVE_SYS_TYPES_H) && ( defined(sun) || defined(__sun) )
#include <sys/types.h>
#endif


#include <pthread.h>    /* AIX must have this included first */

#define _REENTRANT  1  /* Threads requires reentrant code */

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(__cplusplus) && defined(NO_CPLUSPLUS_ALLOCA)
#undef HAVE_ALLOCA
#undef HAVE_ALLOCA_H
#endif
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif



#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if !defined(HAVE_UINT)
#undef HAVE_UINT
#define HAVE_UINT
typedef unsigned int uint;
#endif

/* Declared in int2str() */
extern char _dig_vec_upper[];
extern char _dig_vec_lower[];

#if defined(__cplusplus)
template <class T>
inline bool test(const T a)
{
  return a ? true : false;
}
template <class T, class U>
inline bool test_all_bits(const T a, const U b)
{
  return ((a & b) == b);
}
#else
#define test(a)    ((a) ? 1 : 0)
#define test_all_bits(a,b) (((a) & (b)) == (b))
#endif

#define set_if_bigger(a,b)  do { if ((a) < (b)) (a)=(b); } while(0)

#define set_if_smaller(a,b) do { if ((a) > (b)) (a)=(b); } while(0)
#define set_bits(type, bit_count) (sizeof(type)*8 <= (bit_count) ? ~(type) 0 : ((((type) 1) << (bit_count)) - (type) 1))
#define array_elements(A) ((size_t) (sizeof(A)/sizeof(A[0])))

/* Some types that is different between systems */

typedef int  File;    /* File descriptor */

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

/* file create flags */

#ifndef O_SHARE      /* Probably not windows */
#define O_SHARE    0  /* Flag to my_open for shared files */
#endif /* O_SHARE */

#ifndef O_BINARY
#define O_BINARY  0  /* Flag to my_open for binary files */
#endif

#ifndef FILE_BINARY
#define FILE_BINARY  O_BINARY /* Flag to my_fopen for binary streams */
#endif

#define F_TO_EOF  0L  /* Param to lockf() to lock rest of file */

#ifndef O_TEMPORARY
#define O_TEMPORARY  0
#endif
#ifndef O_SHORT_LIVED
#define O_SHORT_LIVED  0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW      0
#endif



#ifndef FN_LIBCHAR
#define FN_LIBCHAR  '/'
#define FN_ROOTDIR  "/"
#endif
#define MY_NFILE  64  /* This is only used to save filenames */
#ifndef OS_FILE_LIMIT
#define OS_FILE_LIMIT  65535
#endif

/*
  How much overhead does malloc have. The code often allocates
  something like 1024-MALLOC_OVERHEAD bytes
*/
#define MALLOC_OVERHEAD 8

/* get memory in huncs */
#define ONCE_ALLOC_INIT    (uint) (4096-MALLOC_OVERHEAD)
/* Typical record cash */
#define RECORD_CACHE_SIZE  (uint) (64*1024-MALLOC_OVERHEAD)
/* Typical key cash */
#define KEY_CACHE_SIZE    (uint) (8*1024*1024-MALLOC_OVERHEAD)
/* Default size of a key cache block  */
#define KEY_CACHE_BLOCK_SIZE  (uint) 1024


/* Some things that this system doesn't have */

/* Some defines of functions for portability */

#undef remove    /* Crashes MySQL on SCO 5.0.0 */
#ifndef uint64_t2double
#define uint64_t2double(A) ((double) (uint64_t) (A))
#define my_off_t2double(A)  ((double) (my_off_t) (A))
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#define ulong_to_double(X) ((double) (ulong) (X))

#ifndef STACK_DIRECTION
#error "please add -DSTACK_DIRECTION=1 or -1 to your CPPFLAGS"
#endif

#if !defined(HAVE_STRTOK_R)
#define strtok_r(A,B,C) strtok((A),(B))
#endif

#ifdef HAVE_FLOAT_H
#include <float.h>
#else
#if !defined(FLT_MIN)
#define FLT_MIN         ((float)1.40129846432481707e-45)
#endif
#if !defined(FLT_MAX)
#define FLT_MAX         ((float)3.40282346638528860e+38)
#endif
#endif

/* From limits.h instead */
#ifndef DBL_MIN
#define DBL_MIN    4.94065645841246544e-324
#endif
#ifndef DBL_MAX
#define DBL_MAX    1.79769313486231470e+308
#endif
#ifndef SIZE_T_MAX
#define SIZE_T_MAX ~((size_t) 0)
#endif


/* Define missing math constants. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.7182818284590452354
#endif
#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

/*
  Max size that must be added to a so that we know Size to make
  adressable obj.
*/
#if SIZEOF_CHARP == 4
typedef int32_t    my_ptrdiff_t;
#else
typedef int64_t   my_ptrdiff_t;
#endif

#define MY_ALIGN(A,L)  (((A) + (L) - 1) & ~((L) - 1))
#define ALIGN_SIZE(A)  MY_ALIGN((A),sizeof(double))
/* Size to make adressable obj. */
#define ALIGN_PTR(A, t) ((t*) MY_ALIGN((A),sizeof(t)))
/* Offset of field f in structure t */
#define OFFSET(t, f)  ((size_t)(char *)&((t *)0)->f)
#define ADD_TO_PTR(ptr,size,type) (type) ((unsigned char*) (ptr)+size)
#define PTR_BYTE_DIFF(A,B) (my_ptrdiff_t) ((unsigned char*) (A) - (unsigned char*) (B))

#define MY_DIV_UP(A, B) (((A) + (B) - 1) / (B))
#define MY_ALIGNED_BYTE_ARRAY(N, S, T) T N[MY_DIV_UP(S, sizeof(T))]

/* Typdefs for easyier portability */

#if !defined(HAVE_ULONG) && !defined(__USE_MISC)
typedef unsigned long ulong;      /* Short for unsigned long */
#endif

#if SIZEOF_OFF_T > 4 
typedef uint64_t my_off_t;
#else
typedef unsigned long my_off_t;
#endif
#define MY_FILEPOS_ERROR  (~(my_off_t) 0)

typedef off_t os_off_t;

typedef int    myf;  /* Type of MyFlags in my_funcs */
#define MYF(v)		(myf) (v)

/* Defines for time function */
#define SCALE_SEC  100
#define SCALE_USEC  10000
#define MY_HOW_OFTEN_TO_ALARM  2  /* How often we want info on screen */
#define MY_HOW_OFTEN_TO_WRITE  1000  /* How often we want info on screen */


#if defined(HAVE_CHARSET_utf8mb3) || defined(HAVE_CHARSET_utf8mb4)
#define DRIZZLE_UNIVERSAL_CLIENT_CHARSET "utf8"
#else
#define DRIZZLE_UNIVERSAL_CLIENT_CHARSET DRIZZLE_DEFAULT_CHARSET_NAME
#endif

#include <dlfcn.h>

/* FreeBSD 2.2.2 does not define RTLD_NOW) */
#ifndef RTLD_NOW
#define RTLD_NOW 1
#endif

#define cmax(a, b)       ((a) > (b) ? (a) : (b))
#define cmin(a, b)       ((a) < (b) ? (a) : (b))

/* Length of decimal number represented by INT32. */
#define MY_INT32_NUM_DECIMAL_DIGITS 11

/* Length of decimal number represented by INT64. */
#define MY_INT64_NUM_DECIMAL_DIGITS 21

/*
  Only Linux is known to need an explicit sync of the directory to make sure a
  file creation/deletion/renaming in(from,to) this directory durable.
*/
#ifdef TARGET_OS_LINUX
#define NEED_EXPLICIT_SYNC_DIR 1
#endif

#include <libdrizzle/gettext.h>

#endif /* DRIZZLE_SERVER_GLOBAL_H */
