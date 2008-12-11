/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/*  Extra functions used by unireg library */

#ifndef DRIZZLED_UNIREG_H
#define DRIZZLED_UNIREG_H

#include <drizzled/structs.h>				/* All structs we need */

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef NO_ALARM_LOOP
#define NO_ALARM_LOOP		/* lib5 and popen can't use alarm */
#endif


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
#define FIELD_NR_MASK	16383			/* To get fieldnumber */
#define FERR		-1			/* Error from my_functions */
#define CREATE_MODE	0			/* Default mode on new files */
#define NAMES_SEP_CHAR	'\377'			/* Char to sep. names */

#define READ_RECORD_BUFFER	(uint) (IO_SIZE*8) /* Pointer_buffer_size */
#define DISK_BUFFER_SIZE	(uint) (IO_SIZE*16) /* Size of diskbuffer */

#define ME_INFO (ME_HOLDTANG+ME_OLDWIN+ME_NOREFRESH)
#define ME_ERROR (ME_BELL+ME_OLDWIN+ME_NOREFRESH)
#define MYF_RW MYF(MY_WME+MY_NABP)		/* Vid my_read & my_write */

	/* Extern defines */
#define store_record(A,B) memcpy((A)->B,(A)->record[0],(size_t) (A)->s->reclength)
#define restore_record(A,B) memcpy((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define cmp_record(A,B) memcmp((A)->record[0],(A)->B,(size_t) (A)->s->reclength)
#define empty_record(A)                                 \
  do {                                                  \
    restore_record((A),s->default_values);              \
    memset((A)->null_flags, 255, (A)->s->null_bytes);   \
  } while (0)

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

#define SC_INFO_LENGTH 4		/* Form format constant */
#define TE_INFO_LENGTH 3
#define MTYP_NOEMPTY_BIT 128


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

void unireg_init();
void unireg_end(void) __attribute__((noreturn));
void unireg_abort(int exit_code) __attribute__((noreturn));

bool mysql_create_frm(Session *session, const char *file_name,
                      const char *db, const char *table,
                      HA_CREATE_INFO *create_info,
                      List<Create_field> &create_field,
                      uint32_t key_count,KEY *key_info,handler *db_type);
int rea_create_table(Session *session, const char *path,
                     const char *db, const char *table_name,
                     HA_CREATE_INFO *create_info,
                     List<Create_field> &create_field,
                     uint32_t key_count,KEY *key_info,
                     handler *file);


#if defined(__cplusplus)
}
#endif


#endif
