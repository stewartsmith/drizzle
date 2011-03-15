/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/alter_schema.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/schema.h>
#include <drizzled/message.h>
#include <drizzled/sql_lex.h>

#include <string>

using namespace std;

namespace drizzled
{

bool statement::AlterSchema::execute()
{
  LEX_STRING *db= &lex().name;
  message::schema::shared_ptr old_definition;

  if (not validateSchemaOptions())
    return true;

  identifier::Schema schema_identifier(string(db->str, db->length));

  if (not schema::check(session(), schema_identifier))
  {
    my_error(ER_WRONG_DB_NAME, schema_identifier);

    return false;
  }

  identifier::Schema identifier(db->str);
  if (not (old_definition= plugin::StorageEngine::getSchemaDefinition(identifier)))
  {
    my_error(ER_SCHEMA_DOES_NOT_EXIST, identifier); 
    return true;
  }

  if (session().inTransaction())
  {
    my_error(ER_TRANSACTIONAL_DDL_NOT_SUPPORTED, MYF(0));
    return true;
  }
  /*
    @todo right now the logic for alter schema is just sitting here, at some point this should be packaged up in a class/etc.
  */

  // First initialize the schema message
  drizzled::message::schema::init(schema_message, old_definition->name());

  // We set the name from the old version to keep case preference
  schema_message.set_version(old_definition->version());
  schema_message.set_uuid(old_definition->uuid());
  schema_message.mutable_engine()->set_name(old_definition->engine().name());

  // We need to make sure we don't destroy any collation that might have
  // been changed.
  if (not schema_message.has_collation())
  {
    schema_message.set_collation(old_definition->collation());
  }
  
  drizzled::message::update(schema_message);

  bool res= schema::alter(session(), schema_message, *old_definition);

  return not res;
}

} /* namespace drizzled */

