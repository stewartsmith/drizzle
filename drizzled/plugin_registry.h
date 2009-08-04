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

class StorageEngine;
class InfoSchemaTable;
class Function_builder;
class Logging_handler;
class Error_message_handler;
class Authentication;
class QueryCache;
namespace drizzled
{
namespace plugin
{
class Handle;
class SchedulerFactory;
class Listen;
class Replicator;
}
}

class PluginRegistry
{
private:
  std::map<std::string, drizzled::plugin::Handle *>
    plugin_map;

  PluginRegistry(const PluginRegistry&);
public:
  PluginRegistry() {}

  drizzled::plugin::Handle *find(const LEX_STRING *name);

  void add(drizzled::plugin::Handle *plugin);

  std::vector<drizzled::plugin::Handle *> get_list(bool active);
  static PluginRegistry& getPluginRegistry();

  void add(StorageEngine *engine);
  void add(InfoSchemaTable *schema_table);
  void add(Function_builder *udf);
  void add(Logging_handler *handler);
  void add(Error_message_handler *handler);
  void add(Authentication *auth);
  void add(QueryCache *qcache);
  void add(drizzled::plugin::SchedulerFactory *scheduler);
  void add(const drizzled::plugin::Listen &listen_obj);
  void add(drizzled::plugin::Replicator *repl);

  void remove(StorageEngine *engine);
  void remove(InfoSchemaTable *schema_table);
  void remove(Function_builder *udf);
  void remove(Logging_handler *handler);
  void remove(Error_message_handler *handler);
  void remove(Authentication *auth);
  void remove(QueryCache *qcache);
  void remove(drizzled::plugin::SchedulerFactory *scheduler);
  void remove(const drizzled::plugin::Listen &listen_obj);
  void remove(drizzled::plugin::Replicator *repl);
};

#endif /* DRIZZLED_PLUGIN_REGISTRY_H */
