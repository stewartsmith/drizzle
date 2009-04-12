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

#include "drizzled/server_includes.h"
#include "drizzled/plugin_registry.h"

#include "drizzled/plugin.h"
#include "drizzled/show.h"
#include "drizzled/handler.h"
#include "drizzled/errmsg.h"
#include "drizzled/authentication.h"
#include "drizzled/qcache.h"
#include "drizzled/scheduling.h"
#include "drizzled/logging.h"
#include "drizzled/sql_udf.h"
#include "drizzled/protocol.h"
#include "drizzled/transaction_services.h"

#include <string>
#include <vector>
#include <map>

using namespace std;

static PluginRegistry the_registry;
PluginRegistry& PluginRegistry::getPluginRegistry()
{
  return the_registry;
}

st_plugin_int *PluginRegistry::find(const LEX_STRING *name)
{
  string find_str(name->str,name->length);
  transform(find_str.begin(), find_str.end(), find_str.begin(), ::tolower);

  map<string, st_plugin_int *>::iterator map_iter;
  map_iter= plugin_map.find(find_str);
  if (map_iter != plugin_map.end())
    return (*map_iter).second;
  return(0);
}

void PluginRegistry::add(st_plugin_int *plugin)
{
  string add_str(plugin->name.str);
  transform(add_str.begin(), add_str.end(),
            add_str.begin(), ::tolower);

  plugin_map[add_str]= plugin;
}


vector<st_plugin_int *> PluginRegistry::get_list(bool active)
{
  st_plugin_int *plugin= NULL;

  vector <st_plugin_int *> plugins;
  plugins.reserve(plugin_map.size());

  map<string, st_plugin_int *>::iterator map_iter;
  for (map_iter= plugin_map.begin();
       map_iter != plugin_map.end();
       map_iter++)
  {
    plugin= (*map_iter).second;
    if (active)
      plugins.push_back(plugin);
    else if (plugin->isInited)
      plugins.push_back(plugin);
  }

  return plugins;
}

void PluginRegistry::add(StorageEngine *engine)
{
  add_storage_engine(engine);
}

void PluginRegistry::add(InfoSchemaTable *schema_table)
{
  add_infoschema_table(schema_table);
}

void PluginRegistry::add(Function_builder *udf)
{
  add_udf(udf);
}

void PluginRegistry::add(Logging_handler *handler)
{
  add_logger(handler);
}

void PluginRegistry::add(Error_message_handler *handler)
{
  add_errmsg_handler(handler);
}

void PluginRegistry::add(Authentication *auth)
{
  add_authentication(auth);
}

void PluginRegistry::add(QueryCache *qcache)
{
  add_query_cache(qcache);
}

void PluginRegistry::add(SchedulerFactory *factory)
{
  add_scheduler_factory(factory);
}

void PluginRegistry::add(ProtocolFactory *factory)
{
  add_protocol_factory(factory);
}

void PluginRegistry::add(drizzled::plugin::Replicator *repl)
{
  add_replicator(repl);
}

void PluginRegistry::remove(StorageEngine *engine)
{
  remove_storage_engine(engine);
}

void PluginRegistry::remove(InfoSchemaTable *schema_table)
{
  remove_infoschema_table(schema_table);
}

void PluginRegistry::remove(Function_builder *udf)
{
  remove_udf(udf);
}

void PluginRegistry::remove(Logging_handler *handler)
{
  remove_logger(handler);
}

void PluginRegistry::remove(Error_message_handler *handler)
{
  remove_errmsg_handler(handler);
}

void PluginRegistry::remove(Authentication *auth)
{
  remove_authentication(auth);
}

void PluginRegistry::remove(QueryCache *qcache)
{
  remove_query_cache(qcache);
}

void PluginRegistry::remove(SchedulerFactory *factory)
{
  remove_scheduler_factory(factory);
}

void PluginRegistry::remove(ProtocolFactory *factory)
{
  remove_protocol_factory(factory);
}

void PluginRegistry::remove(drizzled::plugin::Replicator *repl)
{
  remove_replicator(repl);
}
