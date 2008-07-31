/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* This is the include file that should be included 'first' in every C file. */

#ifndef _global_h
#define _global_h

#define HAVE_REPLICATION
#define HAVE_EXTERNAL_CLIENT

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
#if defined(__cplusplus) && defined(inline)
#undef inline				/* fix configure problem */
#endif

/* Make it easier to add conditionl code for windows */
#define IF_WIN(A,B) (B)

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
  The macros below are useful in optimising places where it has been
  discovered that cache misses stall the process and where a prefetch
  of the cache line can improve matters. This is available in GCC 3.1.1
  and later versions.
  PREFETCH_READ says that addr is going to be used for reading and that
  it is to be kept in caches if possible for a while
  PREFETCH_WRITE also says that the item to be cached is likely to be
  updated.
  The *LOCALITY scripts are also available for experimentation purposes
  mostly and should only be used if they are verified to improve matters.
  For more input see GCC manual (available in GCC 3.1.1 and later)
*/

#if (__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR > 10)
#define PREFETCH_READ(addr) __builtin_prefetch(addr, 0, 3)
#define PREFETCH_WRITE(addr) \
  __builtin_prefetch(addr, 1, 3)
#define PREFETCH_READ_LOCALITY(addr, locality) \
  __builtin_prefetch(addr, 0, locality)
#define PREFETCH_WRITE_LOCALITY(addr, locality) \
  __builtin_prefetch(addr, 1, locality)
#else
#define PREFETCH_READ(addr)
#define PREFETCH_READ_LOCALITY(addr, locality)
#define PREFETCH_WRITE(addr)
#define PREFETCH_WRITE_LOCALITY(addr, locality)
#endif

/*
  now let's figure out if inline functions are supported
  autoconf defines 'inline' to be empty, if not
*/
#if defined(inline)
#define HAVE_INLINE
#endif
/* helper macro for "instantiating" inline functions */
#define STATIC_INLINE static inline

/*
  The following macros are used to control inlining a bit more than
  usual. These macros are used to ensure that inlining always or
  never occurs (independent of compilation mode).
  For more input see GCC manual (available in GCC 3.1.1 and later)
*/

#if (__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR > 10)
#define ALWAYS_INLINE __attribute__ ((always_inline))
#define NEVER_INLINE __attribute__ ((noinline))
#else
#define ALWAYS_INLINE
#define NEVER_INLINE
#endif

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

#ifdef HAVE_THREADS_WITHOUT_SOCKETS
/* MIT pthreads does not work with unix sockets */
#undef HAVE_SYS_UN_H
#endif

#define __EXTENSIONS__ 1	/* We want some extension */
#ifndef __STDC_EXT__
#define __STDC_EXT__ 1          /* To get large file support on hpux */
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

#if !defined(_THREAD_SAFE) && !defined(_AIX)
#define _THREAD_SAFE            /* Required for OSF1 */
#endif

#include <pthread.h>		/* AIX must have this included first */

#define _REENTRANT	1	/* Threads requires reentrant code */

/* Go around some bugs in different OS and compilers */

#if defined(HAVE_BROKEN_INLINE) && !defined(__cplusplus)
#undef inline
#define inline
#endif

/* gcc/egcs issues */

#if defined(__GNUC) && defined(__EXCEPTIONS)
#error "Please add -fno-exceptions to CXXFLAGS and reconfigure/recompile"
#endif

#if defined(HAVE_STDINT_H)
/* Need to include this _before_ stdlib, so that all defines are right */
/* We are mixing C and C++, so we wan the C limit macros in the C++ too */
/* Enable some extra C99 extensions */
#define __STDC_LIMIT_MACROS
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
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
/* Do not define for ultra sparcs */
#define USE_BMOVE512 1		/* Use this unless system bmove is faster */

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

#define CMP_NUM(a,b)    (((a) < (b)) ? -1 : ((a) == (b)) ? 0 : 1)
#define sgn(a)		(((a) < 0) ? -1 : ((a) > 0) ? 1 : 0)
#define swap_variables(t, a, b) { register t dummy; dummy= a; a= b; b= dummy; }
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
#ifndef Socket_defined
typedef int	my_socket;	/* File descriptor for sockets */
#define INVALID_SOCKET -1
#endif
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

/* #define USE_RECORD_LOCK	*/

	/* Unsigned types supported by the compiler */
#define UNSINT8			/* unsigned int8_t (char) */
#define UNSINT16		/* unsigned int16_t */
#define UNSINT32		/* unsigned int32_t */

	/* General constants */
#define SC_MAXWIDTH	256	/* Max width of screen (for error messages) */
#define FN_LEN		256	/* Max file name len */
#define FN_HEADLEN	253	/* Max length of filepart of file name */
#define FN_EXTLEN	20	/* Max length of extension (part of FN_LEN) */
#define FN_REFLEN	512	/* Max length of full path-name */
#define FN_EXTCHAR	'.'
#define FN_HOMELIB	'~'	/* ~/ is used as abbrev for home dir */
#define FN_CURLIB	'.'	/* ./ is used as abbrev for current dir */
#define FN_PARENTDIR	".."	/* Parent directory; Must be a string */

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
  Io buffer size; Must be a power of 2 and a multiple of 512. May be
  smaller what the disk page size. This influences the speed of the
  isam btree library. eg to big to slow.
*/
#define IO_SIZE			4096
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
#define closesocket(A)	close(A)
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

#define STDCALL

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
#define closesocket(A)	close(A)
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
typedef char		my_bool; /* Small bool */
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



/*
  Define-funktions for reading and storing in machine independent format
  (low byte first)
*/

/* Optimized store functions for Intel x86 */
#if defined(__i386__)
#define sint2korr(A)	(*((int16_t *) (A)))
#define sint3korr(A)	((int32_t) ((((uchar) (A)[2]) & 128) ? \
				  (((uint32_t) 255L << 24) | \
				   (((uint32_t) (uchar) (A)[2]) << 16) |\
				   (((uint32_t) (uchar) (A)[1]) << 8) | \
				   ((uint32_t) (uchar) (A)[0])) : \
				  (((uint32_t) (uchar) (A)[2]) << 16) |\
				  (((uint32_t) (uchar) (A)[1]) << 8) | \
				  ((uint32_t) (uchar) (A)[0])))
#define sint4korr(A)	(*((long *) (A)))
#define uint2korr(A)	(*((uint16_t *) (A)))
#if defined(HAVE_purify)
#define uint3korr(A)	(uint32_t) (((uint32_t) ((uchar) (A)[0])) +\
				  (((uint32_t) ((uchar) (A)[1])) << 8) +\
				  (((uint32_t) ((uchar) (A)[2])) << 16))
#else
/*
   ATTENTION !
   
    Please, note, uint3korr reads 4 bytes (not 3) !
    It means, that you have to provide enough allocated space !
*/
#define uint3korr(A)	(long) (*((unsigned int *) (A)) & 0xFFFFFF)
#endif /* HAVE_purify */
#define uint4korr(A)	(*((uint32_t *) (A)))
#define uint5korr(A)	((uint64_t)(((uint32_t) ((uchar) (A)[0])) +\
				    (((uint32_t) ((uchar) (A)[1])) << 8) +\
				    (((uint32_t) ((uchar) (A)[2])) << 16) +\
				    (((uint32_t) ((uchar) (A)[3])) << 24)) +\
				    (((uint64_t) ((uchar) (A)[4])) << 32))
#define uint6korr(A)	((uint64_t)(((uint32_t)    ((uchar) (A)[0]))          + \
                                     (((uint32_t)    ((uchar) (A)[1])) << 8)   + \
                                     (((uint32_t)    ((uchar) (A)[2])) << 16)  + \
                                     (((uint32_t)    ((uchar) (A)[3])) << 24)) + \
                         (((uint64_t) ((uchar) (A)[4])) << 32) +       \
                         (((uint64_t) ((uchar) (A)[5])) << 40))
#define uint8korr(A)	(*((uint64_t *) (A)))
#define sint8korr(A)	(*((int64_t *) (A)))
#define int2store(T,A)	*((uint16_t*) (T))= (uint16_t) (A)
#define int3store(T,A)  do { *(T)=  (uchar) ((A));\
                            *(T+1)=(uchar) (((uint) (A) >> 8));\
                            *(T+2)=(uchar) (((A) >> 16)); } while (0)
#define int4store(T,A)	*((long *) (T))= (long) (A)
#define int5store(T,A)  do { *(T)= (uchar)((A));\
                             *((T)+1)=(uchar) (((A) >> 8));\
                             *((T)+2)=(uchar) (((A) >> 16));\
                             *((T)+3)=(uchar) (((A) >> 24)); \
                             *((T)+4)=(uchar) (((A) >> 32)); } while(0)
#define int6store(T,A)  do { *(T)=    (uchar)((A));          \
                             *((T)+1)=(uchar) (((A) >> 8));  \
                             *((T)+2)=(uchar) (((A) >> 16)); \
                             *((T)+3)=(uchar) (((A) >> 24)); \
                             *((T)+4)=(uchar) (((A) >> 32)); \
                             *((T)+5)=(uchar) (((A) >> 40)); } while(0)
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
#define float4get(V,M)   do { *((float *) &(V)) = *((float*) (M)); } while(0)
#define float8get(V,M)   doubleget((V),(M))
#define float4store(V,M) memcpy((uchar*) V,(uchar*) (&M),sizeof(float))
#define floatstore(T,V)  memcpy((uchar*)(T), (uchar*)(&V),sizeof(float))
#define floatget(V,M)    memcpy((uchar*) &V,(uchar*) (M),sizeof(float))
#define float8store(V,M) doublestore((V),(M))
#else

/*
  We're here if it's not a IA-32 architecture (Win32 and UNIX IA-32 defines
  were done before)
*/
#define sint2korr(A)	(int16_t) (((int16_t) ((uchar) (A)[0])) +\
				 ((int16_t) ((int16_t) (A)[1]) << 8))
#define sint3korr(A)	((int32_t) ((((uchar) (A)[2]) & 128) ? \
				  (((uint32_t) 255L << 24) | \
				   (((uint32_t) (uchar) (A)[2]) << 16) |\
				   (((uint32_t) (uchar) (A)[1]) << 8) | \
				   ((uint32_t) (uchar) (A)[0])) : \
				  (((uint32_t) (uchar) (A)[2]) << 16) |\
				  (((uint32_t) (uchar) (A)[1]) << 8) | \
				  ((uint32_t) (uchar) (A)[0])))
#define sint4korr(A)	(int32_t) (((int32_t) ((uchar) (A)[0])) +\
				(((int32_t) ((uchar) (A)[1]) << 8)) +\
				(((int32_t) ((uchar) (A)[2]) << 16)) +\
				(((int32_t) ((int16_t) (A)[3]) << 24)))
#define sint8korr(A)	(int64_t) uint8korr(A)
#define uint2korr(A)	(uint16_t) (((uint16_t) ((uchar) (A)[0])) +\
				  ((uint16_t) ((uchar) (A)[1]) << 8))
#define uint3korr(A)	(uint32_t) (((uint32_t) ((uchar) (A)[0])) +\
				  (((uint32_t) ((uchar) (A)[1])) << 8) +\
				  (((uint32_t) ((uchar) (A)[2])) << 16))
#define uint4korr(A)	(uint32_t) (((uint32_t) ((uchar) (A)[0])) +\
				  (((uint32_t) ((uchar) (A)[1])) << 8) +\
				  (((uint32_t) ((uchar) (A)[2])) << 16) +\
				  (((uint32_t) ((uchar) (A)[3])) << 24))
#define uint5korr(A)	((uint64_t)(((uint32_t) ((uchar) (A)[0])) +\
				    (((uint32_t) ((uchar) (A)[1])) << 8) +\
				    (((uint32_t) ((uchar) (A)[2])) << 16) +\
				    (((uint32_t) ((uchar) (A)[3])) << 24)) +\
				    (((uint64_t) ((uchar) (A)[4])) << 32))
#define uint6korr(A)	((uint64_t)(((uint32_t)    ((uchar) (A)[0]))          + \
                                     (((uint32_t)    ((uchar) (A)[1])) << 8)   + \
                                     (((uint32_t)    ((uchar) (A)[2])) << 16)  + \
                                     (((uint32_t)    ((uchar) (A)[3])) << 24)) + \
                         (((uint64_t) ((uchar) (A)[4])) << 32) +       \
                         (((uint64_t) ((uchar) (A)[5])) << 40))
#define uint8korr(A)	((uint64_t)(((uint32_t) ((uchar) (A)[0])) +\
				    (((uint32_t) ((uchar) (A)[1])) << 8) +\
				    (((uint32_t) ((uchar) (A)[2])) << 16) +\
				    (((uint32_t) ((uchar) (A)[3])) << 24)) +\
			(((uint64_t) (((uint32_t) ((uchar) (A)[4])) +\
				    (((uint32_t) ((uchar) (A)[5])) << 8) +\
				    (((uint32_t) ((uchar) (A)[6])) << 16) +\
				    (((uint32_t) ((uchar) (A)[7])) << 24))) <<\
				    32))
#define int2store(T,A)       do { uint def_temp= (uint) (A) ;\
                                  *((uchar*) (T))=  (uchar)(def_temp); \
                                   *((uchar*) (T)+1)=(uchar)((def_temp >> 8)); \
                             } while(0)
#define int3store(T,A)       do { /*lint -save -e734 */\
                                  *((uchar*)(T))=(uchar) ((A));\
                                  *((uchar*) (T)+1)=(uchar) (((A) >> 8));\
                                  *((uchar*)(T)+2)=(uchar) (((A) >> 16)); \
                                  /*lint -restore */} while(0)
#define int4store(T,A)       do { *((char *)(T))=(char) ((A));\
                                  *(((char *)(T))+1)=(char) (((A) >> 8));\
                                  *(((char *)(T))+2)=(char) (((A) >> 16));\
                                  *(((char *)(T))+3)=(char) (((A) >> 24)); } while(0)
#define int5store(T,A)       do { *((char *)(T))=     (char)((A));  \
                                  *(((char *)(T))+1)= (char)(((A) >> 8)); \
                                  *(((char *)(T))+2)= (char)(((A) >> 16)); \
                                  *(((char *)(T))+3)= (char)(((A) >> 24)); \
                                  *(((char *)(T))+4)= (char)(((A) >> 32)); \
		                } while(0)
#define int6store(T,A)       do { *((char *)(T))=     (char)((A)); \
                                  *(((char *)(T))+1)= (char)(((A) >> 8)); \
                                  *(((char *)(T))+2)= (char)(((A) >> 16)); \
                                  *(((char *)(T))+3)= (char)(((A) >> 24)); \
                                  *(((char *)(T))+4)= (char)(((A) >> 32)); \
                                  *(((char *)(T))+5)= (char)(((A) >> 40)); \
                                } while(0)
#define int8store(T,A)       do { uint def_temp= (uint) (A), def_temp2= (uint) ((A) >> 32); \
                                  int4store((T),def_temp); \
                                  int4store((T+4),def_temp2); } while(0)
#ifdef WORDS_BIGENDIAN
#define float4store(T,A) do { *(T)= ((uchar *) &A)[3];\
                              *((T)+1)=(char) ((uchar *) &A)[2];\
                              *((T)+2)=(char) ((uchar *) &A)[1];\
                              *((T)+3)=(char) ((uchar *) &A)[0]; } while(0)

#define float4get(V,M)   do { float def_temp;\
                              ((uchar*) &def_temp)[0]=(M)[3];\
                              ((uchar*) &def_temp)[1]=(M)[2];\
                              ((uchar*) &def_temp)[2]=(M)[1];\
                              ((uchar*) &def_temp)[3]=(M)[0];\
                              (V)=def_temp; } while(0)
#define float8store(T,V) do { *(T)= ((uchar *) &V)[7];\
                              *((T)+1)=(char) ((uchar *) &V)[6];\
                              *((T)+2)=(char) ((uchar *) &V)[5];\
                              *((T)+3)=(char) ((uchar *) &V)[4];\
                              *((T)+4)=(char) ((uchar *) &V)[3];\
                              *((T)+5)=(char) ((uchar *) &V)[2];\
                              *((T)+6)=(char) ((uchar *) &V)[1];\
                              *((T)+7)=(char) ((uchar *) &V)[0]; } while(0)

#define float8get(V,M)   do { double def_temp;\
                              ((uchar*) &def_temp)[0]=(M)[7];\
                              ((uchar*) &def_temp)[1]=(M)[6];\
                              ((uchar*) &def_temp)[2]=(M)[5];\
                              ((uchar*) &def_temp)[3]=(M)[4];\
                              ((uchar*) &def_temp)[4]=(M)[3];\
                              ((uchar*) &def_temp)[5]=(M)[2];\
                              ((uchar*) &def_temp)[6]=(M)[1];\
                              ((uchar*) &def_temp)[7]=(M)[0];\
                              (V) = def_temp; } while(0)
#else
#define float4get(V,M)   memcpy((uchar*) &V,(uchar*) (M),sizeof(float))
#define float4store(V,M) memcpy((uchar*) V,(uchar*) (&M),sizeof(float))

#if defined(__FLOAT_WORD_ORDER) && (__FLOAT_WORD_ORDER == __BIG_ENDIAN)
#define doublestore(T,V) do { *(((char*)T)+0)=(char) ((uchar *) &V)[4];\
                              *(((char*)T)+1)=(char) ((uchar *) &V)[5];\
                              *(((char*)T)+2)=(char) ((uchar *) &V)[6];\
                              *(((char*)T)+3)=(char) ((uchar *) &V)[7];\
                              *(((char*)T)+4)=(char) ((uchar *) &V)[0];\
                              *(((char*)T)+5)=(char) ((uchar *) &V)[1];\
                              *(((char*)T)+6)=(char) ((uchar *) &V)[2];\
                              *(((char*)T)+7)=(char) ((uchar *) &V)[3]; }\
                         while(0)
#define doubleget(V,M)   do { double def_temp;\
                              ((uchar*) &def_temp)[0]=(M)[4];\
                              ((uchar*) &def_temp)[1]=(M)[5];\
                              ((uchar*) &def_temp)[2]=(M)[6];\
                              ((uchar*) &def_temp)[3]=(M)[7];\
                              ((uchar*) &def_temp)[4]=(M)[0];\
                              ((uchar*) &def_temp)[5]=(M)[1];\
                              ((uchar*) &def_temp)[6]=(M)[2];\
                              ((uchar*) &def_temp)[7]=(M)[3];\
                              (V) = def_temp; } while(0)
#endif /* __FLOAT_WORD_ORDER */

#define float8get(V,M)   doubleget((V),(M))
#define float8store(V,M) doublestore((V),(M))
#endif /* WORDS_BIGENDIAN */

#endif /* __i386__ */

/*
  Macro for reading 32-bit integer from network byte order (big-endian)
  from unaligned memory location.
*/
#define int4net(A)        (int32_t) (((uint32_t) ((uchar) (A)[3]))        |\
				  (((uint32_t) ((uchar) (A)[2])) << 8)  |\
				  (((uint32_t) ((uchar) (A)[1])) << 16) |\
				  (((uint32_t) ((uchar) (A)[0])) << 24))
/*
  Define-funktions for reading and storing in machine format from/to
  short/long to/from some place in memory V should be a (not
  register) variable, M is a pointer to byte
*/

#ifdef WORDS_BIGENDIAN

#define ushortget(V,M)  do { V = (uint16_t) (((uint16_t) ((uchar) (M)[1]))+\
                                 ((uint16_t) ((uint16_t) (M)[0]) << 8)); } while(0)
#define shortget(V,M)   do { V = (short) (((short) ((uchar) (M)[1]))+\
                                 ((short) ((short) (M)[0]) << 8)); } while(0)
#define longget(V,M)    do { int32_t def_temp;\
                             ((uchar*) &def_temp)[0]=(M)[0];\
                             ((uchar*) &def_temp)[1]=(M)[1];\
                             ((uchar*) &def_temp)[2]=(M)[2];\
                             ((uchar*) &def_temp)[3]=(M)[3];\
                             (V)=def_temp; } while(0)
#define ulongget(V,M)   do { uint32_t def_temp;\
                            ((uchar*) &def_temp)[0]=(M)[0];\
                            ((uchar*) &def_temp)[1]=(M)[1];\
                            ((uchar*) &def_temp)[2]=(M)[2];\
                            ((uchar*) &def_temp)[3]=(M)[3];\
                            (V)=def_temp; } while(0)
#define shortstore(T,A) do { uint def_temp=(uint) (A) ;\
                             *(((char*)T)+1)=(char)(def_temp); \
                             *(((char*)T)+0)=(char)(def_temp >> 8); } while(0)
#define longstore(T,A)  do { *(((char*)T)+3)=((A));\
                             *(((char*)T)+2)=(((A) >> 8));\
                             *(((char*)T)+1)=(((A) >> 16));\
                             *(((char*)T)+0)=(((A) >> 24)); } while(0)

#define floatget(V,M)     memcpy((uchar*) &V,(uchar*) (M), sizeof(float))
#define floatstore(T,V)   memcpy((uchar*) (T),(uchar*)(&V), sizeof(float))
#define doubleget(V,M)	  memcpy((uchar*) &V,(uchar*) (M), sizeof(double))
#define doublestore(T,V)  memcpy((uchar*) (T),(uchar*) &V, sizeof(double))
#define int64_tget(V,M)   memcpy((uchar*) &V,(uchar*) (M), sizeof(uint64_t))
#define int64_tstore(T,V) memcpy((uchar*) (T),(uchar*) &V, sizeof(uint64_t))

#else

#define ushortget(V,M)	do { V = uint2korr(M); } while(0)
#define shortget(V,M)	do { V = sint2korr(M); } while(0)
#define longget(V,M)	do { V = sint4korr(M); } while(0)
#define ulongget(V,M)   do { V = uint4korr(M); } while(0)
#define shortstore(T,V) int2store(T,V)
#define longstore(T,V)	int4store(T,V)
#ifndef floatstore
#define floatstore(T,V)   memcpy((uchar*) (T),(uchar*) (&V),sizeof(float))
#define floatget(V,M)     memcpy((uchar*) &V, (uchar*) (M), sizeof(float))
#endif
#ifndef doubleget
#define doubleget(V,M)	  memcpy((uchar*) &V,(uchar*) (M),sizeof(double))
#define doublestore(T,V)  memcpy((uchar*) (T),(uchar*) &V,sizeof(double))
#endif /* doubleget */
#define int64_tget(V,M)   memcpy((uchar*) &V,(uchar*) (M),sizeof(uint64_t))
#define int64_tstore(T,V) memcpy((uchar*) (T),(uchar*) &V,sizeof(uint64_t))

#endif /* WORDS_BIGENDIAN */

/* my_sprintf  was here. RIP */

#if defined(HAVE_CHARSET_utf8mb3) || defined(HAVE_CHARSET_utf8mb4)
#define MYSQL_UNIVERSAL_CLIENT_CHARSET "utf8"
#else
#define MYSQL_UNIVERSAL_CLIENT_CHARSET MYSQL_DEFAULT_CHARSET_NAME
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
#endif

/* Length of decimal number represented by INT32. */
#define MY_INT32_NUM_DECIMAL_DIGITS 11

/* Length of decimal number represented by INT64. */
#define MY_INT64_NUM_DECIMAL_DIGITS 21

#ifdef _cplusplus
#include <string>
#endif

/* Define some useful general macros (should be done after all headers). */
#if !defined(max)
#define max(a, b)	((a) > (b) ? (a) : (b))
#define min(a, b)	((a) < (b) ? (a) : (b))
#endif  
/*
  Only Linux is known to need an explicit sync of the directory to make sure a
  file creation/deletion/renaming in(from,to) this directory durable.
*/
#ifdef TARGET_OS_LINUX
#define NEED_EXPLICIT_SYNC_DIR 1
#endif

#endif /* my_global_h */
