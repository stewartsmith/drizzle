/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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


#ifndef DRIZZLED_SECURITY_CONTEXT_H
#define DRIZZLED_SECURITY_CONTEXT_H

namespace drizzled
{

/**
  @class SecurityContext
  @brief A set of Session members describing the current authenticated user.
*/

class SecurityContext {
public:
  SecurityContext() {}

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

  void setUser(const char *newuser)
  {
    user.assign(newuser);
  }

private:
  std::string user;
  std::string ip;
};

} /* namespace drizzled */

#endif /* DRIZZLED_SECURITY_CONTEXT_H */
