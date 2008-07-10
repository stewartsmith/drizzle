/* Copyright (C) 2007 MySQL AB

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


/*
  Functions to autenticate and handle reqests for a connection
*/

#include "mysql_priv.h"

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

  if (!(name= my_strdup(hostname_buff, MYF(0))))
  {
    return NULL;
  }

  return NULL;
}

/**
  Check if user exist and password supplied is correct.

  @param  thd         thread handle, thd->security_ctx->{host,user,ip} are used
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
  should be done with this in mind; 'thd' is INOUT, all other params
  are 'IN'.

  @retval  0  OK; thd->security_ctx->user/master_access/priv_user/db_access and
              thd->db are updated; OK is sent to the client.
  @retval  1  error, e.g. access denied or handshake error, not sent to
              the client. A message is pushed into the error stack.
*/

int
check_user(THD *thd, enum enum_server_command command,
           const char *passwd __attribute__((__unused__)),
           uint passwd_len, const char *db,
           bool check_count)
{
  LEX_STRING db_str= { (char *) db, db ? strlen(db) : 0 };

  /*
    Clear thd->db as it points to something, that will be freed when
    connection is closed. We don't want to accidentally free a wrong
    pointer if connect failed. Also in case of 'CHANGE USER' failure,
    current database will be switched to 'no database selected'.
  */
  thd->reset_db(NULL, 0);

  my_bool opt_secure_auth_local;
  pthread_mutex_lock(&LOCK_global_system_variables);
  opt_secure_auth_local= opt_secure_auth;
  pthread_mutex_unlock(&LOCK_global_system_variables);

  /*
    If the server is running in secure auth mode, short scrambles are 
    forbidden.
  */
  if (opt_secure_auth_local && passwd_len == SCRAMBLE_LENGTH_323)
  {
    my_error(ER_NOT_SUPPORTED_AUTH_MODE, MYF(0));
    general_log_print(thd, COM_CONNECT, ER(ER_NOT_SUPPORTED_AUTH_MODE));
    return(1);
  }
  if (passwd_len != 0 &&
      passwd_len != SCRAMBLE_LENGTH &&
      passwd_len != SCRAMBLE_LENGTH_323)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), thd->main_security_ctx.host_or_ip);
    return(1);
  }

  USER_RESOURCES ur;
  thd->security_ctx->skip_grants();
  memset(&ur, 0, sizeof(USER_RESOURCES));

  if (check_count)
  {
    pthread_mutex_lock(&LOCK_connection_count);
    bool count_ok= connection_count <= max_connections;
    VOID(pthread_mutex_unlock(&LOCK_connection_count));

    if (!count_ok)
    {                                         // too many connections
      my_error(ER_CON_COUNT_ERROR, MYF(0));
      return(1);
    }
  }

  /*
    Log the command before authentication checks, so that the user can
    check the log for the tried login tried and also to detect
    break-in attempts.
  */
  general_log_print(thd, command,
                    (thd->main_security_ctx.priv_user ==
                     thd->main_security_ctx.user ?
                     (char*) "%s@%s on %s" :
                     (char*) "%s@%s as anonymous on %s"),
                    thd->main_security_ctx.user,
                    thd->main_security_ctx.host_or_ip,
                    db ? db : (char*) "");

  /*
    This is the default access rights for the current database.  It's
    set to 0 here because we don't have an active database yet (and we
    may not have an active database to set.
  */
  thd->main_security_ctx.db_access=0;

  /* Change database if necessary */
  if (db && db[0])
  {
    if (mysql_change_db(thd, &db_str, false))
    {
      /* mysql_change_db() has pushed the error message. */
      return(1);
    }
  }
  my_ok(thd);
  thd->password= test(passwd_len);          // remember for error messages 
  /* Ready to handle queries */
  return(0);
}


/*
  Check for maximum allowable user connections, if the mysqld server is
  started with corresponding variable that is greater then 0.
*/

extern "C" uchar *get_key_conn(user_conn *buff, size_t *length,
			      my_bool not_used __attribute__((unused)))
{
  *length= buff->len;
  return (uchar*) buff->user;
}


extern "C" void free_user(struct user_conn *uc)
{
  my_free((char*) uc,MYF(0));
}

void thd_init_client_charset(THD *thd, uint cs_number)
{
  /*
   Use server character set and collation if
   - opt_character_set_client_handshake is not set
   - client has not specified a character set
   - client character set is the same as the servers
   - client character set doesn't exists in server
  */
  if (!opt_character_set_client_handshake ||
      !(thd->variables.character_set_client= get_charset(cs_number, MYF(0))) ||
      !my_strcasecmp(&my_charset_latin1,
                     global_system_variables.character_set_client->name,
                     thd->variables.character_set_client->name))
  {
    thd->variables.character_set_client=
      global_system_variables.character_set_client;
    thd->variables.collation_connection=
      global_system_variables.collation_connection;
    thd->variables.character_set_results=
      global_system_variables.character_set_results;
  }
  else
  {
    thd->variables.character_set_results=
      thd->variables.collation_connection= 
      thd->variables.character_set_client;
  }
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
  Perform handshake, authorize client and update thd ACL variables.

  SYNOPSIS
    check_connection()
    thd  thread handle

  RETURN
     0  success, OK is sent to user, thd is updated.
    -1  error, which is sent to user
   > 0  error code (not sent to user)
*/

static int check_connection(THD *thd)
{
  NET *net= &thd->net;
  ulong pkt_len= 0;
  char *end;

#ifdef SIGNAL_WITH_VIO_CLOSE
  thd->set_active_vio(net->vio);
#endif

  if (!thd->main_security_ctx.host)         // If TCP/IP connection
  {
    char ip[NI_MAXHOST];

    if (vio_peer_addr(net->vio, ip, &thd->peer_port, NI_MAXHOST))
    {
      my_error(ER_BAD_HOST_ERROR, MYF(0), thd->main_security_ctx.host_or_ip);
      return 1;
    }
    if (!(thd->main_security_ctx.ip= my_strdup(ip,MYF(MY_WME))))
      return 1; /* The error is set by my_strdup(). */
    thd->main_security_ctx.host_or_ip= thd->main_security_ctx.ip;
    thd->main_security_ctx.host= ip_to_hostname(&net->vio->remote, 
                                                net->vio->addrLen);
    thd->main_security_ctx.host_or_ip= thd->main_security_ctx.host;
  }
  else /* Hostname given means that the connection was on a socket */
  {
    thd->main_security_ctx.host_or_ip= thd->main_security_ctx.host;
    thd->main_security_ctx.ip= 0;
    /* Reset sin_addr */
    bzero((char*) &net->vio->remote, sizeof(net->vio->remote));
  }
  vio_keepalive(net->vio, true);
  
  ulong server_capabilites;
  {
    /* buff[] needs to big enough to hold the server_version variable */
    char buff[SERVER_VERSION_LENGTH + SCRAMBLE_LENGTH + 64];
    server_capabilites= CLIENT_BASIC_FLAGS;

    if (opt_using_transactions)
      server_capabilites|= CLIENT_TRANSACTIONS;
#ifdef HAVE_COMPRESS
    server_capabilites|= CLIENT_COMPRESS;
#endif /* HAVE_COMPRESS */

    end= strnmov(buff, server_version, SERVER_VERSION_LENGTH) + 1;
    int4store((uchar*) end, thd->thread_id);
    end+= 4;
    /*
      So as check_connection is the only entry point to authorization
      procedure, scramble is set here. This gives us new scramble for
      each handshake.
    */
    create_random_string(thd->scramble, SCRAMBLE_LENGTH, &thd->rand);
    /*
      Old clients does not understand long scrambles, but can ignore packet
      tail: that's why first part of the scramble is placed here, and second
      part at the end of packet.
    */
    end= strmake(end, thd->scramble, SCRAMBLE_LENGTH_323) + 1;
   
    int2store(end, server_capabilites);
    /* write server characteristics: up to 16 bytes allowed */
    end[2]=(char) default_charset_info->number;
    int2store(end+3, thd->server_status);
    bzero(end+5, 13);
    end+= 18;
    /* write scramble tail */
    end= strmake(end, thd->scramble + SCRAMBLE_LENGTH_323, 
                 SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323) + 1;

    /* At this point we write connection message and read reply */
    if (net_write_command(net, (uchar) protocol_version, (uchar*) "", 0,
                          (uchar*) buff, (size_t) (end-buff)) ||
	(pkt_len= my_net_read(net)) == packet_error ||
	pkt_len < MIN_HANDSHAKE_SIZE)
    {
      my_error(ER_HANDSHAKE_ERROR, MYF(0),
               thd->main_security_ctx.host_or_ip);
      return 1;
    }
  }
#ifdef _CUSTOMCONFIG_
#include "_cust_sql_parse.h"
#endif
  if (thd->packet.alloc(thd->variables.net_buffer_length))
    return 1; /* The error is set by alloc(). */

  thd->client_capabilities= uint2korr(net->read_pos);
  if (thd->client_capabilities & CLIENT_PROTOCOL_41)
  {
    thd->client_capabilities|= ((ulong) uint2korr(net->read_pos+2)) << 16;
    thd->max_client_packet_length= uint4korr(net->read_pos+4);
    thd_init_client_charset(thd, (uint) net->read_pos[8]);
    thd->update_charset();
    end= (char*) net->read_pos+32;
  }
  else
  {
    thd->max_client_packet_length= uint3korr(net->read_pos+2);
    end= (char*) net->read_pos+5;
  }
  /*
    Disable those bits which are not supported by the server.
    This is a precautionary measure, if the client lies. See Bug#27944.
  */
  thd->client_capabilities&= server_capabilites;

  if (thd->client_capabilities & CLIENT_IGNORE_SPACE)
    thd->variables.sql_mode|= MODE_IGNORE_SPACE;

  if (end >= (char*) net->read_pos+ pkt_len +2)
  {

    my_error(ER_HANDSHAKE_ERROR, MYF(0), thd->main_security_ctx.host_or_ip);
    return 1;
  }

  if (thd->client_capabilities & CLIENT_INTERACTIVE)
    thd->variables.net_wait_timeout= thd->variables.net_interactive_timeout;
  if ((thd->client_capabilities & CLIENT_TRANSACTIONS) &&
      opt_using_transactions)
    net->return_status= &thd->server_status;

  char *user= end;
  char *passwd= strend(user)+1;
  uint user_len= passwd - user - 1;
  char *db= passwd;
  char db_buff[NAME_LEN + 1];           // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH + 1];	// buffer to store user in utf8
  uint dummy_errors;

  /*
    Old clients send null-terminated string as password; new clients send
    the size (1 byte) + string (not null-terminated). Hence in case of empty
    password both send '\0'.

    This strlen() can't be easily deleted without changing protocol.

    Cast *passwd to an unsigned char, so that it doesn't extend the sign for
    *passwd > 127 and become 2**32-127+ after casting to uint.
  */
  uint passwd_len= thd->client_capabilities & CLIENT_SECURE_CONNECTION ?
    (uchar)(*passwd++) : strlen(passwd);
  db= thd->client_capabilities & CLIENT_CONNECT_WITH_DB ?
    db + passwd_len + 1 : 0;
  /* strlen() can't be easily deleted without changing protocol */
  uint db_len= db ? strlen(db) : 0;

  if (passwd + passwd_len + db_len > (char *)net->read_pos + pkt_len)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), thd->main_security_ctx.host_or_ip);
    return 1;
  }

  /* Since 4.1 all database names are stored in utf8 */
  if (db)
  {
    db_buff[copy_and_convert(db_buff, sizeof(db_buff)-1,
                             system_charset_info,
                             db, db_len,
                             thd->charset(), &dummy_errors)]= 0;
    db= db_buff;
  }

  user_buff[user_len= copy_and_convert(user_buff, sizeof(user_buff)-1,
                                       system_charset_info, user, user_len,
                                       thd->charset(), &dummy_errors)]= '\0';
  user= user_buff;

  /* If username starts and ends in "'", chop them off */
  if (user_len > 1 && user[0] == '\'' && user[user_len - 1] == '\'')
  {
    user[user_len-1]= 0;
    user++;
    user_len-= 2;
  }

  if (thd->main_security_ctx.user)
    x_free(thd->main_security_ctx.user);
  if (!(thd->main_security_ctx.user= my_strdup(user, MYF(MY_WME))))
    return 1; /* The error is set by my_strdup(). */
  return check_user(thd, COM_CONNECT, passwd, passwd_len, db, true);
}


/*
  Setup thread to be used with the current thread

  SYNOPSIS
    bool setup_connection_thread_globals()
    thd    Thread/connection handler

  RETURN
    0   ok
    1   Error (out of memory)
        In this case we will close the connection and increment status
*/

bool setup_connection_thread_globals(THD *thd)
{
  if (thd->store_globals())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES, 1);
    statistic_increment(aborted_connects,&LOCK_status);
    thread_scheduler.end_thread(thd, 0);
    return 1;                                   // Error
  }
  return 0;
}


/*
  Autenticate user, with error reporting

  SYNOPSIS
   login_connection()
   thd        Thread handler

  NOTES
    Connection is not closed in case of errors

  RETURN
    0    ok
    1    error
*/


bool login_connection(THD *thd)
{
  NET *net= &thd->net;
  int error;

  /* Use "connect_timeout" value during connection phase */
  my_net_set_read_timeout(net, connect_timeout);
  my_net_set_write_timeout(net, connect_timeout);
  
  lex_start(thd);

  error= check_connection(thd);
  net_end_statement(thd);

  if (error)
  {						// Wrong permissions
    statistic_increment(aborted_connects,&LOCK_status);
    return(1);
  }
  /* Connect completed, set read/write timeouts back to default */
  my_net_set_read_timeout(net, thd->variables.net_read_timeout);
  my_net_set_write_timeout(net, thd->variables.net_write_timeout);
  return(0);
}


/*
  Close an established connection

  NOTES
    This mainly updates status variables
*/

void end_connection(THD *thd)
{
  NET *net= &thd->net;
  plugin_thdvar_cleanup(thd);

  if (thd->killed || (net->error && net->vio != 0))
  {
    statistic_increment(aborted_threads,&LOCK_status);
  }

  if (net->error && net->vio != 0)
  {
    if (!thd->killed && thd->variables.log_warnings > 1)
    {
      Security_context *sctx= thd->security_ctx;

      sql_print_warning(ER(ER_NEW_ABORTING_CONNECTION),
                        thd->thread_id,(thd->db ? thd->db : "unconnected"),
                        sctx->user ? sctx->user : "unauthenticated",
                        sctx->host_or_ip,
                        (thd->main_da.is_error() ? thd->main_da.message() :
                         ER(ER_UNKNOWN_ERROR)));
    }
  }
}


/*
  Initialize THD to handle queries
*/

void prepare_new_connection_state(THD* thd)
{
  Security_context *sctx= thd->security_ctx;

  if (thd->variables.max_join_size == HA_POS_ERROR)
    thd->options |= OPTION_BIG_SELECTS;
  if (thd->client_capabilities & CLIENT_COMPRESS)
    thd->net.compress=1;				// Use compression

  /*
    Much of this is duplicated in create_embedded_thd() for the
    embedded server library.
    TODO: refactor this to avoid code duplication there
  */
  thd->version= refresh_version;
  thd->proc_info= 0;
  thd->command= COM_SLEEP;
  thd->set_time();
  thd->init_for_queries();

  /* In the past this would only run of the user did not have SUPER_ACL */
  if (sys_init_connect.value_length)
  {
    execute_init_command(thd, &sys_init_connect, &LOCK_sys_init_connect);
    if (thd->is_error())
    {
      thd->killed= THD::KILL_CONNECTION;
      sql_print_warning(ER(ER_NEW_ABORTING_CONNECTION),
                        thd->thread_id,(thd->db ? thd->db : "unconnected"),
                        sctx->user ? sctx->user : "unauthenticated",
                        sctx->host_or_ip, "init_connect command failed");
      sql_print_warning("%s", thd->main_da.message());
    }
    thd->proc_info=0;
    thd->set_time();
    thd->init_for_queries();
  }
}


/*
  Thread handler for a connection

  SYNOPSIS
    handle_one_connection()
    arg		Connection object (THD)

  IMPLEMENTATION
    This function (normally) does the following:
    - Initialize thread
    - Initialize THD to be used with this thread
    - Authenticate user
    - Execute all queries sent on the connection
    - Take connection down
    - End thread  / Handle next connection using thread from thread cache
*/

pthread_handler_t handle_one_connection(void *arg)
{
  THD *thd= (THD*) arg;
  ulong launch_time= (ulong) ((thd->thr_create_utime= my_micro_time()) -
                              thd->connect_utime);

  if (thread_scheduler.init_new_connection_thread())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES, 1);
    statistic_increment(aborted_connects,&LOCK_status);
    thread_scheduler.end_thread(thd,0);
    return 0;
  }
  if (launch_time >= slow_launch_time*1000000L)
    statistic_increment(slow_launch_threads,&LOCK_status);

  /*
    handle_one_connection() is normally the only way a thread would
    start and would always be on the very high end of the stack ,
    therefore, the thread stack always starts at the address of the
    first local variable of handle_one_connection, which is thd. We
    need to know the start of the stack so that we could check for
    stack overruns.
  */
  thd->thread_stack= (char*) &thd;
  if (setup_connection_thread_globals(thd))
    return 0;

  for (;;)
  {
    NET *net= &thd->net;

    if (login_connection(thd))
      goto end_thread;

    prepare_new_connection_state(thd);

    while (!net->error && net->vio != 0 &&
           !(thd->killed == THD::KILL_CONNECTION))
    {
      if (do_command(thd))
	break;
    }
    end_connection(thd);
   
end_thread:
    close_connection(thd, 0, 1);
    if (thread_scheduler.end_thread(thd,1))
      return 0;                                 // Probably no-threads

    /*
      If end_thread() returns, we are either running with
      thread-handler=no-threads or this thread has been schedule to
      handle the next connection.
    */
    thd= current_thd;
    thd->thread_stack= (char*) &thd;
  }
}
