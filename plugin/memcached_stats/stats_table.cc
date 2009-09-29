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

#include "stats_table.h"

#include <libmemcached/memcached.h>

#include <string>
#include <vector>

using namespace std;

int MemcachedStatsISMethods::fillTable(Session *session,
                                       TableList *tables,
                                       COND *)
{
  const CHARSET_INFO * const scs= system_charset_info;
  Table *table= tables->table;

  table->restoreRecordAsDefault();

  memcached_return rc;
  memcached_st *serv= memcached_create(NULL);
  memcached_server_st *tmp_serv=
    memcached_servers_parse(servers_string.c_str());
  memcached_server_push(serv, tmp_serv);
  memcached_server_list_free(tmp_serv);
  memcached_stat_st *stats= memcached_stat(serv, NULL, &rc);
  memcached_server_st *servers= memcached_server_list(serv);

  for (uint32_t i= 0; i < memcached_server_count(serv); i++)
  {
    char **list= memcached_stat_get_keys(serv, &stats[i], &rc);
    char **ptr= NULL;

    table->field[0]->store(memcached_server_name(serv, servers[i]),
                           64,
                           scs);
    table->field[1]->store(memcached_server_port(serv, servers[i]));
    uint32_t col= 2;
    for (ptr= list; *ptr; ptr++)
    {
      char *value= memcached_stat_get_value(serv, &stats[i], *ptr, &rc);
      table->field[col]->store(value,
                               64,
                               scs);
      col++;
      free(value);
    }
    free(list);
  }

  memcached_stat_free(serv, stats);
  memcached_free(serv);

  /* store the actual record now */
  if (schema_table_store_record(session, table))
  {
    return 1;
  }

  return 0;
}

bool createMemcachedStatsColumns(vector<const ColumnInfo *> &cols)
{
  /*
   * Create each column for the memcached stats table.
   */
  const ColumnInfo *name_col= new(std::nothrow) ColumnInfo("NAME",
                                                           32,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Name",
                                                           SKIP_OPEN_TABLE);
  if (! name_col)
  {
    return true;
  }

  const ColumnInfo *port= new(std::nothrow) ColumnInfo("PORT_NUMBER",
                                                       4,
                                                       DRIZZLE_TYPE_LONGLONG,
                                                       0,
                                                       0, 
                                                       "Port Number",
                                                       SKIP_OPEN_TABLE);
  if (! port)
  {
    return true;
  }

  const ColumnInfo *pid= new(std::nothrow) ColumnInfo("PROCESS_ID",
                                                      4,
                                                      DRIZZLE_TYPE_LONGLONG,
                                                      0,
                                                      0, 
                                                      "Process ID",
                                                      SKIP_OPEN_TABLE);
  if (! pid)
  {
    return true;
  }

  const ColumnInfo *uptime= new(std::nothrow) ColumnInfo("UPTIME",
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0, 
                                                         "Uptime",
                                                         SKIP_OPEN_TABLE);
  if (! uptime)
  {
    return true;
  }

  const ColumnInfo *time= new(std::nothrow) ColumnInfo("TIME",
                                                       4,
                                                       DRIZZLE_TYPE_LONGLONG,
                                                       0,
                                                       0, 
                                                       "Time",
                                                       SKIP_OPEN_TABLE);
  if (! time)
  {
    return true;
  }

  const ColumnInfo *version= new(std::nothrow) ColumnInfo("VERSION",
                                                          8,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          0,
                                                          "Version",
                                                          SKIP_OPEN_TABLE);
  if (! version)
  {
    return true;
  }

  const ColumnInfo *ptr_size= new(std::nothrow) ColumnInfo("POINTER_SIZE",
                                                           4,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           0, 
                                                           "Pointer Size",
                                                           SKIP_OPEN_TABLE);
  if (! ptr_size)
  {
    return true;
  }

  const ColumnInfo *r_user= new(std::nothrow) ColumnInfo("RUSAGE_USER",
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0, 
                                                         "rusage user",
                                                         SKIP_OPEN_TABLE);
  if (! r_user)
  {
    return true;
  }

  const ColumnInfo *r_sys= new(std::nothrow) ColumnInfo("RUSAGE_SYSTEM",
                                                        4,
                                                        DRIZZLE_TYPE_LONGLONG,
                                                        0,
                                                        0, 
                                                        "rusage system",
                                                        SKIP_OPEN_TABLE);
  if (! r_sys)
  {
    return true;
  }
  const ColumnInfo *curr_items= new(std::nothrow) ColumnInfo("CURRENT_ITEMS",
                                                             4,
                                                             DRIZZLE_TYPE_LONGLONG,
                                                             0,
                                                             0, 
                                                             "Current Items",
                                                             SKIP_OPEN_TABLE);
  if (! curr_items)
  {
    return true;
  }

  const ColumnInfo *total_items= new(std::nothrow) ColumnInfo("TOTAL_ITEMS",
                                                              4,
                                                              DRIZZLE_TYPE_LONGLONG,
                                                              0,
                                                              0,
                                                              "Total Items",
                                                              SKIP_OPEN_TABLE);
  if (! total_items)
  {
    return true;
  }

  const ColumnInfo *bytes= new(std::nothrow) ColumnInfo("BYTES",
                                                        4,
                                                        DRIZZLE_TYPE_LONGLONG,
                                                        0,
                                                        0,
                                                        "Bytes",
                                                        SKIP_OPEN_TABLE);
  if (! bytes)
  {
    return true;
  }

  const ColumnInfo *curr_cons= new(std::nothrow) ColumnInfo("CURRENT_CONNECTIONS",
                                                            4,
                                                            DRIZZLE_TYPE_LONGLONG,
                                                            0,
                                                            0,
                                                            "Current Connections",
                                                            SKIP_OPEN_TABLE);
  if (! curr_cons)
  {
    return true;
  }

  const ColumnInfo *total_cons= new(std::nothrow) ColumnInfo("TOTAL_CONNECTIONS",
                                                             4,
                                                             DRIZZLE_TYPE_LONGLONG,
                                                             0,
                                                             0,
                                                             "Total Connections",
                                                             SKIP_OPEN_TABLE);
  if (! total_cons)
  {
    return true;
  }

  const ColumnInfo *con_structs= new(std::nothrow) ColumnInfo("CONNECTION_STRUCTURES",
                                                              4,
                                                              DRIZZLE_TYPE_LONGLONG,
                                                              0,
                                                              0,
                                                              "Connection Structures",
                                                              SKIP_OPEN_TABLE);
  if (! con_structs)
  {
    return true;
  }

  const ColumnInfo *cmd_gets= new(std::nothrow) ColumnInfo("GETS",
                                                           4,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           0,
                                                           "Gets",
                                                           SKIP_OPEN_TABLE);
  if (! cmd_gets)
  {
    return true;
  }

  const ColumnInfo *cmd_sets= new(std::nothrow) ColumnInfo("SETS",
                                                           4,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           0,
                                                           "Sets",
                                                           SKIP_OPEN_TABLE);
  if (! cmd_sets)
  {
    return true;
  }

  const ColumnInfo *hits= new(std::nothrow) ColumnInfo("HITS",
                                                       4,
                                                       DRIZZLE_TYPE_LONGLONG,
                                                       0,
                                                       0,
                                                       "Hits",
                                                       SKIP_OPEN_TABLE);
  if (! hits)
  {
    return true;
  }

  const ColumnInfo *misses= new(std::nothrow) ColumnInfo("MISSES",
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0,
                                                         "Misses",
                                                         SKIP_OPEN_TABLE);
  if (! misses)
  {
    return true;
  }

  const ColumnInfo *evicts= new(std::nothrow) ColumnInfo("EVICTIONS",
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0,
                                                         "Evictions",
                                                         SKIP_OPEN_TABLE);
  if (! evicts)
  {
    return true;
  }

  const ColumnInfo *bytes_read= new(std::nothrow) ColumnInfo("BYTES_READ",
                                                             4,
                                                             DRIZZLE_TYPE_LONGLONG,
                                                             0,
                                                             0,
                                                             "bytes read",
                                                             SKIP_OPEN_TABLE);
  if (! bytes_read)
  {
    return true;
  }

  const ColumnInfo *bytes_written= new(std::nothrow) ColumnInfo("BYTES_WRITTEN",
                                                                4,
                                                                DRIZZLE_TYPE_LONGLONG,
                                                                0,
                                                                0,
                                                                "bytes written",
                                                                SKIP_OPEN_TABLE);
  if (! bytes_written)
  {
    return true;
  }

  const ColumnInfo *lim_max_bytes= new(std::nothrow) ColumnInfo("LIMIT_MAXBYTES",
                                                                4,
                                                                DRIZZLE_TYPE_LONGLONG,
                                                                0,
                                                                0,
                                                                "limit maxbytes",
                                                                SKIP_OPEN_TABLE);
  if (! lim_max_bytes)
  {
    return true;
  }

  const ColumnInfo *threads= new(std::nothrow) ColumnInfo("THREADS",
                                                          4,
                                                          DRIZZLE_TYPE_LONGLONG,
                                                          0,
                                                          0,
                                                          "Threads",
                                                          SKIP_OPEN_TABLE);
  if (! threads)
  {
    return true;
  }

  cols.push_back(name_col);
  cols.push_back(port);
  cols.push_back(pid);
  cols.push_back(uptime);
  cols.push_back(time);
  cols.push_back(version);
  cols.push_back(ptr_size);
  cols.push_back(r_user);
  cols.push_back(r_sys);
  cols.push_back(curr_items);
  cols.push_back(total_items);
  cols.push_back(bytes);
  cols.push_back(curr_cons);
  cols.push_back(total_cons);
  cols.push_back(con_structs);
  cols.push_back(cmd_gets);
  cols.push_back(cmd_sets);
  cols.push_back(hits);
  cols.push_back(misses);
  cols.push_back(evicts);
  cols.push_back(bytes_read);
  cols.push_back(bytes_written);
  cols.push_back(lim_max_bytes);
  cols.push_back(threads);

  return false;
}

class DeleteMemcachedCols
{
public:
  template<typename T>
  inline void operator()(const T *ptr) const
  {
    delete ptr;
  }
};

void clearMemcachedColumns(vector<const ColumnInfo *> &cols)
{
  for_each(cols.begin(), cols.end(), DeleteMemcachedCols());
  cols.clear();
}
