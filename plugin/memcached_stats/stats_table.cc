/* 
 * Copyright (c) 2009, Padraig O'Sullivan
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
 *   * Neither the name of Padraig O'Sullivan nor the names of its contributors
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
#include "drizzled/session.h"
#include "drizzled/show.h"
#include "drizzled/error.h"

#include "stats_table.h"
#include "sysvar_holder.h"

#include <libmemcached/memcached.h>

#include <string>
#include <vector>

using namespace std;
using namespace drizzled;

#if !defined(HAVE_MEMCACHED_SERVER_FN)
typedef memcached_server_function memcached_server_fn;
#endif

extern "C"
memcached_return  server_function(const memcached_st *ptr,
                                  memcached_server_st *server,
                                  void *context);

struct server_function_context
{
  Table* table;
  plugin::InfoSchemaTable *schema_table;
  server_function_context(Table *table_arg,
                          plugin::InfoSchemaTable *schema_table_arg)
    : table(table_arg), schema_table(schema_table_arg)
  {}
};

extern "C"
memcached_return  server_function(const memcached_st *const_memc,
                                  memcached_server_st *server,
                                  void *context)
{
  server_function_context *ctx= static_cast<server_function_context *>(context);
  const CHARSET_INFO * const scs= system_charset_info;
  memcached_st memc_stack;
  memcached_st *memc;

  memc= memcached_clone(&memc_stack, const_memc);

  if (not memc)
  {
    my_printf_error(ER_UNKNOWN_ERROR, _("Unable to allocate memory for memcached_clone()."), MYF(0));
    return MEMCACHED_FAILURE;
  }
    
  char *server_name= memcached_server_name(memc, *server);
  in_port_t server_port= memcached_server_port(memc, *server);

  memcached_stat_st stats;
  memcached_return ret= memcached_stat_servername(&stats, NULL,
                                                  server_name, server_port);
  if (ret != MEMCACHED_SUCCESS)
  {
    my_printf_error(ER_UNKNOWN_ERROR, _("Unable get stats from memcached server %s.  Got error from memcached_stat_servername()."), MYF(0), server_name);
    memcached_free(memc);
    return ret;
  }

  char **list= memcached_stat_get_keys(memc, &stats, &ret);
  char **ptr= NULL;

  ctx->table->setWriteSet(0);
  ctx->table->setWriteSet(1);

  ctx->table->field[0]->store(server_name, strlen(server_name), scs);
  ctx->table->field[1]->store(server_port);

  uint32_t col= 2;
  for (ptr= list; *ptr; ptr++)
  {
    char *value= memcached_stat_get_value(memc, &stats, *ptr, &ret);

    ctx->table->setWriteSet(col);
    ctx->table->field[col]->store(value,
                                  strlen(value),
                                  scs);
    col++;
    free(value);
  }
  free(list);
  /* store the actual record now */
  ctx->schema_table->addRow(ctx->table->record[0], ctx->table->s->reclength);
  memcached_free(memc);

  return MEMCACHED_SUCCESS;
}

int MemcachedStatsISMethods::fillTable(Session *,
                                       Table *table,
                                       plugin::InfoSchemaTable *schema_table)
{
  SysvarHolder &sysvar_holder= SysvarHolder::singleton();
  const string servers_string= sysvar_holder.getServersString();

  table->restoreRecordAsDefault();
  if (servers_string.empty())
  {
    my_printf_error(ER_UNKNOWN_ERROR, _("No value in MEMCACHED_STATS_SERVERS variable."), MYF(0));
    return 1;
  } 
 

  memcached_st *memc= memcached_create(NULL);
  if (memc == NULL)
  {
    my_printf_error(ER_UNKNOWN_ERROR, _("Unable to create memcached struct.  Got error from memcached_create()."), MYF(0));
    return 1;
  }

  memcached_server_st *tmp_serv=
    memcached_servers_parse(servers_string.c_str());
  if (tmp_serv == NULL)
  {
    my_printf_error(ER_UNKNOWN_ERROR, _("Unable to create memcached server list.  Got error from memcached_servers_parse(%s)."), MYF(0), servers_string.c_str());
    memcached_free(memc);
    return 1; 
  }

  memcached_server_push(memc, tmp_serv);
  memcached_server_list_free(tmp_serv);

  memcached_server_fn callbacks[1];

  callbacks[0]= server_function;
  server_function_context context(table, schema_table);

  memcached_server_cursor(memc, callbacks, &context, 1);

  memcached_free(memc);

  return 0;
}

bool createMemcachedStatsColumns(vector<const plugin::ColumnInfo *> &cols)
{
  /*
   * Create each column for the memcached stats table.
   */
  const plugin::ColumnInfo *name_col= new(std::nothrow) plugin::ColumnInfo("NAME",
                                                           32,
                                                           DRIZZLE_TYPE_VARCHAR,
                                                           0,
                                                           0,
                                                           "Name");
  if (! name_col)
  {
    return true;
  }

  const plugin::ColumnInfo *port= new(std::nothrow) plugin::ColumnInfo("PORT_NUMBER",
                                                       4,
                                                       DRIZZLE_TYPE_LONGLONG,
                                                       0,
                                                       0, 
                                                       "Port Number");
  if (! port)
  {
    return true;
  }

  const plugin::ColumnInfo *pid= new(std::nothrow) plugin::ColumnInfo("PROCESS_ID",
                                                      4,
                                                      DRIZZLE_TYPE_LONGLONG,
                                                      0,
                                                      0, 
                                                      "Process ID");
  if (! pid)
  {
    return true;
  }

  const plugin::ColumnInfo *uptime= new(std::nothrow) plugin::ColumnInfo("UPTIME",
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0, 
                                                         "Uptime");
  if (! uptime)
  {
    return true;
  }

  const plugin::ColumnInfo *time= new(std::nothrow) plugin::ColumnInfo("TIME",
                                                       4,
                                                       DRIZZLE_TYPE_LONGLONG,
                                                       0,
                                                       0, 
                                                       "Time");
  if (! time)
  {
    return true;
  }

  const plugin::ColumnInfo *version= new(std::nothrow) plugin::ColumnInfo("VERSION",
                                                          8,
                                                          DRIZZLE_TYPE_VARCHAR,
                                                          0,
                                                          0,
                                                          "Version");
  if (! version)
  {
    return true;
  }

  const plugin::ColumnInfo *ptr_size= new(std::nothrow) plugin::ColumnInfo("POINTER_SIZE",
                                                           4,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           0, 
                                                           "Pointer Size");
  if (! ptr_size)
  {
    return true;
  }

  const plugin::ColumnInfo *r_user= new(std::nothrow) plugin::ColumnInfo("RUSAGE_USER",
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0, 
                                                         "rusage user");
  if (! r_user)
  {
    return true;
  }

  const plugin::ColumnInfo *r_sys= new(std::nothrow) plugin::ColumnInfo("RUSAGE_SYSTEM",
                                                        4,
                                                        DRIZZLE_TYPE_LONGLONG,
                                                        0,
                                                        0, 
                                                        "rusage system");
  if (! r_sys)
  {
    return true;
  }
  const plugin::ColumnInfo *curr_items= new(std::nothrow) plugin::ColumnInfo("CURRENT_ITEMS",
                                                             4,
                                                             DRIZZLE_TYPE_LONGLONG,
                                                             0,
                                                             0, 
                                                             "Current Items");
  if (! curr_items)
  {
    return true;
  }

  const plugin::ColumnInfo *total_items= new(std::nothrow) plugin::ColumnInfo("TOTAL_ITEMS",
                                                              4,
                                                              DRIZZLE_TYPE_LONGLONG,
                                                              0,
                                                              0,
                                                              "Total Items");
  if (! total_items)
  {
    return true;
  }

  const plugin::ColumnInfo *bytes= new(std::nothrow) plugin::ColumnInfo("BYTES",
                                                        4,
                                                        DRIZZLE_TYPE_LONGLONG,
                                                        0,
                                                        0,
                                                        "Bytes");
  if (! bytes)
  {
    return true;
  }

  const plugin::ColumnInfo *curr_cons= new(std::nothrow) plugin::ColumnInfo("CURRENT_CONNECTIONS",
                                                            4,
                                                            DRIZZLE_TYPE_LONGLONG,
                                                            0,
                                                            0,
                                                            "Current Connections");
  if (! curr_cons)
  {
    return true;
  }

  const plugin::ColumnInfo *total_cons= new(std::nothrow) plugin::ColumnInfo("TOTAL_CONNECTIONS",
                                                             4,
                                                             DRIZZLE_TYPE_LONGLONG,
                                                             0,
                                                             0,
                                                             "Total Connections");
  if (! total_cons)
  {
    return true;
  }

  const plugin::ColumnInfo *con_structs= new(std::nothrow) plugin::ColumnInfo("CONNECTION_STRUCTURES",
                                                              4,
                                                              DRIZZLE_TYPE_LONGLONG,
                                                              0,
                                                              0,
                                                              "Connection Structures");
  if (! con_structs)
  {
    return true;
  }

  const plugin::ColumnInfo *cmd_gets= new(std::nothrow) plugin::ColumnInfo("GETS",
                                                           4,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           0,
                                                           "Gets");
  if (! cmd_gets)
  {
    return true;
  }

  const plugin::ColumnInfo *cmd_sets= new(std::nothrow) plugin::ColumnInfo("SETS",
                                                           4,
                                                           DRIZZLE_TYPE_LONGLONG,
                                                           0,
                                                           0,
                                                           "Sets");
  if (! cmd_sets)
  {
    return true;
  }

  const plugin::ColumnInfo *hits= new(std::nothrow) plugin::ColumnInfo("HITS",
                                                       4,
                                                       DRIZZLE_TYPE_LONGLONG,
                                                       0,
                                                       0,
                                                       "Hits");
  if (! hits)
  {
    return true;
  }

  const plugin::ColumnInfo *misses= new(std::nothrow) plugin::ColumnInfo("MISSES",
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0,
                                                         "Misses");
  if (! misses)
  {
    return true;
  }

  const plugin::ColumnInfo *evicts= new(std::nothrow) plugin::ColumnInfo("EVICTIONS",
                                                         4,
                                                         DRIZZLE_TYPE_LONGLONG,
                                                         0,
                                                         0,
                                                         "Evictions");
  if (! evicts)
  {
    return true;
  }

  const plugin::ColumnInfo *bytes_read= new(std::nothrow) plugin::ColumnInfo("BYTES_READ",
                                                             4,
                                                             DRIZZLE_TYPE_LONGLONG,
                                                             0,
                                                             0,
                                                             "bytes read");
  if (! bytes_read)
  {
    return true;
  }

  const plugin::ColumnInfo *bytes_written= new(std::nothrow) plugin::ColumnInfo("BYTES_WRITTEN",
                                                                4,
                                                                DRIZZLE_TYPE_LONGLONG,
                                                                0,
                                                                0,
                                                                "bytes written");
  if (! bytes_written)
  {
    return true;
  }

  const plugin::ColumnInfo *lim_max_bytes= new(std::nothrow) plugin::ColumnInfo("LIMIT_MAXBYTES",
                                                                4,
                                                                DRIZZLE_TYPE_LONGLONG,
                                                                0,
                                                                0,
                                                                "limit maxbytes");
  if (! lim_max_bytes)
  {
    return true;
  }

  const plugin::ColumnInfo *threads= new(std::nothrow) plugin::ColumnInfo("THREADS",
                                                          4,
                                                          DRIZZLE_TYPE_LONGLONG,
                                                          0,
                                                          0,
                                                          "Threads");
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

void clearMemcachedColumns(vector<const plugin::ColumnInfo *> &cols)
{
  for_each(cols.begin(), cols.end(), DeleteMemcachedCols());
  cols.clear();
}
