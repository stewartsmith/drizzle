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

#include <string>
#include <vector>
#include <map>

#include "drizzled/service/authentication.h"
#include "drizzled/service/scheduler.h"
#include "drizzled/service/function.h"
#include "drizzled/service/listen.h"
#include "drizzled/service/query_cache.h"
#include "drizzled/service/logging.h"
#include "drizzled/service/error_message.h"
#include "drizzled/service/info_schema.h"
#include "drizzled/service/command_replicator.h"
#include "drizzled/service/command_applier.h"
#include "drizzled/service/storage_engine.h"


namespace drizzled
{
namespace plugin
{
class Handle;
class Plugin;

class Registry
{
private:
  std::map<std::string, Handle *> handle_map;
  ::drizzled::Registry<const Plugin *> plugin_registry;

  Handle *current_handle;

  Registry()
   : handle_map(), plugin_registry(), current_handle(NULL)
  { }

  Registry(const Registry&);
public:

  static plugin::Registry& singleton()
  {
    static plugin::Registry registry;
    return registry;
  }

  Handle *find(const LEX_STRING *name);

  void add(Handle *handle);
  void add(Plugin *plugin);
  void remove(const Plugin *plugin);

  void setCurrentHandle(Handle *plugin)
  {
    current_handle= plugin;
  }

  void clearCurrentHandle()
  {
    current_handle= NULL;
  }

  std::vector<Handle *> get_list(bool active);

  service::CommandReplicator command_replicator;
  service::CommandApplier command_applier;
  service::ErrorMessage error_message;
  service::Authentication authentication;
  service::QueryCache query_cache;
  service::Scheduler scheduler;
  service::Function function;
  service::Listen listen;
  service::Logging logging;
  service::InfoSchema info_schema;
  service::StorageEngine storage_engine;

  void add(CommandReplicator *plugin)
  {
    command_replicator.add(plugin);
  }
  void add(CommandApplier *plugin)
  {
    command_applier.add(plugin);
  }
  void add(ErrorMessage *plugin)
  {
    error_message.add(plugin);
  }
  void add(Authentication *plugin)
  {
    authentication.add(plugin);
  }
  void add(QueryCache *plugin)
  {
    query_cache.add(plugin);
  }
  void add(SchedulerFactory *plugin)
  {
    scheduler.add(plugin);
  }
  void add(Function *plugin)
  {
    function.add(plugin);
  }
  void add(Listen *plugin)
  {
    listen.add(plugin);
  }
  void add(Logging *plugin)
  {
    logging.add(plugin);
  }
  void add(InfoSchemaTable *plugin)
  {
    info_schema.add(plugin);
  }
  void add(StorageEngine *plugin)
  {
    storage_engine.add(plugin);
  }

  void remove(CommandReplicator *plugin)
  {
    command_replicator.remove(plugin);
  }
  void remove(CommandApplier *plugin)
  {
    command_applier.remove(plugin);
  }
  void remove(ErrorMessage *plugin)
  {
    error_message.remove(plugin);
  }
  void remove(Authentication *plugin)
  {
    authentication.remove(plugin);
  }
  void remove(QueryCache *plugin)
  {
    query_cache.remove(plugin);
  }
  void remove(SchedulerFactory *plugin)
  {
    scheduler.remove(plugin);
  }
  void remove(Function *plugin)
  {
    function.remove(plugin);
  }
  void remove(Listen *plugin)
  {
    listen.remove(plugin);
  }
  void remove(Logging *plugin)
  {
    logging.remove(plugin);
  }
  void remove(InfoSchemaTable *plugin)
  {
    info_schema.remove(plugin);
  }
  void remove(StorageEngine *plugin)
  {
    storage_engine.remove(plugin);
  }

  

};

} /* end namespace plugin */
} /* end namespace drizzled */
#endif /* DRIZZLED_PLUGIN_REGISTRY_H */
