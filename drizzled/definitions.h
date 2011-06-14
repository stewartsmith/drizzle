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

/**
 * @file
 *
 * Mostly constants and some macros/functions used by the server
 */

#pragma once

#include <drizzled/enum.h>

#include <stdint.h>

#include "common.h"

namespace drizzled
{

/* Global value for how we extend our temporary directory */
#define GLOBAL_TEMPORARY_EXT ".temporary"

/* These paths are converted to other systems (WIN95) before use */

#define LANGUAGE	"english/"
#define TEMP_PREFIX	"MY"

#define ER(X) ::drizzled::error_message((X))

/* buffer size for strerror_r() */
#define STRERROR_MAX 256

/* extra 4+4 bytes for slave tmp tables */
#define MAX_FIELD_NAME 34			/* Max colum name length +2 */
#define MAX_SYS_VAR_LENGTH 32
#define MAX_INDEXES 64
const uint32_t MAX_KEY = MAX_INDEXES;                     /* Max used keys */
const uint32_t MAX_REF_PARTS = 16;			/* Max parts used as ref */
const uint32_t MAX_KEY_LENGTH = 4096;			 // max possible key
const uint32_t MAX_KEY_LENGTH_DECIMAL_WIDTH = 4; // strlen("4096")

#if SIZEOF_OFF_T > 4
#define MAX_REFLENGTH 8				/* Max length for record ref */
#else
#define MAX_REFLENGTH 4				/* Max length for record ref */
#endif

#define MAX_MBWIDTH		4		/* Max multibyte sequence */
#define MAX_FIELD_CHARLENGTH	255
#define MAX_FIELD_VARCHARLENGTH	65535
#define CONVERT_IF_BIGGER_TO_BLOB 512		/* Used for CREATE ... SELECT */

/* Max column width +1 */
#define MAX_FIELD_WIDTH		(MAX_FIELD_CHARLENGTH*MAX_MBWIDTH+1)

#define MAX_TABLES	(sizeof(table_map)*8-3)	/* Max tables in join */
#define PARAM_TABLE_BIT	(((table_map) 1) << (sizeof(table_map)*8-3))
#define OUTER_REF_TABLE_BIT	(((table_map) 1) << (sizeof(table_map)*8-2))
#define RAND_TABLE_BIT	(((table_map) 1) << (sizeof(table_map)*8-1))
#define PSEUDO_TABLE_BITS (PARAM_TABLE_BIT | OUTER_REF_TABLE_BIT | \
                           RAND_TABLE_BIT)
#define MAX_FIELDS	4096      /* Historical limit from MySQL FRM. */

#define MAX_SELECT_NESTING (sizeof(nesting_map)*8-1)

#define MAX_SORT_MEMORY (2048*1024-MALLOC_OVERHEAD)
#define MIN_SORT_MEMORY (32*1024-MALLOC_OVERHEAD)

#define DEFAULT_ERROR_COUNT	64
#define EXTRA_RECORDS	10			/* Extra records in sort */
#define NAMES_SEP_CHAR	'\377'			/* Char to sep. names */

#define READ_RECORD_BUFFER	(uint32_t) (IO_SIZE*8) /* Pointer_buffer_size */
#define DISK_BUFFER_SIZE	(uint32_t) (IO_SIZE*16) /* Size of diskbuffer */

#define ME_ERROR (ME_BELL+ME_OLDWIN+ME_NOREFRESH)
#define MYF_RW MYF(MY_WME+MY_NABP)		/* Vid my_read & my_write */

/*
  Minimum length pattern before Turbo Boyer-Moore is used
  for SELECT "text" LIKE "%pattern%", excluding the two
  wildcards in class Item_func_like.
*/
#define MIN_TURBOBM_PATTERN_LEN 3

/* Below are #defines that used to be in mysql_priv.h */
/***************************************************************************
  Configuration parameters
****************************************************************************/
#define MAX_FIELDS_BEFORE_HASH	32
#define TABLE_OPEN_CACHE_MIN    64
#define TABLE_OPEN_CACHE_DEFAULT 1024

/*
 Value of 9236 discovered through binary search 2006-09-26 on Ubuntu Dapper
 Drake, libc6 2.3.6-0ubuntu2, Linux kernel 2.6.15-27-686, on x86.  (Added
 100 bytes as reasonable buffer against growth and other environments'
 requirements.)

 Feel free to raise this by the smallest amount you can to get the
 "execution_constants" test to pass.
 */
#define STACK_MIN_SIZE          12000   ///< Abort if less stack during eval.

#define STACK_MIN_SIZE_FOR_OPEN 1024*80
#define STACK_BUFF_ALLOC        352     ///< For stack overrun checks

#define QUERY_ALLOC_BLOCK_SIZE		8192
#define QUERY_ALLOC_PREALLOC_SIZE   	8192
#define RANGE_ALLOC_BLOCK_SIZE		4096
#define TABLE_ALLOC_BLOCK_SIZE		1024
#define WARN_ALLOC_BLOCK_SIZE		2048
#define WARN_ALLOC_PREALLOC_SIZE	1024

/*
  The following parameters is to decide when to use an extra cache to
  optimise seeks when reading a big table in sorted order
*/
#define MIN_FILE_LENGTH_TO_USE_ROW_CACHE (10L*1024*1024)
#define MIN_ROWS_TO_USE_TABLE_CACHE	 100

/**
  The following is used to decide if MySQL should use table scanning
  instead of reading with keys.  The number says how many evaluation of the
  WHERE clause is comparable to reading one extra row from a table.
*/
#define TIME_FOR_COMPARE   5	// 5 compares == one read

/**
  Number of comparisons of table rowids equivalent to reading one row from a
  table.
*/
#define TIME_FOR_COMPARE_ROWID  (TIME_FOR_COMPARE*2)

/*
  For sequential disk seeks the cost formula is:
    DISK_SEEK_BASE_COST + DISK_SEEK_PROP_COST * #blocks_to_skip

  The cost of average seek
    DISK_SEEK_BASE_COST + DISK_SEEK_PROP_COST*BLOCKS_IN_AVG_SEEK =1.0.
*/
#define DISK_SEEK_BASE_COST ((double)0.9)

#define BLOCKS_IN_AVG_SEEK  128

#define DISK_SEEK_PROP_COST ((double)0.1/BLOCKS_IN_AVG_SEEK)


/**
  Number of rows in a reference table when refereed through a not unique key.
  This value is only used when we don't know anything about the key
  distribution.
*/
#define MATCHING_ROWS_IN_OTHER_TABLE 10

/** Don't pack string keys shorter than this (if PACK_KEYS=1 isn't used). */
#define KEY_DEFAULT_PACK_LENGTH 8

/** Characters shown for the command in 'show processlist'. */
#define PROCESS_LIST_WIDTH 100

/* Bits for different SQL modes modes (including ANSI mode) */
#define MODE_NO_ZERO_DATE		(2)
#define MODE_INVALID_DATES		(MODE_NO_ZERO_DATE*2)

#define MY_CHARSET_BIN_MB_MAXLEN 1

// uncachable cause
static const uint32_t UNCACHEABLE_DEPENDENT= 1;
static const uint32_t UNCACHEABLE_RAND= 2;
static const uint32_t UNCACHEABLE_SIDEEFFECT= 3;
/// forcing to save JOIN for explain
static const uint32_t UNCACHEABLE_EXPLAIN= 4;
/** Don't evaluate subqueries in prepare even if they're not correlated */
static const uint32_t UNCACHEABLE_PREPARE= 5;
/* For uncorrelated SELECT in an UNION with some correlated SELECTs */
static const uint32_t UNCACHEABLE_UNITED= 6;

/* Used to check GROUP BY list in the MODE_ONLY_FULL_GROUP_BY mode */
#define UNDEF_POS (-1)

/* Options to add_table_to_list() */
static const uint32_t TL_OPTION_UPDATING= 0;
static const uint32_t TL_OPTION_FORCE_INDEX= 1;
static const uint32_t TL_OPTION_IGNORE_LEAVES= 2;
static const uint32_t TL_OPTION_ALIAS= 3;
static const uint32_t NUM_OF_TABLE_OPTIONS= 4;

/* Some portable defines */

#define portable_sizeof_char_ptr 8

#define TMP_FILE_PREFIX "#sql"			/**< Prefix for tmp tables */
#define TMP_FILE_PREFIX_LENGTH 4

/* Flags for calc_week() function.  */
#define WEEK_MONDAY_FIRST    1
#define WEEK_YEAR            2
#define WEEK_FIRST_WEEKDAY   4

/* used in date and time conversions */
/* Daynumber from year 0 to 9999-12-31 */
#define MAX_DAY_NUMBER 3652424L

#define STRING_BUFFER_USUAL_SIZE 80

typedef void *range_seq_t;

enum ha_stat_type { HA_ENGINE_STATUS, HA_ENGINE_LOGS, HA_ENGINE_MUTEX };
// the following is for checking tables

#define HA_ADMIN_ALREADY_DONE	  1
#define HA_ADMIN_OK               0
#define HA_ADMIN_NOT_IMPLEMENTED -1
#define HA_ADMIN_FAILED		 -2
#define HA_ADMIN_CORRUPT         -3
#define HA_ADMIN_INTERNAL_ERROR  -4
#define HA_ADMIN_INVALID         -5
#define HA_ADMIN_REJECT          -6

/* bits in index_flags(index_number) for what you can do with index */
#define HA_READ_NEXT            1       /* TODO really use this flag */
#define HA_READ_PREV            2       /* supports ::index_prev */
#define HA_READ_ORDER           4       /* index_next/prev follow sort order */
#define HA_READ_RANGE           8       /* can find all records in a range */
#define HA_ONLY_WHOLE_INDEX	16	/* Can't use part key searches */
#define HA_KEYREAD_ONLY         64	/* Support HA_EXTRA_KEYREAD */
/*
  Index scan will not return records in rowid order. Not guaranteed to be
  set for unordered (e.g. HASH) indexes.
*/
#define HA_KEY_SCAN_NOT_ROR     128

/* operations for disable/enable indexes */
#define HA_KEY_SWITCH_NONUNIQ      0
#define HA_KEY_SWITCH_ALL          1
#define HA_KEY_SWITCH_NONUNIQ_SAVE 2
#define HA_KEY_SWITCH_ALL_SAVE     3

/*
  Parameters for open() (in register form->filestat)
  HA_GET_INFO does an implicit HA_ABORT_IF_LOCKED
*/

#define HA_OPEN_KEYFILE		1
#define HA_OPEN_RNDFILE		2
#define HA_GET_INDEX		4
#define HA_GET_INFO		8	/* do a ha_info() after open */
#define HA_READ_ONLY		16	/* File opened as readonly */
/* Try readonly if can't open with read and write */
#define HA_TRY_READ_ONLY	32
#define HA_WAIT_IF_LOCKED	64	/* Wait if locked on open */
#define HA_ABORT_IF_LOCKED	128	/* skip if locked on open.*/
#define HA_BLOCK_LOCK		256	/* unlock when reading some records */
#define HA_OPEN_TEMPORARY	512

/* Some key definitions */
#define HA_KEY_NULL_LENGTH	1
#define HA_KEY_BLOB_LENGTH	2

const uint32_t HA_MAX_REC_LENGTH = 65535;

/* Options of START TRANSACTION statement (and later of SET TRANSACTION stmt) */
enum start_transaction_option_t
{
  START_TRANS_NO_OPTIONS,
  START_TRANS_OPT_WITH_CONS_SNAPSHOT
};

/* Flags for method is_fatal_error */
#define HA_CHECK_DUP_KEY 1
#define HA_CHECK_DUP_UNIQUE 2
#define HA_CHECK_DUP (HA_CHECK_DUP_KEY + HA_CHECK_DUP_UNIQUE)


/* Bits in used_fields */
#define HA_CREATE_USED_AUTO             (1L << 0)
#define HA_CREATE_USED_CHARSET          (1L << 8)
#define HA_CREATE_USED_DEFAULT_CHARSET  (1L << 9)

/*
  The below two are not used (and not handled) in this milestone of this WL
  entry because there seems to be no use for them at this stage of
  implementation.
*/
#define HA_MRR_SINGLE_POINT 1
#define HA_MRR_FIXED_KEY  2

/*
  Indicates that RANGE_SEQ_IF::next(&range) doesn't need to fill in the
  'range' parameter.
*/
#define HA_MRR_NO_ASSOCIATION 4

/*
  The MRR user will provide ranges in key order, and MRR implementation
  must return rows in key order.
*/
#define HA_MRR_SORTED 8

/* MRR implementation doesn't have to retrieve full records */
#define HA_MRR_INDEX_ONLY 16

/*
  The passed memory buffer is of maximum possible size, the caller can't
  assume larger buffer.
*/
#define HA_MRR_LIMITS 32


/*
  Flag set <=> default MRR implementation is used
  (The choice is made by **_info[_const]() function which may set this
   flag. SQL layer remembers the flag value and then passes it to
   multi_read_range_init().
*/
#define HA_MRR_USE_DEFAULT_IMPL 64

typedef int myf;
#define MYF(v)		(static_cast<drizzled::myf>(v))

/*
   "Declared Type Collation"
   A combination of collation and its derivation.

  Flags for collation aggregation modes:
  MY_COLL_ALLOW_SUPERSET_CONV  - allow conversion to a superset
  MY_COLL_ALLOW_COERCIBLE_CONV - allow conversion of a coercible value
                                 (i.e. constant).
  MY_COLL_ALLOW_CONV           - allow any kind of conversion
                                 (combination of the above two)
  MY_COLL_DISALLOW_NONE        - don't allow return DERIVATION_NONE
                                 (e.g. when aggregating for comparison)
  MY_COLL_CMP_CONV             - combination of MY_COLL_ALLOW_CONV
                                 and MY_COLL_DISALLOW_NONE
*/

#define MY_COLL_ALLOW_SUPERSET_CONV   1
#define MY_COLL_ALLOW_COERCIBLE_CONV  2
#define MY_COLL_ALLOW_CONV            3
#define MY_COLL_DISALLOW_NONE         4
#define MY_COLL_CMP_CONV              7

inline static void clear_timestamp_auto_bits(timestamp_auto_set_type &_target_, 
                                             timestamp_auto_set_type _bits_)
{
  _target_= static_cast<timestamp_auto_set_type>(_target_ & ~_bits_);
}

/*
 * The following are for the interface with the .frm file
 */

#define FIELDFLAG_PACK_SHIFT    3
#define FIELDFLAG_MAX_DEC    31

#ifdef __cplusplus
// FIXME: T will just be drizzled::Field::utype, but that would
// require including field.h. Moving the function elsewhere might be a
// better idea. Leaving it for restructuring.
template <typename T> 
T MTYP_TYPENR(const T& type)
{
  return static_cast<T>(type & 127);
}
#else
#define MTYP_TYPENR(type) (type & 127)  /* Remove bits from type */
#endif

inline static uint32_t f_settype(const enum enum_field_types x)
{
  return (uint32_t(x) << FIELDFLAG_PACK_SHIFT);
}

#ifdef __cplusplus
template <class T> void set_if_bigger(T &a, const T &b)
{
  if (a < b)
    a=b;
}

template <class T> void set_if_smaller(T &a, const T &b)
{
  if (a > b)
    a=b;
}
#else
#ifdef __GNUC__
#define set_if_bigger(a,b) do {                 \
  const typeof(a) _a = (a);                     \
  const typeof(b) _b = (b);                     \
  (void) (&_a == &_b);                          \
  if ((a) < (b)) (a)=(b);                       \
  } while(0)
#define set_if_smaller(a,b) do {                \
  const typeof(a) _a = (a);                     \
  const typeof(b) _b = (b);                     \
  (void) (&_a == &_b);                          \
  if ((a) > (b)) (a)=(b);                       \
  } while(0)

#else
#define set_if_bigger(a,b)  do { if ((a) < (b)) (a)=(b); } while(0)
#define set_if_smaller(a,b) do { if ((a) > (b)) (a)=(b); } while(0)
#endif
#endif

template<class T> inline void safe_delete(T*& ptr)
{
  delete ptr;
  ptr = 0;
}

#define array_elements(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))


/* Some types that is different between systems */

#ifndef FN_LIBCHAR
#define FN_LIBCHAR  '/'
#define FN_ROOTDIR  "/"
#endif
#ifndef OS_FILE_LIMIT
#define OS_FILE_LIMIT  65535
#endif

/*
  How much overhead does malloc have. The code often allocates
  something like 1024-MALLOC_OVERHEAD bytes
*/
#define MALLOC_OVERHEAD 8

/* get memory in huncs */
static const uint32_t ONCE_ALLOC_INIT= 4096;
/* Typical record cash */
static const uint32_t RECORD_CACHE_SIZE= 64*1024;


/* Some things that this system doesn't have */

/* Some defines of functions for portability */

#ifndef uint64_t2double
#define uint64_t2double(A) ((double) (uint64_t) (A))
#endif

#ifndef int64_t2double
#define int64_t2double(A) ((double) (int64_t) (A))
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/* From limits.h instead */
#ifndef DBL_MIN
#define DBL_MIN    4.94065645841246544e-324
#endif
#ifndef DBL_MAX
#define DBL_MAX    1.79769313486231470e+308
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
#define MY_ALIGN(A,L)  (((A) + (L) - 1) & ~((L) - 1))
#define ALIGN_SIZE(A)  MY_ALIGN((A),sizeof(double))
#ifdef __cplusplus
#define ADD_TO_PTR(ptr,size,type) (type) (reinterpret_cast<const unsigned char*>(ptr)+size)
#else
 #define ADD_TO_PTR(ptr,size,type) (type) ((unsigned char*) (ptr)+size)
#endif

#define MY_DIV_UP(A, B) (((A) + (B) - 1) / (B))
#define MY_ALIGNED_BYTE_ARRAY(N, S, T) T N[MY_DIV_UP(S, sizeof(T))]

/* Typdefs for easyier portability */


#if defined(SIZEOF_OFF_T)
# if (SIZEOF_OFF_T == 8)
#  define OFF_T_MAX (INT64_MAX)
# else
#  define OFF_T_MAX (INT32_MAX)
# endif
#endif

#define MY_FILEPOS_ERROR  -1

#define DRIZZLE_SERVER

/* Length of decimal number represented by INT32. */
#define MY_INT32_NUM_DECIMAL_DIGITS 11

/* Length of decimal number represented by INT64. */
#define MY_INT64_NUM_DECIMAL_DIGITS 21

/*
  Io buffer size; Must be a power of 2 and
  a multiple of 512. May be
  smaller what the disk page size. This influences the speed of the
  isam btree library. eg to big to slow.
*/
#define IO_SIZE 4096
/* Max file name len */
#define FN_LEN 256
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

typedef uint64_t table_map;   /* Used for table bits in join */
typedef uint32_t nesting_map; /* Used for flags of nesting constructs */

} /* namespace drizzled */

