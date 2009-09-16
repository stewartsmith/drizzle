/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef DRIZZLED_PLUGIN_REGISTRY_H
#define DRIZZLED_PLUGIN_REGISTRY_H

#include "drizzled/slot/function.h"
#include "drizzled/slot/listen.h"

#include <string>
#include <vector>
#include <map>

class StorageEngine;
class InfoSchemaTable;
class Logging_handler;
class Error_message_handler;
class Authentication;
class QueryCache;

namespace drizzled
{
namespace plugin
{
class CommandReplicator;
class CommandApplier;
class Handle;

class Registry
{
private:
  std::map<std::string, Handle *>
    plugin_map;

  Registry() {}
  Registry(const Registry&);
public:

  static plugin::Registry& singleton()
  {
    static plugin::Registry registry;
    return registry;
  }

  Handle *find(const LEX_STRING *name);

  void add(Handle *plugin);

  std::vector<Handle *> get_list(bool active);

  void add(StorageEngine *engine);
  void add(InfoSchemaTable *schema_table);
  void add(Logging_handler *handler);
  void add(Error_message_handler *handler);
  void add(Authentication *auth);
  void add(QueryCache *qcache);
  void add(SchedulerFactory *scheduler);
  void add(drizzled::plugin::CommandReplicator *replicator);
  void add(drizzled::plugin::CommandApplier *applier);

  void remove(StorageEngine *engine);
  void remove(InfoSchemaTable *schema_table);
  void remove(Logging_handler *handler);
  void remove(Error_message_handler *handler);
  void remove(Authentication *auth);
  void remove(QueryCache *qcache);
  void remove(SchedulerFactory *scheduler);
  void remove(drizzled::plugin::CommandReplicator *replicator);
  void remove(drizzled::plugin::CommandApplier *applier);

  ::drizzled::slot::Function function;
  ::drizzled::slot::Listen listen;
};

} /* end namespace plugin */
} /* end namespace drizzled */
#endif /* DRIZZLED_PLUGIN_REGISTRY_H */
