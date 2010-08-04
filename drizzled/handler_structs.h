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

#include <drizzled/base.h>
#include <drizzled/structs.h>
#include <drizzled/definitions.h>
#include <drizzled/lex_string.h>
#include "drizzled/global_charset_info.h"

namespace drizzled
{

typedef struct st_key_cache KEY_CACHE;


namespace plugin
{
class StorageEngine;
}

typedef struct st_ha_create_information
{
  const CHARSET_INFO *table_charset, *default_table_charset;
  const char *alias;
  uint64_t auto_increment_value;
  uint32_t table_options;
  uint32_t used_fields;
  plugin::StorageEngine *db_type;
  bool table_existed;			/* 1 in create if table existed */
} HA_CREATE_INFO;

typedef struct st_ha_alter_information
{
  KeyInfo  *key_info_buffer;
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
  LEX_STRING comment;
} KEY_CREATE_INFO;


typedef struct st_ha_check_opt
{
  st_ha_check_opt() {}                        /* Remove gcc warning */
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

} /* namespace drizzled */

#endif /* DRIZZLED_HANDLER_STRUCTS_H */
