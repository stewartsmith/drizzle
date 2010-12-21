/* 
 * Copyright (C) 2010 Djellel Eddine Difallah
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
 *   * Neither the name of Djellel Eddine Difallah nor the names of its contributors
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
#ifndef PLUGIN_MEMCACHED_QUERY_CACHE_MEMCACHED_QC_H
#define PLUGIN_MEMCACHED_QUERY_CACHE_MEMCACHED_QC_H

#include "drizzled/plugin/query_cache.h"
#include "query_cache_service.h"
#include "drizzled/atomics.h"
#include <libmemcached/memcached.hpp>

namespace drizzled 
{
  class Session;
  class select_result;
  class String;
  class TableList;
}
class MemcachedQueryCache : public drizzled::plugin::QueryCache
{
private:
  pthread_mutex_t mutex;  
  drizzled::QueryCacheService queryCacheService;
  static memcache::Memcache* client;
  static std::string memcached_servers;

public:
  
  explicit MemcachedQueryCache(std::string name_arg, const std::string &servers_arg): drizzled::plugin::QueryCache(name_arg)
  {
    client= new memcache::Memcache(servers_arg);
    pthread_mutex_init(&mutex, NULL);
    queryCacheService= drizzled::QueryCacheService::singleton();
  }
  ~MemcachedQueryCache()
  {
    delete client;
    pthread_mutex_destroy(&mutex);
  };
  bool doIsCached(drizzled::Session *session);
  bool doSendCachedResultset(drizzled::Session *session);
  bool doPrepareResultset(drizzled::Session *session);
  bool doInsertRecord(drizzled::Session *session, drizzled::List<drizzled::Item> &item);
  bool doSetResultset(drizzled::Session *session);
  char* md5_key(const char* str);
  void checkTables(drizzled::Session *session, drizzled::TableList* in_table);
  bool isSelect(std::string query);
  static memcache::Memcache* getClient()
  {
    return client;
  }
  static const char *getServers()
  {
    return memcached_servers.c_str();
  }
  static void setServers(const std::string &server_list)
  {
    memcached_servers.assign(server_list);
    //getClient()->setServers(server_list);
  }

};
#endif /* PLUGIN_MEMCACHED_QUERY_CACHE_MEMCACHED_QC_H */
