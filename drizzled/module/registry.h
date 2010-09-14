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

#ifndef DRIZZLED_MODULE_REGISTRY_H
#define DRIZZLED_MODULE_REGISTRY_H

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "drizzled/gettext.h"
#include "drizzled/unireg.h"
#include "drizzled/errmsg_print.h"

namespace drizzled
{
namespace plugin
{
class Plugin;
}

namespace module
{
class Module;
class Library;

class Registry
{
private:
  std::map<std::string, Library *> library_map;
  std::map<std::string, Module *> module_map;
  std::map<std::string, plugin::Plugin *> plugin_registry;

  Registry()
   : module_map(),
     plugin_registry()
  { }

  Registry(const Registry&);
  Registry& operator=(const Registry&);
  ~Registry();
public:

  static Registry& singleton()
  {
    static Registry *registry= new Registry();
    return *registry;
  }

  static void shutdown();

  Module *find(std::string name);

  void add(Module *module);

  void remove(Module *module);

  std::vector<Module *> getList(bool active);

  const std::map<std::string, plugin::Plugin *> &getPluginsMap() const
  {
    return plugin_registry;
  }

  const std::map<std::string, Module *> &getModulesMap() const
  {
    return module_map;
  }

  Library *addLibrary(const std::string &plugin_name, bool builtin= false);
  void removeLibrary(const std::string &plugin_name);
  Library *findLibrary(const std::string &plugin_name) const;

  void shutdownModules();

  template<class T>
  void add(T *plugin)
  {
    bool failed= false;
    std::string plugin_name(plugin->getName());
    std::transform(plugin_name.begin(), plugin_name.end(),
                   plugin_name.begin(), ::tolower);
    if (plugin_registry.find(plugin_name) != plugin_registry.end())
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Loading plugin %s failed: a plugin by that name already "
                      "exists.\n"), plugin->getName().c_str());
      failed= true;
    }
    if (T::addPlugin(plugin))
      failed= true;
    if (failed)
    {
      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Fatal error: Failed initializing %s plugin.\n"),
                    plugin->getName().c_str());
      unireg_abort(1);
    }
    plugin_registry.insert(std::pair<std::string, plugin::Plugin *>(plugin_name, plugin));
  }

  template<class T>
  void remove(T *plugin)
  {
    std::string plugin_name(plugin->getName());
    std::transform(plugin_name.begin(), plugin_name.end(),
                   plugin_name.begin(), ::tolower);
    T::removePlugin(plugin);
    plugin_registry.erase(plugin_name);
  }


};

} /* namespace module */
} /* namespace drizzled */
#endif /* DRIZZLED_MODULE_REGISTRY_H */
