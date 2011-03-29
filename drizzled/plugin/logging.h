/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for Query Logging plugin
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

#include <drizzled/plugin/plugin.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

class DRIZZLED_API Logging : public Plugin
{
  Logging();
  Logging(const Logging &);
  Logging& operator=(const Logging &);
public:
  explicit Logging(std::string name_arg)
    : Plugin(name_arg, "Logging")
  {}
  virtual ~Logging() {}

  /**
   * Make these no-op rather than pure-virtual so that it's easy for a plugin
   * to only implement one.
   */
  virtual bool pre(Session *) {return false;}
  virtual bool post(Session *) {return false;}
  virtual bool postEnd(Session*) {return false;}
  virtual bool resetGlobalScoreboard() {return false;}

  static bool addPlugin(Logging *handler);
  static void removePlugin(Logging *handler);
  static bool preDo(Session *session);
  static bool postDo(Session *session);
  static bool postEndDo(Session *session);
  static bool resetStats(Session *session);
};

} /* namespace plugin */
} /* namespace drizzled */

