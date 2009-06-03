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

struct st_plugin_int;
class StorageEngine;
struct InfoSchemaTable;
class Function_builder;
class Logging_handler;
class Error_message_handler;
class Authentication;
class QueryCache;
class SchedulerFactory;
class ProtocolFactory;
namespace drizzled
{
namespace plugin
{
class Replicator;
class Applier;
}
}

class PluginRegistry
{
private:
  std::map<std::string, st_plugin_int *>
    plugin_map;

  PluginRegistry(const PluginRegistry&);
public:
  PluginRegistry() {}


  st_plugin_int *find(const LEX_STRING *name);

  void add(st_plugin_int *plugin);

  std::vector<st_plugin_int *> get_list(bool active);
  static PluginRegistry& getPluginRegistry();

  void add(StorageEngine *engine);
  void add(InfoSchemaTable *schema_table);
  void add(Function_builder *udf);
  void add(Logging_handler *handler);
  void add(Error_message_handler *handler);
  void add(Authentication *auth);
  void add(QueryCache *qcache);
  void add(SchedulerFactory *scheduler);
  void add(ProtocolFactory *protocol);
  void add(drizzled::plugin::Replicator *replicator);
  void add(drizzled::plugin::Applier *applier);

  void remove(StorageEngine *engine);
  void remove(InfoSchemaTable *schema_table);
  void remove(Function_builder *udf);
  void remove(Logging_handler *handler);
  void remove(Error_message_handler *handler);
  void remove(Authentication *auth);
  void remove(QueryCache *qcache);
  void remove(SchedulerFactory *scheduler);
  void remove(ProtocolFactory *protocol);
  void remove(drizzled::plugin::Replicator *replicator);
  void remove(drizzled::plugin::Applier *applier);

};

#endif /* DRIZZLED_PLUGIN_REGISTRY_H */
