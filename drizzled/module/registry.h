/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#pragma once

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>

#include <boost/scoped_ptr.hpp>

#include <drizzled/gettext.h>
#include <drizzled/unireg.h>
#include <drizzled/errmsg_print.h>
#include <drizzled/plugin/plugin.h>


namespace drizzled {
namespace module {

class Registry : boost::noncopyable
{
public:
  typedef std::map<std::string, Library*> LibraryMap;
  typedef std::map<std::string, Module*> ModuleMap;
  typedef std::vector<Module*> ModuleList;
private:
  LibraryMap library_registry_;
  ModuleMap module_registry_;
  boost::scoped_ptr<Graph> depend_graph_; 
  
  plugin::Plugin::map plugin_registry;

  bool deps_built_;

  Registry();
  ~Registry();

  void buildDeps();
public:

  static Registry& singleton()
  {
    static Registry *registry= new Registry();
    return *registry;
  }

  void copy(plugin::Plugin::vector &arg);

  static void shutdown();

  Module *find(std::string name);

  void add(Module *module);

  void remove(Module *module);

  std::vector<Module *> getList();

  const plugin::Plugin::map &getPluginsMap() const
  {
    return plugin_registry;
  }

  const ModuleMap &getModulesMap() const
  {
    return module_registry_;
  }

  Library *addLibrary(const std::string &plugin_name, bool builtin= false);
  void removeLibrary(const std::string &plugin_name);
  Library *findLibrary(const std::string &plugin_name) const;

  void shutdownModules();

  template<class T>
  void add(T *plugin)
  {
    bool failed= false;
    std::string plugin_type(plugin->getTypeName());
    std::transform(plugin_type.begin(), plugin_type.end(),
                   plugin_type.begin(), ::tolower);
    std::string plugin_name(plugin->getName());
    std::transform(plugin_name.begin(), plugin_name.end(),
                   plugin_name.begin(), ::tolower);
    if (plugin_registry.find(std::make_pair(plugin_type, plugin_name)) != plugin_registry.end())
    {
      errmsg_printf(error::ERROR,
                    _("Loading plugin %s failed: a %s plugin by that name "
                      "already exists.\n"),
                    plugin->getTypeName().c_str(),
                    plugin->getName().c_str());
      failed= true;
    }
    if (T::addPlugin(plugin))
    {
      failed= true;
    }

    if (failed)
    {
      errmsg_printf(error::ERROR,
                    _("Fatal error: Failed initializing %s::%s plugin.\n"),
                    plugin->getTypeName().c_str(),
                    plugin->getName().c_str());
      unireg_abort(1);
    }
    plugin_registry.insert(std::make_pair(std::make_pair(plugin_type, plugin_name), plugin));
  }

  template<class T>
  void remove(T *plugin)
  {
    std::string plugin_type(plugin->getTypeName());
    std::transform(plugin_type.begin(), plugin_type.end(),
                   plugin_type.begin(), ::tolower);
    std::string plugin_name(plugin->getName());
    std::transform(plugin_name.begin(), plugin_name.end(),
                   plugin_name.begin(), ::tolower);
    T::removePlugin(plugin);
    plugin_registry.erase(std::make_pair(plugin_type, plugin_name));
  }


};

} /* namespace module */
} /* namespace drizzled */
