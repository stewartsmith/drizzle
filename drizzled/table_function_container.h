/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <set>
#include <boost/unordered_map.hpp>
#include <drizzled/util/string.h>

namespace drizzled {

class TableFunctionContainer {
  typedef boost::unordered_map<std::string, plugin::TableFunction *, util::insensitive_hash, util::insensitive_equal_to> ToolMap;

  ToolMap table_map;

public:
  plugin::TableFunction *getFunction(const std::string &path);

  void getNames(const std::string &predicate,
                std::set<std::string> &set_of_names);


  void addFunction(plugin::TableFunction *tool);

  const ToolMap& getTableContainer()
  {
    return table_map;
  }
};

} /* namepsace drizzled */

