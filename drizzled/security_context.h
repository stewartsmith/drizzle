/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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


#ifndef DRIZZLED_SECURITY_CONTEXT_H
#define DRIZZLED_SECURITY_CONTEXT_H

#include <string>

namespace drizzled
{

/**
  @class SecurityContext
  @brief A set of Session members describing the current authenticated user.
*/

class SecurityContext {
public:
  enum PasswordType
  {
    PLAIN_TEXT,
    MYSQL_HASH
  };

  SecurityContext():
    password_type(PLAIN_TEXT)
  { }

  const std::string& getIp() const
  {
    return ip;
  }

  void setIp(const char *newip)
  {
    ip.assign(newip);
  }

  const std::string& getUser() const
  {
    return user;
  }

  void setUser(const std::string &newuser)
  {
    user.assign(newuser);
  }

  PasswordType getPasswordType(void) const
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
  std::string user;
  std::string ip;
  std::string password_context;
};

} /* namespace drizzled */

#endif /* DRIZZLED_SECURITY_CONTEXT_H */
