/* 
 * Copyright (C) 2009, Padraig O'Sullivan
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

#include <config.h>

#include "analysis_table.h"
#include "sysvar_holder.h"

#include <drizzled/error.h>

#include <libmemcached/memcached.h>
#include <libmemcached/server.h>

namespace drizzle_plugin
{

AnalysisTableTool::AnalysisTableTool() :
  plugin::TableFunction("DATA_DICTIONARY", "MEMCACHED_ANALYSIS")
{
  add_field("SERVERS_ANALYZED", plugin::TableFunction::NUMBER);
  add_field("AVERAGE_ITEM_SIZE", plugin::TableFunction::NUMBER);
  add_field("NODE_WITH_MOST_MEM_CONSUMPTION");
  add_field("USED_BYTES", plugin::TableFunction::NUMBER);
  add_field("NODE_WITH_LEAST_FREE_SPACE");
  add_field("FREE_BYTES", plugin::TableFunction::NUMBER);
  add_field("NODE_WITH_LONGEST_UPTIME");
  add_field("LONGEST_UPTIME", plugin::TableFunction::NUMBER);
  add_field("POOL_WIDE_HIT_RATIO", plugin::TableFunction::NUMBER); 
}

AnalysisTableTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  is_done= false;
}

bool AnalysisTableTool::Generator::populate()
{
  if (is_done)
  {
    return false;
  }
  is_done= true;

  drizzled::sys_var *servers_var= drizzled::find_sys_var("memcached_stats_servers");
  assert(servers_var != NULL);

  const string servers_string(static_cast<char *>(servers_var.value_ptr(NULL, 0, NULL)));

  if (servers_string.empty()) 
  {       
    my_printf_error(ER_UNKNOWN_ERROR, _("No value in MEMCACHED_STATS_SERVERS variable."), MYF(0));
    return false;
  }    

  memcached_return rc;
  memcached_st *serv= memcached_create(NULL);
  memcached_server_st *tmp_serv=
    memcached_servers_parse(servers_string.c_str());
  memcached_server_push(serv, tmp_serv);
  memcached_server_list_free(tmp_serv);
  memcached_stat_st *stats= memcached_stat(serv, NULL, &rc);
  memcached_server_st *servers= memcached_server_list(serv);

  uint32_t server_count= memcached_server_count(serv);

  if (server_count > 1)
  {
    memcached_analysis_st *report= memcached_analyze(serv, stats, &rc);

    push(static_cast<uint64_t>(server_count));
    push(static_cast<uint64_t>(report->average_item_size));
    push(memcached_server_name(serv, servers[report->most_consumed_server]));
    push(report->most_used_bytes);
    push(memcached_server_name(serv, servers[report->least_free_server]));
    push(report->least_remaining_bytes);
    push(memcached_server_name(serv, servers[report->oldest_server]));
    push(static_cast<uint64_t>(report->longest_uptime));
    push(static_cast<int64_t>(report->pool_hit_ratio));
    free(report);
  } 

  memcached_stat_free(serv, stats);
  memcached_free(serv);

  return true;
}

} /* namespace drizzle_plugin */
