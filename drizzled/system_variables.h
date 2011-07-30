/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

namespace drizzled {

struct drizzle_system_variables
{
  /*
    How dynamically allocated system variables are handled:

    The global_system_variables and max_system_variables are "authoritative"
    They both should have the same 'version' and 'size'.
    When attempting to access a dynamic variable, if the session version
    is out of date, then the session version is updated and realloced if
    neccessary and bytes copied from global to make up for missing data.
  */
  ulong dynamic_variables_version;
  char * dynamic_variables_ptr;
  uint32_t dynamic_variables_head;  /* largest valid variable offset */
  uint32_t dynamic_variables_size;  /* how many bytes are in use */

  uint64_t myisam_max_extra_sort_file_size;
  uint64_t max_heap_table_size;
  uint64_t tmp_table_size;
  ha_rows select_limit;
  ha_rows max_join_size;
  uint64_t auto_increment_increment;
  uint64_t auto_increment_offset;
  uint64_t bulk_insert_buff_size;
  uint64_t join_buff_size;
  uint32_t max_allowed_packet;
  uint64_t max_error_count;
  uint64_t max_length_for_sort_data;
  size_t max_sort_length;
  uint64_t min_examined_row_limit;
  bool optimizer_prune_level;
  bool log_warnings;

  uint32_t optimizer_search_depth;
  uint32_t div_precincrement;
  uint64_t preload_buff_size;
  uint32_t read_buff_size;
  uint32_t read_rnd_buff_size;
  bool replicate_query;
  size_t sortbuff_size;
  uint32_t thread_handling;
  uint32_t tx_isolation;
  uint32_t completion_type;
  /* Determines which non-standard SQL behaviour should be enabled */
  uint32_t sql_mode;
  uint64_t max_seeks_for_key;
  size_t range_alloc_block_size;
  uint32_t query_alloc_block_size;
  uint32_t query_prealloc_size;
  uint64_t group_concat_max_len;
  uint64_t pseudo_thread_id;

  plugin::StorageEngine *storage_engine;

  /* Only charset part of these variables is sensible */
  const charset_info_st  *character_set_filesystem;

  /* Both charset and collation parts of these variables are important */
  const charset_info_st	*collation_server;

  inline const charset_info_st  *getCollation(void) 
  {
    return collation_server;
  }

  /* Locale Support */
  MY_LOCALE *lc_time_names;
};


} /* namespace drizzled */

