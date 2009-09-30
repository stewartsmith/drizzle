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

#ifndef DRIZZLED_PLUGIN_FUNCTION_H
#define DRIZZLED_PLUGIN_FUNCTION_H


#include <drizzled/item.h>
#include <drizzled/sql_list.h>
#include <drizzled/item/bin_string.h>
#include "drizzled/plugin/plugin.h"
#include "drizzled/function/func.h"

#include <string>
#include <vector>
#include <functional>


namespace drizzled
{
namespace plugin
{

/**
 * Functions in the server: AKA UDF
 */
class Function
  : public Plugin, std::unary_function<MEM_ROOT*, Item_func *>
{
public:
  Function(std::string in_name)
   : Plugin(in_name),
     std::unary_function<MEM_ROOT*, Item_func *>()
  { }
  virtual result_type operator()(argument_type root) const= 0;
  virtual ~Function() {}

  /**
   * Add a new Function factory to the list of factories we manage.
   */
  static void add(const plugin::Function *function_obj);

  /**
   * Remove a Function factory from the list of factory we manage.
   */
  static void remove(const plugin::Function *function_obj);

  /**
   * Accept a new connection (Protocol object) on one of the configured
   * listener interfaces.
   */
  static const plugin::Function *get(const char *name, size_t len=0);

};

template<class T>
class Create_function
 : public Function
{
public:
  typedef T FunctionClass;
  Create_function(std::string in_name)
    : Function(in_name)
  { }
  virtual result_type operator()(argument_type root) const
  {
    return new (root) FunctionClass();
  }
};

} /* namespace plugin */
} /* namespace drizzled */


#endif /* DRIZZLED_PLUGIN_FUNCTION_H */
