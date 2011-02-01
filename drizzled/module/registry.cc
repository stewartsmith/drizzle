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

#include "config.h"

#include <string>
#include <vector>
#include <map>

#include "drizzled/module/registry.h"
#include "drizzled/module/library.h"
#include "drizzled/module/graph.h"
#include "drizzled/module/vertex_handle.h"

#include "drizzled/plugin.h"
#include "drizzled/show.h"
#include "drizzled/cursor.h"
#include "drizzled/abort_exception.h"

#include <boost/bind.hpp>

using namespace std;

namespace drizzled
{

module::Registry::Registry() :
  module_registry_(),
  depend_graph_(new module::Graph()),
  plugin_registry(),
  deps_built_(false)
{ }


module::Registry::~Registry()
{
  plugin::Plugin::map::iterator plugin_iter;

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

#if 0
  @TODO When we delete modules here, we segfault on a bad string. Why?
   ModuleMap::iterator module_iter= module_registry_.begin();

  while (module_iter != module_registry_.end())
  {
    delete (*module_iter).second;
    ++module_iter;
  }
  module_registry_.clear();
#endif
  LibraryMap::iterator library_iter= library_registry_.begin();
  while (library_iter != library_registry_.end())
  {
    delete (*library_iter).second;
    ++library_iter;
  }
  library_registry_.clear();
}

void module::Registry::shutdown()
{
  module::Registry& registry= singleton();
  delete &registry;
}

module::Module *module::Registry::find(std::string name)
{
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  ModuleMap::iterator map_iter;
  map_iter= module_registry_.find(name);
  if (map_iter != module_registry_.end())
    return (*map_iter).second;
  return NULL;
}

void module::Registry::add(module::Module *handle)
{
  std::string add_str(handle->getName());
  transform(add_str.begin(), add_str.end(),
            add_str.begin(), ::tolower);

  module_registry_[add_str]= handle;

  Vertex vertex_info(add_str, handle);
  VertexDesc handle_vertex= boost::add_vertex(depend_graph_->getGraph());
  depend_graph_->properties(handle_vertex)= vertex_info;

  handle->setVertexHandle(new VertexHandle(handle_vertex));

}

void module::Registry::remove(module::Module *handle)
{
  std::string remove_str(handle->getName());
  std::transform(remove_str.begin(), remove_str.end(),
                 remove_str.begin(), ::tolower);

  module_registry_.erase(remove_str);
}

void module::Registry::copy(plugin::Plugin::vector &arg)
{    
  arg.reserve(plugin_registry.size());

  std::transform(plugin_registry.begin(),
                 plugin_registry.end(),
                 std::back_inserter(arg),
                 boost::bind(&plugin::Plugin::map::value_type::second, _1) );
  assert(arg.size() == plugin_registry.size());
}

void module::Registry::buildDeps()
{
  ModuleMap::iterator map_iter= module_registry_.begin();
  while (map_iter != module_registry_.end())
  {
    Module *handle= (*map_iter).second;
    Module::Depends::const_iterator handle_deps= handle->getDepends().begin();
    while (handle_deps != handle->getDepends().end())
    {
      std::string dep_str((*handle_deps));
      transform(dep_str.begin(), dep_str.end(),
                dep_str.begin(), ::tolower);

      bool found_dep= false;
      vertex_iter it= boost::vertices(depend_graph_->getGraph()).first;
      while (it != vertices(depend_graph_->getGraph()).second)
      {
        if (depend_graph_->properties(*it).getName() == dep_str)
        {
          found_dep= true;
          add_edge(handle->getVertexHandle()->getVertexDesc(), *it, depend_graph_->getGraph());
          break;
        }
        ++it;
      }
      if (not found_dep)
      {
        errmsg_printf(error::ERROR,
                      _("Couldn't process plugin module dependencies. "
                        "%s depends on %s but %s is not to be loaded.\n"),
                      handle->getName().c_str(),
                      dep_str.c_str(), dep_str.c_str());
        DRIZZLE_ABORT;
      }

      ++handle_deps;
    }
    ++map_iter;
  }
  deps_built_= true;
}

module::Registry::ModuleList module::Registry::getList()
{
  if (not deps_built_)
  {
    buildDeps();
  }

  std::vector<module::Module *> plugins;

  VertexList vertex_list;

  boost::topological_sort(depend_graph_->getGraph(), std::back_inserter(vertex_list));

  for (VertexList::iterator i = vertex_list.begin();
       i != vertex_list.end(); ++i)
  {
    Module *mod_ptr= depend_graph_->properties(*i).getModule();
    if (mod_ptr != NULL)
    {
      plugins.push_back(mod_ptr);
    }  
  }

  return plugins;
}

module::Library *module::Registry::addLibrary(const std::string &plugin_name,
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
    library_registry_.insert(make_pair(plugin_name, library));
  }

  return library;
}

void module::Registry::removeLibrary(const std::string &plugin_name)
{
  std::map<std::string, module::Library *>::iterator iter=
    library_registry_.find(plugin_name);
  if (iter != library_registry_.end())
  {
    library_registry_.erase(iter);
    delete (*iter).second;
  }
}

module::Library *module::Registry::findLibrary(const std::string &plugin_name) const
{
  LibraryMap::const_iterator iter= library_registry_.find(plugin_name);
  if (iter != library_registry_.end())
    return (*iter).second;
  return NULL;
}

void module::Registry::shutdownModules()
{
  module_shutdown(*this);
}

} /* namespace drizzled */
