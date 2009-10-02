/*
 -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:

 *  Definitions required for Configuration Variables plugin

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

#ifndef DRIZZLED_PLUGIN_SCHEDULER_H
#define DRIZZLED_PLUGIN_SCHEDULER_H

#include <string>
#include <vector>

namespace drizzled
{
namespace plugin
{

/**
 * This class should be used by scheduler plugins to implement custom session
 * schedulers.
 */
class Scheduler
{
public:
  Scheduler(const char *name_arg) : 
    name(name_arg) {}
  virtual ~Scheduler() {}

  /**
   * Add a session to the scheduler. When the scheduler is ready to run the
   * session, it should call session->run().
   */
  virtual bool addSession(Session *session)= 0;

  /**
   * Notify the scheduler that it should be killed gracefully.
   */
  virtual void killSession(Session *) {}

  /**
   * This is called when a scheduler should kill the session immedaitely.
   */
  virtual void killSessionNow(Session *) {}

  static bool addPlugin(plugin::Scheduler *sced);
  static void removePlugin(plugin::Scheduler *sced);
  static bool setPlugin(const std::string& name);
  static Scheduler *getScheduler();

  /* TODO: make this private */
  const string name;
};

} /* end namespace drizzled::plugin */
} /* end namespace drizzled */

#endif /* DRIZZLED_PLUGIN_SCHEDULER_H */
