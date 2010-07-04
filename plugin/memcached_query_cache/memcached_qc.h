/* 
 * Copyright (c) 2010, Djellel Eddine Difallah
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
#ifndef MEMCACHED_QC_H
#define MEMCACHED_QC_H

#include "drizzled/plugin/query_cache.h"
#include "query_cache_service.h"
#include "drizzled/atomics.h"
#include <libmemcached/memcached.hpp>

namespace drizzled 
{
  class Session;
  class select_result;
  class String;
}
class Memcached_Qc : public drizzled::plugin::QueryCache
{
private:
  pthread_mutex_t mutex;  
  memcache::Memcache* client;
  drizzled::QueryCacheService queryCacheService;

public:
  explicit Memcached_Qc(std::string name_arg): drizzled::plugin::QueryCache(name_arg)
  {
    client= new memcache::Memcache("localhost:11211");
    queryCacheService= drizzled::QueryCacheService::singleton();
  }
  ~Memcached_Qc(){}
  bool doIsCached(drizzled::Session *session);
  bool doSendCachedResultset(drizzled::Session *session);
  bool doPrepareResultset(drizzled::Session *session);
  bool doInsertRecord(drizzled::Session *session, drizzled::List<drizzled::Item> &item);
  bool doSetResultset(drizzled::Session *session);
  char* md5_key(const char* str);
  bool isSelect(std::string query);
};
#endif /* MEMCACHED_QC_h */
