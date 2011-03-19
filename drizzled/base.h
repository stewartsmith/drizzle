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

/* This file includes constants used with all databases */

/**
 * @TODO Name this file something better and split it out if necessary.
 *
 * @TODO Convert HA_XXX defines into enums and/or bitmaps
 */

#include <drizzled/definitions.h>

#pragma once

namespace drizzled
{

/* The following is bits in the flag parameter to ha_open() */

#define HA_OPEN_ABORT_IF_LOCKED		0	/* default */
#define HA_OPEN_WAIT_IF_LOCKED		1
#define HA_OPEN_IGNORE_IF_LOCKED	2
#define HA_OPEN_TMP_TABLE		4	/* Table is a temp table */
/* Internal temp table, used for temporary results */
#define HA_OPEN_INTERNAL_TABLE          512

/* The following is parameter to ha_rkey() how to use key */

/*
  We define a complete-field prefix of a key value as a prefix where
  the last included field in the prefix contains the full field, not
  just some bytes from the start of the field. A partial-field prefix
  is allowed to contain only a few first bytes from the last included
  field.

  Below HA_READ_KEY_EXACT, ..., HA_READ_BEFORE_KEY can take a
  complete-field prefix of a key value as the search
  key. HA_READ_PREFIX and HA_READ_PREFIX_LAST could also take a
  partial-field prefix, but currently (4.0.10) they are only used with
  complete-field prefixes. MySQL uses a padding trick to implement
  LIKE 'abc%' queries.

  NOTE that in InnoDB HA_READ_PREFIX_LAST will NOT work with a
  partial-field prefix because InnoDB currently strips spaces from the
  end of varchar fields!
*/

enum ha_rkey_function {
  HA_READ_KEY_EXACT,              /* Find first record else error */
  HA_READ_KEY_OR_NEXT,            /* Record or next record */
  HA_READ_KEY_OR_PREV,            /* Record or previous */
  HA_READ_AFTER_KEY,              /* Find next rec. after key-record */
  HA_READ_BEFORE_KEY,             /* Find next rec. before key-record */
  HA_READ_PREFIX,                 /* Key which as same prefix */
  HA_READ_PREFIX_LAST,            /* Last key with the same prefix */
  HA_READ_PREFIX_LAST_OR_PREV,    /* Last or prev key with the same prefix */
  HA_READ_MBR_CONTAIN,
  HA_READ_MBR_INTERSECT,
  HA_READ_MBR_WITHIN,
  HA_READ_MBR_DISJOINT,
  HA_READ_MBR_EQUAL
};

	/* Key algorithm types */

enum ha_key_alg {
  HA_KEY_ALG_UNDEF=	0,		/* Not specified (old file) */
  HA_KEY_ALG_BTREE=	1,		/* B-tree, default one          */
  HA_KEY_ALG_HASH=	3		/* HASH keys (HEAP tables) */
};

	/* The following is parameter to ha_extra() */

enum ha_extra_function {
  HA_EXTRA_NORMAL=0,			/* Optimize for space (def) */
  HA_EXTRA_QUICK=1,			/* Optimize for speed */
  HA_EXTRA_NOT_USED=2,
  HA_EXTRA_CACHE=3,			/* Cache record in HA_rrnd() */
  HA_EXTRA_NO_CACHE=4,			/* End caching of records (def) */
  HA_EXTRA_NO_READCHECK=5,		/* No readcheck on update */
  HA_EXTRA_READCHECK=6,			/* Use readcheck (def) */
  HA_EXTRA_KEYREAD=7,			/* Read only key to database */
  HA_EXTRA_NO_KEYREAD=8,		/* Normal read of records (def) */
  HA_EXTRA_NO_USER_CHANGE=9,		/* No user is allowed to write */
  HA_EXTRA_KEY_CACHE=10,
  HA_EXTRA_NO_KEY_CACHE=11,
  HA_EXTRA_WAIT_LOCK=12,		/* Wait until file is avalably (def) */
  HA_EXTRA_NO_WAIT_LOCK=13,		/* If file is locked, return quickly */
  HA_EXTRA_WRITE_CACHE=14,		/* Use write cache in ha_write() */
  HA_EXTRA_FLUSH_CACHE=15,		/* flush write_record_cache */
  HA_EXTRA_NO_KEYS=16,			/* Remove all update of keys */
  HA_EXTRA_KEYREAD_CHANGE_POS=17,	/* Keyread, but change pos */
					/* xxxxchk -r must be used */
  HA_EXTRA_REMEMBER_POS=18,		/* Remember pos for next/prev */
  HA_EXTRA_RESTORE_POS=19,
  HA_EXTRA_REINIT_CACHE=20,		/* init cache from current record */
  HA_EXTRA_FORCE_REOPEN=21,		/* Datafile have changed on disk */
  HA_EXTRA_FLUSH,			/* Flush tables to disk */
  HA_EXTRA_NO_ROWS,			/* Don't write rows */
  HA_EXTRA_RESET_STATE,			/* Reset positions */
  HA_EXTRA_IGNORE_DUP_KEY,		/* Dup keys don't rollback everything*/
  HA_EXTRA_NO_IGNORE_DUP_KEY,
  HA_EXTRA_PREPARE_FOR_DROP,
  HA_EXTRA_PREPARE_FOR_UPDATE,		/* Remove read cache if problems */
  HA_EXTRA_PRELOAD_BUFFER_SIZE,         /* Set buffer size for preloading */
  /*
    On-the-fly switching between unique and non-unique key inserting.
  */
  HA_EXTRA_CHANGE_KEY_TO_UNIQUE,
  HA_EXTRA_CHANGE_KEY_TO_DUP,
  /*
    When using HA_EXTRA_KEYREAD, overwrite only key member fields and keep
    other fields intact. When this is off (by default) InnoDB will use memcpy
    to overwrite entire row.
  */
  HA_EXTRA_KEYREAD_PRESERVE_FIELDS,
  /*
    Ignore if the a tuple is not found, continue processing the
    transaction and ignore that 'row'.  Needed for idempotency
    handling on the slave

    Currently only used by NDB storage engine. Partition handler ignores flag.
  */
  HA_EXTRA_IGNORE_NO_KEY,
  HA_EXTRA_NO_IGNORE_NO_KEY,
  /*
    Informs handler that write_row() which tries to insert new row into the
    table and encounters some already existing row with same primary/unique
    key can replace old row with new row instead of reporting error (basically
    it informs handler that we do REPLACE instead of simple INSERT).
    Off by default.
  */
  HA_EXTRA_WRITE_CAN_REPLACE,
  HA_EXTRA_WRITE_CANNOT_REPLACE,
  /*
    Inform handler that delete_row()/update_row() cannot batch deletes/updates
    and should perform them immediately. This may be needed when table has
    AFTER DELETE/UPDATE triggers which access to subject table.
    These flags are reset by the handler::extra(HA_EXTRA_RESET) call.
  */
  HA_EXTRA_DELETE_CANNOT_BATCH,
  HA_EXTRA_UPDATE_CANNOT_BATCH,
  /*
    Inform handler that an "INSERT...ON DUPLICATE KEY UPDATE" will be
    executed. This condition is unset by HA_EXTRA_NO_IGNORE_DUP_KEY.
  */
  HA_EXTRA_INSERT_WITH_UPDATE,
  /* Inform handler that we will do a rename */
  HA_EXTRA_PREPARE_FOR_RENAME
};

	/* The following is parameter to ha_panic() */

enum ha_panic_function {
  HA_PANIC_CLOSE,			/* Close all databases */
  HA_PANIC_WRITE,			/* Unlock and write status */
  HA_PANIC_READ				/* Lock and read keyinfo */
};

	/* The following is parameter to ha_create(); keytypes */

enum ha_base_keytype {
  HA_KEYTYPE_END=0,
  HA_KEYTYPE_TEXT=1,			/* Key is sorted as letters */
  HA_KEYTYPE_BINARY=2,			/* Key is sorted as unsigned chars */
  HA_KEYTYPE_LONG_INT=4,
  HA_KEYTYPE_DOUBLE=6,
  HA_KEYTYPE_ULONG_INT=9,
  HA_KEYTYPE_LONGLONG=10,
  HA_KEYTYPE_ULONGLONG=11,
  /* Varchar (0-255 bytes) with length packed with 1 byte */
  HA_KEYTYPE_VARTEXT1=15,               /* Key is sorted as letters */
  HA_KEYTYPE_VARBINARY1=16,             /* Key is sorted as unsigned chars */
  /* Varchar (0-65535 bytes) with length packed with 2 bytes */
  HA_KEYTYPE_VARTEXT2=17,		/* Key is sorted as letters */
  HA_KEYTYPE_VARBINARY2=18		/* Key is sorted as unsigned chars */
};

	/* These flags kan be OR:ed to key-flag */

#define HA_NOSAME		 1	/* Set if not dupplicated records */
#define HA_PACK_KEY		 2	/* Pack string key to previous key */
#define HA_AUTO_KEY		 16
#define HA_BINARY_PACK_KEY	 32	/* Packing of all keys to prev key */
#define HA_UNIQUE_CHECK		256	/* Check the key for uniqueness */
#define HA_NULL_ARE_EQUAL	2048	/* NULL in key are cmp as equal */
#define HA_GENERATED_KEY	8192	/* Automaticly generated key */

#define HA_KEY_HAS_PART_KEY_SEG 65536   /* Key contains partial segments */

	/* Automatic bits in key-flag */

#define HA_SPACE_PACK_USED	 4	/* Test for if SPACE_PACK used */
#define HA_VAR_LENGTH_KEY	 8
#define HA_NULL_PART_KEY	 64
#define HA_USES_COMMENT          4096
#define HA_USES_BLOCK_SIZE	 ((uint32_t) 32768)
#define HA_SORT_ALLOWS_SAME      512    /* Intern bit when sorting records */

	/* These flags can be added to key-seg-flag */

#define HA_SPACE_PACK		 1	/* Pack space in key-seg */
#define HA_PART_KEY_SEG		 4	/* Used by MySQL for part-key-cols */
#define HA_VAR_LENGTH_PART	 8
#define HA_NULL_PART		 16
#define HA_BLOB_PART		 32
#define HA_SWAP_KEY		 64
#define HA_REVERSE_SORT		 128	/* Sort key in reverse order */
#define HA_NO_SORT               256 /* do not bother sorting on this keyseg */
/*
  End space in unique/varchar are considered equal. (Like 'a' and 'a ')
  Only needed for internal temporary tables.
*/
#define HA_END_SPACE_ARE_EQUAL	 512
#define HA_BIT_PART		1024

	/* optionbits for database */
#define HA_OPTION_PACK_RECORD		1
#define HA_OPTION_PACK_KEYS		2
#define HA_OPTION_COMPRESS_RECORD	4
#define HA_OPTION_TMP_TABLE		16
#define HA_OPTION_NO_PACK_KEYS		128  /* Reserved for MySQL */
#define HA_OPTION_TEMP_COMPRESS_RECORD	((uint32_t) 16384)	/* set by isamchk */
#define HA_OPTION_READ_ONLY_DATA	((uint32_t) 32768)	/* Set by isamchk */

	/* Bits in flag to create() */

#define HA_DONT_TOUCH_DATA	1	/* Don't empty datafile (isamchk) */
#define HA_PACK_RECORD		2	/* Request packed record format */
#define HA_CREATE_TMP_TABLE	4
#define HA_CREATE_KEEP_FILES	16      /* don't overwrite .MYD and MYI */

/*
  The following flags (OR-ed) are passed to handler::info() method.
  The method copies misc handler information out of the storage engine
  to data structures accessible from MySQL

  Same flags are also passed down to mi_status, myrg_status, etc.
*/

/* this one is not used */
#define HA_STATUS_POS            1
/*
  assuming the table keeps shared actual copy of the 'info' and
  local, possibly outdated copy, the following flag means that
  it should not try to get the actual data (locking the shared structure)
  slightly outdated version will suffice
*/
#define HA_STATUS_NO_LOCK        2
/* update the time of the last modification (in handler::update_time) */
#define HA_STATUS_TIME           4
/*
  update the 'constant' part of the info:
  handler::max_data_file_length, max_index_file_length, create_time
  sortkey, ref_length, block_size, data_file_name, index_file_name.
  handler::table->s->keys_in_use, keys_for_keyread, rec_per_key
*/
#define HA_STATUS_CONST          8
/*
  update the 'variable' part of the info:
  handler::records, deleted, data_file_length, index_file_length,
  delete_length, check_time, mean_rec_length
*/
#define HA_STATUS_VARIABLE      16
/*
  get the information about the key that caused last duplicate value error
  update handler::errkey and handler::dupp_ref
  see handler::get_dup_key()
*/
#define HA_STATUS_ERRKEY        32
/*
  update handler::auto_increment_value
*/
#define HA_STATUS_AUTO          64

/*
  Errorcodes given by handler functions

  optimizer::sum_query() assumes these codes are > 1
  Do not add error numbers before HA_ERR_FIRST.
  If necessary to add lower numbers, change HA_ERR_FIRST accordingly.
*/
#define HA_ERR_FIRST            120     /* Copy of first error nr.*/

/* Other constants */

typedef unsigned long key_part_map;
#define HA_WHOLE_KEY  (~(key_part_map)0)

	/* Intern constants in databases */

	/* bits in _search */
#define SEARCH_FIND	1
#define SEARCH_NO_FIND	2
#define SEARCH_SAME	4
#define SEARCH_BIGGER	8
#define SEARCH_SMALLER	16
#define SEARCH_SAVE_BUFF	32
#define SEARCH_UPDATE	64
#define SEARCH_PREFIX	128
#define SEARCH_LAST	256
#define MBR_CONTAIN     512
#define MBR_INTERSECT   1024
#define MBR_WITHIN      2048
#define MBR_DISJOINT    4096
#define MBR_EQUAL       8192
#define SEARCH_NULL_ARE_EQUAL 32768	/* NULL in keys are equal */
#define SEARCH_NULL_ARE_NOT_EQUAL 65536	/* NULL in keys are not equal */

	/* bits in opt_flag */
#define READ_CACHE_USED	2
#define READ_CHECK_USED 4
#define KEY_READ_USED	8
#define WRITE_CACHE_USED 16
#define OPT_NO_ROWS	32

	/* bits in update */
#define HA_STATE_CHANGED	1	/* Database has changed */
#define HA_STATE_AKTIV		2	/* Has a current record */
#define HA_STATE_WRITTEN	4	/* Record is written */
#define HA_STATE_DELETED	8
#define HA_STATE_NEXT_FOUND	16	/* Next found record (record before) */
#define HA_STATE_PREV_FOUND	32	/* Prev found record (record after) */
#define HA_STATE_KEY_CHANGED	128
#define HA_STATE_WRITE_AT_END	256	/* set in _ps_find_writepos */
#define HA_STATE_ROW_CHANGED	1024	/* To invalide ROW cache */
#define HA_STATE_EXTEND_BLOCK	2048
#define HA_STATE_RNEXT_SAME	4096	/* rnext_same occupied lastkey2 */

/* myisampack expects no more than 32 field types. */
enum en_fieldtype {
  FIELD_LAST=-1,FIELD_NORMAL,FIELD_SKIP_ENDSPACE,FIELD_SKIP_PRESPACE,
  FIELD_SKIP_ZERO,FIELD_BLOB,FIELD_CONSTANT,FIELD_INTERVALL,FIELD_ZERO,
  FIELD_VARCHAR,FIELD_CHECK,
  FIELD_enum_val_count
};

enum data_file_type {
  STATIC_RECORD, DYNAMIC_RECORD, COMPRESSED_RECORD, BLOCK_RECORD
};

/* For key ranges */

/* from -inf */
#define NO_MIN_RANGE	1

/* to +inf */
#define NO_MAX_RANGE	2

/*  X < key, i.e. not including the left endpoint */
#define NEAR_MIN	4

/* X > key, i.e. not including the right endpoint */
#define NEAR_MAX	8

/*
  This flag means that index is a unique index, and the interval is
  equivalent to "AND(keypart_i = const_i)", where all of const_i are not NULLs.
*/
#define UNIQUE_RANGE	16

/*
  This flag means that the interval is equivalent to
  "AND(keypart_i = const_i)", where not all key parts may be used but all of
  const_i are not NULLs.
*/
#define EQ_RANGE	32

/*
  This flag has the same meaning as UNIQUE_RANGE, except that for at least
  one keypart the condition is "keypart IS NULL".
*/
#define NULL_RANGE	64

class key_range
{
public:
  const unsigned char *key;
  uint32_t length;
  enum ha_rkey_function flag;
  key_part_map keypart_map;
};

class KEY_MULTI_RANGE
{
public:
  key_range start_key;
  key_range end_key;
  char  *ptr;                 /* Free to use by caller (ptr to row etc) */
  uint32_t  range_flag;           /* key range flags see above */
};


/* For number of records */
typedef uint64_t	ha_rows;

#define HA_POS_ERROR	(~ (::drizzled::ha_rows) 0)
#define HA_OFFSET_ERROR	(~ (::drizzled::internal::my_off_t) 0)

#if SIZEOF_OFF_T == 4
#define MAX_FILE_SIZE	INT32_MAX
#else
#define MAX_FILE_SIZE	INT64_MAX
#endif

inline static uint32_t ha_varchar_packlength(uint32_t field_length)
{
  return (field_length < 256 ? 1 :2);
}


} /* namespace drizzled */

