/* Copyright (C) 2000-2006 MySQL AB

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


/**
 * @file
 *
 * Mostly constants and some macros/functions used by the server  
 */

#ifndef DRIZZLE_SERVER_DEFINITIONS_H
#define DRIZZLE_SERVER_DEFINITIONS_H

#ifndef NO_ALARM_LOOP
#define NO_ALARM_LOOP		/* lib5 and popen can't use alarm */
#endif

/* These paths are converted to other systems (WIN95) before use */

#define LANGUAGE	"english/"
#define ERRMSG_FILE	"errmsg.sys"
#define TEMP_PREFIX	"MY"
#define LOG_PREFIX	"ML"
#define PROGDIR		"bin/"
#ifndef DATADIR
#define DATADIR		"data/"
#endif
#ifndef SHAREDIR
#define SHAREDIR	"share/"
#endif
#ifndef PLUGINDIR
#define PLUGINDIR	"lib/plugin"
#endif

#define ER(X) _(drizzled_error_messages[(X) - ER_ERROR_FIRST])
#define ER_SAFE(X) (((X) >= ER_ERROR_FIRST && (X) <= ER_ERROR_LAST) ? ER(X) : _("Invalid error code"))

#define ERRMAPP 1				/* Errormap f|r my_error */
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

#define MAX_BIT_FIELD_LENGTH    64      /* Max length in bits for bit fields */

#define MAX_DATE_WIDTH		10	/* YYYY-MM-DD */
#define MAX_TIME_WIDTH		23	/* -DDDDDD HH:MM:SS.###### */
#define MAX_DATETIME_FULL_WIDTH 29	/* YYYY-MM-DD HH:MM:SS.###### AM */
#define MAX_DATETIME_WIDTH	19	/* YYYY-MM-DD HH:MM:SS */
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
#define TRANS_MEM_ROOT_BLOCK_SIZE 4096
#define TRANS_MEM_ROOT_PREALLOC   4096

#define DEFAULT_ERROR_COUNT	64
#define EXTRA_RECORDS	10			/* Extra records in sort */
#define SCROLL_EXTRA	5			/* Extra scroll-rows. */
#define FIELD_NAME_USED ((uint) 32768)		/* Bit set if fieldname used */
#define FORM_NAME_USED	((uint) 16384)		/* Bit set if formname used */
#define FIELD_NR_MASK	16383			/* To get fieldnumber */
#define FERR		-1			/* Error from my_functions */
#define CREATE_MODE	0			/* Default mode on new files */
#define NAMES_SEP_CHAR	'\377'			/* Char to sep. names */

#define READ_RECORD_BUFFER	(uint) (IO_SIZE*8) /* Pointer_buffer_size */
#define DISK_BUFFER_SIZE	(uint) (IO_SIZE*16) /* Size of diskbuffer */

#define ME_INFO (ME_HOLDTANG+ME_OLDWIN+ME_NOREFRESH)
#define ME_ERROR (ME_BELL+ME_OLDWIN+ME_NOREFRESH)
#define MYF_RW MYF(MY_WME+MY_NABP)		/* Vid my_read & my_write */

#define SPECIAL_USE_LOCKS	1		/* Lock used databases */
#define SPECIAL_NO_NEW_FUNC	2		/* Skip new functions */
#define SPECIAL_SKIP_SHOW_DB    4               /* Don't allow 'show db' */
#define SPECIAL_WAIT_IF_LOCKED	8		/* Wait if locked database */
#define SPECIAL_SAME_DB_NAME   16		/* form name = file name */
#define SPECIAL_ENGLISH        32		/* English error messages */
#define SPECIAL_NO_RESOLVE     64		/* Don't use gethostname */
#define SPECIAL_NO_PRIOR	128		/* Don't prioritize threads */
#define SPECIAL_BIG_SELECTS	256		/* Don't use heap tables */
#define SPECIAL_NO_HOST_CACHE	512		/* Don't cache hosts */
#define SPECIAL_SHORT_LOG_FORMAT 1024
#define SPECIAL_SAFE_MODE	2048
#define SPECIAL_LOG_QUERIES_NOT_USING_INDEXES 4096 /* Obsolete */

	/* Extern defines */
#define store_record(A,B) memcpy((A)->B,(A)->record[0],(size_t) (A)->s->reclength)
#define restore_record(A,B) memcpy((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define cmp_record(A,B) memcmp((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define empty_record(A)                                 \
  do {                                                  \
    restore_record((A),s->default_values);              \
    memset((A)->null_flags, 255, (A)->s->null_bytes);   \
  } while (0)

	/* Defines for use with openfrm, openprt and openfrd */

#define READ_ALL		1	/* openfrm: Read all parameters */
#define CHANGE_FRM		2	/* openfrm: open .frm as O_RDWR */
#define READ_KEYINFO		4	/* L{s nyckeldata fr}n filen */
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

#define SC_INFO_LENGTH 4		/* Form format constant */
#define TE_INFO_LENGTH 3
#define MTYP_NOEMPTY_BIT 128

#define FRM_VER_TRUE_VARCHAR (FRM_VER+4) /* 10 */
#define DRIZZLE_VERSION_TABLESPACE_IN_FRM_CGE 50120
#define DRIZZLE_VERSION_TABLESPACE_IN_FRM 50205
#define DRIZZLE_VERSION_TABLESPACE_IN_FRM_STR "50205"

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

#define DEFAULT_KEY_CACHE_NAME "default"

#define STORAGE_TYPE_MASK 7
#define COLUMN_FORMAT_MASK 7
#define COLUMN_FORMAT_SHIFT 3

/* Below are #defines that used to be in mysql_priv.h */
/***************************************************************************
  Configuration parameters
****************************************************************************/
#define ACL_CACHE_SIZE		256
#define MAX_PASSWORD_LENGTH	32
#define HOST_CACHE_SIZE		128
#define MAX_ACCEPT_RETRY	10	// Test accept this many times
#define MAX_FIELDS_BEFORE_HASH	32
#define USER_VARS_HASH_SIZE     16
#define TABLE_OPEN_CACHE_MIN    64
#define TABLE_OPEN_CACHE_DEFAULT 64

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

/** 
 * @TODO Move into a drizzled.h since it's only used in drizzled.cc
 *
 * @TODO Rename to DRIZZLED_NET_RETRY_COUNT
 */
#ifndef MYSQLD_NET_RETRY_COUNT
#define MYSQLD_NET_RETRY_COUNT  10	///< Abort read after this many int.
#endif
#define TEMP_POOL_SIZE          128

#define QUERY_ALLOC_BLOCK_SIZE		8192
#define QUERY_ALLOC_PREALLOC_SIZE   	8192
#define TRANS_ALLOC_BLOCK_SIZE		4096
#define TRANS_ALLOC_PREALLOC_SIZE	4096
#define RANGE_ALLOC_BLOCK_SIZE		4096
#define ACL_ALLOC_BLOCK_SIZE		1024
#define UDF_ALLOC_BLOCK_SIZE		1024
#define TABLE_ALLOC_BLOCK_SIZE		1024
#define BDB_LOG_ALLOC_BLOCK_SIZE	1024
#define WARN_ALLOC_BLOCK_SIZE		2048
#define WARN_ALLOC_PREALLOC_SIZE	1024
#define PROFILE_ALLOC_BLOCK_SIZE  2048
#define PROFILE_ALLOC_PREALLOC_SIZE 1024

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

/*
  Default time to wait before aborting a new client connection
  that does not respond to "initial server greeting" timely
*/
#define CONNECT_TIMEOUT		10

/* The following can also be changed from the command line */
#define DEFAULT_CONCURRENCY	10
#define FLUSH_TIME		0		/**< Don't flush tables */
#define MAX_CONNECT_ERRORS	10		///< errors before disabling host

#define INTERRUPT_PRIOR 10
#define CONNECT_PRIOR	9
#define WAIT_PRIOR	8
#define QUERY_PRIOR	6

	/* Bits from testflag */
#define TEST_PRINT_CACHED_TABLES 1
#define TEST_NO_KEY_GROUP	 2
#define TEST_MIT_THREAD		4
#define TEST_BLOCKING		8
#define TEST_KEEP_TMP_TABLES	16
#define TEST_READCHECK		64	/**< Force use of readcheck */
#define TEST_NO_EXTRA		128
#define TEST_CORE_ON_SIGNAL	256	/**< Give core if signal */
#define TEST_NO_STACKTRACE	512
#define TEST_SIGINT		1024	/**< Allow sigint on threads */
#define TEST_SYNCHRONIZATION    2048    /**< get server to do sleep in some places */
#endif /* End ifndef DRIZZLE_CLIENT */

/* The rest of the file is included in the server only */
#ifndef DRIZZLE_CLIENT

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

/* BINLOG_DUMP options */

#define BINLOG_DUMP_NON_BLOCK   1

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

#define tmp_file_prefix "#sql"			/**< Prefix for tmp tables */
#define tmp_file_prefix_length 4

/* Flags for calc_week() function.  */
#define WEEK_MONDAY_FIRST    1
#define WEEK_YEAR            2
#define WEEK_FIRST_WEEKDAY   4

#define STRING_BUFFER_USUAL_SIZE 80

/*
  Some defines for exit codes for ::is_equal class functions.
*/
#define IS_EQUAL_NO 0
#define IS_EQUAL_YES 1
#define IS_EQUAL_PACK_LENGTH 2


#endif /* DRIZZLE_SERVER_DEFINITIONS_H */
