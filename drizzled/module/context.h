/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Monty Taylor
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

/**
 * @file Defines a Plugin Context
 *
 * A module::Context object is a proxy object containing state information
 * about the plugin being registered that knows how to perform registration
 * actions.
 *
 * The plugin registration system creates a new module::Context for each
 * module::Module during the initializtion phase and passes a reference to
 * the module::Context to the module's init method. This allows the plugin
 * to call registration methods without having access to larger module::Registry
 * calls. It also provides a filter layer through which calls are made in order
 * to force things like proper name prefixing and the like.
 */

#include <boost/noncopyable.hpp>
#include <drizzled/module/registry.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace module {

class DRIZZLED_API Context : boost::noncopyable
{
public:

  Context(module::Registry &registry_arg,
          module::Module *module_arg) :
     registry(registry_arg),
     module(module_arg)
  { }

  template<class T>
  void add(T *plugin)
  {
    plugin->setModule(module);
    registry.add(plugin);
  }

  template<class T>
  void remove(T *plugin)
  {
    registry.remove(plugin);
  }

  void registerVariable(sys_var *var);

  option_map getOptions();

  static std::string prepend_name(std::string module_name,
                                  const std::string &var_name);
private:
  module::Registry &registry;
  module::Module *module;
};


} /* namespace module */
} /* namespace drizzled */

