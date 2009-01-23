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
#include <drizzled/scheduler.h>
#include <drizzled/session.h>
#include <drizzled/data_home.h>

extern scheduler_functions thread_scheduler;

#define MIN_HANDSHAKE_SIZE      6

/*
  Get structure for logging connection data for the current user
*/

char *ip_to_hostname(struct sockaddr_storage *in, int addrLen)
{
  char *name;

  int gxi_error;
  char hostname_buff[NI_MAXHOST];

  /* Historical comparison for 127.0.0.1 */
  gxi_error= getnameinfo((struct sockaddr *)in, addrLen,
                         hostname_buff, NI_MAXHOST,
                         NULL, 0, NI_NUMERICHOST);
  if (gxi_error)
  {
    return NULL;
  }

  if (!(name= strdup(hostname_buff)))
  {
    return NULL;
  }

  return NULL;
}

/**
  Check if user exist and password supplied is correct.

  @param  session         thread handle, session->security_ctx->{host,user,ip} are used
  @param  command     originator of the check: now check_user is called
                      during connect and change user procedures; used for
                      logging.
  @param  passwd      scrambled password received from client
  @param  passwd_len  length of scrambled password
  @param  db          database name to connect to, may be NULL
  @param  check_count true if establishing a new connection. In this case
                      check that we have not exceeded the global
                      max_connections limist

  @note Host, user and passwd may point to communication buffer.
  Current implementation does not depend on that, but future changes
  should be done with this in mind; 'session' is INOUT, all other params
  are 'IN'.

  @retval  0  OK
  @retval  1  error, e.g. access denied or handshake error, not sent to
              the client. A message is pushed into the error stack.
*/

int
check_user(Session *session, const char *passwd,
           uint32_t passwd_len, const char *db,
           bool check_count)
{
  LEX_STRING db_str= { (char *) db, db ? strlen(db) : 0 };
  bool is_authenticated;

  /*
    Clear session->db as it points to something, that will be freed when
    connection is closed. We don't want to accidentally free a wrong
    pointer if connect failed. Also in case of 'CHANGE USER' failure,
    current database will be switched to 'no database selected'.
  */
  session->reset_db(NULL, 0);

  if (passwd_len != 0 && passwd_len != SCRAMBLE_LENGTH)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), session->security_ctx.ip.c_str());
    return(1);
  }

  is_authenticated= authenticate_user(session, passwd);

  if (is_authenticated != true)
  {
    my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
             session->security_ctx.user.c_str(),
             session->security_ctx.ip.c_str(),
             passwd_len ? ER(ER_YES) : ER(ER_NO));

    return 1;
  }


  session->security_ctx.skip_grants();

  if (check_count)
  {
    pthread_mutex_lock(&LOCK_connection_count);
    bool count_ok= connection_count <= max_connections;
    pthread_mutex_unlock(&LOCK_connection_count);

    if (!count_ok)
    {                                         // too many connections
      my_error(ER_CON_COUNT_ERROR, MYF(0));
      return(1);
    }
  }

  /* Change database if necessary */
  if (db && db[0])
  {
    if (mysql_change_db(session, &db_str, false))
    {
      /* mysql_change_db() has pushed the error message. */
      return(1);
    }
  }
  my_ok(session);
  session->password= test(passwd_len);          // remember for error messages
  /* Ready to handle queries */
  return(0);
}


/*
  Check for maximum allowable user connections, if the mysqld server is
  started with corresponding variable that is greater then 0.
*/

extern "C" unsigned char *get_key_conn(user_conn *buff, size_t *length,
                               bool not_used __attribute__((unused)))
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
  pthread_detach_this_thread();
  /* Win32 calls this in pthread_create */
  if (my_thread_init())
    return 1;
  return 0;
}

/*
  Perform handshake, authorize client and update session ACL variables.

  SYNOPSIS
    check_connection()
    session  thread handle

  RETURN
     0  success, OK is sent to user, session is updated.
    -1  error, which is sent to user
   > 0  error code (not sent to user)
*/

static int check_connection(Session *session)
{
  NET *net= &session->net;
  uint32_t pkt_len= 0;
  char *end;

  // TCP/IP connection
  {
    char ip[NI_MAXHOST];

    if (net_peer_addr(net, ip, &session->peer_port, NI_MAXHOST))
    {
      my_error(ER_BAD_HOST_ERROR, MYF(0), session->security_ctx.ip.c_str());
      return 1;
    }

    session->security_ctx.ip.assign(ip);
  }
  net_keepalive(net, true);

  uint32_t server_capabilites;
  {
    /* buff[] needs to big enough to hold the server_version variable */
    char buff[SERVER_VERSION_LENGTH + SCRAMBLE_LENGTH + 64];
    server_capabilites= CLIENT_BASIC_FLAGS;

    if (opt_using_transactions)
      server_capabilites|= CLIENT_TRANSACTIONS;
#ifdef HAVE_COMPRESS
    server_capabilites|= CLIENT_COMPRESS;
#endif /* HAVE_COMPRESS */

    end= buff + strlen(server_version);
    if ((end - buff) >= SERVER_VERSION_LENGTH)
      end= buff + (SERVER_VERSION_LENGTH - 1);
    memcpy(buff, server_version, end - buff);
    *end= 0;
    end++;

    int4store((unsigned char*) end, session->thread_id);
    end+= 4;
    /*
      So as check_connection is the only entry point to authorization
      procedure, scramble is set here. This gives us new scramble for
      each handshake.
    */
    create_random_string(session->scramble, SCRAMBLE_LENGTH, &session->rand);
    /*
      Old clients does not understand long scrambles, but can ignore packet
      tail: that's why first part of the scramble is placed here, and second
      part at the end of packet.
    */
    end= strncpy(end, session->scramble, SCRAMBLE_LENGTH_323);
    end+= SCRAMBLE_LENGTH_323 + 1;

    int2store(end, server_capabilites);
    /* write server characteristics: up to 16 bytes allowed */
    end[2]=(char) default_charset_info->number;
    int2store(end+3, session->server_status);
    memset(end+5, 0, 13);
    end+= 18;
    /* write scramble tail */
    size_t scramble_len= SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323;
    end= strncpy(end, session->scramble + SCRAMBLE_LENGTH_323, scramble_len);
    end+= scramble_len + 1;

    /* At this point we write connection message and read reply */
    if (net_write_command(net, (unsigned char) protocol_version, (unsigned char*) "", 0,
                          (unsigned char*) buff, (size_t) (end-buff)) ||
	(pkt_len= my_net_read(net)) == packet_error ||
	pkt_len < MIN_HANDSHAKE_SIZE)
    {
      my_error(ER_HANDSHAKE_ERROR, MYF(0),
               session->security_ctx.ip.c_str());
      return 1;
    }
  }
  if (session->packet.alloc(session->variables.net_buffer_length))
    return 1; /* The error is set by alloc(). */

  session->client_capabilities= uint2korr(net->read_pos);


  session->client_capabilities|= ((uint32_t) uint2korr(net->read_pos+2)) << 16;
  session->max_client_packet_length= uint4korr(net->read_pos+4);
  session->update_charset();
  end= (char*) net->read_pos+32;

  /*
    Disable those bits which are not supported by the server.
    This is a precautionary measure, if the client lies. See Bug#27944.
  */
  session->client_capabilities&= server_capabilites;

  if (end >= (char*) net->read_pos+ pkt_len +2)
  {

    my_error(ER_HANDSHAKE_ERROR, MYF(0), session->security_ctx.ip.c_str());
    return 1;
  }

  if (session->client_capabilities & CLIENT_INTERACTIVE)
    session->variables.net_wait_timeout= session->variables.net_interactive_timeout;
  if ((session->client_capabilities & CLIENT_TRANSACTIONS) &&
      opt_using_transactions)
    net->return_status= &session->server_status;

  char *user= end;
  char *passwd= strchr(user, '\0')+1;
  uint32_t user_len= passwd - user - 1;
  char *db= passwd;
  char db_buff[NAME_LEN + 1];           // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH + 1];	// buffer to store user in utf8
  uint32_t dummy_errors;

  /*
    Old clients send null-terminated string as password; new clients send
    the size (1 byte) + string (not null-terminated). Hence in case of empty
    password both send '\0'.

    This strlen() can't be easily deleted without changing protocol.

    Cast *passwd to an unsigned char, so that it doesn't extend the sign for
    *passwd > 127 and become 2**32-127+ after casting to uint.
  */
  uint32_t passwd_len= session->client_capabilities & CLIENT_SECURE_CONNECTION ?
    (unsigned char)(*passwd++) : strlen(passwd);
  db= session->client_capabilities & CLIENT_CONNECT_WITH_DB ?
    db + passwd_len + 1 : 0;
  /* strlen() can't be easily deleted without changing protocol */
  uint32_t db_len= db ? strlen(db) : 0;

  if (passwd + passwd_len + db_len > (char *)net->read_pos + pkt_len)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), session->security_ctx.ip.c_str());
    return 1;
  }

  /* Since 4.1 all database names are stored in utf8 */
  if (db)
  {
    db_buff[copy_and_convert(db_buff, sizeof(db_buff)-1,
                             system_charset_info,
                             db, db_len,
                             session->charset(), &dummy_errors)]= 0;
    db= db_buff;
  }

  user_buff[user_len= copy_and_convert(user_buff, sizeof(user_buff)-1,
                                       system_charset_info, user, user_len,
                                       session->charset(), &dummy_errors)]= '\0';
  user= user_buff;

  /* If username starts and ends in "'", chop them off */
  if (user_len > 1 && user[0] == '\'' && user[user_len - 1] == '\'')
  {
    user[user_len-1]= 0;
    user++;
    user_len-= 2;
  }

  session->security_ctx.user.assign(user);

  return check_user(session, passwd, passwd_len, db, true);
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
  Autenticate user, with error reporting

  SYNOPSIS
   login_connection()
   session        Thread handler

  NOTES
    Connection is not closed in case of errors

  RETURN
    0    ok
    1    error
*/


bool login_connection(Session *session)
{
  NET *net= &session->net;
  int error;

  /* Use "connect_timeout" value during connection phase */
  my_net_set_read_timeout(net, connect_timeout);
  my_net_set_write_timeout(net, connect_timeout);

  lex_start(session);

  error= check_connection(session);
  net_end_statement(session);

  if (error)
  {						// Wrong permissions
    statistic_increment(aborted_connects,&LOCK_status);
    return(1);
  }
  /* Connect completed, set read/write timeouts back to default */
  my_net_set_read_timeout(net, session->variables.net_read_timeout);
  my_net_set_write_timeout(net, session->variables.net_write_timeout);
  return(0);
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

  /*
    Much of this is duplicated in create_embedded_session() for the
    embedded server library.
    TODO: refactor this to avoid code duplication there
  */
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

    if (login_connection(session))
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
    session->close_connection(NULL, true);
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
