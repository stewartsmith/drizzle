/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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
#include <boost/foreach.hpp>
#include <drizzled/plugin/authentication.h>
#include <drizzled/error.h>
#include <drizzled/gettext.h>
#include <drizzled/identifier.h>
#include <vector>

namespace drizzled {

static std::vector<plugin::Authentication*> all_authentication;

bool plugin::Authentication::addPlugin(plugin::Authentication* auth)
{
  if (auth)
    all_authentication.push_back(auth);
  return false;
}

void plugin::Authentication::removePlugin(plugin::Authentication* auth)
{
  all_authentication.erase(std::find(all_authentication.begin(), all_authentication.end(), auth));
}

bool plugin::Authentication::isAuthenticated(const drizzled::identifier::User& sctx, const std::string& password)
{
  BOOST_FOREACH(plugin::Authentication* auth, all_authentication)
  {
    if (auth->authenticate(sctx, password))
      return true;
  }
  error::access(sctx);
  return false;
}

} /* namespace drizzled */
