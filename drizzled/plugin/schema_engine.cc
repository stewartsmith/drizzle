/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include "drizzled/session.h"

#include "drizzled/global_charset_info.h"
#include "drizzled/charset.h"
#include "drizzled/transaction_services.h"

#include "drizzled/plugin/storage_engine.h"
#include "drizzled/plugin/authorization.h"

namespace drizzled
{

namespace plugin
{

class AddSchemaNames : 
  public std::unary_function<StorageEngine *, void>
{
  SchemaIdentifier::vector &schemas;

public:

  AddSchemaNames(SchemaIdentifier::vector &of_names) :
    schemas(of_names)
  {
  }

  result_type operator() (argument_type engine)
  {
    engine->doGetSchemaIdentifiers(schemas);
  }
};

void StorageEngine::getIdentifiers(Session &session, SchemaIdentifier::vector &schemas)
{
  // Add hook here for engines to register schema.
  std::for_each(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
           AddSchemaNames(schemas));

  plugin::Authorization::pruneSchemaNames(session.user(), schemas);
}

class StorageEngineGetSchemaDefinition: public std::unary_function<StorageEngine *, bool>
{
  const SchemaIdentifier &identifier;
  message::schema::shared_ptr &schema_proto;

public:
  StorageEngineGetSchemaDefinition(const SchemaIdentifier &identifier_arg,
                                   message::schema::shared_ptr &schema_proto_arg) :
    identifier(identifier_arg),
    schema_proto(schema_proto_arg) 
  {
  }

  result_type operator() (argument_type engine)
  {
    return engine->doGetSchemaDefinition(identifier, schema_proto);
  }
};

/*
  Return value is "if parsed"
*/
bool StorageEngine::getSchemaDefinition(const drizzled::TableIdentifier &identifier, message::schema::shared_ptr &proto)
{
  return StorageEngine::getSchemaDefinition(identifier, proto);
}

bool StorageEngine::getSchemaDefinition(const SchemaIdentifier &identifier, message::schema::shared_ptr &proto)
{
  EngineVector::iterator iter=
    std::find_if(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
                 StorageEngineGetSchemaDefinition(identifier, proto));

  if (iter != StorageEngine::getSchemaEngines().end())
  {
    return true;
  }

  return false;
}

bool StorageEngine::doesSchemaExist(const SchemaIdentifier &identifier)
{
  message::schema::shared_ptr proto;

  return StorageEngine::getSchemaDefinition(identifier, proto);
}


const CHARSET_INFO *StorageEngine::getSchemaCollation(const SchemaIdentifier &identifier)
{
  message::schema::shared_ptr schmema_proto;
  bool found;

  found= StorageEngine::getSchemaDefinition(identifier, schmema_proto);

  if (found && schmema_proto->has_collation())
  {
    const std::string buffer= schmema_proto->collation();
    const CHARSET_INFO* cs= get_charset_by_name(buffer.c_str());

    if (not cs)
    {
      std::string path;
      identifier.getSQLPath(path);

      errmsg_printf(ERRMSG_LVL_ERROR,
                    _("Error while loading database options: '%s':"), path.c_str());
      errmsg_printf(ERRMSG_LVL_ERROR, ER(ER_UNKNOWN_COLLATION), buffer.c_str());

      return default_charset_info;
    }

    return cs;
  }

  return default_charset_info;
}

class CreateSchema : 
  public std::unary_function<StorageEngine *, void>
{
  const drizzled::message::Schema &schema_message;

public:

  CreateSchema(const drizzled::message::Schema &arg) :
    schema_message(arg)
  {
  }

  result_type operator() (argument_type engine)
  {
    // @todo eomeday check that at least one engine said "true"
    bool success= engine->doCreateSchema(schema_message);

    if (success) 
    {
      TransactionServices &transaction_services= TransactionServices::singleton();
      transaction_services.allocateNewTransactionId();
    }
  }
};

bool StorageEngine::createSchema(const drizzled::message::Schema &schema_message)
{
  // Add hook here for engines to register schema.
  std::for_each(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
                CreateSchema(schema_message));

  return true;
}

class DropSchema : 
  public std::unary_function<StorageEngine *, void>
{
  uint64_t &success_count;
  const SchemaIdentifier &identifier;

public:

  DropSchema(const SchemaIdentifier &arg, uint64_t &count_arg) :
    success_count(count_arg),
    identifier(arg)
  {
  }

  result_type operator() (argument_type engine)
  {
    // @todo someday check that at least one engine said "true"
    bool success= engine->doDropSchema(identifier);

    if (success)
    {
      success_count++;
      TransactionServices &transaction_services= TransactionServices::singleton();
      transaction_services.allocateNewTransactionId();
    }
  }
};

bool StorageEngine::dropSchema(Session::reference session, SchemaIdentifier::const_reference identifier)
{
  uint64_t counter= 0;

  {
    // Lets delete the temporary tables first outside of locks.  
    std::set<std::string> set_of_names;
    session.doGetTableNames(identifier, set_of_names);

    for (std::set<std::string>::iterator iter= set_of_names.begin(); iter != set_of_names.end(); iter++)
    {
      TableIdentifier table_identifier(identifier, *iter, message::Table::TEMPORARY);
      session.drop_temporary_table(table_identifier);
    }
  }

  // Add hook here for engines to register schema.
  std::for_each(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
                DropSchema(identifier, counter));

  return counter ? true : false;
}

class AlterSchema : 
  public std::unary_function<StorageEngine *, void>
{
  uint64_t &success_count;
  const drizzled::message::Schema &schema_message;

public:

  AlterSchema(const drizzled::message::Schema &arg, uint64_t &count_arg) :
    success_count(count_arg),
    schema_message(arg)
  {
  }

  result_type operator() (argument_type engine)
  {
    // @todo eomeday check that at least one engine said "true"
    bool success= engine->doAlterSchema(schema_message);


    if (success)
    {
      success_count++;
      TransactionServices &transaction_services= TransactionServices::singleton();
      transaction_services.allocateNewTransactionId();
    }
  }
};

bool StorageEngine::alterSchema(const drizzled::message::Schema &schema_message)
{
  uint64_t success_count= 0;

  std::for_each(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
                AlterSchema(schema_message, success_count));

  return success_count ? true : false;
}

} /* namespace plugin */
} /* namespace drizzled */
