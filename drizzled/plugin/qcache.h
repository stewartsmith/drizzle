/*
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

 *  Definitions required for Query Cache plugin

 *  Copyright (C) 2008 Mark Atwood, Toru Maesaka
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

/* 
  This is the API that a qcache plugin must implement.
  it should implement each of these function pointers.
  if a function pointer is NULL (not loaded), that's ok.

  Return:
    false = success
    true  = failure
*/
class QueryCache
{
  std::string name;
public:
  QueryCache(std::string name_arg): name(name_arg) {}
  QueryCache(const char *name_arg): name(name_arg) {}

  std::string getName() { return name; }

  virtual ~QueryCache() {}
  /* Lookup the cache and transmit the data back to the client */
  virtual bool try_fetch_and_send(Session *session,
                                  bool is_transactional)= 0;

  virtual bool set(Session *session, bool is_transactional)= 0;
  virtual bool invalidate_table(Session *session, bool is_transactional)= 0;
  virtual bool invalidate_db(Session *session, const char *db_name,
                             bool transactional)= 0;
  virtual bool flush(Session *session)= 0;
};

#endif /* DRIZZLED_PLUGIN_QUERY_CACHE_H */
