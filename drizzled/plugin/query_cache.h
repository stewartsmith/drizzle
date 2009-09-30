/*
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

 *  Definitions required for Query Cache plugin

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

namespace drizzled
{
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
public:
  explicit QueryCache(std::string name_arg): Plugin(name_arg) {}

  virtual ~QueryCache() {}
  /* Lookup the cache and transmit the data back to the client */
  virtual bool try_fetch_and_send(Session *session,
                                  bool is_transactional)= 0;

  virtual bool set(Session *session, bool is_transactional)= 0;
  virtual bool invalidate_table(Session *session, bool is_transactional)= 0;
  virtual bool invalidate_db(Session *session, const char *db_name,
                             bool transactional)= 0;
  virtual bool flush(Session *session)= 0;

  static void add(QueryCache *handler);
  static void remove(QueryCache *handler);

  /* These are the functions called by the rest of the Drizzle server */
  static bool do_try_fetch_and_send(Session *session, bool transactional);
  static bool do_set(Session *session, bool transactional);
  static bool do_invalidate_table(Session *session, bool transactional);
  static bool do_invalidate_db(Session *session, const char *db_name,
                            bool transactional);
  static bool do_flush(Session *session);
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_QUERY_CACHE_H */
