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

#include <config.h>
#include <boost/foreach.hpp>
#include <drizzled/plugin/table_function.h>
#include <drizzled/table_function_container.h>
#include <drizzled/util/find_ptr.h>

using namespace std;

namespace drizzled {

plugin::TableFunction *TableFunctionContainer::getFunction(const std::string &path)
{
  ToolMap::mapped_type* ptr= find_ptr(table_map, path);
  return ptr ? *ptr : NULL;
}

void TableFunctionContainer::getNames(const string &predicate, std::set<std::string> &set_of_names)
{
  BOOST_FOREACH(ToolMap::reference i, table_map)
  {
    if (i.second->visible() && (predicate.empty() || boost::iequals(predicate, i.second->getSchemaHome())))
      set_of_names.insert(i.second->getTableLabel());
  }
}

void TableFunctionContainer::addFunction(plugin::TableFunction *tool)
{
  std::pair<ToolMap::iterator, bool> ret= table_map.insert(std::make_pair(tool->getPath(), tool));
  assert(ret.second);
}

} /* namespace drizzled */
