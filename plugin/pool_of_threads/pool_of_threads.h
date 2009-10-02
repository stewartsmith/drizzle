/*
 * - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 * Copyright (C) 2009 Sun Microsystems
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <drizzled/server_includes.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin/scheduler.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <drizzled/plugin/client.h>
#include "session_scheduler.h"
#include <string>
#include <queue>
#include <set>
#include <event.h>


/**
 * @brief 
 *  Derived class for pool of threads scheduler.
 */
class PoolOfThreadsScheduler: public drizzled::plugin::Scheduler
{
private:
  pthread_attr_t attr;

public:
  PoolOfThreadsScheduler(const char *name_arg): 
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
  }

  ~PoolOfThreadsScheduler();

  /**
   * @brief 
   *  Notify the thread pool about a new connection
   *
   * @param[in] the newly connected session 
   *
   * @return 
   *  True if there is an error.
   */
  bool addSession(Session *session);
  
  
  /**
   * @brief
   *  Signal a waiting connection it's time to die.
   *
   * @details 
   *  This function will signal libevent the Session should be killed.
   *
   * @param[in]  session The connection to kill
   */
  void killSession(Session *session);

  /**
   * @brief
   *  Create all threads for the thread pool
   *
   * @details
   *  After threads are created we wait until all threads has signaled that
   *  they have started before we return
   *
   * @retval 0 Ok
   * @retval 1 We got an error creating the thread pool. In this case we will abort all created threads.
   */
  bool libevent_init(void);
}; 
