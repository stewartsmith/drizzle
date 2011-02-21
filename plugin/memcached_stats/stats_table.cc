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

#include "stats_table.h"

#include <drizzled/error.h>
#include <libmemcached/server.h>

#if !defined(HAVE_MEMCACHED_SERVER_FN)
typedef memcached_server_function memcached_server_fn;
#endif

namespace drizzle_plugin
{

extern "C"
memcached_return  server_function(const memcached_st *ptr,
                                  memcached_server_st *server,
                                  void *context);

struct server_function_context
{
  StatsTableTool::Generator* generator; 
  server_function_context(StatsTableTool::Generator *generator_arg)
    : generator(generator_arg)
  {}
};


extern "C"
memcached_return  server_function(const memcached_st *memc,
                                  memcached_server_st *server,
                                  void *context)
{
  server_function_context *ctx= static_cast<server_function_context *>(context);

  const char *server_name= memcached_server_name(*memc, *server);
  in_port_t server_port= memcached_server_port(*memc, *server);

  memcached_stat_st stats;
  memcached_return ret= memcached_stat_servername(&stats, NULL,
                                                  server_name, server_port);

  if (ret != MEMCACHED_SUCCESS)
  {
    my_printf_error(ER_UNKNOWN_ERROR, _("Unable get stats from memcached server %s.  Got error from memcached_stat_servername()."), MYF(0), server_name);
    return ret;
  }

  char **list= memcached_stat_get_keys((memcached_st *)memc, &stats, &ret);
  char **ptr= NULL;
 
  ctx->generator->push(server_name);
  ctx->generator->push(static_cast<uint64_t>(server_port));

  for (ptr= list; *ptr; ptr++)
  {
    char *value= memcached_stat_get_value((memcached_st *)memc, &stats, *ptr, &ret);
    ctx->generator->push(value);
    free(value);
  }
  free(list);

  return MEMCACHED_SUCCESS;
}


StatsTableTool::StatsTableTool() :
  plugin::TableFunction("DATA_DICTIONARY", "MEMCACHED_STATS")
{
  add_field("NAME");
  add_field("PORT_NUMBER", plugin::TableFunction::NUMBER);
  add_field("PROCESS_ID", plugin::TableFunction::NUMBER);
  add_field("UPTIME", plugin::TableFunction::NUMBER);
  add_field("TIME", plugin::TableFunction::NUMBER);
  add_field("VERSION");
  add_field("POINTER_SIZE", plugin::TableFunction::NUMBER);
  add_field("RUSAGE_USER", plugin::TableFunction::NUMBER);
  add_field("RUSAGE_SYSTEM", plugin::TableFunction::NUMBER);
  add_field("CURRENT_ITEMS", plugin::TableFunction::NUMBER);
  add_field("TOTAL_ITEMS", plugin::TableFunction::NUMBER);
  add_field("BYTES",  plugin::TableFunction::NUMBER);
  add_field("CURRENT_CONNECTIONS", plugin::TableFunction::NUMBER);
  add_field("TOTAL_CONNECTIONS", plugin::TableFunction::NUMBER);
  add_field("CONNECTION_STRUCTURES", plugin::TableFunction::NUMBER);
  add_field("GETS", plugin::TableFunction::NUMBER);
  add_field("SETS", plugin::TableFunction::NUMBER);
  add_field("HITS", plugin::TableFunction::NUMBER);
  add_field("MISSES", plugin::TableFunction::NUMBER); 
  add_field("EVICTIONS", plugin::TableFunction::NUMBER);
  add_field("BYTES_READ", plugin::TableFunction::NUMBER);
  add_field("BYTES_WRITTEN", plugin::TableFunction::NUMBER);
  add_field("LIMIT_MAXBYTES", plugin::TableFunction::NUMBER);
  add_field("THREADS", plugin::TableFunction::NUMBER);
}


StatsTableTool::Generator::Generator(drizzled::Field **arg) :
  plugin::TableFunction::Generator(arg)
{
  /* This will be set to the real number if we initialize properly below */
  number_of_hosts= 0;
  
  host_number= 0;

  /* set to NULL if we are not able to init we dont want to call delete on this */
  memc= NULL;

  drizzled::sys_var *servers_var= drizzled::find_sys_var("memcached_stats_servers");
  assert(servers_var != NULL);

  const string servers_string(static_cast<char *>(servers_var.value_ptr(NULL, 0, NULL)));

  if (servers_string.empty())
  {
    my_printf_error(ER_UNKNOWN_ERROR, _("No value in MEMCACHED_STATS_SERVERS variable."), MYF(0));
    return; 
  }

  memc= memcached_create(NULL);
  if (memc == NULL)
  {
    my_printf_error(ER_UNKNOWN_ERROR, _("Unable to create memcached struct.  Got error from memcached_create()."), MYF(0));
    return;
  }

  memcached_server_st *tmp_serv=
    memcached_servers_parse(servers_string.c_str());
  if (tmp_serv == NULL)
  {
    my_printf_error(ER_UNKNOWN_ERROR, _("Unable to create memcached server list.  Got error from memcached_servers_parse(%s)."), MYF(0), servers_string.c_str());
    return;
  }

  memcached_server_push(memc, tmp_serv);
  memcached_server_list_free(tmp_serv);

  number_of_hosts= memc->number_of_hosts;  
}


StatsTableTool::Generator::~Generator()
{
  if (memc != NULL)
  {
    memcached_free(memc);
  }
}


bool StatsTableTool::Generator::populate()
{
  if (host_number == number_of_hosts)
  {
    return false;
  }

  server_function_context context(this);

  memcached_server_function callbacks[1];
  callbacks[0]= server_function;

  unsigned int iferror; 
  iferror= (*callbacks[0])(memc, &memc->servers[host_number], (void *)&context); 

  if (iferror)
  {
    return false;
  }

  host_number++;
 
  return true;
}

} /* namespace drizzle_plugin */
