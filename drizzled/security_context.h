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


/**
  @class SecurityContext
  @brief A set of Session members describing the current authenticated user.
*/

class SecurityContext {
public:
  SecurityContext() {}
  /*
    host - host of the client
    user - user of the client, set to NULL until the user has been read from
    the connection
    priv_user - The user privilege we are using. May be "" for anonymous user.
    ip - client IP
  */

  std::string& getIp()
  {
    return ip;
  }
  void setIp(char * newip)
  {
    ip.assign(newip);
  }

  std::string& getUser()
  {
    return user;
  }
  void setUser(char * newuser)
  {
    user.assign(newuser);
  }

private:
  std::string user;
  std::string ip;
};

#endif /* DRIZZLED_SECURITY_CONTEXT_H */
