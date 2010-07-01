/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for Query Cache plugin
 *
 *  Copyright (C) 2008 Sun Microsystems, Toru Maesaka, Djellel Eddine Difallah
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

#include "drizzled/plugin.h"
#include "drizzled/plugin/plugin.h"
#include <drizzled/sql_list.h>

namespace drizzled
{
class Session;
class select_result;

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
  
  /* these are the Query Cache interface functions */
  
  /* Lookup the cache and transmit the data back to the client */
  virtual bool tryFetchAndSend(Session *session)= 0;
  /* Send the current Resultset to the cache */
  virtual bool setResultset(Session *session)= 0;
  /* initiate a new Resultset (header) */
  virtual bool prepareResultset(Session *session)= 0;
  /* push a record to the current Resultset */
  virtual bool insertRecord(Session *session, List<Item> &item)= 0;
  
  static bool addPlugin(QueryCache *handler);
  static void removePlugin(QueryCache *handler);

  /* These are the functions called by the rest of the Drizzle server */
  static bool tryFetchAndSendDo(Session *session);
  static bool prepareResultsetDo(Session *session);
  static bool setResultsetDo(Session *session);
  static bool insertRecordDo(Session *session, List<Item> &item);
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_QUERY_CACHE_H */
