/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

namespace drizzled {

extern struct global_counters current_global_counters;

/* 
 * These statistics are global and are not per session
 * they are not reset once initialized. 
 */
typedef struct global_counters
{
  uint64_t max_used_connections;
  uint64_t connections;
  uint64_t locks_immediate;
  uint64_t locks_waited;
} global_counters;

/* 
 * These statistics are per session and are reset at the end
 * of each session, after being copied into a global 
 * system_status_var
 */
typedef struct system_status_var
{
  uint64_t aborted_connects;
  uint64_t aborted_threads;
  uint64_t access_denied;
  uint64_t bytes_received;
  uint64_t bytes_sent;
  uint64_t com_other;
  uint64_t created_tmp_disk_tables;
  uint64_t created_tmp_tables;
  uint64_t ha_commit_count;
  uint64_t ha_delete_count;
  uint64_t ha_read_first_count;
  uint64_t ha_read_last_count;
  uint64_t ha_read_key_count;
  uint64_t ha_read_next_count;
  uint64_t ha_read_prev_count;
  uint64_t ha_read_rnd_count;
  uint64_t ha_read_rnd_next_count;
  uint64_t ha_rollback_count;
  uint64_t ha_update_count;
  uint64_t ha_write_count;
  uint64_t ha_prepare_count;
  uint64_t ha_savepoint_count;
  uint64_t ha_savepoint_rollback_count;

  uint64_t select_full_join_count;
  uint64_t select_full_range_join_count;
  uint64_t select_range_count;
  uint64_t select_range_check_count;
  uint64_t select_scan_count;
  uint64_t long_query_count;
  uint64_t filesort_merge_passes;
  uint64_t filesort_range_count;
  uint64_t filesort_rows;
  uint64_t filesort_scan_count;
  uint64_t connection_time;
  uint64_t execution_time_nsec;
  uint64_t updated_row_count;
  uint64_t deleted_row_count;
  uint64_t inserted_row_count;
  /*
    Number of statements sent from the client
  */
  uint64_t questions;
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

