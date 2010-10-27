/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Sun Microsystems
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

#include "config.h"
#include "drizzled/plugin/table_function.h"
#include "drizzled/table_function_container.h"

#include <iostream>

using namespace std;

namespace drizzled
{

plugin::TableFunction *TableFunctionContainer::getFunction(const std::string &path)
{
  ToolMap::iterator iter= table_map.find(path);

  if (iter == table_map.end())
  {
    return NULL;
  }
  return (*iter).second;
}

void TableFunctionContainer::getNames(const string &predicate,
                                      std::set<std::string> &set_of_names)
{
  for (ToolMap::iterator it= table_map.begin();
       it != table_map.end();
       it++)
  {
    plugin::TableFunction *tool= (*it).second;

    if (tool->visable())
    {
      if (predicate.length())
      {
        if (boost::iequals(predicate, tool->getSchemaHome()))
        {
          set_of_names.insert(tool->getTableLabel());
        }
      }
      else
      {
        set_of_names.insert(tool->getTableLabel());
      }
    }
  }
}

void TableFunctionContainer::addFunction(plugin::TableFunction *tool)
{
  std::pair<ToolMap::iterator, bool> ret;

  ret= table_map.insert(std::make_pair(tool->getPath(), tool));
  assert(ret.second == true);
}

} /* namespace drizzled */
