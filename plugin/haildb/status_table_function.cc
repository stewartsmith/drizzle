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

#include <config.h>
#include <drizzled/plugin/table_function.h>

#include <haildb.h>

#include "status_table_function.h"

using namespace std;
using namespace drizzled;

class LibInnoDBStatusTool : public drizzled::plugin::TableFunction
{
public:

  LibInnoDBStatusTool();

  LibInnoDBStatusTool(const char *table_arg) :
    drizzled::plugin::TableFunction("data_dictionary", table_arg)
  { }

  ~LibInnoDBStatusTool() {}

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

  LibInnoDBStatusTool::Generator *generator(drizzled::Field **arg)
  {
    return new Generator(arg);
  }
};

LibInnoDBStatusTool::LibInnoDBStatusTool() :
  plugin::TableFunction("DATA_DICTIONARY", "HAILDB_STATUS")
{
  add_field("NAME");
  add_field("VALUE", plugin::TableFunction::NUMBER);
}

LibInnoDBStatusTool::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  names_next(0)
{
  ib_err_t err= ib_status_get_all(&names, &names_count);
  assert(err == DB_SUCCESS);
}

LibInnoDBStatusTool::Generator::~Generator()
{
  free(names);
}

bool LibInnoDBStatusTool::Generator::populate()
{
  if (names[names_next] != NULL)
  {
    const char* config_name= names[names_next];

    push(config_name);

    ib_i64_t value;
    ib_err_t err= ib_status_get_i64(config_name, &value);
    assert(err == DB_SUCCESS);

    push(value);

    names_next++;
    return true;
  }

  return false; // No more rows
}

static LibInnoDBStatusTool *status_tool;

int status_table_function_initialize(drizzled::module::Context &context)
{
  status_tool= new LibInnoDBStatusTool();
  context.add(status_tool);
	return 0;
}
