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

#include <config.h>
#include "status_helper.h"
#include <boost/lexical_cast.hpp>
#include <drizzled/open_tables_state.h>
#include <drizzled/set_var.h>
#include <drizzled/drizzled.h>
#include <plugin/myisam/myisam.h>
#include <sstream>

using namespace std;

namespace drizzled {

extern time_t server_start_time;
extern time_t flush_status_time;

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

string StatusHelper::fillHelper(system_status_var *status_var, char *value, SHOW_TYPE show_type)
{
  ostringstream oss;
  string return_value;

  switch (show_type) {
  case SHOW_DOUBLE_STATUS:
    value= ((char *) status_var + (ulong) value);
    /* fall through */
  case SHOW_DOUBLE:
    oss.precision(6);
    oss << *(double *) value;
    return_value= oss.str();
    break;
  case SHOW_LONG_STATUS:
    value= ((char *) status_var + (ulong) value);
    /* fall through */
  case SHOW_LONG:
    return_value=boost::lexical_cast<std::string>(*(long*) value);
    break;
  case SHOW_LONGLONG_STATUS:
    value= ((char *) status_var + (uint64_t) value);
    /* fall through */
  case SHOW_LONGLONG:
    return_value=boost::lexical_cast<std::string>(*(int64_t*) value);
    break;
  case SHOW_SIZE:
    return_value=boost::lexical_cast<std::string>(*(size_t*) value);
    break;
  case SHOW_HA_ROWS:
    return_value=boost::lexical_cast<std::string>((int64_t) *(ha_rows*) value);
    break;
  case SHOW_BOOL:
  case SHOW_MY_BOOL:
    return_value= *(bool*) value ? "ON" : "OFF";
    break;
  case SHOW_INT:
  case SHOW_INT_NOFLUSH: // the difference lies in refresh_status()
    return_value=boost::lexical_cast<std::string>((long) *(uint32_t*) value);
    break;
  case SHOW_CHAR:
    {
      if (value)
        return_value= value;
      break;
    }
  case SHOW_CHAR_PTR:
    {
      if (*(char**) value)
        return_value= *(char**) value;

      break;
    }
  case SHOW_UNDEF:
    break;                                        // Return empty string
  case SHOW_SYS:                                  // Cannot happen
  default:
    assert(0);
    break;
  }

  return return_value;
}

drizzle_show_var StatusHelper::status_vars_defs[]=
{
  {"Aborted_clients",           (char*) offsetof(system_status_var, aborted_threads), SHOW_LONGLONG_STATUS},
  {"Aborted_connects",          (char*) offsetof(system_status_var, aborted_connects), SHOW_LONGLONG_STATUS},
  {"Bytes_received",            (char*) offsetof(system_status_var, bytes_received), SHOW_LONGLONG_STATUS},
  {"Bytes_sent",                (char*) offsetof(system_status_var, bytes_sent), SHOW_LONGLONG_STATUS},
  {"Connections",               (char*) &current_global_counters.connections,  SHOW_LONGLONG},
  {"Created_tmp_disk_tables",   (char*) offsetof(system_status_var, created_tmp_disk_tables), SHOW_LONGLONG_STATUS},
  {"Created_tmp_tables",        (char*) offsetof(system_status_var, created_tmp_tables), SHOW_LONGLONG_STATUS},
  {"Flush_commands",            (char*) &g_refresh_version, SHOW_INT_NOFLUSH},
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
  {"Last_query_cost",           (char*) offsetof(system_status_var, last_query_cost), SHOW_DOUBLE_STATUS},
  {"Max_used_connections",      (char*) &current_global_counters.max_used_connections,  SHOW_LONGLONG},
  {"Questions",                 (char*) offsetof(system_status_var, questions), SHOW_LONGLONG_STATUS},
  {"Select_full_join",          (char*) offsetof(system_status_var, select_full_join_count), SHOW_LONGLONG_STATUS},
  {"Select_full_range_join",    (char*) offsetof(system_status_var, select_full_range_join_count), SHOW_LONGLONG_STATUS},
  {"Select_range",              (char*) offsetof(system_status_var, select_range_count), SHOW_LONGLONG_STATUS},
  {"Select_range_check",        (char*) offsetof(system_status_var, select_range_check_count), SHOW_LONGLONG_STATUS},
  {"Select_scan",               (char*) offsetof(system_status_var, select_scan_count), SHOW_LONGLONG_STATUS},
  {"Sessions_connected",         (char*) &show_connection_count_cont_new,  SHOW_FUNC},
  {"Slow_queries",              (char*) offsetof(system_status_var, long_query_count), SHOW_LONGLONG_STATUS},
  {"Sort_merge_passes",         (char*) offsetof(system_status_var, filesort_merge_passes), SHOW_LONGLONG_STATUS},
  {"Sort_range",                (char*) offsetof(system_status_var, filesort_range_count), SHOW_LONGLONG_STATUS},
  {"Sort_rows",                 (char*) offsetof(system_status_var, filesort_rows), SHOW_LONGLONG_STATUS},
  {"Sort_scan",                 (char*) offsetof(system_status_var, filesort_scan_count), SHOW_LONGLONG_STATUS},
  {"Table_locks_immediate",     (char*) &current_global_counters.locks_immediate,        SHOW_LONGLONG},
  {"Table_locks_waited",        (char*) &current_global_counters.locks_waited,           SHOW_LONGLONG},
  {"Uptime",                    (char*) &show_starttime_cont_new,         SHOW_FUNC},
  {"Uptime_since_flush_status", (char*) &show_flushstatustime_cont_new,   SHOW_FUNC},
  {NULL, NULL, SHOW_LONGLONG}
};

} /* namespace drizzled */
