/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *  Copyright (C) 2010 Joseph Daly 
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

#ifndef DRIZZLED_STATISTICS_VARIABLES_H
#define DRIZZLED_STATISTICS_VARIABLES_H

namespace drizzled
{

extern struct global_counters current_global_counters;

typedef struct global_counters
{
  uint64_t aborted_threads;
  uint64_t aborted_connects;
  uint32_t max_used_connections;
  uint32_t locks_immediate;
  uint32_t locks_waited;
} global_counters;

typedef struct system_status_var
{
  uint64_t bytes_received;
  uint64_t bytes_sent;
  ulong com_other;
  ulong created_tmp_disk_tables;
  ulong created_tmp_tables;
  ulong ha_commit_count;
  ulong ha_delete_count;
  ulong ha_read_first_count;
  ulong ha_read_last_count;
  ulong ha_read_key_count;
  ulong ha_read_next_count;
  ulong ha_read_prev_count;
  ulong ha_read_rnd_count;
  ulong ha_read_rnd_next_count;
  ulong ha_rollback_count;
  ulong ha_update_count;
  ulong ha_write_count;
  ulong ha_prepare_count;
  ulong ha_savepoint_count;
  ulong ha_savepoint_rollback_count;

  /* KEY_CACHE parts. These are copies of the original */
  ulong key_blocks_changed;
  ulong key_blocks_used;
  ulong key_cache_r_requests;
  ulong key_cache_read;
  ulong key_cache_w_requests;
  ulong key_cache_write;
  /* END OF KEY_CACHE parts */

  ulong select_full_join_count;
  ulong select_full_range_join_count;
  ulong select_range_count;
  ulong select_range_check_count;
  ulong select_scan_count;
  ulong long_query_count;
  ulong filesort_merge_passes;
  ulong filesort_range_count;
  ulong filesort_rows;
  ulong filesort_scan_count;
  /*
    Number of statements sent from the client
  */
  ulong questions;
  /*
    IMPORTANT!
    SEE last_system_status_var DEFINITION BELOW.

    Below 'last_system_status_var' are all variables which doesn't make any
    sense to add to the /global/ status variable counter.
  */
  double last_query_cost;
} system_status_var;

#define last_system_status_var questions

} /* namespace drizzled */

#endif /* DRIZZLED_STATISTICS_VARIABLES_H */
