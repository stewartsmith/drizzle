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

#pragma once

#include <boost/bind.hpp>

#include <drizzled/identifier.h>
#include <drizzled/error.h>
#include <drizzled/catalog.h>
#include <drizzled/plugin/catalog.h>

#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>

namespace drizzled {
namespace catalog  {

class Cache
{
  static inline Cache &singleton()
  {
    static Cache open_cache;

    return open_cache;
  }

  size_t size() const
  {
    return cache.size();
  }

  void rehash(size_t arg)
  {
    cache.rehash(arg);
  }

  Instance::shared_ptr find(const identifier::Catalog &identifier, drizzled::error_t &error);
  bool exist(const identifier::Catalog &identifier);
  bool erase(const identifier::Catalog &identifier, drizzled::error_t &error);
  bool insert(const identifier::Catalog &identifier, Instance::shared_ptr instance, drizzled::error_t &error);
  bool lock(const identifier::Catalog &identifier, drizzled::error_t &error);
  bool unlock(const identifier::Catalog &identifier, drizzled::error_t &error);

  friend class drizzled::generator::catalog::Cache;
  friend class drizzled::plugin::Catalog;
  friend class drizzled::catalog::lock::Erase;
  friend class drizzled::catalog::lock::Create;

  void copy(catalog::Instance::vector &vector);

  typedef boost::unordered_map< identifier::Catalog, catalog::Instance::shared_ptr> unordered_map;

  unordered_map cache;
  boost::mutex _mutex;
};


namespace lock {

class Erase
{
  bool _locked;
  const identifier::Catalog &identifier;
  drizzled::error_t error;

public:
  Erase(const identifier::Catalog &identifier_arg) :
    _locked(false),
    identifier(identifier_arg)
  {
    init();
  }

  bool locked () const
  {
    return _locked;
  }

  ~Erase()
  {
    if (_locked)
    {
      if (not catalog::Cache::singleton().unlock(identifier, error))
      {
        my_error(error, identifier);
        assert(0);
      }
    }
  }

private:
  void init()
  {
    // We insert a lock into the cache, if this fails we bail.
    if (not catalog::Cache::singleton().lock(identifier, error))
    {
      assert(0);
      return;
    }

    _locked= true;
  }
};


class Create
{
  bool _locked;
  const identifier::Catalog &identifier;
  drizzled::error_t error;

public:
  Create(const identifier::Catalog &identifier_arg) :
    _locked(false),
    identifier(identifier_arg)
  {
    init();
  }

  bool locked () const
  {
    return _locked;
  }

  ~Create()
  {
    if (_locked)
    {
      if (not catalog::Cache::singleton().unlock(identifier, error))
      {
        my_error(error, identifier);
        assert(0);
      }
    }
  }


private:
  void init()
  {
    // We insert a lock into the cache, if this fails we bail.
    if (not catalog::Cache::singleton().lock(identifier, error))
    {
      my_error(error, identifier);
      return;
    }

    _locked= true;
  }
};

} /* namespace lock */
} /* namespace catalog */
} /* namespace drizzled */

