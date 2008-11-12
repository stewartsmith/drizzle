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

#ifndef DRIZZLED_HANDLER_STRUCTS_H
#define DRIZZLED_HANDLER_STRUCTS_H

#include <stdint.h>
#include <time.h>

#include <drizzled/base.h>
#include <drizzled/structs.h>
#include <drizzled/definitions.h>
#include <drizzled/lex_string.h>

class Ha_trx_info;
struct handlerton;
struct st_key;
typedef struct st_key KEY;
struct st_key_cache;
typedef struct st_key_cache KEY_CACHE;

struct Session_TRANS
{
  /* true is not all entries in the ht[] support 2pc */
  bool        no_2pc;
  /* storage engines that registered in this transaction */
  Ha_trx_info *ha_list;
  /*
    The purpose of this flag is to keep track of non-transactional
    tables that were modified in scope of:
    - transaction, when the variable is a member of
    Session::transaction.all
    - top-level statement or sub-statement, when the variable is a
    member of Session::transaction.stmt
    This member has the following life cycle:
    * stmt.modified_non_trans_table is used to keep track of
    modified non-transactional tables of top-level statements. At
    the end of the previous statement and at the beginning of the session,
    it is reset to false.  If such functions
    as mysql_insert, mysql_update, mysql_delete etc modify a
    non-transactional table, they set this flag to true.  At the
    end of the statement, the value of stmt.modified_non_trans_table
    is merged with all.modified_non_trans_table and gets reset.
    * all.modified_non_trans_table is reset at the end of transaction

    * Since we do not have a dedicated context for execution of a
    sub-statement, to keep track of non-transactional changes in a
    sub-statement, we re-use stmt.modified_non_trans_table.
    At entrance into a sub-statement, a copy of the value of
    stmt.modified_non_trans_table (containing the changes of the
    outer statement) is saved on stack. Then
    stmt.modified_non_trans_table is reset to false and the
    substatement is executed. Then the new value is merged with the
    saved value.
  */
  bool modified_non_trans_table;

  void reset() { no_2pc= false; modified_non_trans_table= false; }
};

typedef struct {
  uint64_t data_file_length;
  uint64_t max_data_file_length;
  uint64_t index_file_length;
  uint64_t delete_length;
  ha_rows records;
  uint32_t mean_rec_length;
  time_t create_time;
  time_t check_time;
  time_t update_time;
  uint64_t check_sum;
} PARTITION_INFO;

typedef struct st_ha_create_information
{
  const CHARSET_INFO *table_charset, *default_table_charset;
  LEX_STRING connect_string;
  LEX_STRING comment;
  const char *data_file_name, *index_file_name;
  const char *alias;
  uint64_t max_rows,min_rows;
  uint64_t auto_increment_value;
  uint32_t table_options;
  uint32_t avg_row_length;
  uint32_t used_fields;
  uint32_t key_block_size;
  uint32_t block_size;
  handlerton *db_type;
  enum row_type row_type;
  uint32_t null_bits;                       /* NULL bits at start of record */
  uint32_t options;                         /* OR of HA_CREATE_ options */
  uint32_t extra_size;                      /* length of extra data segment */
  bool table_existed;			/* 1 in create if table existed */
  bool frm_only;                        /* 1 if no ha_create_table() */
  bool varchar;                         /* 1 if table has a VARCHAR */
  enum ha_choice page_checksum;         /* If we have page_checksums */
} HA_CREATE_INFO;

typedef struct st_ha_alter_information
{
  KEY  *key_info_buffer;
  uint32_t key_count;
  uint32_t index_drop_count;
  uint32_t *index_drop_buffer;
  uint32_t index_add_count;
  uint32_t *index_add_buffer;
  void *data;
} HA_ALTER_INFO;


typedef struct st_key_create_information
{
  enum ha_key_alg algorithm;
  uint32_t block_size;
  LEX_STRING parser_name;
  LEX_STRING comment;
} KEY_CREATE_INFO;


typedef struct st_ha_check_opt
{
  st_ha_check_opt() {}                        /* Remove gcc warning */
  uint32_t sort_buffer_size;
  uint32_t flags;       /* isam layer flags (e.g. for myisamchk) */
  /* sql layer flags - for something myisamchk cannot do */
  uint32_t sql_flags;
  /* new key cache when changing key cache */
  KEY_CACHE *key_cache;
  void init();
} HA_CHECK_OPT;


typedef struct st_range_seq_if
{
  /*
    Initialize the traversal of range sequence

    SYNOPSIS
    init()
    init_params  The seq_init_param parameter
    n_ranges     The number of ranges obtained
    flags        A combination of HA_MRR_SINGLE_POINT, HA_MRR_FIXED_KEY

    RETURN
    An opaque value to be used as RANGE_SEQ_IF::next() parameter
  */
  range_seq_t (*init)(void *init_params, uint32_t n_ranges, uint32_t flags);


  /*
    Get the next range in the range sequence

    SYNOPSIS
    next()
    seq    The value returned by RANGE_SEQ_IF::init()
    range  OUT Information about the next range

    RETURN
    0 - Ok, the range structure filled with info about the next range
    1 - No more ranges
  */
  uint32_t (*next) (range_seq_t seq, KEY_MULTI_RANGE *range);
} RANGE_SEQ_IF;

/*
  This is a buffer area that the handler can use to store rows.
  'end_of_used_area' should be kept updated after calls to
  read-functions so that other parts of the code can use the
  remaining area (until next read calls is issued).
*/

typedef struct st_handler_buffer
{
  unsigned char *buffer;         /* Buffer one can start using */
  unsigned char *buffer_end;     /* End of buffer */
  unsigned char *end_of_used_area;     /* End of area that was used by handler */
} HANDLER_BUFFER;

#endif /* DRIZZLED_HANDLER_STRUCTS_H */
