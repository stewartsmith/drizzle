/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2009, Patrick "CaptTofu" Galbraith, Padraig O'Sullivan
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
 *   * Neither the name of Patrick Galbraith nor the names of its contributors
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
#include <drizzled/item/func.h>
#include <drizzled/function/str/strfunc.h>

#include "memcached_functions.h"
#include "memc_stats.h"

#include <libmemcached/memcached.h>

#include <string>
#include <algorithm>

using namespace std;
using namespace drizzled;

void MemcachedStats::setFailureString(const char *error)
{
  size_t size= strlen(error);
  failure_buff.realloc(size);
  failure_buff.length(size);
  memcpy(failure_buff.ptr(), error, size);
}

String *MemcachedStats::val_str(String *str)
{
  memcached_return rc;
  unsigned int count;
  char buff[100];
  memcached_stat_st *stat;
  memcached_server_st *servers;
  memcached_server_st *server_list;
  String *server_names;


  if (arg_count != 1 ||
      ! (server_names= args[0]->val_str(str)) ||
      ! memc)
  {
    setFailureString("USAGE: memc_stats('<server list>')");
    return &failure_buff;
  }

  servers= memcached_servers_parse(server_names->c_ptr());
  if (servers == NULL)
  {
    setFailureString(" ERROR: unable to parse servers string!");
    return &failure_buff;
  }
  memcached_server_push(memc, servers);
  memcached_server_list_free(servers);

  stat= memcached_stat(memc, NULL, &rc);

  if (rc != MEMCACHED_SUCCESS && rc != MEMCACHED_SOME_ERRORS)
  {
    snprintf(buff, 100, "Failure to communicate with servers (%s)\n",
            memcached_strerror(memc, rc));

    setFailureString(buff);
    return &failure_buff;
  }

  server_list= memcached_server_list(memc);

  results_buff.length(0);
  snprintf(buff, 100, "Listing %u Server\n\n", memcached_server_count(memc));
  results_buff.append(buff);

  for (count= 0; count < memcached_server_count(memc); count++)
  {
    char **list;
    char **ptr;

    list= memcached_stat_get_keys(memc, &stat[count], &rc);

    snprintf(buff, 100, "Server: %s (%u)\n",
            memcached_server_name(memc, server_list[count]),
            memcached_server_port(memc, server_list[count]));


    results_buff.append(buff);

    for (ptr= list; *ptr; ptr++)
    {
      char *value= memcached_stat_get_value(memc, &stat[count], *ptr, &rc);

      snprintf(buff, 100, "\t %s: %s\n", *ptr, value);
      free(value);
      results_buff.append(buff);
    }

    free(list);
    results_buff.append("\n");
  }

  free(stat);

  return &results_buff;

}
