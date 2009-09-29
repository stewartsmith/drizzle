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
#include "drizzled/plugin/registry.h"

#include "drizzled/plugin.h"
#include "drizzled/show.h"
#include "drizzled/handler.h"

#include <string>
#include <vector>
#include <map>

using namespace std;

namespace drizzled
{

plugin::Handle *plugin::Registry::find(const LEX_STRING *name)
{
  string find_str(name->str,name->length);
  transform(find_str.begin(), find_str.end(), find_str.begin(), ::tolower);

  map<string, plugin::Handle *>::iterator map_iter;
  map_iter= handle_map.find(find_str);
  if (map_iter != handle_map.end())
    return (*map_iter).second;
  return(0);
}

void plugin::Registry::add(plugin::Handle *handle)
{
  string add_str(handle->getName());
  transform(add_str.begin(), add_str.end(),
            add_str.begin(), ::tolower);

  handle_map[add_str]= handle;
}

void plugin::Registry::add(plugin::Plugin *plugin)
{
  plugin->setHandle(current_handle);
  plugin_registry.add(plugin); 
}

void plugin::Registry::remove(const plugin::Plugin *plugin)
{
  plugin_registry.remove(plugin);
}

vector<plugin::Handle *> plugin::Registry::get_list(bool active)
{
  plugin::Handle *plugin= NULL;

  vector <plugin::Handle *> plugins;
  plugins.reserve(handle_map.size());

  map<string, plugin::Handle *>::iterator map_iter;
  for (map_iter= handle_map.begin();
       map_iter != handle_map.end();
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

  void plugin::Registry::add(plugin::CommandReplicator *plugin)
  {
    addPlugin(plugin);
    command_replicator.add(plugin);
  }
  void plugin::Registry::add(plugin::CommandApplier *plugin)
  {
    addPlugin(plugin);
    command_applier.add(plugin);
  }
  void plugin::Registry::add(plugin::ErrorMessage *plugin)
  {
    addPlugin(plugin);
    error_message.add(plugin);
  }
  void plugin::Registry::add(plugin::Authentication *plugin)
  {
    addPlugin(plugin);
    authentication.add(plugin);
  }
  void plugin::Registry::add(plugin::QueryCache *plugin)
  {
    addPlugin(plugin);
    query_cache.add(plugin);
  }
  void plugin::Registry::add(plugin::SchedulerFactory *plugin)
  {
    addPlugin(plugin);
    scheduler.add(plugin);
  }
  void plugin::Registry::add(plugin::Function *plugin)
  {
    addPlugin(plugin);
    function.add(plugin);
  }
  void plugin::Registry::add(plugin::Listen *plugin)
  {
    addPlugin(plugin);
    listen.add(plugin);
  }
  void plugin::Registry::add(plugin::Logging *plugin)
  {
    addPlugin(plugin);
    logging.add(plugin);
  }
  void plugin::Registry::add(plugin::InfoSchemaTable *plugin)
  {
    addPlugin(plugin);
    info_schema.add(plugin);
  }

  void plugin::Registry::remove(plugin::CommandReplicator *plugin)
  {
    removePlugin(plugin);
    command_replicator.remove(plugin);
  }
  void plugin::Registry::remove(plugin::CommandApplier *plugin)
  {
    removePlugin(plugin);
    command_applier.remove(plugin);
  }
  void plugin::Registry::remove(plugin::ErrorMessage *plugin)
  {
    removePlugin(plugin);
    error_message.remove(plugin);
  }
  void plugin::Registry::remove(plugin::Authentication *plugin)
  {
    removePlugin(plugin);
    authentication.remove(plugin);
  }
  void plugin::Registry::remove(plugin::QueryCache *plugin)
  {
    removePlugin(plugin);
    query_cache.remove(plugin);
  }
  void plugin::Registry::remove(plugin::SchedulerFactory *plugin)
  {
    removePlugin(plugin);
    scheduler.remove(plugin);
  }
  void plugin::Registry::remove(plugin::Function *plugin)
  {
    removePlugin(plugin);
    function.remove(plugin);
  }
  void plugin::Registry::remove(plugin::Listen *plugin)
  {
    removePlugin(plugin);
    listen.remove(plugin);
  }
  void plugin::Registry::remove(plugin::Logging *plugin)
  {
    removePlugin(plugin);
    logging.remove(plugin);
  }
  void plugin::Registry::remove(plugin::InfoSchemaTable *plugin)
  {
    removePlugin(plugin);
    info_schema.remove(plugin);
  }
} /* namespace drizzled */
