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

#include "config.h"
#include <drizzled/show.h>
#include <drizzled/session.h>
#include <drizzled/statement/alter_schema.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/db.h>
#include <drizzled/message.h>

#include <string>

using namespace std;

namespace drizzled
{

bool statement::AlterSchema::execute()
{
  LEX_STRING *db= &session->lex->name;
  message::schema::shared_ptr old_definition;

  if (not validateSchemaOptions())
    return true;

  SchemaIdentifier schema_identifier(string(db->str, db->length));

  if (not check_db_name(session, schema_identifier))
  {
    std::string path;
    schema_identifier.getSQLPath(path);
    my_error(ER_WRONG_DB_NAME, MYF(0), path.c_str());

    return false;
  }

  SchemaIdentifier identifier(db->str);
  if (not plugin::StorageEngine::getSchemaDefinition(identifier, old_definition))
  {
    my_error(ER_SCHEMA_DOES_NOT_EXIST, MYF(0), db->str);
    return true;
  }

  if (session->inTransaction())
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION, 
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), 
               MYF(0));
    return true;
  }
  /*
    @todo right now the logic for alter schema is just sitting here, at some point this should be packaged up in a class/etc.
  */

  // We set the name from the old version to keep case preference
  schema_message.set_name(old_definition->name());
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

  bool res= alter_db(session, schema_message);

  return not res;
}

} /* namespace drizzled */

