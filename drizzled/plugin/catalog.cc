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

#include "drizzled/plugin/catalog.h"
#include "drizzled/catalog/cache.h"
#include "drizzled/error.h"

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

bool Catalog::create(const identifier::Catalog &identifier)
{
  message::catalog::shared_ptr message= message::catalog::make_shared(identifier);
  return create(identifier, message);
}

bool Catalog::create(const identifier::Catalog &identifier, message::catalog::shared_ptr &message)
{
  catalog::error_t error;
  bool success;

  assert(message);

  // We insert a lock into the cache, if this fails we bail.
  if (not catalog::Cache::singleton().lock(identifier, error))
  {
    catalog::error(error, identifier);

    return false;
  }

  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    if (ref->create(identifier, message))
    {
      success= true;
      break;
    }
  }

  if (not catalog::Cache::singleton().unlock(identifier, error))
  {
    catalog::error(error, identifier);
  }

  return success;
}

bool Catalog::drop(const identifier::Catalog &identifier)
{
  catalog::error_t error;

  static drizzled::identifier::Catalog LOCAL_IDENTIFIER("local");
  if (identifier == LOCAL_IDENTIFIER)
  {
    my_error(drizzled::ER_CATALOG_NO_DROP_LOCAL, MYF(0));
    return false;
  }

  
  // We insert a lock into the cache, if this fails we bail.
  if (not catalog::Cache::singleton().lock(identifier, error))
  {
    catalog::error(error, identifier);

    return false;
  }

  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    if (ref->drop(identifier))
    {
      return true;
    }
  }

  if (not catalog::Cache::singleton().unlock(identifier, error))
  {
    catalog::error(error, identifier);
  }

  return false;
}

bool Catalog::lock(const identifier::Catalog &identifier)
{
  catalog::error_t error;
  
  // We insert a lock into the cache, if this fails we bail.
  if (not catalog::Cache::singleton().lock(identifier, error))
  {
    catalog::error(error, identifier);

    return false;
  }

  return true;
}


bool Catalog::unlock(const identifier::Catalog &identifier)
{
  catalog::error_t error;
  if (not catalog::Cache::singleton().unlock(identifier, error))
  {
    catalog::error(error, identifier);
  }

  return false;
}

bool plugin::Catalog::addPlugin(plugin::Catalog *arg)
{
  Engines::singleton().catalogs().push_back(arg->engine());

  return false;
}

bool plugin::Catalog::exist(const identifier::Catalog &identifier)
{
  if (catalog::Cache::singleton().exist(identifier))
    return true;

  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    if (ref->exist(identifier))
      return true;
  }

  return false;
}

void plugin::Catalog::getIdentifiers(identifier::Catalog::vector &identifiers)
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

bool plugin::Catalog::getMessage(const identifier::Catalog &identifier, message::catalog::shared_ptr &message)
{
  catalog::error_t error;
  catalog::Instance::shared_ptr instance= catalog::Cache::singleton().find(identifier, error);

  if (instance and instance->message())
  {
    message= instance->message();

    if (message)
      return message;
  }

  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    if (ref->getMessage(identifier, message))
      return true;
  }

  return false;
}

bool plugin::Catalog::getInstance(const identifier::Catalog &identifier, catalog::Instance::shared_ptr &instance)
{
  catalog::error_t error;
  instance= catalog::Cache::singleton().find(identifier, error);

  if (instance)
    return true;

  BOOST_FOREACH(catalog::Engine::vector::const_reference ref, Engines::singleton().catalogs())
  {
    if (ref->getInstance(identifier, instance))
      return true;
  }

  // If this should fail, we are in a world of pain.
  catalog::Cache::singleton().insert(identifier, instance, error);

  return true;
}


void plugin::Catalog::removePlugin(plugin::Catalog *arg)
{
  Engines::singleton().catalogs().erase(find(Engines::singleton().catalogs().begin(),
                                             Engines::singleton().catalogs().end(),
                                             arg->engine()));
}

} /* namespace plugin */
} /* namespace drizzled */
