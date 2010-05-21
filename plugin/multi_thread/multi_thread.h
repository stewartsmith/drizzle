/* Copyright (C) 2009 Sun Microsystems

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef PLUGIN_MULTI_THREAD_MULTI_THREAD_H
#define PLUGIN_MULTI_THREAD_MULTI_THREAD_H

#include <drizzled/atomics.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin/scheduler.h>
#include "drizzled/internal/my_sys.h"
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <string>

class MultiThreadScheduler: public drizzled::plugin::Scheduler
{
private:
  drizzled::atomic<uint32_t> thread_count;
  pthread_attr_t attr;

public:
  MultiThreadScheduler(const char *name_arg): 
    Scheduler(name_arg)
  {
    struct sched_param tmp_sched_param;

    memset(&tmp_sched_param, 0, sizeof(struct sched_param));

    /* Setup attribute parameter for session threads. */
    (void) pthread_attr_init(&attr);
    (void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    tmp_sched_param.sched_priority= WAIT_PRIOR;
    (void) pthread_attr_setschedparam(&attr, &tmp_sched_param);

    thread_count= 0;
  }

  ~MultiThreadScheduler();
  bool addSession(drizzled::Session *session);
  void killSessionNow(drizzled::Session *session);
  
  void runSession(drizzled::Session *session)
  {
    if (drizzled::internal::my_thread_init())
    {
      session->disconnect(drizzled::ER_OUT_OF_RESOURCES, true);
      status_var_increment(drizzled::current_global_counters.aborted_connects);
      killSessionNow(session);
    }

    session->thread_stack= (char*) &session;
    session->run();
    killSessionNow(session);
  }
};

#endif /* PLUGIN_MULTI_THREAD_MULTI_THREAD_H */
