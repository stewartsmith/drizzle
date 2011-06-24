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

#include <config.h>

#include <string>
#include <vector>
#include <map>

#include <drizzled/module/registry.h>
#include <drizzled/module/library.h>
#include <drizzled/module/graph.h>
#include <drizzled/module/vertex_handle.h>

#include <drizzled/plugin.h>
#include <drizzled/show.h>
#include <drizzled/cursor.h>
#include <drizzled/abort_exception.h>
#include <drizzled/util/find_ptr.h>

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

using namespace std;

namespace drizzled {

module::Registry::Registry() :
  module_registry_(),
  depend_graph_(new module::Graph()),
  plugin_registry(),
  deps_built_(false)
{ }


module::Registry::~Registry()
{
  /* Give all plugins a chance to cleanup, before
   * all plugins are deleted.
   * This can be used if shutdown code references
   * other plugins.
   */
  BOOST_FOREACH(plugin::Plugin::map::reference it, plugin_registry)
    it.second->shutdownPlugin();

  plugin::Plugin::vector error_plugins;
  BOOST_FOREACH(plugin::Plugin::map::reference it, plugin_registry)
  {
    if (it.second->removeLast())
      error_plugins.push_back(it.second);
    else
      delete it.second;
  }

  BOOST_FOREACH(plugin::Plugin::vector::reference it, error_plugins)
    delete it;

  plugin_registry.clear();

#if 0
  /*
  @TODO When we delete modules here, we segfault on a bad string. Why?
   */

  BOOST_FOREACH(ModuleMap::reference it, module_registry_)
    delete it.second;
  module_registry_.clear();
#endif
  BOOST_FOREACH(LibraryMap::reference it, library_registry_)
    delete it.second;
  library_registry_.clear();
}

void module::Registry::shutdown()
{
  delete &singleton();
}

module::Module* module::Registry::find(const std::string& name)
{
  return find_ptr2(module_registry_, boost::to_lower_copy(name));
}

void module::Registry::add(module::Module *handle)
{
  std::string add_str(boost::to_lower_copy(handle->getName()));

  module_registry_[add_str]= handle;

  Vertex vertex_info(add_str, handle);
  VertexDesc handle_vertex= boost::add_vertex(depend_graph_->getGraph());
  depend_graph_->properties(handle_vertex)= vertex_info;

  handle->setVertexHandle(new VertexHandle(handle_vertex));
}

void module::Registry::remove(module::Module *handle)
{
  module_registry_.erase(boost::to_lower_copy(handle->getName()));
}

void module::Registry::buildDeps()
{
  BOOST_FOREACH(ModuleMap::reference map_iter, module_registry_)
  {
    Module* handle= map_iter.second;
    BOOST_FOREACH(Module::Depends::const_reference handle_deps, handle->getDepends())
    {
      std::string dep_str(boost::to_lower_copy(handle_deps));
      bool found_dep= false;
      for (vertex_iter it= boost::vertices(depend_graph_->getGraph()).first; it != vertices(depend_graph_->getGraph()).second; it++)
      {
        if (depend_graph_->properties(*it).getName() == dep_str)
        {
          found_dep= true;
          add_edge(handle->getVertexHandle()->getVertexDesc(), *it, depend_graph_->getGraph());
          break;
        }
      }
      if (not found_dep)
      {
        errmsg_printf(error::ERROR, _("Couldn't process plugin module dependencies. %s depends on %s but %s is not to be loaded.\n"),
          handle->getName().c_str(), dep_str.c_str(), dep_str.c_str());
        DRIZZLE_ABORT;
      }
    }
  }
  deps_built_= true;
}

module::Registry::ModuleList module::Registry::getList()
{
  if (not deps_built_)
    buildDeps();
  VertexList vertex_list;
  boost::topological_sort(depend_graph_->getGraph(), std::back_inserter(vertex_list));
  ModuleList plugins;
  BOOST_FOREACH(VertexList::reference it, vertex_list)
  {
    if (Module* mod_ptr= depend_graph_->properties(it).getModule())
      plugins.push_back(mod_ptr);
  }
  return plugins;
}

module::Library *module::Registry::addLibrary(const std::string &plugin_name, bool builtin)
{
  /* If this dll is already loaded just return it */
  module::Library *library= findLibrary(plugin_name);
  if (library)
    return library;

  library= module::Library::loadLibrary(plugin_name, builtin);
  if (library)
  {
    /* Add this dll to the map */
    library_registry_.insert(make_pair(plugin_name, library));
  }
  return library;
}

void module::Registry::removeLibrary(const std::string &plugin_name)
{
  LibraryMap::iterator iter= library_registry_.find(plugin_name);
  if (iter != library_registry_.end())
  {
    delete iter->second;
    library_registry_.erase(iter);
  }
}

module::Library *module::Registry::findLibrary(const std::string &plugin_name) const
{
  return find_ptr2(library_registry_, plugin_name);
}

void module::Registry::shutdownModules()
{
  module_shutdown(*this);
}

} /* namespace drizzled */
