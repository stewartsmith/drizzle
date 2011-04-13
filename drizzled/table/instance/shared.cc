/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2011 Brian Aker
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

#include <drizzled/definition/cache.h>
#include <drizzled/error.h>
#include <drizzled/message/schema.h>
#include <drizzled/plugin/event_observer.h>
#include <drizzled/table/instance/shared.h>
#include <drizzled/plugin/storage_engine.h>

namespace drizzled {
namespace table {
namespace instance {

Shared::Shared(const identifier::Table::Type type_arg,
               const identifier::Table &identifier,
               char *path_arg, uint32_t path_length_arg) :
  TableShare(type_arg, identifier, path_arg, path_length_arg),
  event_observers(NULL)
{
}

Shared::Shared(const identifier::Table &identifier,
               message::schema::shared_ptr schema_message) :
  TableShare(message::Table::STANDARD, identifier, NULL, 0),
  _schema(schema_message),
  event_observers(NULL)
{
}

Shared::Shared(const identifier::Table &identifier) :
  TableShare(identifier, identifier.getKey()),
  event_observers(NULL)
{
}

bool Shared::is_replicated() const
{
  if (_schema)
  {
    if (not message::is_replicated(*_schema))
      return false;
  }

  assert(getTableMessage());
  return message::is_replicated(*getTableMessage());
}


Shared::shared_ptr Shared::foundTableShare(Shared::shared_ptr share)
{
  /*
    We found an existing table definition. Return it if we didn't get
    an error when reading the table definition from file.
  */
  if (share->error)
  {
    /* Table definition contained an error */
    share->open_table_error(share->error, share->open_errno, share->errarg);

    return Shared::shared_ptr();
  }

  share->incrementTableCount();

  return share;
}



/*
  Get a shared instance for a table.

  get_table_share()
  session			Thread handle
  table_list		Table that should be opened
  key			Table cache key
  key_length		Length of key
  error			out: Error code from open_table_def()

  IMPLEMENTATION
  Get a table definition from the table definition cache.
  If it doesn't exist, create a new from the table definition file.

  NOTES
  We must have wrlock on table::Cache::mutex() when we come here
  (To be changed later)

  RETURN
  0  Error
#  Share for table
*/

Shared::shared_ptr Shared::make_shared(Session *session, 
                                       const identifier::Table &identifier,
                                       int &in_error)
{
  Shared::shared_ptr share;

  in_error= 0;

  /* Read table definition from cache */
  if ((share= definition::Cache::find(identifier.getKey())))
    return foundTableShare(share);
  
  drizzled::message::schema::shared_ptr schema_message_ptr= plugin::StorageEngine::getSchemaDefinition(identifier);

  if (not schema_message_ptr)
  {
    drizzled::my_error(ER_SCHEMA_DOES_NOT_EXIST, identifier);
    return Shared::shared_ptr();
  }

  share.reset(new Shared(identifier, schema_message_ptr));

  if (share->open_table_def(*session, identifier))
  {
    in_error= share->error;

    return Shared::shared_ptr();
  }
  share->incrementTableCount();				// Mark in use
  
  plugin::EventObserver::registerTableEvents(*share);

  bool ret= definition::Cache::insert(identifier.getKey(), share);

  if (not ret)
  {
    drizzled::my_error(ER_UNKNOWN_ERROR);
    return Shared::shared_ptr();
  }

  return share;
}

Shared::~Shared()
{
  assert(getTableCount() == 0);
  plugin::EventObserver::deregisterTableEvents(*this);
}


/*****************************************************************************
  Functions to handle table definition cach (TableShare)
 *****************************************************************************/

/*
  Mark that we are not using table share anymore.

  SYNOPSIS
  release()
  share		Table share

  IMPLEMENTATION
  If ref_count goes to zero and (we have done a refresh or if we have
  already too many open table shares) then delete the definition.
*/

void release(TableShare *share)
{
  bool to_be_deleted= false;
  //safe_mutex_assert_owner(table::Cache::mutex().native_handle);

  share->lock();
  if (not share->decrementTableCount())
  {
    to_be_deleted= true;
  }
  share->unlock();

  if (to_be_deleted)
  {
    definition::Cache::erase(share->getCacheKey());
  }
}

void release(TableShare::shared_ptr &share)
{
  bool to_be_deleted= false;
#if 0
  safe_mutex_assert_owner(table::Cache::mutex().native_handle);
#endif

  share->lock();
  if (not share->decrementTableCount())
  {
    to_be_deleted= true;
  }
  share->unlock();

  if (to_be_deleted)
  {
    definition::Cache::erase(share->getCacheKey());
  }
}

void release(const identifier::Table &identifier)
{
  TableShare::shared_ptr share= definition::Cache::find(identifier.getKey());
  if (share)
  {
    share->resetVersion(); 
    if (share->getTableCount() == 0)
    {
      definition::Cache::erase(identifier.getKey());
    }
  }
}


} /* namespace instance */
} /* namespace table */
} /* namespace drizzled */
