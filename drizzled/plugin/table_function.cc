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

#include <config.h>

#include <drizzled/current_session.h>
#include <drizzled/gettext.h>
#include <drizzled/charset.h>
#include <drizzled/plugin/table_function.h>
#include <drizzled/session.h>
#include <drizzled/show.h>
#include <drizzled/table_function_container.h>
#include <drizzled/sql_lex.h>

#include <vector>

namespace drizzled {

static TableFunctionContainer table_functions;

void plugin::TableFunction::init()
{
  drizzled::message::table::init(proto, getTableLabel(), identifier.getSchemaName(), "FunctionEngine");
  proto.set_type(drizzled::message::Table::FUNCTION);
  proto.set_creation_timestamp(0);
  proto.set_update_timestamp(0);
  message::set_is_replicated(proto, false);
  message::set_definer(proto, SYSTEM_USER);
}

bool plugin::TableFunction::addPlugin(plugin::TableFunction *tool)
{
  assert(tool != NULL);
  table_functions.addFunction(tool);
  return false;
}

plugin::TableFunction *plugin::TableFunction::getFunction(const std::string &arg)
{
  return table_functions.getFunction(arg);
}

void plugin::TableFunction::getNames(const std::string &arg,
                                     std::set<std::string> &set_of_names)
{
  table_functions.getNames(arg, set_of_names);
}

LEX& plugin::TableFunction::Generator::lex()
{
	return getSession().lex();
}

statement::Statement& plugin::TableFunction::Generator::statement()
{
	return *lex().statement;
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
  field_constraints->set_is_notnull(not is_default_null);

  switch (type)
  {
  case TableFunction::STRING:
    {
      drizzled::message::Table::Field::StringFieldOptions *string_field_options;
      if (field_length >= TABLE_FUNCTION_BLOB_SIZE)
      {
        field->set_type(drizzled::message::Table::Field::BLOB);
        string_field_options= field->mutable_string_options();
        string_field_options->set_collation_id(default_charset_info->number);
        string_field_options->set_collation(default_charset_info->name);
      }
      else
      {
        field->set_type(drizzled::message::Table::Field::VARCHAR);
        string_field_options= field->mutable_string_options();
        string_field_options->set_length(field_length);
      }
    }
    break;
  case TableFunction::VARBINARY:
    {
      drizzled::message::Table::Field::StringFieldOptions *string_field_options;
      field->set_type(drizzled::message::Table::Field::VARCHAR);

      string_field_options= field->mutable_string_options();
      string_field_options->set_length(field_length);
      string_field_options->set_collation(my_charset_bin.csname);
      string_field_options->set_collation_id(my_charset_bin.number);
    }
    break;
  case TableFunction::NUMBER:
    field->set_type(drizzled::message::Table::Field::BIGINT);
    break;
  case TableFunction::SIZE:
    field->set_type(drizzled::message::Table::Field::BIGINT);
    field_constraints->set_is_unsigned(true);
    break;
  case TableFunction::BOOLEAN: // Currently BOOLEAN always has a value
    field->set_type(drizzled::message::Table::Field::BOOLEAN);
    field_constraints->set_is_unsigned(true);
    break;
  }
}

plugin::TableFunction::Generator::Generator(Field **arg) :
  columns(arg),
  session(current_session)
{
  scs= system_charset_info;
}

bool plugin::TableFunction::Generator::sub_populate(uint32_t field_size)
{
  columns_iterator= columns;
  bool ret= populate();
  uint64_t difference= columns_iterator - columns;

  if (ret)
    assert(difference == field_size);
  return ret;
}

void plugin::TableFunction::Generator::push(uint64_t arg)
{
  (*columns_iterator)->store(static_cast<int64_t>(arg), true);
  (*columns_iterator)->set_notnull();
  columns_iterator++;
}

void plugin::TableFunction::Generator::push(int64_t arg)
{
  (*columns_iterator)->store(arg, false);
  (*columns_iterator)->set_notnull();
  columns_iterator++;
}

void plugin::TableFunction::Generator::push(const char *arg, uint32_t length)
{
  assert(columns_iterator);
  assert(*columns_iterator);
  assert(arg);
  length= length ? length : strlen(arg);

  if ((*columns_iterator)->char_length() < length)
    length= (*columns_iterator)->char_length();

  (*columns_iterator)->store(arg, length, scs);
  (*columns_iterator)->set_notnull();
  columns_iterator++;
}

void plugin::TableFunction::Generator::push()
{
  /* Only accept NULLs */
  assert((*columns_iterator)->maybe_null());
  (*columns_iterator)->set_null();
  columns_iterator++;
}

void plugin::TableFunction::Generator::push(const std::string& arg)
{
  push(arg.c_str(), arg.length());
}

void plugin::TableFunction::Generator::push(bool arg)
{
  if (arg)
    (*columns_iterator)->store("YES", 3, scs);
  else
    (*columns_iterator)->store("NO", 2, scs);
  columns_iterator++;
}

bool plugin::TableFunction::Generator::isWild(const std::string &predicate)
{
  return lex().wild ? wild_case_compare(system_charset_info, predicate.c_str(), lex().wild->ptr()) : false;
}

} /* namespace drizzled */
