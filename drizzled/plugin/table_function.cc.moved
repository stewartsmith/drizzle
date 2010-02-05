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

TableFunctionContainer table_functions;

bool plugin::TableFunction::addPlugin(plugin::Tool *tool)
{
  assert(tool != NULL);
  table_functions.addTool(*tool); 
  return false;
}

void plugin::TableFunction::removePlugin(plugin::Tool *tool)
{
/* TODO - We should do this or valgrind will be unhappy */
}


} /* namespace drizzled */
