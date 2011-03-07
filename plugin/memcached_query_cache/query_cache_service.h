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
#ifndef PLUGIN_MEMCACHED_QUERY_CACHE_QUERY_CACHE_SERVICE_H
#define PLUGIN_MEMCACHED_QUERY_CACHE_QUERY_CACHE_SERVICE_H

#include <drizzled/message/resultset.pb.h>
#include <drizzled/sql_list.h>
#include <map>
#include <vector>

namespace drizzled
{
class TableList;
class Item;

namespace message
{
class Resultset;
}

class QueryCacheService
{
public:

  typedef std::map<std::string, drizzled::message::Resultset> CacheEntries;
  typedef std::pair<const std::string, drizzled::message::Resultset> CacheEntry;
  typedef std::map<std::string, std::vector<std::string>> CachedTablesEntries;
  typedef std::pair<const std::string, std::vector<std::string>> CachedTablesEntry;

  static const size_t DEFAULT_RECORD_SIZE= 100;
  static CacheEntries cache;
  static CachedTablesEntries cachedTables;

  /**
   * Singleton method
   * Returns the singleton instance of QueryCacheService
   */
  static inline QueryCacheService &singleton()
  {
    static QueryCacheService query_cache_service;
    return query_cache_service;
  };

  /**
   * Method which returns the active Resultset message
   * for the supplied Session.  If one is not found, a new Resultset
   * message is allocated, initialized, and returned.
   *
   * @param The session processing the Select
   */
  drizzled::message::Resultset *setCurrentResultsetMessage(drizzled::Session *in_session);

  /**
   * Helper method which initializes the header message for
   * a Resultset.
   *
   * @param[inout] Resultset message container to modify
   * @param[in] Pointer to the Session doing the processing
   * @param[in] Pointer to the Table being inserted into
   */
  void setResultsetHeader(drizzled::message::Resultset &resultset,
                       drizzled::Session *in_session,
                       drizzled::TableList *in_table);
  /**
   * Creates a new SelectRecord GPB message and pushes it to
   * currrent Resultset.
   *
   * @param Pointer to the Session which has inserted a record
   * @param Pointer to the List<Items> to add
   */
  bool addRecord(drizzled::Session *in_session, drizzled::List<drizzled::Item> &list);

  /* cache fetching */
  static bool isCached(std::string query);
  
}; 
} /* namespace drizzled */
#endif /* PLUGIN_MEMCACHED_QUERY_CACHE_QUERY_CACHE_SERVICE_H */
