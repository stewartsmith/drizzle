/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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
#include "plugin/show_dictionary/dictionary.h"
#include "drizzled/identifier.h"
#include "drizzled/message.h"
#include "drizzled/message/statement_transform.h"
#include <google/protobuf/text_format.h>
#include <string>

using namespace std;
using namespace drizzled;

#define MAX_TABLE_DEFINITION_LENGTH 64000

ShowCreateTable::ShowCreateTable() :
  plugin::TableFunction("DATA_DICTIONARY", "TABLE_SQL_DEFINITION")
{
  add_field("TABLE_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("TABLE_SQL_DEFINITION", plugin::TableFunction::STRING, MAX_TABLE_DEFINITION_LENGTH, false);
}

ShowCreateTable::Generator::Generator(Field **arg) :
  plugin::TableFunction::Generator(arg),
  is_table_primed(false)
{
  statement::Select *select= static_cast<statement::Select *>(getSession().lex->statement);

  if (not select->getShowTable().empty() && not select->getShowSchema().empty())
  {
    table_name.append(select->getShowTable().c_str());
    TableIdentifier identifier(select->getShowSchema().c_str(), select->getShowTable().c_str());

    int error= plugin::StorageEngine::getTableDefinition(getSession(),
                                                         identifier,
                                                         table_message);

    if (error == EEXIST)
      is_table_primed= true;

    std::string foo;
    google::protobuf::TextFormat::PrintToString(table_message, &foo);

    const char *was_true= is_table_primed ? "true" : "false";
    std::cerr << identifier << "->Table MEssage (" << was_true << ") "<< foo <<  "\n";
  }
}

bool ShowCreateTable::Generator::populate()
{
  enum drizzled::message::TransformSqlError transform_err;

  const char *was_true= is_table_primed ? "true" : "false";
  if (not is_table_primed)
    return false;

  std::string create_sql;
  transform_err= message::transformTableDefinitionToSql(table_message,
                                                        create_sql,
                                                        message::DRIZZLE,
                                                        false);
  if (transform_err != drizzled::message::NONE)
  {
    std::cerr << "We got an error on transformation\n";
    return false;
  }
  std::cerr << "Table name " << table_name << " : " << was_true << " : " << create_sql << "\n";

  push(table_name);
  push(create_sql.length() < MAX_TABLE_DEFINITION_LENGTH ? create_sql : create_sql.substr(0, MAX_TABLE_DEFINITION_LENGTH));
  is_table_primed= false;

  return true;
}
