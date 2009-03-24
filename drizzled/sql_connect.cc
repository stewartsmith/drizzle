/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
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


/*
  Functions to autenticate and handle reqests for a connection
*/
#include <drizzled/server_includes.h>

#include <drizzled/error.h>
#include <drizzled/sql_parse.h>
#include <drizzled/scheduling.h>
#include <drizzled/session.h>
#include <drizzled/connect.h>


/*
  Initialize connection threads
*/
bool init_new_connection_handler_thread()
{
  if (my_thread_init())
    return 1;
  return 0;
}

/*
  Thread handler for a connection

  SYNOPSIS
    handle_one_connection()
    arg		Connection object (Session)

  IMPLEMENTATION
    This function (normally) does the following:
    - Initialize thread
    - Initialize Session to be used with this thread
    - Authenticate user
    - Execute all queries sent on the connection
    - Take connection down
    - End thread  / Handle next connection using thread from thread cache
*/
pthread_handler_t handle_one_connection(void *arg)
{
  Session *session= static_cast<Session*>(arg);
  uint32_t launch_time= (uint32_t) ((session->thr_create_utime= my_micro_time()) -
                              session->connect_utime);

  Scheduler &thread_scheduler= get_thread_scheduler();
  if (thread_scheduler.init_new_connection_thread())
  {
    session->disconnect(ER_OUT_OF_RESOURCES, true);
    statistic_increment(aborted_connects,&LOCK_status);
    thread_scheduler.end_thread(session,0);
    return 0;
  }
  if (launch_time >= slow_launch_time*1000000L)
    statistic_increment(slow_launch_threads,&LOCK_status);

  /*
    handle_one_connection() is normally the only way a thread would
    start and would always be on the very high end of the stack ,
    therefore, the thread stack always starts at the address of the
    first local variable of handle_one_connection, which is session. We
    need to know the start of the stack so that we could check for
    stack overruns.
  */
  session->thread_stack= (char*) &session;
  if (! session->initGlobals())
    return 0;

  for (;;)
  {
    NET *net= &session->net;

    if (! session->authenticate())
      goto end_thread;

    session->prepareForQueries();

    while (!net->error && net->vio != 0 &&
           !(session->killed == Session::KILL_CONNECTION))
    {
      if (! session->executeStatement())
	      break;
    }

end_thread:
    session->disconnect(0, true);
    if (thread_scheduler.end_thread(session, 1))
      return 0;                                 // Probably no-threads

    /*
      If end_thread() returns, we are either running with
      thread-handler=no-threads or this thread has been schedule to
      handle the next connection.
    */
    session= current_session;
    session->thread_stack= (char*) &session;
  }
}
