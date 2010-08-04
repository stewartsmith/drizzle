/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifndef PLUGIN_POOL_OF_THREADS_POOL_OF_THREADS_H
#define PLUGIN_POOL_OF_THREADS_POOL_OF_THREADS_H

#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/plugin/scheduler.h>
#include <drizzled/sql_parse.h>
#include <drizzled/session.h>
#include <drizzled/plugin/client.h>
#include "session_scheduler.h"
#include <string>
#include <queue>
#include <boost/unordered_set.hpp>
#include <event.h>


/**
 * @brief
 *  Derived class for pool of threads scheduler.
 */
class PoolOfThreadsScheduler: public drizzled::plugin::Scheduler
{
private:
  pthread_attr_t attr;

  pthread_mutex_t LOCK_session_add;    /* protects sessions_need_adding */
  pthread_mutex_t LOCK_session_kill;    /* protects sessions_to_be_killed */
  /**
   * LOCK_event_loop protects the non-thread safe libevent calls (event_add
   * and event_del) and sessions_need_processing and sessions_waiting_for_io.
   */
  pthread_mutex_t LOCK_event_loop;

  std::queue<drizzled::Session *> sessions_need_adding; /* queue of sessions to add to libevent queue */
  std::queue<drizzled::Session *> sessions_to_be_killed; /* queue of sessions to be killed */
  std::queue<drizzled::Session *> sessions_need_processing; /* queue of sessions that needs some processing */
  /**
   * Collection of sessions with added events
   *
   * This should really be a collection of unordered Sessions. No one is more
   * promising to encounter an io event earlier than another; so no order
   * indeed!
   */
  boost::unordered_set<drizzled::Session *> sessions_waiting_for_io;

public:
  PoolOfThreadsScheduler(const char *name_arg);
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
  bool addSession(drizzled::Session *session);
  
  
  /**
   * @brief
   *  Signal a waiting connection it's time to die.
   *
   * @details 
   *  This function will signal libevent the Session should be killed.
   *
   * @param[in]  session The connection to kill
   */
  void killSession(drizzled::Session *session);

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

  void doIO(session_scheduler *sched);
  void killSession(int Fd);
  void addSession(int Fd);
  void *mainLoop();
  void sessionAddToQueue(session_scheduler *sched);


};

#endif /* PLUGIN_POOL_OF_THREADS_POOL_OF_THREADS_H */
