/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for Query Cache plugin
 *
 *  Copyright (C) 2008 Sun Microsystems, Toru Maesaka
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#ifndef DRIZZLED_PLUGIN_QUERY_CACHE_H
#define DRIZZLED_PLUGIN_QUERY_CACHE_H

#include "drizzled/plugin/plugin.h"

namespace drizzled
{
class Session;

namespace plugin
{

/* 
  This is the API that a qcache plugin must implement.
  it should implement each of these function pointers.
  if a function pointer is NULL (not loaded), that's ok.

  Return:
    false = success
    true  = failure
*/
class QueryCache : public Plugin
{
  QueryCache();
  QueryCache(const QueryCache &);
  QueryCache& operator=(const QueryCache &);
public:
  explicit QueryCache(std::string name_arg)
    : Plugin(name_arg, "QueryCache")
  {}

  virtual ~QueryCache() {}
  /* Lookup the cache and transmit the data back to the client */
  virtual bool tryFetchAndSend(Session *session,
                               bool is_transactional)= 0;

  virtual bool set(Session *session, bool is_transactional)= 0;
  virtual bool invalidateTable(Session *session, bool is_transactional)= 0;
  virtual bool invalidateDb(Session *session, const char *db_name,
                            bool transactional)= 0;
  virtual bool flush(Session *session)= 0;

  static bool addPlugin(QueryCache *handler);
  static void removePlugin(QueryCache *handler);

  /* These are the functions called by the rest of the Drizzle server */
  static bool tryFetchAndSendDo(Session *session, bool transactional);
  static bool setDo(Session *session, bool transactional);
  static bool invalidateTableDo(Session *session, bool transactional);
  static bool invalidateDbDo(Session *session, const char *db_name,
                            bool transactional);
  static bool flushDo(Session *session);
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_QUERY_CACHE_H */
