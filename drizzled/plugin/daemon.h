/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Daemon Plugin Interface
 *
 *  Copyright (C) 2010 Monty Taylor
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

#include <drizzled/plugin/plugin.h>

#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

class DRIZZLED_API Daemon : public Plugin
{
  Daemon();
  Daemon(const Daemon &);
  Daemon& operator=(const Daemon &);
public:
  explicit Daemon(std::string name_arg)
    : Plugin(name_arg, "Daemon")
  {}

  virtual ~Daemon() {}

  /**
   * Standard plugin system registration hooks
   */
  static bool addPlugin(Daemon *)
  {
    return false;
  }
  static void removePlugin(Daemon *)
  {}

};

} /* namespace plugin */
} /* namespace drizzled */

