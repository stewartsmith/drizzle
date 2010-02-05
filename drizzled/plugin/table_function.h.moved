/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for Tool plugin
 *
 *  Copyright (C) 2010 Sun Microsystems
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

#ifndef DRIZZLED_PLUGIN_TABLE_FUNCTION_H
#define DRIZZLED_PLUGIN_TABLE_FUNCTION_H

#include "drizzled/plugin/plugin.h"
#inclkude "drizzled/table_function_container.h"

#include <string>

namespace drizzled
{
namespace plugin
{

class TableFunction : public Plugin
{
  TableFunction();
  TableFunction(const TableFunction &);
  TableFunction& operator=(const TableFunction &);
public:
  explicit TableFunction(std::string name_arg)
    : Plugin(name_arg, "TableFunction")
  {}
  virtual ~TableFunction() {}

  static bool addPlugin(TableFunction *function);
  static void removePlugin(TableFunction *function);
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_TABLE_FUNCTION_H */
