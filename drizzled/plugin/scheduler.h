/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Definitions required for Configuration Variables plugin
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
#include <drizzled/session.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

/**
 * This class should be used by scheduler plugins to implement custom session
 * schedulers.
 */
class DRIZZLED_API Scheduler : public Plugin
{
  /* Disable default constructors */
  Scheduler();
  Scheduler(const Scheduler &);
  Scheduler& operator=(const Scheduler &);
public:
  explicit Scheduler(std::string name_arg)
    : Plugin(name_arg, "Scheduler")
  {}

  /**
   * Add a session to the scheduler. When the scheduler is ready to run the
   * session, it should call session->run().
   */
  virtual bool addSession(const Session::shared_ptr&)= 0;

  /**
   * Notify the scheduler that it should be killed gracefully.
   */
  virtual void killSession(Session*) {}

  /**
   * This is called when a scheduler should kill the session immedaitely.
   */
  virtual void killSessionNow(const Session::shared_ptr&) {}

  static bool addPlugin(plugin::Scheduler*);
  static void removePlugin(plugin::Scheduler*);
  static bool setPlugin(const std::string& name);
  static Scheduler* getScheduler();
};

} /* namespace plugin */
} /* namespace drizzled */

