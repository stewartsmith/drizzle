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
#include <plugin/function_engine/function.h>
#include <plugin/function_engine/cursor.h>
#include <string>

using namespace std;
using namespace drizzled;

Function::Function(const std::string &name_arg) :
  drizzled::plugin::StorageEngine(name_arg,
                                  HTON_ALTER_NOT_SUPPORTED |
                                  HTON_HAS_SCHEMA_DICTIONARY |
                                  HTON_SKIP_STORE_LOCK |
                                  HTON_TEMPORARY_NOT_SUPPORTED),
  information_message(new message::Schema),
  data_dictionary_message(new message::Schema)

{
  information_message->set_name(INFORMATION_SCHEMA_IDENTIFIER.getSchemaName());
  information_message->set_collation("utf8_general_ci");
  message::set_is_replicated(*information_message, false);

  data_dictionary_message->set_name(DATA_DICTIONARY_IDENTIFIER.getSchemaName());
  data_dictionary_message->set_collation("utf8_general_ci");
  message::set_is_replicated(*data_dictionary_message, false);
}


Cursor *Function::create(Table &table)
{
  return new FunctionCursor(*this, table);
}

int Function::doGetTableDefinition(Session &,
                                   const identifier::Table &identifier,
                                   message::Table &table_proto)
{
  drizzled::plugin::TableFunction *function= getFunction(identifier.getPath());

  if (not function)
  {
    return ENOENT;
  }

  function->define(table_proto);

  return EEXIST;
}

void Function::doGetSchemaIdentifiers(identifier::schema::vector& schemas)
{
  schemas.push_back(INFORMATION_SCHEMA_IDENTIFIER);
  schemas.push_back(DATA_DICTIONARY_IDENTIFIER);
}

drizzled::message::schema::shared_ptr Function::doGetSchemaDefinition(const identifier::Schema &schema_identifier)
{
  drizzled::message::schema::shared_ptr schema_message;

  if (schema_identifier == INFORMATION_SCHEMA_IDENTIFIER)
  {
    schema_message= information_message;
  }
  else if (schema_identifier == DATA_DICTIONARY_IDENTIFIER)
  {
    schema_message= data_dictionary_message;
  }
  else
  {
    return drizzled::message::schema::shared_ptr();
  }

  return schema_message;
}

bool Function::doCanCreateTable(const drizzled::identifier::Table &table_identifier)
{
  if (static_cast<const identifier::Schema&>(table_identifier) == INFORMATION_SCHEMA_IDENTIFIER)
  {
    return false;
  }

  else if (static_cast<const identifier::Schema&>(table_identifier) == DATA_DICTIONARY_IDENTIFIER)
  {
    return false;
  }

  return true;
}

bool Function::doDoesTableExist(Session&, const identifier::Table &identifier)
{
  return getFunction(identifier.getPath());
}


void Function::doGetTableIdentifiers(drizzled::CachedDirectory&,
                                     const drizzled::identifier::Schema &schema_identifier,
                                     drizzled::identifier::table::vector &set_of_identifiers)
{
  set<std::string> set_of_names;
  drizzled::plugin::TableFunction::getNames(schema_identifier.getSchemaName(), set_of_names);

  BOOST_FOREACH(const std::string& iter, set_of_names)
  {
    set_of_identifiers.push_back(identifier::Table(schema_identifier, iter, drizzled::message::Table::FUNCTION));
  }
}

static int init(drizzled::module::Context &context)
{
  context.add(new Function("FunctionEngine"));

  return 0;
}

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "FunctionEngine",
  "1.0",
  "Brian Aker",
  "Function Engine provides the infrastructure for Table Functions,etc.",
  PLUGIN_LICENSE_GPL,
  init,     /* Plugin Init */
  NULL,               /* depends */
  NULL                /* config options   */
}
DRIZZLE_DECLARE_PLUGIN_END;
