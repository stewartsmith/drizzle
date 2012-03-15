/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Toru Maesaka
 *  Copyright (C) 2010 Djellel Eddine Difallah
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

#pragma once

#include <drizzled/plugin.h>
#include <drizzled/plugin/plugin.h>
#include <drizzled/sql_list.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

/* 
  This is the API that a qcache plugin must implement.
*/

class DRIZZLED_API QueryCache : public Plugin
{
public:  
  explicit QueryCache(const std::string& name)
    : Plugin(name, "QueryCache")
  {}

  /* these are the Query Cache interface functions */

  /* Lookup the cache and transmit the data back to the client */
  virtual bool doIsCached(Session*)= 0;  
  /* Lookup the cache and transmit the data back to the client */
  virtual bool doSendCachedResultset(Session*)= 0;
  /* Send the current Resultset to the cache */
  virtual bool doSetResultset(Session*)= 0;
  /* initiate a new Resultset (header) */
  virtual bool doPrepareResultset(Session*)= 0;
  /* push a record to the current Resultset */
  virtual bool doInsertRecord(Session*, List<Item>&)= 0;

  static bool addPlugin(QueryCache*);
  static void removePlugin(QueryCache*);

  /* These are the functions called by the rest of the Drizzle server */
  static bool isCached(Session*);
  static bool sendCachedResultset(Session*);
  static bool prepareResultset(Session*);
  static bool setResultset(Session*);
  static bool insertRecord(Session*, List<Item>&);
};

} /* namespace plugin */
} /* namespace drizzled */
