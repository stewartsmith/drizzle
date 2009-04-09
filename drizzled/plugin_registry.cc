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

#include <string>
#include <vector>
#include <map>

using namespace std;

static Plugin_registry the_registry;
Plugin_registry& Plugin_registry::get_plugin_registry()
{
  return the_registry;
}

st_plugin_int *Plugin_registry::find(const LEX_STRING *name, int type)
{
  uint32_t i;
  string find_str(name->str,name->length);
  transform(find_str.begin(), find_str.end(), find_str.begin(), ::tolower);

  map<string, st_plugin_int *>::iterator map_iter;
  if (type == DRIZZLE_ANY_PLUGIN)
  {
    for (i= 0; i < DRIZZLE_MAX_PLUGIN_TYPE_NUM; i++)
    {
      map_iter= plugin_map[i].find(find_str);
      if (map_iter != plugin_map[i].end())
        return (*map_iter).second;
    }
  }
  else
  {
    map_iter= plugin_map[type].find(find_str);
    if (map_iter != plugin_map[type].end())
      return (*map_iter).second;
  }
  return(0);
}

void Plugin_registry::add(st_mysql_plugin *handle, st_plugin_int *plugin)
{
  string add_str(plugin->name.str);
  transform(add_str.begin(), add_str.end(),
            add_str.begin(), ::tolower);

  plugin_map[handle->type][add_str]= plugin;
}


void Plugin_registry::get_list(uint32_t type,
                                    vector<st_plugin_int *> &plugins,
                                    bool active)
{
  st_plugin_int *plugin= NULL;
  plugins.reserve(plugin_map[type].size());
  map<string, st_plugin_int *>::iterator map_iter;

  for (map_iter= plugin_map[type].begin();
       map_iter != plugin_map[type].end();
       map_iter++)
  {
    plugin= (*map_iter).second;
    if (active)
      plugins.push_back(plugin);
    else if (plugin->isInited)
      plugins.push_back(plugin);
  }
}
