/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
 *  Copyright (C) 2008 Sun Microsystems
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

#pragma once

#include <string>
#include <boost/shared_ptr.hpp>
#include <drizzled/common_fwd.h>
#include <drizzled/identifier.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace identifier {

/**
  @class User
  @brief A set of Session members describing the current authenticated user.
*/

class User : public Identifier
{
public:
  DRIZZLED_API static user::mptr make_shared();

  enum PasswordType
  {
    NONE,
    PLAIN_TEXT,
    MYSQL_HASH
  };

  User():
    password_type(NONE)
  { }

  User(const std::string &username_arg):
    password_type(NONE),
    _user(username_arg),
    _address("")
  { }

  virtual std::string getSQLPath() const
  {
    return _user.empty() ? "<no user>" : _user;
  }

  bool hasPassword() const
  {
    return password_type != NONE;
  }

  const std::string& address() const
  {
    return _address;
  }

  void setAddress(const char *newip)
  {
    _address = newip;
  }

  const std::string& username() const
  {
    return _user;
  }

  void setUser(const std::string &newuser)
  {
    _user = newuser;
  }

  PasswordType getPasswordType() const
  {
    return password_type;
  }

  void setPasswordType(PasswordType newpassword_type)
  {
    password_type= newpassword_type;
  }

  const std::string& getPasswordContext() const
  {
    return password_context;
  }

  void setPasswordContext(const char *newpassword_context, size_t size)
  {
    password_context.assign(newpassword_context, size);
  }

private:
  PasswordType password_type;
  std::string _user;
  std::string _address;
  std::string password_context;
};

} /* namespace identifier */
} /* namespace drizzled */
