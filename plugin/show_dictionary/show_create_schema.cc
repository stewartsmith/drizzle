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

#include <config.h>
#include <plugin/show_dictionary/dictionary.h>
#include <drizzled/identifier.h>
#include <drizzled/message.h>
#include <drizzled/message/statement_transform.h>
#include <string>

using namespace std;
using namespace drizzled;

ShowCreateSchema::ShowCreateSchema() :
  show_dictionary::Show("SCHEMA_SQL_DEFINITION")
{
  add_field("SCHEMA_NAME", plugin::TableFunction::STRING, MAXIMUM_IDENTIFIER_LENGTH, false);
  add_field("SCHEMA_SQL_DEFINITION", plugin::TableFunction::STRING, TABLE_FUNCTION_BLOB_SIZE, false);
}

ShowCreateSchema::Generator::Generator(Field **arg) :
  show_dictionary::Show::Generator(arg),
  if_not_exists(false)
{
  if (not isShowQuery())
   return;

  statement::Show& select= static_cast<statement::Show&>(statement());

  if (not select.getShowSchema().empty())
  {
    schema_name.append(select.getShowTable());
    identifier::Schema identifier(select.getShowSchema());

    if (not plugin::Authorization::isAuthorized(*getSession().user(),
                                                identifier, false))
    {
      drizzled::error::access(*getSession().user(), identifier);
      return;
    }

    schema_message= plugin::StorageEngine::getSchemaDefinition(identifier);

    if_not_exists= select.getShowExists();
  }
}

bool ShowCreateSchema::Generator::populate()
{
  if (not schema_message)
    return false;

  std::string buffer;

  /* This needs to be moved out to its own function */
  {
    buffer.append("CREATE DATABASE ");

    if (if_not_exists)
      buffer.append("IF NOT EXISTS ");

    buffer.append("`");
    buffer.append(schema_message->name());
    buffer.append("`");

    if (schema_message->has_collation())
    {
      buffer.append(" COLLATE = ");
      buffer.append(schema_message->collation());
    }

    if (not message::is_replicated(*schema_message))
    {
      buffer.append(" REPLICATE = FALSE");
    }
  }

  push(schema_message->name());
  push(buffer);

  schema_message.reset();

  return true;
}
