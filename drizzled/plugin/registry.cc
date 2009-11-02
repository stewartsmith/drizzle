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
#include "drizzled/cursor.h"

#include <string>
#include <vector>
#include <map>

using namespace std;

namespace drizzled
{

plugin::Module *plugin::Registry::find(const LEX_STRING *name)
{
  string find_str(name->str,name->length);
  transform(find_str.begin(), find_str.end(), find_str.begin(), ::tolower);

  map<string, plugin::Module *>::iterator map_iter;
  map_iter= module_map.find(find_str);
  if (map_iter != module_map.end())
    return (*map_iter).second;
  return(0);
}

void plugin::Registry::add(plugin::Module *handle)
{
  string add_str(handle->getName());
  transform(add_str.begin(), add_str.end(),
            add_str.begin(), ::tolower);

  module_map[add_str]= handle;
}


vector<plugin::Module *> plugin::Registry::getList(bool active)
{
  plugin::Module *plugin= NULL;

  vector <plugin::Module *> plugins;
  plugins.reserve(module_map.size());

  map<string, plugin::Module *>::iterator map_iter;
  for (map_iter= module_map.begin();
       map_iter != module_map.end();
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

} /* namespace drizzled */
