/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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


#ifndef DRIZZLED_SERVICE_AUTHENTICATION_H
#define DRIZZLED_SERVICE_AUTHENTICATION_H

#include "drizzled/plugin/authentication.h"

class Session;

namespace drizzled
{
namespace plugin
{
  class Authentication;
}

namespace service
{

class Authentication
{
private:
  std::vector<plugin::Authentication *> all_authentication;
  bool are_plugins_loaded;

public:
  Authentication() : all_authentication(), are_plugins_loaded(false) {}
  ~Authentication() {}
   
  void add(plugin::Authentication *auth);
  void remove(plugin::Authentication *auth);
  bool authenticate(Session *session, const char *password);
};

} /* namespace service */
} /* namespace drizzled */

#endif /* DRIZZLED_SERVICE_AUTHENTICATION_H */
