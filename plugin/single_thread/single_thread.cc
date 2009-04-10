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
#include <drizzled/serialize/serialize.h>
#include <drizzled/connect.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <drizzled/connect.h>
#include <string>
using namespace std;

class Single_thread_scheduler : public Scheduler
{
public:
  Single_thread_scheduler()
    : Scheduler(1) {}

  virtual bool init_new_connection_thread(void) {return 0;}
  
  /*
    Simple scheduler that use the main thread to handle the request
  
    NOTES
    This is only used for debugging, when starting mysqld with
    --thread-handling=no-threads or --one-thread
  
    When we enter this function, LOCK_thread_count is held!
  */
  
  virtual bool add_connection(Session *session)
  {
    handle_one_connection((void*) session);
  
    return false;
  }
  
  
  /*
    End connection, in case when we are using 'no-threads'
  */
  
  virtual bool end_thread(Session *session, bool)
  {
    unlink_session(session);   /* locks LOCK_thread_count and deletes session */
  
    return true;                                     // Abort handle_one_connection
  }
  
  
  virtual uint32_t count(void)
  {
    return 0;
  }
  
};


class SingleThreadFactory : public SchedulerFactory
{
public:
  SingleThreadFactory() : SchedulerFactory("single_thread") {}
  ~SingleThreadFactory() { if (scheduler != NULL) delete scheduler; }
  Scheduler *operator() ()
  {
    if (scheduler == NULL)
      scheduler= new Single_thread_scheduler();
    return scheduler;
  }
};

static int init(void *p)
{
  SchedulerFactory **factory= static_cast<SchedulerFactory **>(p);
  *factory= new SingleThreadFactory();

  return 0;
}

static int deinit(void *p)
{
  SingleThreadFactory *factory= static_cast<SingleThreadFactory *>(p);
  delete factory;
  return 0;
}

static struct st_mysql_sys_var* system_variables[]= {
  NULL
};

drizzle_declare_plugin(single_thread)
{
  DRIZZLE_SCHEDULING_PLUGIN,
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
