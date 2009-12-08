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

/**
 * @file
 *
 * Mostly constants and some macros/functions used by the server
 */

#ifndef DRIZZLED_DEFINITIONS_H
#define DRIZZLED_DEFINITIONS_H

#include <drizzled/enum.h>

#include <stdint.h>

/* These paths are converted to other systems (WIN95) before use */

#define LANGUAGE	"english/"
#define TEMP_PREFIX	"MY"
#define LOG_PREFIX	"ML"
#define PROGDIR		"bin/"

#define ER(X) error_message((X))

#define LIBLEN FN_REFLEN-FN_LEN			/* Max l{ngd p} dev */
/* extra 4+4 bytes for slave tmp tables */
#define MAX_DBKEY_LENGTH (NAME_LEN*2+1+1+4+4)
#define MAX_ALIAS_NAME 256
#define MAX_FIELD_NAME 34			/* Max colum name length +2 */
#define MAX_SYS_VAR_LENGTH 32
#define MAX_KEY MAX_INDEXES                     /* Max used keys */
#define MAX_REF_PARTS 16			/* Max parts used as ref */
#define MAX_KEY_LENGTH 4096			/* max possible key */
#define MAX_KEY_LENGTH_DECIMAL_WIDTH 4          /* strlen("4096") */
#if SIZEOF_OFF_T > 4
#define MAX_REFLENGTH 8				/* Max length for record ref */
#else
#define MAX_REFLENGTH 4				/* Max length for record ref */
#endif
#define MAX_HOSTNAME  61			/* len+1 in mysql.user */

#define MAX_MBWIDTH		4		/* Max multibyte sequence */
#define MAX_FIELD_CHARLENGTH	255
#define MAX_FIELD_VARCHARLENGTH	65535
#define CONVERT_IF_BIGGER_TO_BLOB 512		/* Used for CREATE ... SELECT */

/* Max column width +1 */
#define MAX_FIELD_WIDTH		(MAX_FIELD_CHARLENGTH*MAX_MBWIDTH+1)

#define MAX_DATETIME_COMPRESSED_WIDTH 14  /* YYYYMMDDHHMMSS */

#define MAX_TABLES	(sizeof(table_map)*8-3)	/* Max tables in join */
#define PARAM_TABLE_BIT	(((table_map) 1) << (sizeof(table_map)*8-3))
#define OUTER_REF_TABLE_BIT	(((table_map) 1) << (sizeof(table_map)*8-2))
#define RAND_TABLE_BIT	(((table_map) 1) << (sizeof(table_map)*8-1))
#define PSEUDO_TABLE_BITS (PARAM_TABLE_BIT | OUTER_REF_TABLE_BIT | \
                           RAND_TABLE_BIT)
#define MAX_FIELDS	4096			/* Limit in the .frm file */

#define MAX_SELECT_NESTING (sizeof(nesting_map)*8-1)

#define MAX_SORT_MEMORY (2048*1024-MALLOC_OVERHEAD)
#define MIN_SORT_MEMORY (32*1024-MALLOC_OVERHEAD)

/* Memory allocated when parsing a statement / saving a statement */
#define MEM_ROOT_BLOCK_SIZE       8192
#define MEM_ROOT_PREALLOC         8192

#define DEFAULT_ERROR_COUNT	64
#define EXTRA_RECORDS	10			/* Extra records in sort */
#define SCROLL_EXTRA	5			/* Extra scroll-rows. */
#define FIELD_NAME_USED ((uint32_t) 32768)		/* Bit set if fieldname used */
#define FIELD_NR_MASK	16383			/* To get fieldnumber */
#define FERR		-1			/* Error from my_functions */
#define CREATE_MODE	0			/* Default mode on new files */
#define NAMES_SEP_CHAR	'\377'			/* Char to sep. names */

#define READ_RECORD_BUFFER	(uint32_t) (IO_SIZE*8) /* Pointer_buffer_size */
#define DISK_BUFFER_SIZE	(uint32_t) (IO_SIZE*16) /* Size of diskbuffer */

#define ME_INFO (ME_HOLDTANG+ME_OLDWIN+ME_NOREFRESH)
#define ME_ERROR (ME_BELL+ME_OLDWIN+ME_NOREFRESH)
#define MYF_RW MYF(MY_WME+MY_NABP)		/* Vid my_read & my_write */

	/* Defines for use with openfrm, openprt and openfrd */

#define READ_ALL		1	/* openfrm: Read all parameters */
#define EXTRA_RECORD		8	/* Reservera plats f|r extra record */
#define DONT_GIVE_ERROR		256	/* Don't do frm_error on openfrm  */
#define DELAYED_OPEN		4096	/* Open table later */
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine
  The flag means that we need to open FRM file only to get necessary data.
*/
#define OPEN_FRM_FILE_ONLY     32768
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine
  The flag means that we need to process tables only to get necessary data.
  Views are not processed.
*/
#define OPEN_TABLE_ONLY        OPEN_FRM_FILE_ONLY*2
/**
  This flag is used in function get_all_tables() which fills
  I_S tables with data which are retrieved from frm files and storage engine.
  The flag means that I_S table uses optimization algorithm.
*/
#define OPTIMIZE_I_S_TABLE     OPEN_TABLE_ONLY*2

/*
  Minimum length pattern before Turbo Boyer-Moore is used
  for SELECT "text" LIKE "%pattern%", excluding the two
  wildcards in class Item_func_like.
*/
#define MIN_TURBOBM_PATTERN_LEN 3

/*
   Defines for binary logging.
   Do not decrease the value of BIN_LOG_HEADER_SIZE.
   Do not even increase it before checking code.
*/

#define BIN_LOG_HEADER_SIZE    4

#define COLUMN_FORMAT_MASK 7
#define COLUMN_FORMAT_SHIFT 3

/* Below are #defines that used to be in mysql_priv.h */
/***************************************************************************
  Configuration parameters
****************************************************************************/
#define MAX_ACCEPT_RETRY	10	// Test accept this many times
#define MAX_FIELDS_BEFORE_HASH	32
#define USER_VARS_HASH_SIZE     16
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

#define TEMP_POOL_SIZE          128

#define QUERY_ALLOC_BLOCK_SIZE		8192
#define QUERY_ALLOC_PREALLOC_SIZE   	8192
#define TRANS_ALLOC_BLOCK_SIZE		4096
#define TRANS_ALLOC_PREALLOC_SIZE	4096
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
#define MIN_ROWS_TO_USE_BULK_INSERT	 100

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
/* Characters shown for the command in 'information_schema.processlist' */
#define PROCESS_LIST_INFO_WIDTH 65535

#define PRECISION_FOR_DOUBLE 53
#define PRECISION_FOR_FLOAT  24

/* The following can also be changed from the command line */
#define DEFAULT_CONCURRENCY	10
#define FLUSH_TIME		0		/**< Don't flush tables */
#define MAX_CONNECT_ERRORS	10		///< errors before disabling host

#define INTERRUPT_PRIOR 10
#define CONNECT_PRIOR	9
#define WAIT_PRIOR	8
#define QUERY_PRIOR	6

/* Bits from testflag */
enum test_flag_bit
{
  TEST_PRINT_CACHED_TABLES= 1,
  TEST_NO_KEY_GROUP,
  TEST_MIT_THREAD,
  TEST_KEEP_TMP_TABLES,
  TEST_READCHECK, /**< Force use of readcheck */
  TEST_NO_EXTRA,
  TEST_CORE_ON_SIGNAL, /**< Give core if signal */
  TEST_NO_STACKTRACE,
  TEST_SIGINT, /**< Allow sigint on threads */
  TEST_SYNCHRONIZATION /**< get server to do sleep in some places */
};

/* Bits for different SQL modes modes (including ANSI mode) */
#define MODE_NO_ZERO_DATE		(2)
#define MODE_INVALID_DATES		(MODE_NO_ZERO_DATE*2)

/* @@optimizer_switch flags */
#define OPTIMIZER_SWITCH_NO_MATERIALIZATION 1
#define OPTIMIZER_SWITCH_NO_SEMIJOIN 2

#define MY_CHARSET_BIN_MB_MAXLEN 1

// uncachable cause
#define UNCACHEABLE_DEPENDENT   1
#define UNCACHEABLE_RAND        2
#define UNCACHEABLE_SIDEEFFECT	4
/// forcing to save JOIN for explain
#define UNCACHEABLE_EXPLAIN     8
/** Don't evaluate subqueries in prepare even if they're not correlated */
#define UNCACHEABLE_PREPARE    16
/* For uncorrelated SELECT in an UNION with some correlated SELECTs */
#define UNCACHEABLE_UNITED     32

/* Used to check GROUP BY list in the MODE_ONLY_FULL_GROUP_BY mode */
#define UNDEF_POS (-1)

/* sql_show.cc:show_log_files() */
#define SHOW_LOG_STATUS_FREE "FREE"
#define SHOW_LOG_STATUS_INUSE "IN USE"

/* Options to add_table_to_list() */
#define TL_OPTION_UPDATING	1
#define TL_OPTION_FORCE_INDEX	2
#define TL_OPTION_IGNORE_LEAVES 4
#define TL_OPTION_ALIAS         8

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

/*
  Some defines for exit codes for ::is_equal class functions.
*/
#define IS_EQUAL_NO 0
#define IS_EQUAL_YES 1
#define IS_EQUAL_PACK_LENGTH 2


typedef uint64_t query_id_t;
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

#define HA_NOT_DELETE_WITH_CACHE (1 << 18)

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
#define HA_DO_INDEX_COND_PUSHDOWN  256 /* Supports Index Condition Pushdown */

/* operations for disable/enable indexes */
#define HA_KEY_SWITCH_NONUNIQ      0
#define HA_KEY_SWITCH_ALL          1
#define HA_KEY_SWITCH_NONUNIQ_SAVE 2
#define HA_KEY_SWITCH_ALL_SAVE     3

/*
  Note: the following includes binlog and closing 0.
  so: innodb + bdb + ndb + binlog + myisam + myisammrg + archive +
      example + csv + heap + blackhole + federated + 0
  (yes, the sum is deliberately inaccurate)
  TODO remove the limit, use dynarrays
*/
#define MAX_HA 15

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

/* For transactional LOCK Table. handler::lock_table() */
#define HA_LOCK_IN_SHARE_MODE      F_RDLCK
#define HA_LOCK_IN_EXCLUSIVE_MODE  F_WRLCK

/* Some key definitions */
#define HA_KEY_NULL_LENGTH	1
#define HA_KEY_BLOB_LENGTH	2

#define HA_MAX_REC_LENGTH	65535

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
#define HA_CREATE_USED_ROW_FORMAT       (1L << 15)
#define HA_CREATE_USED_KEY_BLOCK_SIZE   (1L << 19)
#define HA_CREATE_USED_BLOCK_SIZE       (1L << 22)

#define MAXGTRIDSIZE 64
#define MAXBQUALSIZE 64

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
#define MYF(v)		(myf) (v)

#define MY_I_S_MAYBE_NULL 1
#define MY_I_S_UNSIGNED   2


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
#define clear_timestamp_auto_bits(_target_, _bits_) \
  (_target_)= (enum timestamp_auto_set_type)((int)(_target_) & ~(int)(_bits_))

/*
 * The following are for the interface with the .frm file
 */

#define FIELDFLAG_PACK_SHIFT    3
#define FIELDFLAG_MAX_DEC    31

#define MTYP_TYPENR(type) (type & 127)  /* Remove bits from type */

#define f_packtype(x)   (((x) >> FIELDFLAG_PACK_SHIFT) & 15)
#define f_settype(x)    (((int) x) << FIELDFLAG_PACK_SHIFT)

#endif /* DRIZZLED_DEFINITIONS_H */
