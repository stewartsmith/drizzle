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
#include <netdb.h>

#include <drizzled/authentication.h>
#include <drizzled/db.h>
#include <drizzled/error.h>
#include <drizzled/sql_parse.h>
#include <drizzled/plugin_scheduling.h>
#include <drizzled/session.h>
#include <drizzled/data_home.h>
#include <drizzled/connect.h>

extern scheduling_st thread_scheduler;


/*
  Check for maximum allowable user connections, if the mysqld server is
  started with corresponding variable that is greater then 0.
*/
extern "C" unsigned char *get_key_conn(user_conn *buff, size_t *length,
                               bool )
{
  *length= buff->len;
  return (unsigned char*) buff->user;
}

extern "C" void free_user(struct user_conn *uc)
{
  free((char*) uc);
}

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
  Setup thread to be used with the current thread

  SYNOPSIS
    bool setup_connection_thread_globals()
    session    Thread/connection handler

  RETURN
    0   ok
    1   Error (out of memory)
        In this case we will close the connection and increment status
*/
bool setup_connection_thread_globals(Session *session)
{
  if (session->store_globals())
  {
    session->close_connection(ER_OUT_OF_RESOURCES, true);
    statistic_increment(aborted_connects,&LOCK_status);
    thread_scheduler.end_thread(session, 0);
    return 1;                                   // Error
  }
  return 0;
}

/*
  Close an established connection

  NOTES
    This mainly updates status variables
*/
void end_connection(Session *session)
{
  NET *net= &session->net;
  plugin_sessionvar_cleanup(session);

  if (session->killed || (net->error && net->vio != 0))
  {
    statistic_increment(aborted_threads,&LOCK_status);
  }

  if (net->error && net->vio != 0)
  {
    if (!session->killed && session->variables.log_warnings > 1)
    {
      Security_context *sctx= &session->security_ctx;

      errmsg_printf(ERRMSG_LVL_WARN, ER(ER_NEW_ABORTING_CONNECTION),
                        session->thread_id,(session->db ? session->db : "unconnected"),
                        sctx->user.empty() == false ? sctx->user.c_str() : "unauthenticated",
                        sctx->ip.c_str(),
                        (session->main_da.is_error() ? session->main_da.message() :
                         ER(ER_UNKNOWN_ERROR)));
    }
  }
}

/*
  Initialize Session to handle queries
*/
void prepare_new_connection_state(Session* session)
{
  Security_context *sctx= &session->security_ctx;

  if (session->variables.max_join_size == HA_POS_ERROR)
    session->options |= OPTION_BIG_SELECTS;
  if (session->client_capabilities & CLIENT_COMPRESS)
    session->net.compress=1;				// Use compression

  session->version= refresh_version;
  session->set_proc_info(0);
  session->command= COM_SLEEP;
  session->set_time();
  session->init_for_queries();

  /* In the past this would only run of the user did not have SUPER_ACL */
  if (sys_init_connect.value_length)
  {
    execute_init_command(session, &sys_init_connect, &LOCK_sys_init_connect);
    if (session->is_error())
    {
      session->killed= Session::KILL_CONNECTION;
      errmsg_printf(ERRMSG_LVL_WARN, ER(ER_NEW_ABORTING_CONNECTION),
                        session->thread_id,(session->db ? session->db : "unconnected"),
                        sctx->user.empty() == false ? sctx->user.c_str() : "unauthenticated",
                        sctx->ip.c_str(), "init_connect command failed");
      errmsg_printf(ERRMSG_LVL_WARN, "%s", session->main_da.message());
    }
    session->set_proc_info(0);
    session->set_time();
    session->init_for_queries();
  }
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
  Session *session= (Session*) arg;
  uint32_t launch_time= (uint32_t) ((session->thr_create_utime= my_micro_time()) -
                              session->connect_utime);

  if (thread_scheduler.init_new_connection_thread())
  {
    session->close_connection(ER_OUT_OF_RESOURCES, true);
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
  if (setup_connection_thread_globals(session))
    return 0;

  for (;;)
  {
    NET *net= &session->net;

    if (! session->authenticate())
      goto end_thread;

    prepare_new_connection_state(session);

    while (!net->error && net->vio != 0 &&
           !(session->killed == Session::KILL_CONNECTION))
    {
      if (do_command(session))
	break;
    }
    end_connection(session);

end_thread:
    session->close_connection(0, true);
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
