/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (c) 2009, Patrick "CaptTofu" Galbraith, Padraig O'Sullivan
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

#include <drizzled/server_includes.h>
#include <drizzled/function/str/strfunc.h>
#include <drizzled/plugin/function.h>

#include "memcached_functions.h"
#include "memc_servers_set.h"
#include "memc_behavior_set.h"
#include "memc_behavior_get.h"
#include "memc_stats.h"
#include "memc_get.h"
#include "memc_get_by_key.h"
#include "memc_set.h"
#include "memc_set_by_key.h"
#include "memc_add.h"
#include "memc_add_by_key.h"
#include "memc_replace.h"
#include "memc_replace_by_key.h"
#include "memc_delete.h"
#include "memc_delete_by_key.h"
#include "memc_append.h"
#include "memc_append_by_key.h"
#include "memc_prepend.h"
#include "memc_prepend_by_key.h"
#include "memc_cas.h"
#include "memc_cas_by_key.h"
#include "memc_increment.h"
#include "memc_decrement.h"
#include "memc_misc.h"

#include <libmemcached/memcached.h>

#include <string>

using namespace std;
using namespace drizzled;

/*
 * A global memcached data structure needed by
 * the various libmemcached API functions.
 */
memcached_st *memc= NULL;

/*
 * The memcached UDF's.
 */
plugin::Create_function<MemcachedServersSet> *memc_servers_set= NULL;
plugin::Create_function<MemcachedBehaviorSet> *memc_behavior_set= NULL;
plugin::Create_function<MemcachedBehaviorGet> *memc_behavior_get= NULL;
plugin::Create_function<MemcachedStats> *memc_stats= NULL;
plugin::Create_function<MemcachedGet> *memc_get= NULL;
plugin::Create_function<MemcachedGetByKey> *memc_get_by_key= NULL;
plugin::Create_function<MemcachedSet> *memc_set= NULL;
plugin::Create_function<MemcachedSetByKey> *memc_set_by_key= NULL;
plugin::Create_function<MemcachedAdd> *memc_add= NULL;
plugin::Create_function<MemcachedAddByKey> *memc_add_by_key= NULL;
plugin::Create_function<MemcachedReplace> *memc_replace= NULL;
plugin::Create_function<MemcachedReplaceByKey> *memc_replace_by_key= NULL;
plugin::Create_function<MemcachedIncrement> *memc_increment= NULL;
plugin::Create_function<MemcachedDecrement> *memc_decrement= NULL;
plugin::Create_function<MemcachedDelete> *memc_delete= NULL;
plugin::Create_function<MemcachedDeleteByKey> *memc_delete_by_key= NULL;
plugin::Create_function<MemcachedAppend> *memc_append= NULL;
plugin::Create_function<MemcachedAppendByKey> *memc_append_by_key= NULL;
plugin::Create_function<MemcachedPrepend> *memc_prepend= NULL;
plugin::Create_function<MemcachedPrependByKey> *memc_prepend_by_key= NULL;
plugin::Create_function<MemcachedCas> *memc_cas= NULL;
plugin::Create_function<MemcachedCasByKey> *memc_cas_by_key= NULL;
plugin::Create_function<MemcachedServerCount> *memc_serv_count= NULL;
plugin::Create_function<MemcachedVersion> *memc_version= NULL;

bool initMemcUDF()
{
  memc_servers_set= new plugin::Create_function<MemcachedServersSet>("memc_servers_set");
  if (memc_servers_set == NULL)
  {
    return true;
  }

  memc_behavior_set= new plugin::Create_function<MemcachedBehaviorSet>("memc_behavior_set");
  if (memc_behavior_set == NULL)
  {
    return true;
  }

  memc_behavior_get= new plugin::Create_function<MemcachedBehaviorGet>("memc_behavior_get");
  if (memc_behavior_get == NULL)
  {
    return true;
  }

  memc_stats= new plugin::Create_function<MemcachedStats>("memc_stats");
  if (memc_stats == NULL)
  {
    return true;
  }

  memc_get= new plugin::Create_function<MemcachedGet>("memc_get");
  if (memc_get == NULL)
  {
    return true;
  }

  memc_get_by_key= new plugin::Create_function<MemcachedGetByKey>("memc_get_by_key");
  if (memc_get_by_key == NULL)
  {
    return true;
  }

  memc_set= new plugin::Create_function<MemcachedSet>("memc_set");
  if (memc_set == NULL)
  {
    return true;
  }
  memc_set_by_key= new plugin::Create_function<MemcachedSetByKey>("memc_set_by_key");
  if (memc_set_by_key == NULL)
  {
    return true;
  }

  memc_add= new plugin::Create_function<MemcachedAdd>("memc_add");
  if (memc_add== NULL)
  {
    return true;
  }

  memc_add_by_key= new plugin::Create_function<MemcachedAddByKey>("memc_add_by_key");
  if (memc_add_by_key == NULL)
  {
    return true;
  }

  memc_replace= new plugin::Create_function<MemcachedReplace>("memc_replace");
  if (memc_replace== NULL)
  {
    return true;
  }

  memc_replace_by_key= new plugin::Create_function<MemcachedReplaceByKey>("memc_replace_by_key");
  if (memc_replace_by_key == NULL)
  {
    return true;
  }

  memc_delete= new plugin::Create_function<MemcachedDelete>("memc_delete");
  if (memc_delete == NULL)
  {
    return true;
  }

  memc_delete_by_key= new plugin::Create_function<MemcachedDeleteByKey>("memc_delete_by_key");
  if (memc_delete_by_key == NULL)
  {
    return true;
  }

  memc_append= new plugin::Create_function<MemcachedAppend>("memc_append");
  if (memc_append == NULL)
  {
    return true;
  }

  memc_append_by_key= new plugin::Create_function<MemcachedAppendByKey>("memc_append_by_key");
  if (memc_append_by_key == NULL)
  {
    return true;
  }

  memc_prepend= new plugin::Create_function<MemcachedPrepend>("memc_prepend");
  if (memc_prepend == NULL)
  {
    return true;
  }

  memc_prepend_by_key= new plugin::Create_function<MemcachedPrependByKey>("memc_prepend_by_key");
  if (memc_prepend_by_key == NULL)
  {
    return true;
  }

  memc_cas= new plugin::Create_function<MemcachedCas>("memc_cas");
  if (memc_cas == NULL)
  {
    return true;
  }

  memc_cas_by_key= new plugin::Create_function<MemcachedCasByKey>("memc_cas_by_key");
  if (memc_cas_by_key == NULL)
  {
    return true;
  }

  memc_serv_count= new plugin::Create_function<MemcachedServerCount>("memc_server_count");
  if (memc_serv_count == NULL)
  {
    return true;
  }

  memc_version= new plugin::Create_function<MemcachedVersion>("memc_libmemcached_version");
  if (memc_version == NULL)
  {
    return true;
  }

  memc_increment= new plugin::Create_function<MemcachedIncrement>("memc_increment");
  if (memc_increment == NULL)
  {
    return true;
  }

  memc_decrement= new plugin::Create_function<MemcachedDecrement>("memc_decrement");
  if (memc_decrement == NULL)
  {
    return true;
  }

  return false;
}

void cleanupMemcUDF()
{
  delete memc_servers_set;
  delete memc_behavior_set;
  delete memc_behavior_get;
  delete memc_stats;
  delete memc_get;
  delete memc_get_by_key;
  delete memc_set;
  delete memc_set_by_key;
  delete memc_add;
  delete memc_add_by_key;
  delete memc_replace;
  delete memc_replace_by_key;
  delete memc_delete;
  delete memc_delete_by_key;
  delete memc_append;
  delete memc_append_by_key;
  delete memc_prepend;
  delete memc_prepend_by_key;
  delete memc_cas;
  delete memc_cas_by_key;
  delete memc_serv_count;
  delete memc_version;
  delete memc_increment;
  delete memc_decrement;
}

static int memcachedInit(drizzled::plugin::Registry &registry)
{
  if (initMemcUDF())
  {
    return 1;
  }

  memc= memcached_create(NULL);

  registry.add(memc_servers_set);
  registry.add(memc_behavior_set);
  registry.add(memc_behavior_get);
  registry.add(memc_stats);
  registry.add(memc_get);
  registry.add(memc_get_by_key);
  registry.add(memc_set);
  registry.add(memc_set_by_key);
  registry.add(memc_add);
  registry.add(memc_add_by_key);
  registry.add(memc_replace);
  registry.add(memc_replace_by_key);
  registry.add(memc_delete);
  registry.add(memc_delete_by_key);
  registry.add(memc_append);
  registry.add(memc_append_by_key);
  registry.add(memc_prepend);
  registry.add(memc_prepend_by_key);
  registry.add(memc_cas);
  registry.add(memc_cas_by_key);
  registry.add(memc_serv_count);
  registry.add(memc_version);
  registry.add(memc_increment);
  registry.add(memc_decrement);

  return 0;
}

static int memcachedDone(drizzled::plugin::Registry &registry)
{

  memcached_free(memc);

  registry.remove(memc_servers_set);
  registry.remove(memc_behavior_set);
  registry.remove(memc_behavior_get);
  registry.remove(memc_stats);
  registry.remove(memc_get);
  registry.remove(memc_get_by_key);
  registry.remove(memc_set);
  registry.remove(memc_set_by_key);
  registry.remove(memc_add);
  registry.remove(memc_add_by_key);
  registry.remove(memc_replace);
  registry.remove(memc_replace_by_key);
  registry.remove(memc_delete);
  registry.remove(memc_delete_by_key);
  registry.remove(memc_append);
  registry.remove(memc_append_by_key);
  registry.remove(memc_prepend);
  registry.remove(memc_prepend_by_key);
  registry.remove(memc_cas);
  registry.remove(memc_cas_by_key);
  registry.remove(memc_serv_count);
  registry.remove(memc_version);
  registry.remove(memc_increment);
  registry.remove(memc_decrement);

  cleanupMemcUDF();

  return 0;
}

drizzle_declare_plugin(memcached_functions)
{
  "memcached_functions",
  "0.1",
  "Patrick Galbraith, Ronald Bradford, Padraig O'Sullivan",
  "Memcached UDF Plugin",
  PLUGIN_LICENSE_GPL,
  memcachedInit,
  memcachedDone,
  NULL,
  NULL,
  NULL
}
drizzle_declare_plugin_end;
