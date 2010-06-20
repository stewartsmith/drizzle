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

#ifndef DRIZZLED_MODULE_OPTION_MAP_H
#define DRIZZLED_MODULE_OPTION_MAP_H

#include "drizzled/visibility.h"

#include <boost/program_options.hpp>

#include <string>

namespace drizzled
{
namespace module
{

/**
 * Options proxy wrapper. Provides pre-pending of module name to each option
 * which is added.
 */
class DRIZZLED_API option_map
{
  const std::string &module_name;
  const boost::program_options::variables_map vm;

public:

  option_map(const std::string &module_name_in,
             const boost::program_options::variables_map &vm_in);
  option_map(const option_map &old);

  const boost::program_options::variable_value& operator[](const std::string &name_in) const
  {
    std::string new_name(module_name);
    new_name.push_back('.');
    new_name.append(name_in);
    return vm[new_name];
  }

private:
  
  /**
   * Don't allow default construction.
   */
  option_map();

  /**
   * Don't allow assignment of objects.
   */
  option_map& operator=(const option_map &);
};

} /* namespace module */
} /* namespace drizzled */


#endif /* DRIZZLED_MODULE_OPTION_MAP_H */
