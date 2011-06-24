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


#include <drizzled/item/func.h>
#include <drizzled/plugin.h>
#include <drizzled/plugin/plugin.h>

#include <string>
#include <vector>
#include <functional>

#include <boost/unordered_map.hpp>

#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

/**
 * Functions in the server: AKA UDF
 */
class DRIZZLED_API Function : public Plugin
{
public:
  Function(std::string in_name) : Plugin(in_name, "Function")
  { }
  virtual Item_func* operator()(memory::Root*) const= 0;

  /**
   * Add a new Function factory to the list of factories we manage.
   */
  static bool addPlugin(const plugin::Function *function_obj);

  /**
   * Remove a Function factory from the list of factory we manage.
   */
  static void removePlugin(const plugin::Function *function_obj);

  static const plugin::Function *get(const std::string &name);

  typedef boost::unordered_map<std::string, const plugin::Function *, util::insensitive_hash, util::insensitive_equal_to> UdfMap;
  typedef boost::unordered_map<std::string, const plugin::Function *, util::insensitive_hash, util::insensitive_equal_to> Map;

  static const UdfMap &getMap();
};

template<class T>
class Create_function : public Function
{
public:
  Create_function(const std::string& in_name) : Function(in_name)
  { 
  }

  virtual Item_func* operator()(memory::Root* root) const
  {
    return new (*root) T();
  }
};

} /* namespace plugin */
} /* namespace drizzled */


