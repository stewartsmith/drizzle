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

/**
 * @file
 * @brief An Proxy Wrapper around boost::program_options::variables_map
 */

#pragma once

#include <boost/program_options.hpp>
#include <drizzled/module/option_context.h>
#include <drizzled/visibility.h>
#include <string>

namespace drizzled {
namespace module {

/**
 * Options proxy wrapper. Provides pre-pending of module name to each option
 * which is added.
 */
class DRIZZLED_API option_map
{
public:
  const boost::program_options::variables_map &vm;

  option_map(const std::string &module_name_in,
             const boost::program_options::variables_map &vm_in);

  const boost::program_options::variable_value& operator[](const std::string &name_in) const
  {
    return vm[option_context::prepend_name(module_name, name_in.c_str())];
  }

  size_t count(const std::string &name_in) const
  {
    return vm.count(option_context::prepend_name(module_name, name_in.c_str()));
  }

private:
  const std::string &module_name;
};

} /* namespace module */
} /* namespace drizzled */


