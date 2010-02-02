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

#ifndef DRIZZLED_TABLE_FUNCTION_CONTAINER_H
#define DRIZZLED_TABLE_FUNCTION_CONTAINER_H

#include <set>

class TableFunctionContainer {
  typedef drizzled::hash_map<std::string, Tool *> ToolMap;
  typedef std::pair<std::string, Tool&> ToolMapPair;

  ToolMap table_map;

public:
  Tool *getTool(const char *path)
  {
    ToolMap::iterator iter= table_map.find(path);

    if (iter == table_map.end())
    {
      assert(path == NULL);
    }
    return (*iter).second;
  }

  void getNames(const std::string predicate,
                std::set<std::string> &set_of_names)
  {
    for (ToolMap::iterator it= table_map.begin();
         it != table_map.end();
         it++)
    {
      Tool *tool= (*it).second;

      if (predicate.length())
      {
        if (not predicate.compare(tool->getSchemaHome()))
        {
          set_of_names.insert(tool->getName());
        }
      }
      else
      {
        set_of_names.insert(tool->getName());
      }
    }
  }

  Tool *getTool(const std::string &path)
  {
    ToolMap::iterator iter= table_map.find(path);

    if (iter == table_map.end())
    {
      return NULL;
    }
    return (*iter).second;
  }

  void addTool(Tool& tool)
  {
    std::pair<ToolMap::iterator, bool> ret;
    std::string schema= tool.getSchemaHome();
    std::string path= tool.getPath();

    transform(path.begin(), path.end(),
              path.begin(), ::tolower);

    transform(schema.begin(), schema.end(),
              schema.begin(), ::tolower);

    ret= table_map.insert(std::make_pair(path, &tool));
    assert(ret.second == true);
  }

  const ToolMap& getTableContainer()
  {
    return table_map;
  }
};

#endif // DRIZZLED_TABLE_FUNCTION_CONTAINER_H
