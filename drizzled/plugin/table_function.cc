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

#include "config.h"
#include <drizzled/plugin/table_function.h>
#include <drizzled/gettext.h>
#include "drizzled/plugin/registry.h"

#include <vector>

class Session;

using namespace std;

namespace drizzled
{

typedef hash_map<string, Tool *> ToolMap;
typedef pair<string, Tool&> ToolMapPair;

ToolMap table_map;
SchemaList schema_list;


bool plugin::TableFunction::addPlugin(plugin::Tool *tool)
{
  pair<ToolMap::iterator, bool> ret;
  string schema= tool->getSchemaHome();
  string path= tool->getPath();

  transform(path.begin(), path.end(),
      path.begin(), ::tolower);

  transform(schema.begin(), schema.end(),
      schema.begin(), ::tolower);

  schema_list.insert(schema);

  ret= table_map.insert(make_pair(path, tool));
  assert(ret.second == true);
  return false;
}

void plugin::TableFunction::removePlugin(plugin::Tool *tool)
{
  pair<ToolMap::iterator, bool> ret;
  string schema= tool->getSchemaHome();
  string path= tool->getPath();

  transform(path.begin(), path.end(),
      path.begin(), ::tolower);

  transform(schema.begin(), schema.end(),
      schema.begin(), ::tolower);

  schema_list.erase(schema);

  ret= table_map.remove(make_pair(path, tool));
}


} /* namespace drizzled */
