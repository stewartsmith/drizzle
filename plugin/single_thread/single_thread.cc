/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <drizzled/server_includes.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin/scheduler.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <string>

using namespace std;
using namespace drizzled;

/**
 * Simple scheduler that uses the main thread to handle the request. This
 * should only be used for debugging.
 */
class SingleThreadScheduler : public plugin::Scheduler
{
public:
  SingleThreadScheduler() : Scheduler() {}

  /* When we enter this function, LOCK_thread_count is held! */
  virtual bool addSession(Session *session)
  {
    if (my_thread_init())
    {
      session->disconnect(ER_OUT_OF_RESOURCES, true);
      statistic_increment(aborted_connects, &LOCK_status);
      return true;
    }

    /*
      This is not the real thread start beginning, but there is not an easy
      way to find it.
    */
    session->thread_stack= (char *)&session;

    session->run();
    killSessionNow(session);
    return false;
  }

  virtual void killSessionNow(Session *session)
  {
    unlink_session(session);
  }
};


class SingleThreadFactory : public plugin::SchedulerFactory
{
public:
  SingleThreadFactory() : SchedulerFactory("single_thread") {}
  ~SingleThreadFactory() { if (scheduler != NULL) delete scheduler; }
  plugin::Scheduler *operator() ()
  {
    if (scheduler == NULL)
      scheduler= new SingleThreadScheduler();
    return scheduler;
  }
};

SingleThreadFactory *factory= NULL;

static int init(PluginRegistry &registry)
{
  factory= new SingleThreadFactory();
  registry.add(factory);
  return 0;
}

static int deinit(PluginRegistry &registry)
{
  if (factory)
  {
    registry.remove(factory);
    delete factory;
  }
  return 0;
}

static struct st_mysql_sys_var* system_variables[]= {
  NULL
};

drizzle_declare_plugin(single_thread)
{
  "single_thread",
  "0.1",
  "Brian Aker",
  "Single Thread Scheduler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  system_variables,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
