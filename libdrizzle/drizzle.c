/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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

#include <drizzled/global.h>
#include <signal.h>
#include <errno.h>

#include <drizzled/common.h>
#include <libdrizzle/libdrizzle.h>
#include <libdrizzle/pack.h>
#include <libdrizzle/errmsg.h>
#include <libdrizzle/drizzle.h>
#include <drizzled/gettext.h>
#include <libdrizzle/net_serv.h>
#include <libdrizzle/drizzle_data.h>
#include <libdrizzle/local_infile.h>

#include "libdrizzle_priv.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <netdb.h>
#include <assert.h>
#include <pwd.h>
#include <sys/socket.h>


#define CONNECT_TIMEOUT 0

static bool drizzle_client_init= false;
unsigned int drizzle_server_last_errno;

/* Server error code and message */
char drizzle_server_last_error[LIBDRIZZLE_ERRMSG_SIZE];

/*
  Note that the drizzle argument must be initialized with drizzle_init()
  before calling drizzle_connect !
*/



static DRIZZLE_METHODS client_methods=
{
  cli_read_query_result,                       /* read_query_result */
  cli_advanced_command,                        /* advanced_command */
  cli_read_rows,                               /* read_rows */
  cli_use_result,                              /* use_result */
  cli_fetch_lengths,                           /* fetch_lengths */
  cli_flush_use_result,                        /* flush_use_result */
  cli_list_fields,                             /* list_fields */
  cli_unbuffered_fetch,                        /* unbuffered_fetch */
  cli_read_statistics,                         /* read_statistics */
  cli_read_query_result,                       /* next_result */
  cli_read_change_user_result,                 /* read_change_user_result */
};



/****************************************************************************
  Init DRIZZLE structure or allocate one
****************************************************************************/

DRIZZLE *
drizzle_create(DRIZZLE *ptr)
{

  if (!drizzle_client_init)
  {
    drizzle_client_init=true;

    if (!drizzle_get_default_port())
    {
      drizzle_set_default_port(DRIZZLE_PORT);
      {
        struct servent *serv_ptr;
        char *env;

        /*
          if builder specifically requested a default port, use that
          (even if it coincides with our factory default).
          only if they didn't do we check /etc/services (and, failing
          on that, fall back to the factory default of 4427).
          either default can be overridden by the environment variable
          DRIZZLE_TCP_PORT, which in turn can be overridden with command
          line options.
        */

#if DRIZZLE_PORT_DEFAULT == 0
        if ((serv_ptr = getservbyname("drizzle", "tcp")))
          drizzle_set_default_port((uint32_t) ntohs((uint16_t) serv_ptr->s_port));
#endif
        if ((env = getenv("DRIZZLE_TCP_PORT")))
          drizzle_set_default_port((uint32_t) atoi(env));
      }
    }
#if defined(SIGPIPE)
    (void) signal(SIGPIPE, SIG_IGN);
#endif
  }

  if (ptr == NULL)
  {
    ptr= (DRIZZLE *) malloc(sizeof(DRIZZLE));

    if (ptr == NULL)
    {
      drizzle_set_error(NULL, CR_OUT_OF_MEMORY, sqlstate_get_unknown());
      return 0;
    }
    memset(ptr, 0, sizeof(DRIZZLE));
    ptr->free_me=1;
  }
  else
  {
    memset(ptr, 0, sizeof(DRIZZLE));
  }

  ptr->options.connect_timeout= CONNECT_TIMEOUT;
  strcpy(ptr->net.sqlstate, sqlstate_get_not_error());

  /*
    Only enable LOAD DATA INFILE by default if configured with
    --enable-local-infile
  */

#if defined(ENABLED_LOCAL_INFILE)
  ptr->options.client_flag|= CLIENT_LOCAL_FILES;
#endif

  ptr->options.methods_to_use= DRIZZLE_OPT_GUESS_CONNECTION;
  ptr->options.report_data_truncation= true;  /* default */

  /*
    By default we don't reconnect because it could silently corrupt data (after
    reconnection you potentially lose table locks, user variables, session
    variables (transactions but they are specifically dealt with in
    drizzle_reconnect()).
    This is a change: < 5.0.3 drizzle->reconnect was set to 1 by default.
    How this change impacts existing apps:
    - existing apps which relyed on the default will see a behaviour change;
    they will have to set reconnect=1 after drizzle_connect().
    - existing apps which explicitely asked for reconnection (the only way they
    could do it was by setting drizzle.reconnect to 1 after drizzle_connect())
    will not see a behaviour change.
    - existing apps which explicitely asked for no reconnection
    (drizzle.reconnect=0) will not see a behaviour change.
  */
  ptr->reconnect= 0;

  return ptr;
}


static void read_user_name(char *name)
{
  if (geteuid() == 0)
    strcpy(name,"root");    /* allow use of surun */
  else
  {
#ifdef HAVE_GETPWUID
    struct passwd *skr;
    const char *str;
    if ((str=getlogin()) == NULL)
    {
      if ((skr=getpwuid(geteuid())) != NULL)
  str=skr->pw_name;
      else if (!(str=getenv("USER")) && !(str=getenv("LOGNAME")) &&
         !(str=getenv("LOGIN")))
  str="UNKNOWN_USER";
    }
    strncpy(name,str,USERNAME_LENGTH);
#elif HAVE_CUSERID
    (void) cuserid(name);
#else
    strcpy(name,"UNKNOWN_USER");
#endif
  }
  return;
}

DRIZZLE *
drizzle_connect(DRIZZLE *drizzle,const char *host, const char *user,
                const char *passwd, const char *db,
                uint32_t port,
                const char * unix_port __attribute__((__unused__)),
                uint32_t client_flag)
{
  char          buff[NAME_LEN+USERNAME_LENGTH+100];
  char          *end,*host_info=NULL;
  uint32_t         pkt_length;
  NET           *net= &drizzle->net;

  drizzle->methods= &client_methods;
  net->vio = 0;        /* If something goes wrong */
  drizzle->client_flag=0;      /* For handshake */

  /* Some empty-string-tests are done because of ODBC */
  if (!host || !host[0])
    host=drizzle->options.host;
  if (!user || !user[0])
  {
    user=drizzle->options.user;
    if (!user)
      user= "";
  }
  if (!passwd)
  {
    passwd=drizzle->options.password;
    if (!passwd)
      passwd= "";
  }
  if (!db || !db[0])
    db=drizzle->options.db;
  if (!port)
    port=drizzle->options.port;

  drizzle->server_status=SERVER_STATUS_AUTOCOMMIT;

  /*
    Part 0: Grab a socket and connect it to the server
  */
  if (!net->vio)
  {
    struct addrinfo *res_lst, hints, *t_res;
    int gai_errno;
    char port_buf[NI_MAXSERV];

    if (!port)
      port= drizzle_get_default_port();

    if (!host)
      host= LOCAL_HOST;

    snprintf(host_info=buff, sizeof(buff)-1, ER(CR_TCP_CONNECTION), host);

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype= SOCK_STREAM;

    snprintf(port_buf, NI_MAXSERV, "%d", port);
    gai_errno= getaddrinfo(host, port_buf, &hints, &res_lst);

    if (gai_errno != 0)
    {
      drizzle_set_extended_error(drizzle, CR_UNKNOWN_HOST, sqlstate_get_unknown(),
                                 ER(CR_UNKNOWN_HOST), host, errno);

      goto error;
    }

    for (t_res= res_lst; t_res != NULL; t_res= t_res->ai_next)
    {
      int sock= socket(t_res->ai_family, t_res->ai_socktype,
                       t_res->ai_protocol);
      if (sock < 0)
        continue;

      net->vio= vio_new(sock, VIO_TYPE_TCPIP, VIO_BUFFERED_READ);
      if (! net->vio )
      {
        close(sock);
        continue;
      }

      if (connect_with_timeout(sock, t_res->ai_addr, t_res->ai_addrlen, drizzle->options.connect_timeout))
      {
        vio_delete(net->vio);
        net->vio= 0;
        continue;
      }
      break;
    }

    freeaddrinfo(res_lst);
  }

  if (!net->vio)
  {
    drizzle_set_extended_error(drizzle, CR_CONN_HOST_ERROR, sqlstate_get_unknown(),
                               ER(CR_CONN_HOST_ERROR), host, port, errno);
    goto error;
  }

  if (my_net_init(net, net->vio))
  {
    vio_delete(net->vio);
    net->vio = 0;
    drizzle_set_error(drizzle, CR_OUT_OF_MEMORY, sqlstate_get_unknown());
    goto error;
  }
  vio_keepalive(net->vio,true);

  /* If user set read_timeout, let it override the default */
  if (drizzle->options.read_timeout)
    my_net_set_read_timeout(net, drizzle->options.read_timeout);

  /* If user set write_timeout, let it override the default */
  if (drizzle->options.write_timeout)
    my_net_set_write_timeout(net, drizzle->options.write_timeout);

  if (drizzle->options.max_allowed_packet)
    net->max_packet_size= drizzle->options.max_allowed_packet;

  /* Get version info */
  drizzle->protocol_version= PROTOCOL_VERSION;  /* Assume this */
  if (drizzle->options.connect_timeout &&
      vio_poll_read(net->vio, drizzle->options.connect_timeout))
  {
    drizzle_set_extended_error(drizzle, CR_SERVER_LOST, sqlstate_get_unknown(),
                               ER(CR_SERVER_LOST_INITIAL_COMM_WAIT),
                               errno);
    goto error;
  }

  /*
    Part 1: Connection established, read and parse first packet
  */

  if ((pkt_length=cli_safe_read(drizzle)) == packet_error)
  {
    if (drizzle->net.last_errno == CR_SERVER_LOST)
      drizzle_set_extended_error(drizzle, CR_SERVER_LOST, sqlstate_get_unknown(),
                                 ER(CR_SERVER_LOST_INITIAL_COMM_READ),
                                 errno);
    goto error;
  }
  /* Check if version of protocol matches current one */

  drizzle->protocol_version= net->read_pos[0];
  if (drizzle->protocol_version != PROTOCOL_VERSION)
  {
    drizzle_set_extended_error(drizzle, CR_VERSION_ERROR, sqlstate_get_unknown(),
                               ER(CR_VERSION_ERROR), drizzle->protocol_version,
                               PROTOCOL_VERSION);
    goto error;
  }
  end= strchr((char*) net->read_pos+1, '\0');
  drizzle->thread_id=uint4korr(end+1);
  end+=5;
  /*
    Scramble is split into two parts because old clients does not understand
    long scrambles; here goes the first part.
  */
  strncpy(drizzle->scramble, end, SCRAMBLE_LENGTH_323);
  end+= SCRAMBLE_LENGTH_323+1;

  if (pkt_length >= (uint32_t) (end+1 - (char*) net->read_pos))
    drizzle->server_capabilities=uint2korr(end);
  if (pkt_length >= (uint32_t) (end+18 - (char*) net->read_pos))
  {
    /* New protocol with 16 bytes to describe server characteristics */
    drizzle->server_language=end[2];
    drizzle->server_status=uint2korr(end+3);
  }
  end+= 18;
  if (pkt_length >= (uint32_t) (end + SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323 + 1 -
                            (char *) net->read_pos))
    strncpy(drizzle->scramble+SCRAMBLE_LENGTH_323, end,
            SCRAMBLE_LENGTH-SCRAMBLE_LENGTH_323);
  else
    drizzle->server_capabilities&= ~CLIENT_SECURE_CONNECTION;

  if (drizzle->options.secure_auth && passwd[0] &&
      !(drizzle->server_capabilities & CLIENT_SECURE_CONNECTION))
  {
    drizzle_set_error(drizzle, CR_SECURE_AUTH, sqlstate_get_unknown());
    goto error;
  }

  /* Save connection information */
  if (!(drizzle->host_info= (char *)malloc(strlen(host_info)+1+strlen(host)+1
                                           +(end - (char*) net->read_pos))) ||
      !(drizzle->user=strdup(user)) ||
      !(drizzle->passwd=strdup(passwd)))
  {
    drizzle_set_error(drizzle, CR_OUT_OF_MEMORY, sqlstate_get_unknown());
    goto error;
  }
  drizzle->host= drizzle->host_info+strlen(host_info)+1;
  drizzle->server_version= drizzle->host+strlen(host)+1;
  strcpy(drizzle->host_info,host_info);
  strcpy(drizzle->host,host);
  strcpy(drizzle->server_version,(char*) net->read_pos+1);
  drizzle->port=port;

  /*
    Part 2: format and send client info to the server for access check
  */

  client_flag|=drizzle->options.client_flag;
  client_flag|=CLIENT_CAPABILITIES;
  if (client_flag & CLIENT_MULTI_STATEMENTS)
    client_flag|= CLIENT_MULTI_RESULTS;

  if (db)
    client_flag|=CLIENT_CONNECT_WITH_DB;

  /* Remove options that server doesn't support */
  client_flag= ((client_flag &
                 ~(CLIENT_COMPRESS | CLIENT_SSL)) |
                (client_flag & drizzle->server_capabilities));
  client_flag&= ~CLIENT_COMPRESS;

  int4store(buff, client_flag);
  int4store(buff+4, net->max_packet_size);
  buff[8]= (char) 45; // utf8 charset number
  memset(buff+9, 0, 32-9);
  end= buff+32;

  drizzle->client_flag=client_flag;

  /* This needs to be changed as it's not useful with big packets */
  if (user && user[0])
    strncpy(end,user,USERNAME_LENGTH);          /* Max user name */
  else
    read_user_name((char*) end);

  /* We have to handle different version of handshake here */
  end= strchr(end, '\0') + 1;
  if (passwd[0])
  {
    {
      *end++= SCRAMBLE_LENGTH;
      memset(end, 0, SCRAMBLE_LENGTH-1);
      memcpy(end, passwd, strlen(passwd));
      end+= SCRAMBLE_LENGTH;
    }
  }
  else
    *end++= '\0';                               /* empty password */

  /* Add database if needed */
  if (db && (drizzle->server_capabilities & CLIENT_CONNECT_WITH_DB))
  {
    end= strncpy(end, db, NAME_LEN) + NAME_LEN + 1;
    drizzle->db= strdup(db);
    db= 0;
  }
  /* Write authentication package */
  if (my_net_write(net, (unsigned char*) buff, (size_t) (end-buff)) || net_flush(net))
  {
    drizzle_set_extended_error(drizzle, CR_SERVER_LOST, sqlstate_get_unknown(),
                               ER(CR_SERVER_LOST_SEND_AUTH),
                               errno);
    goto error;
  }

  /*
    Part 3: Authorization data's been sent. Now server can reply with
    OK-packet, or re-request scrambled password.
  */

  if ((pkt_length=cli_safe_read(drizzle)) == packet_error)
  {
    if (drizzle->net.last_errno == CR_SERVER_LOST)
      drizzle_set_extended_error(drizzle, CR_SERVER_LOST, sqlstate_get_unknown(),
                                 ER(CR_SERVER_LOST_READ_AUTH),
                                 errno);
    goto error;
  }

  if (client_flag & CLIENT_COMPRESS)    /* We will use compression */
    net->compress=1;


  if (db && drizzle_select_db(drizzle, db))
  {
    if (drizzle->net.last_errno == CR_SERVER_LOST)
      drizzle_set_extended_error(drizzle, CR_SERVER_LOST, sqlstate_get_unknown(),
                                 ER(CR_SERVER_LOST_SETTING_DB),
                                 errno);
    goto error;
  }


  return(drizzle);

error:
  {
    /* Free alloced memory */
    drizzle_disconnect(drizzle);
    drizzle_close_free(drizzle);
    if (!(((uint32_t) client_flag) & CLIENT_REMEMBER_OPTIONS))
      drizzle_close_free_options(drizzle);
  }
  return(0);
}




/**************************************************************************
  Set current database
**************************************************************************/

int
drizzle_select_db(DRIZZLE *drizzle, const char *db)
{
  int error;

  if ((error=simple_command(drizzle,COM_INIT_DB, (const unsigned char*) db,
                            (uint32_t) strlen(db),0)))
    return(error);
  if (drizzle->db != NULL)
    free(drizzle->db);
  drizzle->db=strdup(db);
  return(0);
}

bool drizzle_reconnect(DRIZZLE *drizzle)
{
  DRIZZLE tmp_drizzle;
  assert(drizzle);

  if (!drizzle->reconnect ||
      (drizzle->server_status & SERVER_STATUS_IN_TRANS) || !drizzle->host_info)
  {
    /* Allow reconnect next time */
    drizzle->server_status&= ~SERVER_STATUS_IN_TRANS;
    drizzle_set_error(drizzle, CR_SERVER_GONE_ERROR, sqlstate_get_unknown());
    return(1);
  }
  drizzle_create(&tmp_drizzle);
  tmp_drizzle.options= drizzle->options;
  tmp_drizzle.options.my_cnf_file= tmp_drizzle.options.my_cnf_group= 0;

  if (!drizzle_connect(&tmp_drizzle,drizzle->host,drizzle->user,drizzle->passwd,
                       drizzle->db, drizzle->port, 0,
                       drizzle->client_flag | CLIENT_REMEMBER_OPTIONS))
  {
    drizzle->net.last_errno= tmp_drizzle.net.last_errno;
    strcpy(drizzle->net.last_error, tmp_drizzle.net.last_error);
    strcpy(drizzle->net.sqlstate, tmp_drizzle.net.sqlstate);
    return(1);
  }

  tmp_drizzle.reconnect= 1;
  tmp_drizzle.free_me= drizzle->free_me;

  /* Don't free options as these are now used in tmp_drizzle */
  memset(&drizzle->options, 0, sizeof(drizzle->options));
  drizzle->free_me=0;
  drizzle_close(drizzle);
  *drizzle=tmp_drizzle;
  net_clear(&drizzle->net, 1);
  drizzle->affected_rows= ~(uint64_t) 0;
  return(0);
}

/**************************************************************************
  Shut down connection
**************************************************************************/

void drizzle_disconnect(DRIZZLE *drizzle)
{
  int save_errno= errno;
  if (drizzle->net.vio != 0)
  {
    vio_delete(drizzle->net.vio);
    drizzle->net.vio= 0;          /* Marker */
  }
  net_end(&drizzle->net);
  free_old_query(drizzle);
  errno= save_errno;
}


/*************************************************************************
  Send a QUIT to the server and close the connection
  If handle is alloced by DRIZZLE connect free it.
*************************************************************************/

void drizzle_close_free_options(DRIZZLE *drizzle)
{
  if (drizzle->options.user != NULL)
    free(drizzle->options.user);
  if (drizzle->options.host != NULL)
    free(drizzle->options.host);
  if (drizzle->options.password != NULL)
    free(drizzle->options.password);
  if (drizzle->options.db != NULL)
    free(drizzle->options.db);
  if (drizzle->options.my_cnf_file != NULL)
    free(drizzle->options.my_cnf_file);
  if (drizzle->options.my_cnf_group != NULL)
    free(drizzle->options.my_cnf_group);
  if (drizzle->options.client_ip != NULL)
    free(drizzle->options.client_ip);
  memset(&drizzle->options, 0, sizeof(drizzle->options));
  return;
}


void drizzle_close_free(DRIZZLE *drizzle)
{
  if (drizzle->host_info != NULL)
    free((unsigned char*) drizzle->host_info);
  if (drizzle->user != NULL)
    free(drizzle->user);
  if (drizzle->passwd != NULL)
    free(drizzle->passwd);
  if (drizzle->db != NULL)
    free(drizzle->db);
  if (drizzle->info_buffer != NULL)
    free(drizzle->info_buffer);
  drizzle->info_buffer= 0;

  /* Clear pointers for better safety */
  drizzle->host_info= drizzle->user= drizzle->passwd= drizzle->db= 0;
}


void drizzle_close(DRIZZLE *drizzle)
{
  if (drizzle)          /* Some simple safety */
  {
    /* If connection is still up, send a QUIT message */
    if (drizzle->net.vio != 0)
    {
      free_old_query(drizzle);
      drizzle->status=DRIZZLE_STATUS_READY; /* Force command */
      drizzle->reconnect=0;
      simple_command(drizzle,COM_QUIT,(unsigned char*) 0,0,1);
      drizzle_disconnect(drizzle);      /* Sets drizzle->net.vio= 0 */
    }
    drizzle_close_free_options(drizzle);
    drizzle_close_free(drizzle);
    if (drizzle->free_me)
      free((unsigned char*) drizzle);
  }
  return;
}


bool cli_read_query_result(DRIZZLE *drizzle)
{
  unsigned char *pos;
  uint32_t field_count;
  DRIZZLE_DATA *fields;
  uint32_t length;

  if ((length = cli_safe_read(drizzle)) == packet_error)
    return(1);
  free_old_query(drizzle);    /* Free old result */
get_info:
  pos=(unsigned char*) drizzle->net.read_pos;
  if ((field_count= net_field_length(&pos)) == 0)
  {
    drizzle->affected_rows= net_field_length_ll(&pos);
    drizzle->insert_id=    net_field_length_ll(&pos);

    drizzle->server_status= uint2korr(pos); pos+=2;
    drizzle->warning_count= uint2korr(pos); pos+=2;

    if (pos < drizzle->net.read_pos+length && net_field_length(&pos))
      drizzle->info=(char*) pos;
    return(0);
  }
  if (field_count == NULL_LENGTH)    /* LOAD DATA LOCAL INFILE */
  {
    int error;

    if (!(drizzle->options.client_flag & CLIENT_LOCAL_FILES))
    {
      drizzle_set_error(drizzle, CR_MALFORMED_PACKET, sqlstate_get_unknown());
      return(1);
    }

    error= handle_local_infile(drizzle,(char*) pos);
    if ((length= cli_safe_read(drizzle)) == packet_error || error)
      return(1);
    goto get_info;        /* Get info packet */
  }
  if (!(drizzle->server_status & SERVER_STATUS_AUTOCOMMIT))
    drizzle->server_status|= SERVER_STATUS_IN_TRANS;

  if (!(fields=cli_read_rows(drizzle,(DRIZZLE_FIELD*)0, 7)))
    return(1);
  if (!(drizzle->fields= unpack_fields(fields, (uint32_t) field_count, 0)))
    return(1);
  drizzle->status= DRIZZLE_STATUS_GET_RESULT;
  drizzle->field_count= (uint32_t) field_count;
  return(0);
}


/*
  Send the query and return so we can do something else.
  Needs to be followed by drizzle_read_query_result() when we want to
  finish processing it.
*/

int32_t
drizzle_send_query(DRIZZLE *drizzle, const char* query, uint32_t length)
{
  return(simple_command(drizzle, COM_QUERY, (unsigned char*) query, length, 1));
}


int32_t
drizzle_real_query(DRIZZLE *drizzle, const char *query, uint32_t length)
{
  if (drizzle_send_query(drizzle,query,length))
    return(1);
  return((int) (*drizzle->methods->read_query_result)(drizzle));
}


/**************************************************************************
  Alloc result struct for buffered results. All rows are read to buffer.
  drizzle_data_seek may be used.
**************************************************************************/

DRIZZLE_RES * drizzle_store_result(DRIZZLE *drizzle)
{
  DRIZZLE_RES *result;

  if (!drizzle->fields)
    return(0);
  if (drizzle->status != DRIZZLE_STATUS_GET_RESULT)
  {
    drizzle_set_error(drizzle, CR_COMMANDS_OUT_OF_SYNC, sqlstate_get_unknown());
    return(0);
  }
  drizzle->status=DRIZZLE_STATUS_READY;    /* server is ready */
  if (!(result=(DRIZZLE_RES*) malloc((uint32_t) (sizeof(DRIZZLE_RES)+
                sizeof(uint32_t) *
                drizzle->field_count))))
  {
    drizzle_set_error(drizzle, CR_OUT_OF_MEMORY, sqlstate_get_unknown());
    return(0);
  }
  memset(result, 0,(sizeof(DRIZZLE_RES)+ sizeof(uint32_t) *
                    drizzle->field_count));
  result->methods= drizzle->methods;
  result->eof= 1;        /* Marker for buffered */
  result->lengths= (uint32_t*) (result+1);
  if (!(result->data=
  (*drizzle->methods->read_rows)(drizzle,drizzle->fields,drizzle->field_count)))
  {
    free((unsigned char*) result);
    return(0);
  }
  drizzle->affected_rows= result->row_count= result->data->rows;
  result->data_cursor=  result->data->data;
  result->fields=  drizzle->fields;
  result->field_count=  drizzle->field_count;
  /* The rest of result members is zeroed in malloc */
  drizzle->fields=0;        /* fields is now in result */
  /* just in case this was mistakenly called after drizzle_stmt_execute() */
  drizzle->unbuffered_fetch_owner= 0;
  return(result);        /* Data fetched */
}


/**************************************************************************
  Alloc struct for use with unbuffered reads. Data is fetched by domand
  when calling to drizzle_fetch_row.
  DRIZZLE_DATA_seek is a noop.

  No other queries may be specified with the same DRIZZLE handle.
  There shouldn't be much processing per row because DRIZZLE server shouldn't
  have to wait for the client (and will not wait more than 30 sec/packet).
**************************************************************************/

DRIZZLE_RES * cli_use_result(DRIZZLE *drizzle)
{
  DRIZZLE_RES *result;

  if (!drizzle->fields)
    return(0);
  if (drizzle->status != DRIZZLE_STATUS_GET_RESULT)
  {
    drizzle_set_error(drizzle, CR_COMMANDS_OUT_OF_SYNC, sqlstate_get_unknown());
    return(0);
  }
  if (!(result=(DRIZZLE_RES*) malloc(sizeof(*result)+
                                     sizeof(uint32_t)*drizzle->field_count)))
    return(0);
  memset(result, 0, sizeof(*result)+ sizeof(uint32_t)*drizzle->field_count);
  result->lengths=(uint32_t*) (result+1);
  result->methods= drizzle->methods;
  if (!(result->row=(DRIZZLE_ROW)
        malloc(sizeof(result->row[0])*(drizzle->field_count+1))))
  {          /* Ptrs: to one row */
    free((unsigned char*) result);
    return(0);
  }
  result->fields=  drizzle->fields;
  result->field_count=  drizzle->field_count;
  result->current_field=0;
  result->handle=  drizzle;
  result->current_row=  0;
  drizzle->fields=0;      /* fields is now in result */
  drizzle->status=DRIZZLE_STATUS_USE_RESULT;
  drizzle->unbuffered_fetch_owner= &result->unbuffered_fetch_cancelled;
  return(result);      /* Data is read to be fetched */
}



/**
   Set the internal error message to DRIZZLE handler

   @param drizzle connection handle (client side)
   @param errcode  CR_ error code, passed to ER macro to get
   error text
   @parma sqlstate SQL standard sqlstate
*/

void drizzle_set_error(DRIZZLE *drizzle, int errcode, const char *sqlstate)
{
  NET *net;
  assert(drizzle != 0);

  if (drizzle)
  {
    net= &drizzle->net;
    net->last_errno= errcode;
    strcpy(net->last_error, ER(errcode));
    strcpy(net->sqlstate, sqlstate);
  }
  else
  {
    drizzle_server_last_errno= errcode;
    strcpy(drizzle_server_last_error, ER(errcode));
  }
  return;
}


unsigned int drizzle_errno(const DRIZZLE *drizzle)
{
  return drizzle ? drizzle->net.last_errno : drizzle_server_last_errno;
}


const char * drizzle_error(const DRIZZLE *drizzle)
{
  return drizzle ? _(drizzle->net.last_error) : _(drizzle_server_last_error);
}

/**
   Set an error message on the client.

   @param drizzle connection handle
   @param errcode   CR_* errcode, for client errors
   @param sqlstate  SQL standard sql state, sqlstate_get_unknown() for the
   majority of client errors.
   @param format    error message template, in sprintf format
   @param ...       variable number of arguments
*/

void drizzle_set_extended_error(DRIZZLE *drizzle, int errcode,
                                const char *sqlstate,
                                const char *format, ...)
{
  NET *net;
  va_list args;
  assert(drizzle != 0);

  net= &drizzle->net;
  net->last_errno= errcode;
  va_start(args, format);
  vsnprintf(net->last_error, sizeof(net->last_error)-1,
            format, args);
  va_end(args);
  strcpy(net->sqlstate, sqlstate);

  return;
}



/*
  Flush result set sent from server
*/

void cli_flush_use_result(DRIZZLE *drizzle)
{
  /* Clear the current execution status */
  for (;;)
  {
    uint32_t pkt_len;
    if ((pkt_len=cli_safe_read(drizzle)) == packet_error)
      break;
    if (pkt_len <= 8 && drizzle->net.read_pos[0] == DRIZZLE_PROTOCOL_NO_MORE_DATA)
    {
      char *pos= (char*) drizzle->net.read_pos + 1;
      drizzle->warning_count=uint2korr(pos); pos+=2;
      drizzle->server_status=uint2korr(pos); pos+=2;

      break;                            /* End of data */
    }
  }
  return;
}

/**************************************************************************
  Get column lengths of the current row
  If one uses drizzle_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

void cli_fetch_lengths(uint32_t *to, DRIZZLE_ROW column, uint32_t field_count)
{
  uint32_t *prev_length;
  char *start=0;
  DRIZZLE_ROW end;

  prev_length=0;        /* Keep gcc happy */
  for (end=column + field_count + 1 ; column != end ; column++, to++)
  {
    if (!*column)
    {
      *to= 0;          /* Null */
      continue;
    }
    if (start)          /* Found end of prev string */
      *prev_length= (uint32_t) (*column-start-1);
    start= *column;
    prev_length= to;
  }
}

