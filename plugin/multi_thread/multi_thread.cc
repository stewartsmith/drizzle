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
#include <drizzled/connect.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <drizzled/connect.h>
#include <string>

using namespace std;

static uint32_t max_threads;

class Multi_thread_scheduler: public Scheduler
{
  drizzled::atomic<uint32_t> thread_count;
  pthread_attr_t multi_thread_attrib;

public:
  Multi_thread_scheduler(uint32_t threads): Scheduler(threads)
  {
    thread_count= 0;
    /* Parameter for threads created for connections */
    (void) pthread_attr_init(&multi_thread_attrib);
    (void) pthread_attr_setdetachstate(&multi_thread_attrib,
                                       PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&multi_thread_attrib, PTHREAD_SCOPE_SYSTEM);
    {
      struct sched_param tmp_sched_param;
  
      memset(&tmp_sched_param, 0, sizeof(tmp_sched_param));
      tmp_sched_param.sched_priority= WAIT_PRIOR;
      (void)pthread_attr_setschedparam(&multi_thread_attrib, &tmp_sched_param);
    }
  }

  ~Multi_thread_scheduler()
  {
    (void) pthread_mutex_lock(&LOCK_thread_count);
    while (thread_count)
    {
      pthread_cond_wait(&COND_thread_count, &LOCK_thread_count);
    }
    (void) pthread_mutex_unlock(&LOCK_thread_count);
    
    pthread_attr_destroy(&multi_thread_attrib);
  }

  virtual bool add_connection(Session *session)
  {
    int error;
  
    thread_count++;
  
    if ((error= pthread_create(&session->real_id, &multi_thread_attrib, handle_one_connection, static_cast<void*>(session))))
      return true;
  
    return false;
  }
  
  
  /*
    End connection, in case when we are using 'no-threads'
  */
  
  virtual bool end_thread(Session *session, bool)
  {
    unlink_session(session);   /* locks LOCK_thread_count and deletes session */
    thread_count--;
  
    my_thread_end();
    pthread_exit(0);
  
    return true; // We should never reach this point
  }
  
  virtual uint32_t count(void)
  {
    return thread_count;
  }
};

class MultiThreadFactory : public SchedulerFactory
{
public:
  MultiThreadFactory() : SchedulerFactory("multi_thread") {}
  ~MultiThreadFactory() { if (scheduler != NULL) delete scheduler; }
  Scheduler *operator() ()
  {
    if (scheduler == NULL)
      scheduler= new Multi_thread_scheduler(max_threads);
    return scheduler;
  }
};
static MultiThreadFactory *factory= NULL;

static int init(PluginRegistry &registry)
{
  factory= new MultiThreadFactory();
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

static DRIZZLE_SYSVAR_UINT(max_threads, max_threads,
                           PLUGIN_VAR_RQCMDARG,
                           N_("Maximum number of user threads available."),
                           NULL, NULL, 2048, 1, 4048, 0);

static struct st_mysql_sys_var* system_variables[]= {
  DRIZZLE_SYSVAR(max_threads),
  NULL
};

drizzle_declare_plugin(multi_thread)
{
  DRIZZLE_SCHEDULING_PLUGIN,
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
