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
#include <drizzled/table_function_container.h>
#include <drizzled/gettext.h>
#include "drizzled/plugin/registry.h"
#include "drizzled/global_charset_info.h"

#include <vector>

using namespace std;

using namespace drizzled;

static TableFunctionContainer table_functions;

void plugin::TableFunction::init()
{
  drizzled::message::Table::StorageEngine *engine;
  drizzled::message::Table::TableOptions *table_options;

  proto.set_name(identifier.getTableName());
  proto.set_type(drizzled::message::Table::FUNCTION);

  table_options= proto.mutable_options();
  table_options->set_collation_id(default_charset_info->number);
  table_options->set_collation(default_charset_info->name);

  engine= proto.mutable_engine();
  engine->set_name("FunctionEngine");
}

bool plugin::TableFunction::addPlugin(plugin::TableFunction *tool)
{
  assert(tool != NULL);
  table_functions.addFunction(tool); 
  return false;
}

plugin::TableFunction *plugin::TableFunction::getFunction(const string &arg)
{
  return table_functions.getFunction(arg);
}

void plugin::TableFunction::getNames(const string &arg,
                                     set<std::string> &set_of_names)
{
  table_functions.getNames(arg, set_of_names);
}

plugin::TableFunction::Generator *plugin::TableFunction::generator(Field **arg)
{
  return new Generator(arg);
}

void plugin::TableFunction::add_field(const char *label,
                              uint32_t field_length)
{
  add_field(label, TableFunction::STRING, field_length);
}

void plugin::TableFunction::add_field(const char *label,
                              TableFunction::ColumnType type,
                              bool is_default_null)
{
  add_field(label, type, 5, is_default_null);
}

void plugin::TableFunction::add_field(const char *label,
                              TableFunction::ColumnType type,
                              uint32_t field_length,
                              bool is_default_null)
{
  drizzled::message::Table::Field *field;
  drizzled::message::Table::Field::FieldOptions *field_options;
  drizzled::message::Table::Field::FieldConstraints *field_constraints;

  field= proto.add_field();
  field->set_name(label);

  field_options= field->mutable_options();
  field_constraints= field->mutable_constraints();
  field_options->set_default_null(is_default_null);
  field_constraints->set_is_nullable(is_default_null);

  switch (type) 
  {
  default:
  case TableFunction::BOOLEAN:
    field_length= 5;
  case TableFunction::STRING:
    drizzled::message::Table::Field::StringFieldOptions *string_field_options;
    field->set_type(drizzled::message::Table::Field::VARCHAR);

    string_field_options= field->mutable_string_options();
    string_field_options->set_length(field_length);

    break;
  case TableFunction::NUMBER:
    field->set_type(drizzled::message::Table::Field::BIGINT);
    break;
  }
}

plugin::TableFunction::Generator::Generator(Field **arg) :
  columns(arg)
{
  scs= system_charset_info;
}

bool plugin::TableFunction::Generator::sub_populate()
{
  columns_iterator= columns;
  return populate();
}

void plugin::TableFunction::Generator::push(uint64_t arg)
{
  (*columns_iterator)->store(static_cast<int64_t>(arg), false);
  columns_iterator++;
}

void plugin::TableFunction::Generator::push(uint32_t arg)
{
  (*columns_iterator)->store(static_cast<int64_t>(arg), false);
  columns_iterator++;
}

void plugin::TableFunction::Generator::push(int64_t arg)
{
  (*columns_iterator)->store(arg, false);
  columns_iterator++;
}

void plugin::TableFunction::Generator::push(int32_t arg)
{
  (*columns_iterator)->store(arg, false);
  columns_iterator++;
}

void plugin::TableFunction::Generator::push(const char *arg, uint32_t length)
{
  assert(columns_iterator);
  assert(*columns_iterator);
  assert(arg);
  (*columns_iterator)->store(arg, length ? length : strlen(arg), scs);
  columns_iterator++;
}

void plugin::TableFunction::Generator::push(const std::string& arg)
{
  (*columns_iterator)->store(arg.c_str(), arg.length(), scs);
  columns_iterator++;
}

void plugin::TableFunction::Generator::push(bool arg)
{
  if (arg)
  {
    (*columns_iterator)->store("TRUE", 4, scs);
  }
  else
  {
    (*columns_iterator)->store("FALSE", 5, scs);
  }

  columns_iterator++;
}
