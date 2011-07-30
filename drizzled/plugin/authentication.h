/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for Authentication plugin
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

#pragma once

#include <string>

#include <drizzled/plugin.h>
#include <drizzled/plugin/plugin.h>
#include <drizzled/identifier.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

class DRIZZLED_API Authentication : public Plugin
{
public:
  explicit Authentication(std::string name_arg)
    : Plugin(name_arg, "Authentication")
  {}
  virtual bool authenticate(const identifier::User&, const std::string &passwd)= 0;

  static bool addPlugin(plugin::Authentication*);
  static void removePlugin(plugin::Authentication*);
  static bool isAuthenticated(const drizzled::identifier::User&, const std::string &password);
};

} /* namespace plugin */
} /* namespace drizzled */

