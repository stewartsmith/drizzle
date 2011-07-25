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

#pragma once

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
#include <drizzled/definitions.h>
#include <drizzled/lex_string.h>
#include <drizzled/structs.h>

namespace drizzled {

typedef struct st_ha_create_information
{
  const charset_info_st *table_charset, *default_table_charset;
  const char *alias;
  uint64_t auto_increment_value;
  uint32_t table_options;
  uint32_t used_fields;
  plugin::StorageEngine *db_type;
  bool table_existed;			/* 1 in create if table existed */

  st_ha_create_information() :
    table_charset(0),
    default_table_charset(0),
    alias(0),
    auto_increment_value(0),
    table_options(0),
    used_fields(0),
    db_type(0),
    table_existed(0)
  { }
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

  st_ha_alter_information() :
    key_info_buffer(0),
    key_count(0),
    index_drop_count(0),
    index_drop_buffer(0),
    index_add_count(0),
    index_add_buffer(0),
    data(0)
  { }

} HA_ALTER_INFO;


typedef struct key_create_information_st
{
  ha_key_alg algorithm;
  uint32_t block_size;
  LEX_STRING comment;
} KEY_CREATE_INFO;


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

} /* namespace drizzled */

