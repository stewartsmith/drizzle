/*
 * Copyright (c) 2010, Joseph Daly <skinny.moey@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of Joseph Daly nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <drizzled/plugin.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/session.h>
#include <drizzled/drizzled.h>
#include "status_vars.h"

//TODO check if KEY_CACHE variables are needed
#include "plugin/myisam/myisam.h"

using namespace drizzled;

static int show_starttime_new(drizzle_show_var *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (time(NULL) - server_start_time);
  return 0;
}

static int show_flushstatustime_new(drizzle_show_var *var, char *buff)
{
  var->type= SHOW_LONG;
  var->value= buff;
  *((long *)buff)= (long) (time(NULL) - flush_status_time);
  return 0;
}

static int show_connection_count_new(drizzle_show_var *var, char *buff)
{
  var->type= SHOW_INT;
  var->value= buff;
  *((uint32_t *)buff)= connection_count;
  return 0;
}

static st_show_var_func_container show_starttime_cont_new= { &show_starttime_new };

static st_show_var_func_container show_flushstatustime_cont_new= { &show_flushstatustime_new };

static st_show_var_func_container show_connection_count_cont_new= { &show_connection_count_new };

drizzle_show_var StatusVars::status_vars_defs[]= 
{
  {"Aborted_clients",           (char*) &current_global_counters.aborted_threads, SHOW_LONGLONG},
  {"Aborted_connects",          (char*) &current_global_counters.aborted_connects, SHOW_LONGLONG},
  {"Bytes_received",            (char*) offsetof(system_status_var, bytes_received), SHOW_LONGLONG_STATUS},
  {"Bytes_sent",                (char*) offsetof(system_status_var, bytes_sent), SHOW_LONGLONG_STATUS},
  {"Connections",               (char*) &global_thread_id, SHOW_INT_NOFLUSH},
  {"Created_tmp_disk_tables",   (char*) offsetof(system_status_var, created_tmp_disk_tables), SHOW_LONGLONG_STATUS},
  {"Created_tmp_tables",        (char*) offsetof(system_status_var, created_tmp_tables), SHOW_LONGLONG_STATUS},
  {"Flush_commands",            (char*) &refresh_version,    SHOW_INT_NOFLUSH},
  {"Handler_commit",            (char*) offsetof(system_status_var, ha_commit_count), SHOW_LONGLONG_STATUS},
  {"Handler_delete",            (char*) offsetof(system_status_var, ha_delete_count), SHOW_LONGLONG_STATUS},
  {"Handler_prepare",           (char*) offsetof(system_status_var, ha_prepare_count),  SHOW_LONGLONG_STATUS},
  {"Handler_read_first",        (char*) offsetof(system_status_var, ha_read_first_count), SHOW_LONGLONG_STATUS},
  {"Handler_read_key",          (char*) offsetof(system_status_var, ha_read_key_count), SHOW_LONGLONG_STATUS},
  {"Handler_read_next",         (char*) offsetof(system_status_var, ha_read_next_count), SHOW_LONGLONG_STATUS},
  {"Handler_read_prev",         (char*) offsetof(system_status_var, ha_read_prev_count), SHOW_LONGLONG_STATUS},
  {"Handler_read_rnd",          (char*) offsetof(system_status_var, ha_read_rnd_count), SHOW_LONGLONG_STATUS},
  {"Handler_read_rnd_next",     (char*) offsetof(system_status_var, ha_read_rnd_next_count), SHOW_LONGLONG_STATUS},
  {"Handler_rollback",          (char*) offsetof(system_status_var, ha_rollback_count), SHOW_LONGLONG_STATUS},
  {"Handler_savepoint",         (char*) offsetof(system_status_var, ha_savepoint_count), SHOW_LONGLONG_STATUS},
  {"Handler_savepoint_rollback",(char*) offsetof(system_status_var, ha_savepoint_rollback_count), SHOW_LONGLONG_STATUS},
  {"Handler_update",            (char*) offsetof(system_status_var, ha_update_count), SHOW_LONGLONG_STATUS},
  {"Handler_write",             (char*) offsetof(system_status_var, ha_write_count), SHOW_LONGLONG_STATUS},
  {"Key_blocks_not_flushed",    (char*) offsetof(KEY_CACHE, global_blocks_changed), SHOW_KEY_CACHE_LONG},
  {"Key_blocks_unused",         (char*) offsetof(KEY_CACHE, blocks_unused), SHOW_KEY_CACHE_LONG},
  {"Key_blocks_used",           (char*) offsetof(KEY_CACHE, blocks_used), SHOW_KEY_CACHE_LONG},
  {"Key_read_requests",         (char*) offsetof(KEY_CACHE, global_cache_r_requests), SHOW_KEY_CACHE_LONGLONG},
  {"Key_reads",                 (char*) offsetof(KEY_CACHE, global_cache_read), SHOW_KEY_CACHE_LONGLONG},
  {"Key_write_requests",        (char*) offsetof(KEY_CACHE, global_cache_w_requests), SHOW_KEY_CACHE_LONGLONG},
  {"Key_writes",                (char*) offsetof(KEY_CACHE, global_cache_write), SHOW_KEY_CACHE_LONGLONG},
  {"Last_query_cost",           (char*) offsetof(system_status_var, last_query_cost), SHOW_DOUBLE_STATUS},
  {"Max_used_connections",      (char*) &current_global_counters.max_used_connections,  SHOW_LONGLONG},
  {"Questions",                 (char*) offsetof(system_status_var, questions), SHOW_LONGLONG_STATUS},
  {"Select_full_join",          (char*) offsetof(system_status_var, select_full_join_count), SHOW_LONGLONG_STATUS},
  {"Select_full_range_join",    (char*) offsetof(system_status_var, select_full_range_join_count), SHOW_LONGLONG_STATUS},
  {"Select_range",              (char*) offsetof(system_status_var, select_range_count), SHOW_LONGLONG_STATUS},
  {"Select_range_check",        (char*) offsetof(system_status_var, select_range_check_count), SHOW_LONGLONG_STATUS},
  {"Select_scan",               (char*) offsetof(system_status_var, select_scan_count), SHOW_LONGLONG_STATUS},
  {"Slow_queries",              (char*) offsetof(system_status_var, long_query_count), SHOW_LONGLONG_STATUS},
  {"Sort_merge_passes",         (char*) offsetof(system_status_var, filesort_merge_passes), SHOW_LONGLONG_STATUS},
  {"Sort_range",                (char*) offsetof(system_status_var, filesort_range_count), SHOW_LONGLONG_STATUS},
  {"Sort_rows",                 (char*) offsetof(system_status_var, filesort_rows), SHOW_LONGLONG_STATUS},
  {"Sort_scan",                 (char*) offsetof(system_status_var, filesort_scan_count), SHOW_LONGLONG_STATUS},
  {"Table_locks_immediate",     (char*) &current_global_counters.locks_immediate,        SHOW_LONGLONG},
  {"Table_locks_waited",        (char*) &current_global_counters.locks_waited,           SHOW_LONGLONG},
  {"Threads_connected",         (char*) &show_connection_count_cont_new,  SHOW_FUNC},
  {"Uptime",                    (char*) &show_starttime_cont_new,         SHOW_FUNC},
  {"Uptime_since_flush_status", (char*) &show_flushstatustime_cont_new,   SHOW_FUNC},
  {NULL, NULL, SHOW_LONGLONG}
};

StatusVars::StatusVars()
{
  status_var_counters= (system_status_var*) malloc(sizeof(system_status_var));
  memset(status_var_counters, 0, sizeof(system_status_var));
}

StatusVars::StatusVars(const StatusVars &status_vars)
{
  status_var_counters= (system_status_var*) malloc(sizeof(system_status_var));
  memset(status_var_counters, 0, sizeof(system_status_var));
  copySystemStatusVar(status_var_counters, status_vars.status_var_counters); 
}

StatusVars::~StatusVars()
{
  free(status_var_counters); 
}

void StatusVars::copySystemStatusVar(system_status_var *to_var, 
                                     system_status_var *from_var)
{
  uint64_t *end= (uint64_t*) ((unsigned char*) to_var + offsetof(system_status_var,
                 last_system_status_var) + sizeof(uint64_t));

  uint64_t *to= (uint64_t*) to_var;
  uint64_t *from= (uint64_t*) from_var;

  while (to != end)
  {
    *(to++)= *(from++);
  }
}

void StatusVars::merge(StatusVars *status_vars)
{
  system_status_var* from_var= status_vars->getStatusVarCounters(); 

  uint64_t *end= (uint64_t*) ((unsigned char*) status_var_counters + offsetof(system_status_var,
                 last_system_status_var) + sizeof(uint64_t));

  uint64_t *to= (uint64_t*) status_var_counters;
  uint64_t *from= (uint64_t*) from_var;

  while (to != end)
  {
    *(to++)+= *(from++);
  }
}

void StatusVars::reset()
{
  memset(status_var_counters, 0, sizeof(system_status_var));
}

void StatusVars::logStatusVar(Session *session)
{
  copySystemStatusVar(status_var_counters, &session->status_var);
}

bool StatusVars::hasBeenFlushed(Session *session)
{
  system_status_var *current_status_var= &session->status_var;

  /* check bytes received if its lower then a flush has occurred */
  uint64_t current_bytes_received= current_status_var->bytes_received;
  uint64_t my_bytes_received= status_var_counters->bytes_received;
  if (current_bytes_received < my_bytes_received)
  {
    return true;
  }
  else 
  {
    return false;
  }
}
