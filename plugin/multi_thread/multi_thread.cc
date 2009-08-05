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
#include <drizzled/atomics.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin/scheduler.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <string>

using namespace std;
using namespace drizzled;

/* Configuration variables. */
static uint32_t max_threads;

/**
 * Function to be run as a thread for each session.
 */
namespace
{
  extern "C" pthread_handler_t session_thread(void *arg);
}

class MultiThreadScheduler: public plugin::Scheduler
{
private:
  drizzled::atomic<uint32_t> thread_count;
  pthread_attr_t attr;

public:
  MultiThreadScheduler(): Scheduler()
  {
    struct sched_param tmp_sched_param;

    /* Setup attribute parameter for session threads. */
    (void) pthread_attr_init(&attr);
    (void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    memset(&tmp_sched_param, 0, sizeof(tmp_sched_param));
    tmp_sched_param.sched_priority= WAIT_PRIOR;
    (void) pthread_attr_setschedparam(&attr, &tmp_sched_param);

    thread_count= 0;
  }

  ~MultiThreadScheduler()
  {
    (void) pthread_mutex_lock(&LOCK_thread_count);
    while (thread_count)
    {
      pthread_cond_wait(&COND_thread_count, &LOCK_thread_count);
    }

    (void) pthread_mutex_unlock(&LOCK_thread_count);
    (void) pthread_attr_destroy(&attr);
  }

  virtual bool addSession(Session *session)
  {
    if (thread_count >= max_threads)
      return true;

    thread_count++;
  
    if (pthread_create(&session->real_id, &attr, session_thread,
                       static_cast<void*>(session)))
    {
      thread_count--;
      return true;
    }
  
    return false;
  }
  
  void runSession(Session *session)
  {
    if (my_thread_init())
    {
      session->disconnect(ER_OUT_OF_RESOURCES, true);
      statistic_increment(aborted_connects, &LOCK_status);
      killSessionNow(session);
    }

    session->thread_stack= (char*) &session;
    session->run();
    killSessionNow(session);
  }

  void killSessionNow(Session *session)
  {
    /* Locks LOCK_thread_count and deletes session */
    unlink_session(session);
    thread_count--;
    my_thread_end();
    pthread_exit(0);
    /* We should never reach this point. */
  }
};

namespace
{
  extern "C" pthread_handler_t session_thread(void *arg)
  {
    Session *session= static_cast<Session*>(arg);
    MultiThreadScheduler *scheduler= static_cast<MultiThreadScheduler*>(session->scheduler);
    scheduler->runSession(session);
    return NULL;
  }
}

class MultiThreadFactory : public plugin::SchedulerFactory
{
public:
  MultiThreadFactory() : SchedulerFactory("multi_thread")
  {
    addAlias("multi-thread");
  }

  ~MultiThreadFactory()
  {
    if (scheduler != NULL)
      delete scheduler;
  }

  plugin::Scheduler *operator() ()
  {
    if (scheduler == NULL)
      scheduler= new MultiThreadScheduler();
    return scheduler;
  }
};

static MultiThreadFactory *factory= NULL;

static int init(drizzled::plugin::Registry &registry)
{
  factory= new MultiThreadFactory();
  registry.add(factory);
  return 0;
}

static int deinit(drizzled::plugin::Registry &registry)
{
  if (factory)
  {
    registry.remove(factory);
    delete factory;
  }
  return 0;
}

static DRIZZLE_SYSVAR_UINT(max_threads, max_threads,
                           PLUGIN_VAR_RQCMDARG,
                           N_("Maximum number of user threads available."),
                           NULL, NULL, 2048, 1, 4096, 0);

static struct st_mysql_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(max_threads),
  NULL
};

drizzle_declare_plugin(multi_thread)
{
  "multi_thread",
  "0.1",
  "Brian Aker",
  "One Thread Per Session Scheduler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  system_variables,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
