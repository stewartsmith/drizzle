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

#include <boost/thread/mutex.hpp>
#include <boost/make_shared.hpp>

#include <drizzled/message/catalog.h>

namespace drizzled {
namespace catalog {

class Instance
{
  Instance() :
    _locked(false),
    _lock_id(0)
  { };

  bool _locked;
  drizzled::session_id_t _lock_id;
  message::catalog::shared_ptr _message;
  mutable boost::mutex _schema_lock;
  mutable boost::mutex _system_variable_lock;


public:
  typedef boost::shared_ptr<Instance> shared_ptr;
  typedef std::vector<shared_ptr> vector;
  typedef const Instance* const_pointer;
  typedef const Instance& const_reference;
  typedef Instance& reference;

  Instance(message::catalog::shared_ptr &message_arg)
  {
    _message= message_arg;
  };

  Instance(const message::catalog::shared_ptr &message_arg)
  {
    _message= message_arg;
  };

  static shared_ptr make_shared(message::catalog::shared_ptr &message_arg)
  {
    assert(not message_arg->name().empty());
    return boost::make_shared<Instance>(message_arg);
  };

  static shared_ptr make_shared(const identifier::Catalog &identifier)
  {
    drizzled::message::catalog::shared_ptr new_message= drizzled::message::catalog::make_shared(identifier);
    assert(not new_message->name().empty());
    return boost::make_shared<Instance>(new_message);
  }

  const std::string &getName() const
  {
    assert(_message);
    return _message->name();
  }

  const std::string &name() const
  {
    assert(_message);
    return _message->name();
  }

  message::catalog::shared_ptr message() const
  {
    return _message;
  }

  bool locked() const
  {
    return _locked;
  }

  bool lock(drizzled::session_id_t id)
  {
    if (_locked and _lock_id == id)
    {
      assert(0); // We shouldn't support recursion
      return true;
    }
    else if (_locked)
    {
      return false;
    }

    _locked= true;
    _lock_id= id;

    return true;
  }

  bool unlock(drizzled::session_id_t id)
  {
    if (_locked and _lock_id == id)
    {
      _locked= false;
      _lock_id= 0;

      return true;
    }

    return false;
  }

  boost::mutex &schemaLock()
  {
    return _schema_lock;
  }

  boost::mutex &systemVariableLock()
  {
    return _system_variable_lock;
  }
};

} /* namespace catalog */
} /* namespace drizzled */

