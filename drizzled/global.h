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

#define HAVE_REPLICATION

/*
  InnoDB depends on some MySQL internals which other plugins should not
  need.  This is because of InnoDB's foreign key support, "safe" binlog
  truncation, and other similar legacy features.

  We define accessors for these internals unconditionally, but do not
  expose them in mysql/plugin.h.  They are declared in ha_innodb.h for
  InnoDB's use.
*/
#define INNODB_COMPATIBILITY_HOOKS

/* to make command line shorter we'll define USE_PRAGMA_INTERFACE here */
#ifdef USE_PRAGMA_IMPLEMENTATION
#define USE_PRAGMA_INTERFACE
#endif

#if defined(i386) && !defined(__i386__)
#define __i386__
#endif

/* Macros to make switching between C and C++ mode easier */
#ifdef __cplusplus
#define C_MODE_START    extern "C" {
#define C_MODE_END	}
#else
#define C_MODE_START
#define C_MODE_END
#endif

#include "config.h"

/*
  The macros below are borrowed from include/linux/compiler.h in the
  Linux kernel. Use them to indicate the likelyhood of the truthfulness
  of a condition. This serves two purposes - newer versions of gcc will be
  able to optimize for branch predication, which could yield siginficant
  performance gains in frequently executed sections of the code, and the
  other reason to use them is for documentation
*/

#if !defined(__GNUC__) || (__GNUC__ == 2 && __GNUC_MINOR__ < 96)
#define __builtin_expect(x, expected_value) (x)
#endif

#define likely(x)	__builtin_expect((x),1)
#define unlikely(x)	__builtin_expect((x),0)

/*
 *   Disable __attribute__ for non GNU compilers, since we're using them
 *     only to either generate or suppress warnings.
 *     */
#ifndef __attribute__
# if !defined(__GNUC__)
#  define __attribute__(A)
# endif
#endif


/* Fix problem with S_ISLNK() on Linux */
#if defined(TARGET_OS_LINUX) || defined(__GLIBC__)
#undef  _GNU_SOURCE
#define _GNU_SOURCE 1
#endif


/*
  Temporary solution to solve bug#7156. Include "sys/types.h" before
  the thread headers, else the function madvise() will not be defined
*/
#if defined(HAVE_SYS_TYPES_H) && ( defined(sun) || defined(__sun) )
#include <sys/types.h>
#endif

#define __EXTENSIONS__ 1	/* We want some extension */

#if defined(HAVE_STDINT_H)
/* Need to include this _before_ stdlib, so that all defines are right */
/* We are mixing C and C++, so we wan the C limit macros in the C++ too */
/* Enable some extra C99 extensions */
#undef _STDINT_H
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
/* 
  We include the following because currently Google #$@#$ Protocol Buffers possibly break stdint defines. 
  Or I am wrong, very possible, and hope someone finds the solution.

  Taken from /usr/include/stdint.h
*/

/* 7.18.2 Limits of specified-width integer types:
 *   These #defines specify the minimum and maximum limits
 *   of each of the types declared above.
 */


#if (!defined(INT16_MAX))
/* 7.18.2.1 Limits of exact-width integer types */
#define INT8_MAX         127
#define INT16_MAX        32767
#define INT32_MAX        2147483647
#define INT64_MAX        9223372036854775807LL

#define INT8_MIN          -128
#define INT16_MIN         -32768

   /*
      Note:  the literal "most negative int" cannot be written in C --
      the rules in the standard (section 6.4.4.1 in C99) will give it
      an unsigned type, so INT32_MIN (and the most negative member of
      any larger signed type) must be written via a constant expression.
   */
#define INT32_MIN        (-INT32_MAX-1)
#define INT64_MIN        (-INT64_MAX-1)

#define UINT8_MAX         255
#define UINT16_MAX        65535
#define UINT32_MAX        4294967295U
#define UINT64_MAX        18446744073709551615ULL

/* 7.18.2.2 Limits of minimum-width integer types */
#define INT_LEAST8_MIN    INT8_MIN
#define INT_LEAST16_MIN   INT16_MIN
#define INT_LEAST32_MIN   INT32_MIN
#define INT_LEAST64_MIN   INT64_MIN

#define INT_LEAST8_MAX    INT8_MAX
#define INT_LEAST16_MAX   INT16_MAX
#define INT_LEAST32_MAX   INT32_MAX
#define INT_LEAST64_MAX   INT64_MAX

#define UINT_LEAST8_MAX   UINT8_MAX
#define UINT_LEAST16_MAX  UINT16_MAX
#define UINT_LEAST32_MAX  UINT32_MAX
#define UINT_LEAST64_MAX  UINT64_MAX

/* 7.18.2.3 Limits of fastest minimum-width integer types */
#define INT_FAST8_MIN     INT8_MIN
#define INT_FAST16_MIN    INT16_MIN
#define INT_FAST32_MIN    INT32_MIN
#define INT_FAST64_MIN    INT64_MIN

#define INT_FAST8_MAX     INT8_MAX
#define INT_FAST16_MAX    INT16_MAX
#define INT_FAST32_MAX    INT32_MAX
#define INT_FAST64_MAX    INT64_MAX

#define UINT_FAST8_MAX    UINT8_MAX
#define UINT_FAST16_MAX   UINT16_MAX
#define UINT_FAST32_MAX   UINT32_MAX
#define UINT_FAST64_MAX   UINT64_MAX

/* 7.18.2.4 Limits of integer types capable of holding object pointers */

#if __WORDSIZE == 64
#define INTPTR_MIN	  INT64_MIN
#define INTPTR_MAX	  INT64_MAX
#else
#define INTPTR_MIN        INT32_MIN
#define INTPTR_MAX        INT32_MAX
#endif

#if __WORDSIZE == 64
#define UINTPTR_MAX	  UINT64_MAX
#else
#define UINTPTR_MAX       UINT32_MAX
#endif

/* 7.18.2.5 Limits of greatest-width integer types */
#define INTMAX_MIN        INT64_MIN
#define INTMAX_MAX        INT64_MAX

#define UINTMAX_MAX       UINT64_MAX

/* 7.18.3 "Other" */
#if __WORDSIZE == 64
#define PTRDIFF_MIN	  INT64_MIN
#define PTRDIFF_MAX	  INT64_MAX
#else
#define PTRDIFF_MIN       INT32_MIN
#define PTRDIFF_MAX       INT32_MAX
#endif

/* We have no sig_atomic_t yet, so no SIG_ATOMIC_{MIN,MAX}.
   Should end up being {-127,127} or {0,255} ... or bigger.
   My bet would be on one of {U}INT32_{MIN,MAX}. */

#if __WORDSIZE == 64
#define SIZE_MAX	  UINT64_MAX
#else
#define SIZE_MAX          UINT32_MAX
#endif

#ifndef WCHAR_MAX
#  ifdef __WCHAR_MAX__
#    define WCHAR_MAX     __WCHAR_MAX__
#  else
#    define WCHAR_MAX     0x7fffffff
#  endif
#endif

/* WCHAR_MIN should be 0 if wchar_t is an unsigned type and
   (-WCHAR_MAX-1) if wchar_t is a signed type.  Unfortunately,
   it turns out that -fshort-wchar changes the signedness of
   the type. */
#ifndef WCHAR_MIN
#  if WCHAR_MAX == 0xffff
#    define WCHAR_MIN       0
#  else
#    define WCHAR_MIN       (-WCHAR_MAX-1)
#  endif
#endif

#define WINT_MIN	  INT32_MIN
#define WINT_MAX	  INT32_MAX

#define SIG_ATOMIC_MIN	  INT32_MIN
#define SIG_ATOMIC_MAX	  INT32_MAX
#endif

#else
#error "You must have inttypes!"
#endif


/*
  Solaris 9 include file <sys/feature_tests.h> refers to X/Open document

    System Interfaces and Headers, Issue 5

  saying we should define _XOPEN_SOURCE=500 to get POSIX.1c prototypes,
  but apparently other systems (namely FreeBSD) don't agree.

  On a newer Solaris 10, the above file recognizes also _XOPEN_SOURCE=600.
  Furthermore, it tests that if a program requires older standard
  (_XOPEN_SOURCE<600 or _POSIX_C_SOURCE<200112L) it cannot be
  run on a new compiler (that defines _STDC_C99) and issues an #error.
  It's also an #error if a program requires new standard (_XOPEN_SOURCE=600
  or _POSIX_C_SOURCE=200112L) and a compiler does not define _STDC_C99.

  To add more to this mess, Sun Studio C compiler defines _STDC_C99 while
  C++ compiler does not!

  So, in a desperate attempt to get correct prototypes for both
  C and C++ code, we define either _XOPEN_SOURCE=600 or _XOPEN_SOURCE=500
  depending on the compiler's announced C standard support.

  Cleaner solutions are welcome.
*/
#ifdef __sun
#if __STDC_VERSION__ - 0 >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif
#endif

#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS /* We want posix threads */
#endif

#define _REENTRANT	1	/* Some thread libraries require this */

#include <pthread.h>		/* AIX must have this included first */

#define _REENTRANT	1	/* Threads requires reentrant code */


/* gcc/egcs issues */

#if defined(__GNUC) && defined(__EXCEPTIONS)
#error "Please add -fno-exceptions to CXXFLAGS and reconfigure/recompile"
#endif

#ifndef stdin
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#include <math.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_TIMEB_H
#include <sys/timeb.h>				/* Avoid warnings on SCO */
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
#endif /* TIME_WITH_SYS_TIME */
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

#include <errno.h>				/* Recommended by debian */

#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

/*
  A lot of our programs uses asserts, so better to always include it
*/
#include <assert.h>

/* an assert that works at compile-time. only for constant expression */
#ifndef __GNUC__
#define compile_time_assert(X)  do { } while(0)
#else
#define compile_time_assert(X)                                  \
  do                                                            \
  {                                                             \
    char compile_time_assert[(X) ? 1 : -1]                      \
                             __attribute__ ((unused));          \
  } while(0)
#endif

/* Declare madvise where it is not declared for C++, like Solaris */
#if HAVE_MADVISE && !defined(HAVE_DECL_MADVISE) && defined(__cplusplus)
extern "C" int madvise(void *addr, size_t len, int behav);
#endif

/* We can not live without the following defines */

#define USE_MYFUNC 1		/* Must use syscall indirection */
#define MASTER 1		/* Compile without unireg */
#define ENGLISH 1		/* Messages in English */
#define POSIX_MISTAKE 1		/* regexp: Fix stupid spec error */

#define QUOTE_ARG(x)		#x	/* Quote argument (before cpp) */
#define STRINGIFY_ARG(x) QUOTE_ARG(x)	/* Quote argument, after cpp */
/* Does the system remember a signal handler after a signal ? */
#ifndef HAVE_BSD_SIGNALS
#define DONT_REMEMBER_SIGNAL
#endif

/* Define void to stop lint from generating "null effekt" comments */
#ifndef DONT_DEFINE_VOID
#ifdef _lint
int	__void__;
#define VOID(X)		(__void__ = (int) (X))
#else
#undef VOID
#define VOID(X)		(X)
#endif
#endif /* DONT_DEFINE_VOID */

#if !defined(HAVE_UINT)
#undef HAVE_UINT
#define HAVE_UINT
typedef unsigned int uint;
typedef unsigned short ushort;
#endif

/* Declared in int2str() */
extern char _dig_vec_upper[];
extern char _dig_vec_lower[];

#define CMP_NUM(a,b)    (((a) < (b)) ? -1 : ((a) == (b)) ? 0 : 1)
#define sgn(a)		(((a) < 0) ? -1 : ((a) > 0) ? 1 : 0)
#define test(a)		((a) ? 1 : 0)
#define set_if_bigger(a,b)  do { if ((a) < (b)) (a)=(b); } while(0)
#define set_if_smaller(a,b) do { if ((a) > (b)) (a)=(b); } while(0)
#define test_all_bits(a,b) (((a) & (b)) == (b))
#define set_bits(type, bit_count) (sizeof(type)*8 <= (bit_count) ? ~(type) 0 : ((((type) 1) << (bit_count)) - (type) 1))
#define array_elements(A) ((uint32_t) (sizeof(A)/sizeof(A[0])))
#ifndef HAVE_RINT
#define rint(A) floor((A)+(((A) < 0)? -0.5 : 0.5))
#endif

#if defined(__GNUC__)
#define function_volatile	volatile
#define my_reinterpret_cast(A) reinterpret_cast<A>
#define my_const_cast(A) const_cast<A>
# ifndef GCC_VERSION
#  define GCC_VERSION (__GNUC__ * 1000 + __GNUC_MINOR__)
# endif
#elif !defined(my_reinterpret_cast)
#define my_reinterpret_cast(A) (A)
#define my_const_cast(A) (A)
#endif

/* From old s-system.h */

/*
  Support macros for non ansi & other old compilers. Since such
  things are no longer supported we do nothing. We keep then since
  some of our code may still be needed to upgrade old customers.
*/
#define _VARARGS(X) X
#define _STATIC_VARARGS(X) X
#define _PC(X)	X

#define MIN_ARRAY_SIZE	0	/* Zero or One. Gcc allows zero*/
#define ASCII_BITS_USED 8	/* Bit char used */

/* Some types that is different between systems */

typedef int	File;		/* File descriptor */
/* Type for fuctions that handles signals */
#define sig_handler RETSIGTYPE
C_MODE_START
typedef void	(*sig_return)(void);/* Returns type from signal */
typedef int	(*qsort_cmp)(const void *,const void *);
typedef int	(*qsort_cmp2)(void*, const void *,const void *);
C_MODE_END
#define qsort_t RETQSORTTYPE	/* Broken GCC cant handle typedef !!!! */
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
typedef SOCKET_SIZE_TYPE size_socket;

#ifndef SOCKOPT_OPTLEN_TYPE
#define SOCKOPT_OPTLEN_TYPE size_socket
#endif

/* file create flags */

#ifndef O_SHARE			/* Probably not windows */
#define O_SHARE		0	/* Flag to my_open for shared files */
#endif /* O_SHARE */

#ifndef O_BINARY
#define O_BINARY	0	/* Flag to my_open for binary files */
#endif

#ifndef FILE_BINARY
#define FILE_BINARY	O_BINARY /* Flag to my_fopen for binary streams */
#endif

#define F_TO_EOF	0L	/* Param to lockf() to lock rest of file */

#ifndef O_TEMPORARY
#define O_TEMPORARY	0
#endif
#ifndef O_SHORT_LIVED
#define O_SHORT_LIVED	0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW      0
#endif



#ifndef FN_LIBCHAR
#define FN_LIBCHAR	'/'
#define FN_ROOTDIR	"/"
#endif
#define MY_NFILE	64	/* This is only used to save filenames */
#ifndef OS_FILE_LIMIT
#define OS_FILE_LIMIT	65535
#endif

/* #define EXT_IN_LIBNAME     */
/* #define FN_NO_CASE_SENCE   */
/* #define FN_UPPER_CASE TRUE */

/*
  How much overhead does malloc have. The code often allocates
  something like 1024-MALLOC_OVERHEAD bytes
*/
#define MALLOC_OVERHEAD 8

	/* get memory in huncs */
#define ONCE_ALLOC_INIT		(uint) (4096-MALLOC_OVERHEAD)
	/* Typical record cash */
#define RECORD_CACHE_SIZE	(uint) (64*1024-MALLOC_OVERHEAD)
	/* Typical key cash */
#define KEY_CACHE_SIZE		(uint) (8*1024*1024-MALLOC_OVERHEAD)
	/* Default size of a key cache block  */
#define KEY_CACHE_BLOCK_SIZE	(uint) 1024


	/* Some things that this system doesn't have */

/* Some defines of functions for portability */

#undef remove		/* Crashes MySQL on SCO 5.0.0 */
#ifndef uint64_t2double
#define uint64_t2double(A) ((double) (uint64_t) (A))
#define my_off_t2double(A)  ((double) (my_off_t) (A))
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#define ulong_to_double(X) ((double) (ulong) (X))
#define SET_STACK_SIZE(X)	/* Not needed on real machines */

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
#define DBL_MIN		4.94065645841246544e-324
#endif
#ifndef DBL_MAX
#define DBL_MAX		1.79769313486231470e+308
#endif
#ifndef SIZE_T_MAX
#define SIZE_T_MAX ~((size_t) 0)
#endif

#ifndef isfinite
#ifdef HAVE_FINITE
#define isfinite(x) finite(x)
#else
#define finite(x) (1.0 / fabs(x) > 0.0)
#endif /* HAVE_FINITE */
#endif /* isfinite */

#ifndef HAVE_ISNAN
#define isnan(x) ((x) != (x))
#endif

#ifdef HAVE_ISINF
/* isinf() can be used in both C and C++ code */
#define my_isinf(X) isinf(X)
#else
#define my_isinf(X) (!isfinite(X) && !isnan(X))
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
typedef int32_t		my_ptrdiff_t;
#else
typedef int64_t 	my_ptrdiff_t;
#endif

#define MY_ALIGN(A,L)	(((A) + (L) - 1) & ~((L) - 1))
#define ALIGN_SIZE(A)	MY_ALIGN((A),sizeof(double))
/* Size to make adressable obj. */
#define ALIGN_PTR(A, t) ((t*) MY_ALIGN((A),sizeof(t)))
			 /* Offset of field f in structure t */
#define OFFSET(t, f)	((size_t)(char *)&((t *)0)->f)
#define ADD_TO_PTR(ptr,size,type) (type) ((uchar*) (ptr)+size)
#define PTR_BYTE_DIFF(A,B) (my_ptrdiff_t) ((uchar*) (A) - (uchar*) (B))

#define MY_DIV_UP(A, B) (((A) + (B) - 1) / (B))
#define MY_ALIGNED_BYTE_ARRAY(N, S, T) T N[MY_DIV_UP(S, sizeof(T))]

/*
  Custom version of standard offsetof() macro which can be used to get
  offsets of members in class for non-POD types (according to the current
  version of C++ standard offsetof() macro can't be used in such cases and
  attempt to do so causes warnings to be emitted, OTOH in many cases it is
  still OK to assume that all instances of the class has the same offsets
  for the same members).

  This is temporary solution which should be removed once File_parser class
  and related routines are refactored.
*/

#define my_offsetof(TYPE, MEMBER) \
        ((size_t)((char *)&(((TYPE *)0x10)->MEMBER) - (char*)0x10))

#define NullS		(char *) 0

/* Typdefs for easyier portability */

#ifndef HAVE_UCHAR
typedef unsigned char	uchar;	/* Short for unsigned char */
#endif

#if !defined(HAVE_ULONG) && !defined(__USE_MISC)
typedef unsigned long ulong;		  /* Short for unsigned long */
#endif

#define MY_ERRPTR ((void*)(intptr)1)

#if SIZEOF_OFF_T > 4 
typedef uint64_t my_off_t;
#else
typedef unsigned long my_off_t;
#endif 
#define MY_FILEPOS_ERROR	(~(my_off_t) 0)

typedef off_t os_off_t;

#define socket_errno	errno
#define SOCKET_EINTR	EINTR
#define SOCKET_EAGAIN	EAGAIN
#define SOCKET_ETIMEDOUT SOCKET_EINTR
#define SOCKET_EWOULDBLOCK EWOULDBLOCK
#define SOCKET_EADDRINUSE EADDRINUSE
#define SOCKET_ENFILE	ENFILE
#define SOCKET_EMFILE	EMFILE

typedef uint8_t		int7;	/* Most effective integer 0 <= x <= 127 */
typedef short		int15;	/* Most effective integer 0 <= x <= 32767 */
typedef int		myf;	/* Type of MyFlags in my_funcs */
#if !defined(bool) && (!defined(HAVE_BOOL) || !defined(__cplusplus))
typedef char		bool;	/* Ordinary boolean values 0 1 */
#endif
	/* Macros for converting *constants* to the right type */
#define INT8(v)		(int8_t) (v)
#define INT16(v)	(int16_t) (v)
#define INT32(v)	(int32_t) (v)
#define MYF(v)		(myf) (v)

/* Defines for time function */
#define SCALE_SEC	100
#define SCALE_USEC	10000
#define MY_HOW_OFTEN_TO_ALARM	2	/* How often we want info on screen */
#define MY_HOW_OFTEN_TO_WRITE	1000	/* How often we want info on screen */




/* my_sprintf  was here. RIP */

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

/*
 *  Include standard definitions of operator new and delete.
 */
#ifdef __cplusplus
#include <new>
#include <string>
#include <algorithm>
using namespace std;
#endif
#define max(a, b)       ((a) > (b) ? (a) : (b))
#define min(a, b)       ((a) < (b) ? (a) : (b))

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
