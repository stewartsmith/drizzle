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

#include "config.h"
#include <plugin/multi_thread/multi_thread.h>
#include "drizzled/pthread_globals.h"

using namespace std;
using namespace drizzled;

/* Configuration variables. */
static uint32_t max_threads;

/* Global's (TBR) */
static MultiThreadScheduler *scheduler= NULL;

/**
 * Function to be run as a thread for each session.
 */
namespace
{
  extern "C" pthread_handler_t session_thread(void *arg);
}

namespace
{
  extern "C" pthread_handler_t session_thread(void *arg)
  {
    Session *session= static_cast<Session*>(arg);
    MultiThreadScheduler *sched= static_cast<MultiThreadScheduler*>(session->scheduler);
    sched->runSession(session);
    return NULL;
  }
}


bool MultiThreadScheduler::addSession(Session *session)
{
  if (thread_count >= max_threads)
    return true;

  thread_count.increment();

  if (pthread_create(&session->real_id, &attr, session_thread,
                     static_cast<void*>(session)))
  {
    thread_count.decrement();
    return true;
  }

  return false;
}


void MultiThreadScheduler::killSessionNow(Session *session)
{
  /* Locks LOCK_thread_count and deletes session */
  Session::unlink(session);
  thread_count.decrement();
  internal::my_thread_end();
  pthread_exit(0);
  /* We should never reach this point. */
}

MultiThreadScheduler::~MultiThreadScheduler()
{
  (void) pthread_mutex_lock(&LOCK_thread_count);
  while (thread_count)
  {
    pthread_cond_wait(&COND_thread_count, &LOCK_thread_count);
  }

  (void) pthread_mutex_unlock(&LOCK_thread_count);
  (void) pthread_attr_destroy(&attr);
}

  
static int init(drizzled::plugin::Context &context)
{
  scheduler= new MultiThreadScheduler("multi_thread");
  context.add(scheduler);

  return 0;
}

static DRIZZLE_SYSVAR_UINT(max_threads, max_threads,
                           PLUGIN_VAR_RQCMDARG,
                           N_("Maximum number of user threads available."),
                           NULL, NULL, 2048, 1, 4096, 0);

static drizzle_sys_var* sys_variables[]= {
  DRIZZLE_SYSVAR(max_threads),
  NULL
};

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "multi_thread",
  "0.1",
  "Brian Aker",
  "One Thread Per Session Scheduler",
  PLUGIN_LICENSE_GPL,
  init, /* Plugin Init */
  sys_variables,   /* system variables */
  NULL    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
