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

#include <config.h>
#include <boost/algorithm/string.hpp>
#include <drizzled/module/context.h>
#include <drizzled/module/option_map.h>
#include <drizzled/module/module.h>
#include <drizzled/drizzled.h>
#include <drizzled/sys_var.h>

namespace drizzled {
namespace module {

module::option_map Context::getOptions()
{
  return module::option_map(module->getName(), getVariablesMap());
}

void Context::registerVariable(sys_var *var)
{
  var->setName(prepend_name(module->getName(), var->getName()));
  module->addMySysVar(var);
  add_sys_var_to_list(var);
}

std::string Context::prepend_name(std::string module_name,
                                  const std::string &var_name)
{
  module_name.push_back('_');
  module_name.append(var_name);
  std::replace(module_name.begin(), module_name.end(), '-', '_');
  return boost::to_lower_copy(module_name);
}

} /* namespace module */
} /* namespace drizzled */
