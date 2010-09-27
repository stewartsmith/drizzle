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

#include "config.h"

#include <string>
#include <vector>
#include <map>

#include "drizzled/module/registry.h"
#include "drizzled/module/library.h"

#include "drizzled/plugin.h"
#include "drizzled/show.h"
#include "drizzled/cursor.h"

using namespace std;

namespace drizzled
{


module::Registry::~Registry()
{
  map<string, plugin::Plugin *>::iterator plugin_iter;

  /* Give all plugins a chance to cleanup, before
   * all plugins are deleted.
   * This can be used if shutdown code references
   * other plugins.
   */
  plugin_iter= plugin_registry.begin();
  while (plugin_iter != plugin_registry.end())
  {
    (*plugin_iter).second->shutdownPlugin();
    ++plugin_iter;
  }

  plugin_iter= plugin_registry.begin();
  while (plugin_iter != plugin_registry.end())
  {
    delete (*plugin_iter).second;
    ++plugin_iter;
  }
  plugin_registry.clear();

  /*
    @TODO When we delete modules here, we segfault on a bad string. Why?
    map<string, module::Module *>::iterator module_iter= module_map.begin();
  while (module_iter != module_map.end())
  {
    delete (*module_iter).second;
    ++module_iter;
  }
  module_map.clear();
  */
  map<string, module::Library *>::iterator library_iter= library_map.begin();
  while (library_iter != library_map.end())
  {
    delete (*library_iter).second;
    ++library_iter;
  }
  library_map.clear();
}

void module::Registry::shutdown()
{
  module::Registry& registry= singleton();
  delete &registry;
}

module::Module *module::Registry::find(string name)
{
  transform(name.begin(), name.end(), name.begin(), ::tolower);

  map<string, module::Module *>::iterator map_iter;
  map_iter= module_map.find(name);
  if (map_iter != module_map.end())
    return (*map_iter).second;
  return(0);
}

void module::Registry::add(module::Module *handle)
{
  string add_str(handle->getName());
  transform(add_str.begin(), add_str.end(),
            add_str.begin(), ::tolower);

  module_map[add_str]= handle;
}

void module::Registry::remove(module::Module *handle)
{
  string remove_str(handle->getName());
  transform(remove_str.begin(), remove_str.end(),
            remove_str.begin(), ::tolower);

  module_map.erase(remove_str);
}

vector<module::Module *> module::Registry::getList(bool active)
{
  module::Module *plugin= NULL;

  vector<module::Module *> plugins;
  plugins.reserve(module_map.size());

  map<string, module::Module *>::iterator map_iter;
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

module::Library *module::Registry::addLibrary(const string &plugin_name,
                                              bool builtin)
{

  /* If this dll is already loaded just return it */
  module::Library *library= findLibrary(plugin_name);
  if (library != NULL)
  {
    return library;
  }

  library= module::Library::loadLibrary(plugin_name, builtin);
  if (library != NULL)
  {
    /* Add this dll to the map */
    library_map.insert(make_pair(plugin_name, library));
  }

  return library;
}

void module::Registry::removeLibrary(const string &plugin_name)
{
  map<string, module::Library *>::iterator iter=
    library_map.find(plugin_name);
  if (iter != library_map.end())
  {
    library_map.erase(iter);
    delete (*iter).second;
  }
}

module::Library *module::Registry::findLibrary(const string &plugin_name) const
{
  map<string, module::Library *>::const_iterator iter=
    library_map.find(plugin_name);
  if (iter != library_map.end())
    return (*iter).second;
  return NULL;
}

void module::Registry::shutdownModules()
{
  module_shutdown(*this);
}

} /* namespace drizzled */
