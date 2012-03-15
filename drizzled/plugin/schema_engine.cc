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

#include <config.h>

#include <drizzled/session.h>
#include <drizzled/sql_base.h>
#include <drizzled/charset.h>
#include <drizzled/transaction_services.h>
#include <drizzled/open_tables_state.h>
#include <drizzled/table/cache.h>
#include <drizzled/plugin/storage_engine.h>
#include <drizzled/plugin/authorization.h>

namespace drizzled {
namespace plugin {

class AddSchemaNames :
  public std::unary_function<StorageEngine *, void>
{
  identifier::schema::vector &schemas;

public:

  AddSchemaNames(identifier::schema::vector &of_names) :
    schemas(of_names)
  {
  }

  result_type operator() (argument_type engine)
  {
    engine->doGetSchemaIdentifiers(schemas);
  }
};

void StorageEngine::getIdentifiers(Session &session, identifier::schema::vector &schemas)
{
  // Add hook here for engines to register schema.
  std::for_each(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
           AddSchemaNames(schemas));

  plugin::Authorization::pruneSchemaNames(*session.user(), schemas);
}

class StorageEngineGetSchemaDefinition: public std::unary_function<StorageEngine *, bool>
{
  const identifier::Schema &identifier;
  message::schema::shared_ptr &schema_proto;

public:
  StorageEngineGetSchemaDefinition(const identifier::Schema &identifier_arg,
                                   message::schema::shared_ptr &schema_proto_arg) :
    identifier(identifier_arg),
    schema_proto(schema_proto_arg)
  {
  }

  result_type operator() (argument_type engine)
  {
    schema_proto= engine->doGetSchemaDefinition(identifier);
    return schema_proto;
  }
};

/*
  Return value is "if parsed"
*/
message::schema::shared_ptr StorageEngine::getSchemaDefinition(const drizzled::identifier::Table &identifier)
{
  identifier::Schema schema_identifier= identifier;
  return StorageEngine::getSchemaDefinition(schema_identifier);
}

message::schema::shared_ptr StorageEngine::getSchemaDefinition(const identifier::Schema &identifier)
{
  message::schema::shared_ptr proto;

  EngineVector::iterator iter=
    std::find_if(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
                 StorageEngineGetSchemaDefinition(identifier, proto));

  if (iter != StorageEngine::getSchemaEngines().end())
  {
    return proto;
  }

  return message::schema::shared_ptr();
}

bool StorageEngine::doesSchemaExist(const identifier::Schema &identifier)
{
  message::schema::shared_ptr proto;

  return StorageEngine::getSchemaDefinition(identifier);
}


const charset_info_st *StorageEngine::getSchemaCollation(const identifier::Schema &identifier)
{
  message::schema::shared_ptr schmema_proto= StorageEngine::getSchemaDefinition(identifier);
  if (not schmema_proto || not schmema_proto->has_collation())
		return default_charset_info;
  const std::string buffer= schmema_proto->collation();
  if (const charset_info_st* cs= get_charset_by_name(buffer.c_str()))
		return cs;
  errmsg_printf(error::ERROR, _("Error while loading database options: '%s':"), identifier.getSQLPath().c_str());
  errmsg_printf(error::ERROR, ER(ER_UNKNOWN_COLLATION), buffer.c_str());
  return default_charset_info;
}

class CreateSchema :
  public std::unary_function<StorageEngine *, void>
{
  const drizzled::message::Schema &schema_message;
  uint64_t &success_count;

public:

  CreateSchema(const drizzled::message::Schema &arg, uint64_t &success_count_arg) :
    schema_message(arg),
    success_count(success_count_arg)
  {
  }

  result_type operator() (argument_type engine)
  {
    // @todo eomeday check that at least one engine said "true"
    bool success= engine->doCreateSchema(schema_message);

    if (success)
    {
      success_count++;
      TransactionServices::allocateNewTransactionId();
    }
  }
};

bool StorageEngine::createSchema(const drizzled::message::Schema &schema_message)
{
  // Add hook here for engines to register schema.
  uint64_t success_count= 0;
  std::for_each(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
                CreateSchema(schema_message, success_count));

  if (success_count)
  {
    TransactionServices::allocateNewTransactionId();
  }

  return (bool)success_count;
}

class DropSchema :
  public std::unary_function<StorageEngine *, void>
{
  uint64_t &success_count;
  const identifier::Schema &identifier;

public:

  DropSchema(const identifier::Schema &arg, uint64_t &count_arg) :
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
      TransactionServices::allocateNewTransactionId();
    }
  }
};

static bool drop_all_tables_in_schema(Session& session,
                                      const identifier::Schema& identifier,
                                      identifier::table::vector &dropped_tables,
                                      uint64_t &deleted)
{
  plugin::StorageEngine::getIdentifiers(session, identifier, dropped_tables);

  for (identifier::table::vector::iterator it= dropped_tables.begin(); it != dropped_tables.end(); it++)
  {
    boost::mutex::scoped_lock scopedLock(table::Cache::mutex());

    message::table::shared_ptr message= StorageEngine::getTableMessage(session, *it, false);
    if (not message)
    {
      my_error(ER_TABLE_DROP, *it);
      return false;
    }

    table::Cache::removeTable(session, *it, RTFC_WAIT_OTHER_THREAD_FLAG | RTFC_CHECK_KILLED_FLAG);
    if (not plugin::StorageEngine::dropTable(session, *it))
    {
      my_error(ER_TABLE_DROP, *it);
      return false;
    }
    TransactionServices::dropTable(session, *it, *message, true);
    deleted++;
  }

  return true;
}

bool StorageEngine::dropSchema(Session& session,
                               const identifier::Schema& identifier,
                               message::schema::const_reference schema_message)
{
  uint64_t deleted= 0;
  bool error= false;
  identifier::table::vector dropped_tables;

  do
  {
    // Remove all temp tables first, this prevents loss of table from
    // shadowing (ie temp over standard table)
    {
      // Lets delete the temporary tables first outside of locks.
      identifier::table::vector set_of_identifiers;
      session.open_tables.doGetTableIdentifiers(identifier, set_of_identifiers);

      for (identifier::table::vector::iterator iter= set_of_identifiers.begin(); iter != set_of_identifiers.end(); iter++)
      {
        if (session.open_tables.drop_temporary_table(*iter))
        {
          my_error(ER_TABLE_DROP, *iter);
          error= true;
          break;
        }
      }
    }

    /* After deleting database, remove all cache entries related to schema */
    table::Cache::removeSchema(identifier);

    if (not drop_all_tables_in_schema(session, identifier, dropped_tables, deleted))
    {
      error= true;
      my_error(ER_DROP_SCHEMA, identifier);
      break;
    }

    uint64_t counter= 0;
    // Add hook here for engines to register schema.
    std::for_each(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
                  DropSchema(identifier, counter));

    if (not counter)
    {
      my_error(ER_DROP_SCHEMA, identifier);
      error= true;

      break;
    }
    else
    {
      /* We've already verified that the schema does exist, so safe to log it */
      TransactionServices::dropSchema(session, identifier, schema_message);
    }
  } while (0);

  if (deleted > 0)
  {
    session.clear_error();
    session.server_status|= SERVER_STATUS_DB_DROPPED;
    session.my_ok((uint32_t) deleted);
    session.server_status&= ~SERVER_STATUS_DB_DROPPED;
  }


  return error;
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
    }
  }
};

bool StorageEngine::alterSchema(const drizzled::message::Schema &schema_message)
{
  uint64_t success_count= 0;

  std::for_each(StorageEngine::getSchemaEngines().begin(), StorageEngine::getSchemaEngines().end(),
                AlterSchema(schema_message, success_count));

  if (success_count)
  {
    TransactionServices::allocateNewTransactionId();
  }

  return success_count ? true : false;
}

} /* namespace plugin */
} /* namespace drizzled */
