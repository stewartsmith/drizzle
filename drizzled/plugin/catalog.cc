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

#include <drizzled/plugin/catalog.h>
#include <drizzled/catalog/cache.h>
#include <drizzled/catalog/local.h>
#include <drizzled/error.h>

#include <boost/foreach.hpp>

namespace drizzled
{
namespace plugin
{

// Private container we use for holding the instances of engines passed to
// use from the catalog plugins.
class Engines {
  catalog::Engine::vector _catalogs;

public:
  static Engines& singleton()
  {
    static Engines ptr;
    return ptr;
  }

  catalog::Engine::vector &catalogs()
  {
    return _catalogs;
  }
};

Catalog::~Catalog()
{
}

bool Catalog::create(const identifier::Catalog& identifier)
{
  message::catalog::shared_ptr message= message::catalog::make_shared(identifier);
  return create(identifier, message);
}

bool Catalog::create(const identifier::Catalog& identifier, message::catalog::shared_ptr &message)
{
  assert(message);

  catalog::lock::Create lock(identifier);

  if (not lock.locked())
  {
    my_error(ER_CATALOG_NO_LOCK, MYF(0), identifier.getName().c_str());
    return false;
  }

  size_t create_count= 0;
  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    if (ref->create(identifier, message))
      create_count++;
  }
  assert(create_count < 2);

  if (not create_count)
  {
    my_error(ER_CATALOG_CANNOT_CREATE, MYF(0), identifier.getName().c_str());
    return false;
  }

  return true;
}

bool Catalog::drop(const identifier::Catalog& identifier)
{
  if (identifier == drizzled::catalog::local_identifier())
  {
    my_error(drizzled::ER_CATALOG_NO_DROP_LOCAL, MYF(0));
    return false;
  }

  catalog::lock::Erase lock(identifier);
  if (not lock.locked())
  {
    my_error(ER_CATALOG_NO_LOCK, MYF(0), identifier.getName().c_str());
    return false; 
  }

  
  size_t drop_count= 0;
  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    if (ref->drop(identifier))
      drop_count++;
  }
  assert(drop_count < 2);

  if (not drop_count)
  {
    my_error(ER_CATALOG_DOES_NOT_EXIST, MYF(0), identifier.getName().c_str());
    return false;
  }

  return true;
}

bool Catalog::lock(const identifier::Catalog& identifier)
{
  drizzled::error_t error;
  
  // We insert a lock into the cache, if this fails we bail.
  if (not catalog::Cache::lock(identifier, error))
  {
    my_error(error, identifier);

    return false;
  }

  return true;
}


bool Catalog::unlock(const identifier::Catalog& identifier)
{
  drizzled::error_t error;
  if (not catalog::Cache::unlock(identifier, error))
  {
    my_error(error, identifier);
  }

  return false;
}

bool plugin::Catalog::addPlugin(plugin::Catalog *arg)
{
  Engines::singleton().catalogs().push_back(arg->engine());

  return false;
}

bool plugin::Catalog::exist(const identifier::Catalog& identifier)
{
  if (catalog::Cache::exist(identifier))
    return true;

  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    if (ref->exist(identifier))
      return true;
  }

  return false;
}

void plugin::Catalog::getIdentifiers(identifier::catalog::vector &identifiers)
{
  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    ref->getIdentifiers(identifiers);
  }
}

void plugin::Catalog::getMessages(message::catalog::vector &messages)
{
  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    ref->getMessages(messages);
  }
}

message::catalog::shared_ptr plugin::Catalog::getMessage(const identifier::Catalog& identifier)
{
  drizzled::error_t error;
  catalog::Instance::shared_ptr instance= catalog::Cache::find(identifier, error);
  message::catalog::shared_ptr message;

  if (instance and instance->message())
  {
    return instance->message();
  }

  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    if ((message= ref->getMessage(identifier)))
      return message;
  }

  return message;
}

catalog::Instance::shared_ptr plugin::Catalog::getInstance(const identifier::Catalog& identifier)
{
  drizzled::error_t error;
  catalog::Instance::shared_ptr instance= catalog::Cache::find(identifier, error);

  if (instance)
    return instance;

  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    message::catalog::shared_ptr message;
    if (message= ref->getMessage(identifier))
    {
      instance= catalog::Instance::make_shared(message);
      // If this should fail inserting into the cache, we are in a world of
      // pain.
      catalog::Cache::insert(identifier, instance, error);

      return instance;
    }
  }

  return catalog::Instance::shared_ptr();
}


void plugin::Catalog::removePlugin(plugin::Catalog *)
{
}

} /* namespace plugin */
} /* namespace drizzled */
