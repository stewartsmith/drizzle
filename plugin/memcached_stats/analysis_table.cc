/*
 *  Copyright (C) 2009 Sun Microsystems
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

#include "drizzled/server_includes.h"
#include "drizzled/session.h"
#include "drizzled/show.h"

#include "analysis_table.h"
#include "sysvar_holder.h"

#include <libmemcached/memcached.h>

#include <string>
#include <vector>

using namespace std;

int MemcachedAnalysisISMethods::fillTable(Session *session,
                                          TableList *tables,
                                          COND *)
{
  const CHARSET_INFO * const scs= system_charset_info;
  Table *table= tables->table;
  SysvarHolder &sysvar_holder= SysvarHolder::singleton();
  const string servers_string= sysvar_holder.getServersString();

  memcached_return rc;
  memcached_st *serv= memcached_create(NULL);
  memcached_server_st *tmp_serv=
    memcached_servers_parse(servers_string.c_str());
  memcached_server_push(serv, tmp_serv);
  memcached_server_list_free(tmp_serv);
  memcached_stat_st *stats= memcached_stat(serv, NULL, &rc);
  memcached_analysis_st *report= memcached_analyze(serv, stats, &rc);
  memcached_server_st *servers= memcached_server_list(serv);

  uint32_t server_count= memcached_server_count(serv);

  table->restoreRecordAsDefault();

  table->field[0]->store(server_count);
  table->field[1]->store(report->average_item_size);

  if (server_count > 1)
  {
    table->field[2]->store(memcached_server_name(serv, 
                                                 servers[report->most_consumed_server]),
                           64,
                           scs);
    table->field[3]->store(report->most_used_bytes);
    table->field[4]->store(memcached_server_name(serv, 
                                                 servers[report->least_free_server]),
                           64,
                           scs);
    table->field[5]->store(report->least_remaining_bytes);
    table->field[6]->store(memcached_server_name(serv, 
                                                 servers[report->oldest_server]),
                           64,
                           scs);
    table->field[7]->store(report->longest_uptime);
    table->field[8]->store(report->pool_hit_ratio);
  }

  free(report);
  memcached_stat_free(serv, stats);
  memcached_free(serv);

  /* store the actual record now */
  if (schema_table_store_record(session, table))
  {
    return 1;
  }

  return 0;
}

bool createMemcachedAnalysisColumns(vector<const ColumnInfo *> &cols)
{
  /*
   * Create each column for the memcached analysis table.
   */
  const ColumnInfo *num_analyzed= new(std::nothrow) ColumnInfo("SERVERS_ANALYZED",
                                                               4,
                                                               DRIZZLE_TYPE_LONGLONG,
                                                               0,
                                                               0, 
                                                               "Num of Servers Analyzed",
                                                               SKIP_OPEN_TABLE);
  if (! num_analyzed)
  {
    return true;
  }

  const ColumnInfo *avg_size= new(std::nothrow) ColumnInfo("AVERAGE_ITEM_SIZE",
                                                           4,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           0, 
                                                           "Average Item Size",
                                                           SKIP_OPEN_TABLE);
  if (! avg_size)
  {
    return true;
  }

  const ColumnInfo *mem_node= new(std::nothrow) ColumnInfo("NODE_WITH_MOST_MEM_CONSUMPTION",
                                                           32,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Node with Most Memory Consumption",
                                                           SKIP_OPEN_TABLE);
  if (! mem_node)
  {
    return true;
  }

  const ColumnInfo *used_bytes= new(std::nothrow) ColumnInfo("USED_BYTES",
                                                             4,
                                                             DRIZZLE_TYPE_LONGLONG,
                                                             0,
                                                             0,
                                                             "Used Bytes",
                                                             SKIP_OPEN_TABLE);
  if (! used_bytes)
  {
    return true;
  }

  const ColumnInfo *free_node= new(std::nothrow) ColumnInfo("NODE_WITH_LEAST_FREE_SPACE",
                                                            32,
                                                            DRIZZLE_TYPE_VARCHAR,
                                                            0,
                                                            0,
                                                            "Node with Least Free Space",
                                                            SKIP_OPEN_TABLE);
  if (! free_node)
  {
    return true;
  }

  const ColumnInfo *free_bytes= new(std::nothrow) ColumnInfo("FREE_BYTES",
                                                             4,
                                                             DRIZZLE_TYPE_LONGLONG,
                                                             0,
                                                             0,
                                                             "Free Bytes",
                                                             SKIP_OPEN_TABLE);
  if (! free_bytes)
  {
    return true;
  }

  const ColumnInfo *up_node= new(std::nothrow) ColumnInfo("NODE_WITH_LONGEST_UPTIME",
                                                          32,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          0,
                                                          "Node with Longest Uptime",
                                                          SKIP_OPEN_TABLE);
  if (! up_node)
  {
    return true;
  }

  const ColumnInfo *uptime= new(std::nothrow) ColumnInfo("LONGEST_UPTIME",
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0,
                                                         "Longest Uptime",
                                                         SKIP_OPEN_TABLE);
  if (! uptime)
  {
    return true;
  }

  const ColumnInfo *hit_ratio= new(std::nothrow) ColumnInfo("POOL_WIDE_HIT_RATIO",
                                                            4,
                                                            DRIZZLE_TYPE_LONGLONG,
                                                            0,
                                                            0,
                                                            "Pool-wide Hit Ratio",
                                                            SKIP_OPEN_TABLE);
  if (! hit_ratio)
  {
    return true;
  }


  cols.push_back(num_analyzed);
  cols.push_back(avg_size);
  cols.push_back(mem_node);
  cols.push_back(used_bytes);
  cols.push_back(free_node);
  cols.push_back(free_bytes);
  cols.push_back(up_node);
  cols.push_back(uptime);
  cols.push_back(hit_ratio);

  return false;
}

