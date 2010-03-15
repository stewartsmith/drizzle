/*
  Copyright (C) 2010 Stewart Smith

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "config.h"
#include "drizzled/plugin/table_function.h"

#include "embedded_innodb-1.0/innodb.h"

#include "config_table_function.h"

using namespace std;
using namespace drizzled;

class LibInnoDBConfigTool : public drizzled::plugin::TableFunction
{
public:

  LibInnoDBConfigTool();

  LibInnoDBConfigTool(const char *table_arg) :
    drizzled::plugin::TableFunction("data_dictionary", table_arg)
  { }

  ~LibInnoDBConfigTool() {}

  class Generator : public drizzled::plugin::TableFunction::Generator
  {
  private:
    const char **names;
    uint32_t names_count;
    uint32_t names_next;
  public:
    Generator(drizzled::Field **arg);
    ~Generator();

    bool populate();
  };

  LibInnoDBConfigTool::Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

LibInnoDBConfigTool::LibInnoDBConfigTool() :
  plugin::TableFunction("DATA_DICTIONARY", "INNODB_CONFIGURATION")
{
  add_field("NAME");
  add_field("TYPE");
  add_field("VALUE");
}

LibInnoDBConfigTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  names_next(0)
{
  ib_err_t err= ib_cfg_get_all(&names, &names_count);
  assert(err == DB_SUCCESS);
}

LibInnoDBConfigTool::Generator::~Generator()
{
  free(names);
}

bool LibInnoDBConfigTool::Generator::populate()
{
  if (names_next < names_count)
  {
    push(names[names_next]);
    push("");
    push("");

    names_next++;
    return true;
  }
  return false; // No more rows
}

static LibInnoDBConfigTool *config_tool;

int config_table_function_initialize(drizzled::plugin::Registry &registry)
{
  config_tool= new(std::nothrow)LibInnoDBConfigTool();
  registry.add(config_tool);

  return 0;
}

int config_table_function_finalize(drizzled::plugin::Registry &registry)
{
  registry.remove(config_tool);
  delete config_tool;

  return 0;
}
