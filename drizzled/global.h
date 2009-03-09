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

#include <config.h>

#if defined(__cplusplus)

# include CSTDINT_H
# include CINTTYPES_H
# include <cstdio>
# include <cstdlib>
# include <cstddef>
# include <cassert>
# include <cerrno>
# include <sstream>
#else
# include <stdint.h>
# include <inttypes.h>
# include <stdio.h>
# include <stdlib.h>
# include <stddef.h>
# include <errno.h>        /* Recommended by debian */
/*
  A lot of our programs uses asserts, so better to always include it
*/
# include <assert.h>
# include <stdbool.h>

#endif // __cplusplus

#include <math.h>

#ifndef EOVERFLOW
#define EOVERFLOW 84
#endif


#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifndef HAVE_DECL_TIMEGM
#include <gnulib/time.h>
# if defined(__cplusplus)
extern "C"
# endif
time_t timegm (struct tm *__tm);
#endif /* HAVE_DECL_TIMEGM */


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


#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

/* Declared in int2str() */
extern char _dig_vec_upper[];
extern char _dig_vec_lower[];

#define set_if_bigger(a,b)  do { if ((a) < (b)) (a)=(b); } while(0)

#define set_if_smaller(a,b) do { if ((a) > (b)) (a)=(b); } while(0)
#define array_elements(A) ((size_t) (sizeof(A)/sizeof(A[0])))

/* Some types that is different between systems */

typedef int  File;    /* File descriptor */

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
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
#define ONCE_ALLOC_INIT    (uint32_t) (4096-MALLOC_OVERHEAD)
/* Typical record cash */
#define RECORD_CACHE_SIZE  (uint32_t) (64*1024-MALLOC_OVERHEAD)
/* Typical key cash */
#define KEY_CACHE_SIZE    (uint32_t) (8*1024*1024-MALLOC_OVERHEAD)
/* Default size of a key cache block  */
#define KEY_CACHE_BLOCK_SIZE  (uint32_t) 1024


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
typedef ptrdiff_t my_ptrdiff_t;

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

typedef uint64_t my_off_t;

#if defined(SIZEOF_OFF_T)
# if (SIZEOF_OFF_T == 8)
#  define OFF_T_MAX (INT64_MAX)
# else
#  define OFF_T_MAX (INT32_MAX)
# endif
#endif

#define MY_FILEPOS_ERROR  -1

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

#define DRIZZLE_SERVER

/* Length of decimal number represented by INT32. */
#define MY_INT32_NUM_DECIMAL_DIGITS 11

/* Length of decimal number represented by INT64. */
#define MY_INT64_NUM_DECIMAL_DIGITS 21

#define PROTOCOL_VERSION 10
/*
  Io buffer size; Must be a power of 2 and
  a multiple of 512. May be
  smaller what the disk page size. This influences the speed of the
  isam btree library. eg to big to slow.
*/
#define IO_SIZE 4096
/* Max file name len */
#define FN_LEN 256
/* Max length of extension (part of FN_LEN) */
#define FN_EXTLEN 20
/* Max length of full path-name */
#define FN_REFLEN 512
/* File extension character */
#define FN_EXTCHAR '.'
/* ~ is used as abbrev for home dir */
#define FN_HOMELIB '~'
/* ./ is used as abbrev for current dir */
#define FN_CURLIB '.'
/* Parent directory; Must be a string */
#define FN_PARENTDIR ".."

/* Quote argument (before cpp) */
#ifndef QUOTE_ARG
# define QUOTE_ARG(x) #x
#endif
/* Quote argument, (after cpp) */
#ifndef STRINGIFY_ARG
# define STRINGIFY_ARG(x) QUOTE_ARG(x)
#endif

/*
 * The macros below are borrowed from include/linux/compiler.h in the
 * Linux kernel. Use them to indicate the likelyhood of the truthfulness
 * of a condition. This serves two purposes - newer versions of gcc will be
 * able to optimize for branch predication, which could yield siginficant
 * performance gains in frequently executed sections of the code, and the
 * other reason to use them is for documentation
 */
#if !defined(__GNUC__)
#define __builtin_expect(x, expected_value) (x)
#endif

#define likely(x)  __builtin_expect((x),1)
#define unlikely(x)  __builtin_expect((x),0)


/*
  Only Linux is known to need an explicit sync of the directory to make sure a
  file creation/deletion/renaming in(from,to) this directory durable.
*/
#ifdef TARGET_OS_LINUX
#define NEED_EXPLICIT_SYNC_DIR 1
#endif

/* We need to turn off _DTRACE_VERSION if we're not going to use dtrace */
#if !defined(HAVE_DTRACE)
# undef _DTRACE_VERSION
# define _DTRACE_VERSION 0
#endif

#endif /* DRIZZLE_SERVER_GLOBAL_H */
