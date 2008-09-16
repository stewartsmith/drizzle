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

/*
  This file is included by both libdrizzle.c (the DRIZZLE client C API)
  and the drizzled server to connect to another DRIZZLE server.

  The differences for the two cases are:

  - Things that only works for the client:
  - Trying to automaticly determinate user name if not supplied to
    drizzle_connect()
  - Support for reading local file with LOAD DATA LOCAL
  - SHARED memory handling
  - Prepared statements
 
  - Things that only works for the server
  - Alarm handling on connect
 
  In all other cases, the code should be idential for the client and
  server.
*/

#include <stdarg.h>

#include <drizzled/global.h>

#include "libdrizzle.h"

#include <sys/poll.h>
#include <sys/ioctl.h>

#include <netdb.h>

/* Remove client convenience wrappers */
#undef max_allowed_packet
#undef net_buffer_length

#define CLI_DRIZZLE_CONNECT drizzle_connect

#include <libdrizzle/errmsg.h>
#include <vio/violite.h>

#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#ifdef   HAVE_PWD_H
#include <pwd.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_SELECT_H
#  include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <sys/un.h>

#include <errno.h>
#define SOCKET_ERROR -1

#define CONNECT_TIMEOUT 0

#include <drizzled/version.h>
#include <libdrizzle/sql_common.h>
#include <libdrizzle/gettext.h>
#include "local_infile.h"

#define CLIENT_CAPABILITIES (CLIENT_LONG_PASSWORD | CLIENT_LONG_FLAG |  \
                             CLIENT_TRANSACTIONS |                      \
                             CLIENT_SECURE_CONNECTION)

uint    drizzle_port=0;
const char  *unknown_sqlstate= "HY000";
const char  *not_error_sqlstate= "00000";
const char  *cant_connect_sqlstate= "08001";

static bool drizzle_client_init= false;
static bool org_my_init_done= false;

static void drizzle_close_free_options(DRIZZLE *drizzle);
static void drizzle_close_free(DRIZZLE *drizzle);

static int wait_for_data(int fd, int32_t timeout);
int connect_with_timeout(int fd, const struct sockaddr *name, uint namelen, int32_t timeout);

/* Server error code and message */
unsigned int drizzle_server_last_errno;
char drizzle_server_last_error[DRIZZLE_ERRMSG_SIZE];

/****************************************************************************
  A modified version of connect().  connect_with_timeout() allows you to specify
  a timeout value, in seconds, that we should wait until we
  derermine we can't connect to a particular host.  If timeout is 0,
  connect_with_timeout() will behave exactly like connect().

  Base version coded by Steve Bernacki, Jr. <steve@navinet.net>
*****************************************************************************/


int connect_with_timeout(int fd, const struct sockaddr *name, uint namelen, int32_t timeout)
{
  int flags, res, s_err;

  /*
    If they passed us a timeout of zero, we should behave
    exactly like the normal connect() call does.
  */

  if (timeout == 0)
    return connect(fd, (struct sockaddr*) name, namelen);

  flags = fcntl(fd, F_GETFL, 0);    /* Set socket to not block */
#ifdef O_NONBLOCK
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);  /* and save the flags..  */
#endif

  res= connect(fd, (struct sockaddr*) name, namelen);
  s_err= errno;      /* Save the error... */
  fcntl(fd, F_SETFL, flags);
  if ((res != 0) && (s_err != EINPROGRESS))
  {
    errno= s_err;      /* Restore it */
    return(-1);
  }
  if (res == 0)        /* Connected quickly! */
    return(0);

  return wait_for_data(fd, timeout);
}


/*
  Wait up to timeout seconds for a connection to be established.

  We prefer to do this with poll() as there is no limitations with this.
  If not, we will use select()
*/

static int wait_for_data(int fd, int32_t timeout)
{
  struct pollfd ufds;
  int res;

  ufds.fd= fd;
  ufds.events= POLLIN | POLLPRI;
  if (!(res= poll(&ufds, 1, (int) timeout*1000)))
  {
    errno= EINTR;
    return -1;
  }
  if (res < 0 || !(ufds.revents & (POLLIN | POLLPRI)) || (ufds.revents & POLLHUP))
    return -1;
  return 0;
}


#if defined(HAVE_GETPWUID) && defined(NO_GETPWUID_DECL)
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif

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


/**
  Set the internal error message to DRIZZLE handler

  @param drizzle connection handle (client side)
  @param errcode  CR_ error code, passed to ER macro to get
                  error text
  @parma sqlstate SQL standard sqlstate
*/

void set_drizzle_error(DRIZZLE *drizzle, int errcode, const char *sqlstate)
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

/**
  Clear possible error state of struct NET

  @param net  clear the state of the argument
*/

void net_clear_error(NET *net)
{
  net->last_errno= 0;
  net->last_error[0]= '\0';
  strcpy(net->sqlstate, not_error_sqlstate);
}

/**
  Set an error message on the client.

  @param drizzle connection handle
  @param errcode   CR_* errcode, for client errors
  @param sqlstate  SQL standard sql state, unknown_sqlstate for the
                   majority of client errors.
  @param format    error message template, in sprintf format
  @param ...       variable number of arguments
*/

static void set_drizzle_extended_error(DRIZZLE *drizzle, int errcode,
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

/*****************************************************************************
  Read a packet from server. Give error message if socket was down
  or packet is an error message
*****************************************************************************/

uint32_t cli_safe_read(DRIZZLE *drizzle)
{
  NET *net= &drizzle->net;
  uint32_t len=0;

  if (net->vio != 0)
    len=my_net_read(net);

  if (len == packet_error || len == 0)
  {
#ifdef DRIZZLE_SERVER
    if (net->vio && vio_was_interrupted(net->vio))
      return (packet_error);
#endif /*DRIZZLE_SERVER*/
    end_server(drizzle);
    set_drizzle_error(drizzle, net->last_errno == CR_NET_PACKET_TOO_LARGE ?
                      CR_NET_PACKET_TOO_LARGE : CR_SERVER_LOST,
                      unknown_sqlstate);
    return (packet_error);
  }
  if (net->read_pos[0] == 255)
  {
    if (len > 3)
    {
      char *pos=(char*) net->read_pos+1;
      net->last_errno=uint2korr(pos);
      pos+=2;
      len-=2;
      if (pos[0] == '#')
      {
        strncpy(net->sqlstate, pos+1, SQLSTATE_LENGTH);
        pos+= SQLSTATE_LENGTH+1;
      }
      else
      {
        /*
          The SQL state hasn't been received -- it should be reset to HY000
          (unknown error sql state).
        */

        strcpy(net->sqlstate, unknown_sqlstate);
      }

      strncpy(net->last_error,(char*) pos, min((uint) len,
              (uint32_t) sizeof(net->last_error)-1));
    }
    else
      set_drizzle_error(drizzle, CR_UNKNOWN_ERROR, unknown_sqlstate);
    /*
      Cover a protocol design error: error packet does not
      contain the server status. Therefore, the client has no way
      to find out whether there are more result sets of
      a multiple-result-set statement pending. Luckily, in 5.0 an
      error always aborts execution of a statement, wherever it is
      a multi-statement or a stored procedure, so it should be
      safe to unconditionally turn off the flag here.
    */
    drizzle->server_status&= ~SERVER_MORE_RESULTS_EXISTS;

    return(packet_error);
  }
  return len;
}

void free_rows(DRIZZLE_DATA *cur)
{
  if (cur)
  {
    if (cur->data != NULL)
    {
      struct st_drizzle_rows * row= cur->data;
      uint64_t x;
      for (x= 0; x< cur->rows; x++)
      {
        struct st_drizzle_rows * next_row= row->next;
        free(row);
        row= next_row;
      }
    }
    free((uchar*) cur);
  }
}

bool
cli_advanced_command(DRIZZLE *drizzle, enum enum_server_command command,
         const unsigned char *header, uint32_t header_length,
         const unsigned char *arg, uint32_t arg_length, bool skip_check)
{
  NET *net= &drizzle->net;
  bool result= 1;
  bool stmt_skip= false;

  if (drizzle->net.vio == 0)
  {            /* Do reconnect if possible */
    if (drizzle_reconnect(drizzle) || stmt_skip)
      return(1);
  }
  if (drizzle->status != DRIZZLE_STATUS_READY ||
      drizzle->server_status & SERVER_MORE_RESULTS_EXISTS)
  {
    set_drizzle_error(drizzle, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
    return(1);
  }

  net_clear_error(net);
  drizzle->info=0;
  drizzle->affected_rows= ~(uint64_t) 0;
  /*
    We don't want to clear the protocol buffer on COM_QUIT, because if
    the previous command was a shutdown command, we may have the
    response for the COM_QUIT already in the communication buffer
  */
  net_clear(&drizzle->net, (command != COM_QUIT));

  if (net_write_command(net,(uchar) command, header, header_length,
      arg, arg_length))
  {
    if (net->last_errno == CR_NET_PACKET_TOO_LARGE)
    {
      set_drizzle_error(drizzle, CR_NET_PACKET_TOO_LARGE, unknown_sqlstate);
      goto end;
    }
    end_server(drizzle);
    if (drizzle_reconnect(drizzle) || stmt_skip)
      goto end;
    if (net_write_command(net,(uchar) command, header, header_length,
        arg, arg_length))
    {
      set_drizzle_error(drizzle, CR_SERVER_GONE_ERROR, unknown_sqlstate);
      goto end;
    }
  }
  result=0;
  if (!skip_check)
    result= ((drizzle->packet_length=cli_safe_read(drizzle)) == packet_error ?
       1 : 0);
end:
  return(result);
}

void free_old_query(DRIZZLE *drizzle)
{
  if (drizzle->fields)
  {
    /* TODO - we need to de-alloc field storage */
    free(drizzle->fields->catalog);
    free(drizzle->fields->db);
    free(drizzle->fields->table);
    free(drizzle->fields->org_table);
    free(drizzle->fields->name);
    free(drizzle->fields->org_name);
    free(drizzle->fields->def);
    free(drizzle->fields);

  }
  /* init_alloc_root(&drizzle->field_alloc,8192,0); */ /* Assume rowlength < 8192 */
  drizzle->fields= 0;
  drizzle->field_count= 0;      /* For API */
  drizzle->warning_count= 0;
  drizzle->info= 0;
  return;
}

/*
  Flush result set sent from server
*/

static void cli_flush_use_result(DRIZZLE *drizzle)
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
  Shut down connection
**************************************************************************/

void end_server(DRIZZLE *drizzle)
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


void
drizzle_free_result(DRIZZLE_RES *result)
{
  if (result)
  {
    DRIZZLE *drizzle= result->handle;
    if (drizzle)
    {
      if (drizzle->unbuffered_fetch_owner == &result->unbuffered_fetch_cancelled)
        drizzle->unbuffered_fetch_owner= 0;
      if (drizzle->status == DRIZZLE_STATUS_USE_RESULT)
      {
        (*drizzle->methods->flush_use_result)(drizzle);
        drizzle->status=DRIZZLE_STATUS_READY;
        if (drizzle->unbuffered_fetch_owner)
          *drizzle->unbuffered_fetch_owner= true;
      }
    }
    free_rows(result->data);
    /* TODO: free result->fields */
    if (result->row)
      free((uchar*) result->row);
    free((uchar*) result);
  }
}


/**************************************************************************
  Get column lengths of the current row
  If one uses drizzle_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

static void cli_fetch_lengths(uint32_t *to, DRIZZLE_ROW column, uint32_t field_count)
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

/***************************************************************************
  Change field rows to field structs
***************************************************************************/

DRIZZLE_FIELD *
unpack_fields(DRIZZLE_DATA *data, uint fields,
              bool default_value)
{
  DRIZZLE_ROWS  *row;
  DRIZZLE_FIELD  *field,*result;
  uint32_t lengths[9];        /* Max of fields */

  field= result= (DRIZZLE_FIELD*) malloc((uint) sizeof(*field)*fields);
  if (!result)
  {
    free_rows(data);        /* Free old data */
    return(0);
  }
  memset((char*) field, 0, (uint) sizeof(DRIZZLE_FIELD)*fields);

  for (row= data->data; row ; row = row->next,field++)
  {
    uchar *pos;
    /* fields count may be wrong */
    assert((uint) (field - result) < fields);
    cli_fetch_lengths(&lengths[0], row->data, default_value ? 8 : 7);
    field->catalog=   strdup((char*) row->data[0]);
    field->db=        strdup((char*) row->data[1]);
    field->table=     strdup((char*) row->data[2]);
    field->org_table= strdup((char*) row->data[3]);
    field->name=      strdup((char*) row->data[4]);
    field->org_name=  strdup((char*) row->data[5]);

    field->catalog_length=  lengths[0];
    field->db_length=    lengths[1];
    field->table_length=  lengths[2];
    field->org_table_length=  lengths[3];
    field->name_length=  lengths[4];
    field->org_name_length=  lengths[5];

    /* Unpack fixed length parts */
    pos= (uchar*) row->data[6];
    field->charsetnr= uint2korr(pos);
    field->length=  (uint) uint4korr(pos+2);
    field->type=  (enum enum_field_types) pos[6];
    field->flags=  uint2korr(pos+7);
    field->decimals=  (uint) pos[9];

    /* Test if field is Internal Number Format */
    if (((field->type <= DRIZZLE_TYPE_LONGLONG) &&
         (field->type != DRIZZLE_TYPE_TIMESTAMP)) ||
        (field->length == 14) ||
        (field->length == 8))
      field->flags|= NUM_FLAG;
    if (default_value && row->data[7])
    {
      field->def= (char *)malloc(lengths[7]);
      memcpy(field->def, row->data[7], lengths[7]);
      field->def_length= lengths[7];
    }
    else
      field->def=0;
    field->max_length= 0;
  }

  free_rows(data);        /* Free old data */
  return(result);
}

/* Read all rows (fields or data) from server */

DRIZZLE_DATA *cli_read_rows(DRIZZLE *drizzle, DRIZZLE_FIELD *DRIZZLE_FIELDs, uint32_t fields)
{
  uint32_t  field;
  uint32_t pkt_len;
  uint32_t len;
  unsigned char *cp;
  char  *to, *end_to;
  DRIZZLE_DATA *result;
  DRIZZLE_ROWS **prev_ptr,*cur;
  NET *net = &drizzle->net;

  if ((pkt_len= cli_safe_read(drizzle)) == packet_error)
    return(0);
  if (!(result=(DRIZZLE_DATA*) malloc(sizeof(DRIZZLE_DATA))))
  {
    set_drizzle_error(drizzle, CR_OUT_OF_MEMORY, unknown_sqlstate);
    return(0);
  }
  memset(result, 0, sizeof(DRIZZLE_DATA));
  prev_ptr= &result->data;
  result->rows=0;
  result->fields=fields;

  /*
    The last EOF packet is either a 254 (0xFE) character followed by 1-7 status bytes.

    This doesn't conflict with normal usage of 254 which stands for a
    string where the length of the string is 8 bytes. (see net_field_length())
  */

  while (*(cp=net->read_pos) != DRIZZLE_PROTOCOL_NO_MORE_DATA || pkt_len >= 8)
  {
    result->rows++;
    if (!(cur= (DRIZZLE_ROWS*) malloc(sizeof(DRIZZLE_ROWS))) ||
        !(cur->data= ((DRIZZLE_ROW) malloc((fields+1)*sizeof(char *)+pkt_len))))
    {
      free_rows(result);
      set_drizzle_error(drizzle, CR_OUT_OF_MEMORY, unknown_sqlstate);
      return(0);
    }
    *prev_ptr=cur;
    prev_ptr= &cur->next;
    to= (char*) (cur->data+fields+1);
    end_to=to+pkt_len-1;
    for (field=0 ; field < fields ; field++)
    {
      if ((len= net_field_length(&cp)) == NULL_LENGTH)
      {            /* null field */
        cur->data[field] = 0;
      }
      else
      {
        cur->data[field] = to;
        if (len > (uint32_t) (end_to - to))
        {
          free_rows(result);
          set_drizzle_error(drizzle, CR_MALFORMED_PACKET, unknown_sqlstate);
          return(0);
        }
        memcpy(to, cp, len);
        to[len]=0;
        to+=len+1;
        cp+=len;
        if (DRIZZLE_FIELDs)
        {
          if (DRIZZLE_FIELDs[field].max_length < len)
            DRIZZLE_FIELDs[field].max_length=len;
        }
      }
    }
    cur->data[field]=to;      /* End of last field */
    if ((pkt_len=cli_safe_read(drizzle)) == packet_error)
    {
      free_rows(result);
      return(0);
    }
  }
  *prev_ptr=0;          /* last pointer is null */
  if (pkt_len > 1)        /* DRIZZLE 4.1 protocol */
  {
    drizzle->warning_count= uint2korr(cp+1);
    drizzle->server_status= uint2korr(cp+3);
  }
  return(result);
}

/*
  Read one row. Uses packet buffer as storage for fields.
  When next packet is read, the previous field values are destroyed
*/


static int32_t
read_one_row(DRIZZLE *drizzle, uint32_t fields, DRIZZLE_ROW row, uint32_t *lengths)
{
  uint field;
  uint32_t pkt_len,len;
  uchar *pos, *prev_pos, *end_pos;
  NET *net= &drizzle->net;

  if ((pkt_len=cli_safe_read(drizzle)) == packet_error)
    return -1;
  if (pkt_len <= 8 && net->read_pos[0] == DRIZZLE_PROTOCOL_NO_MORE_DATA)
  {
    if (pkt_len > 1)        /* DRIZZLE 4.1 protocol */
    {
      drizzle->warning_count= uint2korr(net->read_pos+1);
      drizzle->server_status= uint2korr(net->read_pos+3);
    }
    return 1;        /* End of data */
  }
  prev_pos= 0;        /* allowed to write at packet[-1] */
  pos=net->read_pos;
  end_pos=pos+pkt_len;
  for (field=0 ; field < fields ; field++)
  {
    if ((len= net_field_length(&pos)) == NULL_LENGTH)
    {            /* null field */
      row[field] = 0;
      *lengths++=0;
    }
    else
    {
      if (len > (uint32_t) (end_pos - pos))
      {
        set_drizzle_error(drizzle, CR_UNKNOWN_ERROR, unknown_sqlstate);
        return -1;
      }
      row[field] = (char*) pos;
      pos+=len;
      *lengths++=len;
    }
    if (prev_pos)
      *prev_pos=0;        /* Terminate prev field */
    prev_pos=pos;
  }
  row[field]=(char*) prev_pos+1;    /* End of last field */
  *prev_pos=0;          /* Terminate last field */
  return 0;
}


/****************************************************************************
  Init DRIZZLE structure or allocate one
****************************************************************************/

DRIZZLE *
drizzle_create(DRIZZLE *ptr)
{

  if (!drizzle_client_init)
  {
    drizzle_client_init=true;

    if (!drizzle_port)
    {
      drizzle_port = DRIZZLE_PORT;
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
          drizzle_port = (uint) ntohs((ushort) serv_ptr->s_port);
#endif
        if ((env = getenv("DRIZZLE_TCP_PORT")))
          drizzle_port =(uint) atoi(env);
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
      set_drizzle_error(NULL, CR_OUT_OF_MEMORY, unknown_sqlstate);
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
  strcpy(ptr->net.sqlstate, not_error_sqlstate);

  /*
    Only enable LOAD DATA INFILE by default if configured with
    --enable-local-infile
  */

#if defined(ENABLED_LOCAL_INFILE) && !defined(DRIZZLE_SERVER)
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


/*
  Free all memory and resources used by the client library

  NOTES
    When calling this there should not be any other threads using
    the library.

    To make things simpler when used with windows dll's (which calls this
    function automaticly), it's safe to call this function multiple times.
*/


void drizzle_server_end()
{
  if (!drizzle_client_init)
    return;

  vio_end();

  drizzle_client_init= org_my_init_done= 0;
}


/*
  Note that the drizzle argument must be initialized with drizzle_init()
  before calling drizzle_connect !
*/

static bool cli_read_query_result(DRIZZLE *drizzle);
static DRIZZLE_RES *cli_use_result(DRIZZLE *drizzle);

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


DRIZZLE *
CLI_DRIZZLE_CONNECT(DRIZZLE *drizzle,const char *host, const char *user,
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
      port= drizzle_port;

    if (!host)
      host= LOCAL_HOST;

    snprintf(host_info=buff, sizeof(buff)-1, ER(CR_TCP_CONNECTION), host);

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype= SOCK_STREAM;

    snprintf(port_buf, NI_MAXSERV, "%d", port);
    gai_errno= getaddrinfo(host, port_buf, &hints, &res_lst);

    if (gai_errno != 0)
    {
      set_drizzle_extended_error(drizzle, CR_UNKNOWN_HOST, unknown_sqlstate,
                                 ER(CR_UNKNOWN_HOST), host, errno);

      goto error;
    }

    for (t_res= res_lst; t_res != NULL; t_res= t_res->ai_next)
    {
      int sock= socket(t_res->ai_family, t_res->ai_socktype,
                       t_res->ai_protocol);
      if (sock == SOCKET_ERROR)
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
    set_drizzle_extended_error(drizzle, CR_CONN_HOST_ERROR, unknown_sqlstate,
                               ER(CR_CONN_HOST_ERROR), host, socket_errno);
    goto error;
  }

  if (my_net_init(net, net->vio))
  {
    vio_delete(net->vio);
    net->vio = 0;
    set_drizzle_error(drizzle, CR_OUT_OF_MEMORY, unknown_sqlstate);
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
    set_drizzle_extended_error(drizzle, CR_SERVER_LOST, unknown_sqlstate,
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
      set_drizzle_extended_error(drizzle, CR_SERVER_LOST, unknown_sqlstate,
                               ER(CR_SERVER_LOST_INITIAL_COMM_READ),
                               errno);
    goto error;
  }
  /* Check if version of protocol matches current one */

  drizzle->protocol_version= net->read_pos[0];
  if (drizzle->protocol_version != PROTOCOL_VERSION)
  {
    set_drizzle_extended_error(drizzle, CR_VERSION_ERROR, unknown_sqlstate,
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

  if (pkt_length >= (uint) (end+1 - (char*) net->read_pos))
    drizzle->server_capabilities=uint2korr(end);
  if (pkt_length >= (uint) (end+18 - (char*) net->read_pos))
  {
    /* New protocol with 16 bytes to describe server characteristics */
    drizzle->server_language=end[2];
    drizzle->server_status=uint2korr(end+3);
  }
  end+= 18;
  if (pkt_length >= (uint) (end + SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323 + 1 -
                           (char *) net->read_pos))
    strncpy(drizzle->scramble+SCRAMBLE_LENGTH_323, end,
            SCRAMBLE_LENGTH-SCRAMBLE_LENGTH_323);
  else
    drizzle->server_capabilities&= ~CLIENT_SECURE_CONNECTION;

  if (drizzle->options.secure_auth && passwd[0] &&
      !(drizzle->server_capabilities & CLIENT_SECURE_CONNECTION))
  {
    set_drizzle_error(drizzle, CR_SECURE_AUTH, unknown_sqlstate);
    goto error;
  }

  /* Save connection information */
  if (!(drizzle->host_info= (char *)malloc(strlen(host_info)+1+strlen(host)+1
                                           +(end - (char*) net->read_pos))) ||
      !(drizzle->user=strdup(user)) ||
      !(drizzle->passwd=strdup(passwd)))
  {
    set_drizzle_error(drizzle, CR_OUT_OF_MEMORY, unknown_sqlstate);
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
  if (my_net_write(net, (uchar*) buff, (size_t) (end-buff)) || net_flush(net))
  {
    set_drizzle_extended_error(drizzle, CR_SERVER_LOST, unknown_sqlstate,
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
      set_drizzle_extended_error(drizzle, CR_SERVER_LOST, unknown_sqlstate,
                               ER(CR_SERVER_LOST_READ_AUTH),
                               errno);
    goto error;
  }

  if (client_flag & CLIENT_COMPRESS)    /* We will use compression */
    net->compress=1;


  if (db && drizzle_select_db(drizzle, db))
  {
    if (drizzle->net.last_errno == CR_SERVER_LOST)
        set_drizzle_extended_error(drizzle, CR_SERVER_LOST, unknown_sqlstate,
                                 ER(CR_SERVER_LOST_SETTING_DB),
                                 errno);
    goto error;
  }


  return(drizzle);

error:
  {
    /* Free alloced memory */
    end_server(drizzle);
    drizzle_close_free(drizzle);
    if (!(((uint32_t) client_flag) & CLIENT_REMEMBER_OPTIONS))
      drizzle_close_free_options(drizzle);
  }
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
    set_drizzle_error(drizzle, CR_SERVER_GONE_ERROR, unknown_sqlstate);
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
  Set current database
**************************************************************************/

int
drizzle_select_db(DRIZZLE *drizzle, const char *db)
{
  int error;

  if ((error=simple_command(drizzle,COM_INIT_DB, (const uchar*) db,
                            (uint32_t) strlen(db),0)))
    return(error);
  if (drizzle->db != NULL)
    free(drizzle->db);
  drizzle->db=strdup(db);
  return(0);
}


/*************************************************************************
  Send a QUIT to the server and close the connection
  If handle is alloced by DRIZZLE connect free it.
*************************************************************************/

static void drizzle_close_free_options(DRIZZLE *drizzle)
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


static void drizzle_close_free(DRIZZLE *drizzle)
{
  if (drizzle->host_info != NULL)
    free((uchar*) drizzle->host_info);
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
      simple_command(drizzle,COM_QUIT,(uchar*) 0,0,1);
      end_server(drizzle);      /* Sets drizzle->net.vio= 0 */
    }
    drizzle_close_free_options(drizzle);
    drizzle_close_free(drizzle);
    if (drizzle->free_me)
      free((uchar*) drizzle);
  }
  return;
}


static bool cli_read_query_result(DRIZZLE *drizzle)
{
  uchar *pos;
  uint32_t field_count;
  DRIZZLE_DATA *fields;
  uint32_t length;

  if ((length = cli_safe_read(drizzle)) == packet_error)
    return(1);
  free_old_query(drizzle);    /* Free old result */
get_info:
  pos=(uchar*) drizzle->net.read_pos;
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
      set_drizzle_error(drizzle, CR_MALFORMED_PACKET, unknown_sqlstate);
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
  if (!(drizzle->fields= unpack_fields(fields, (uint) field_count, 0)))
    return(1);
  drizzle->status= DRIZZLE_STATUS_GET_RESULT;
  drizzle->field_count= (uint) field_count;
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
  return(simple_command(drizzle, COM_QUERY, (uchar*) query, length, 1));
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
    set_drizzle_error(drizzle, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
    return(0);
  }
  drizzle->status=DRIZZLE_STATUS_READY;    /* server is ready */
  if (!(result=(DRIZZLE_RES*) malloc((uint) (sizeof(DRIZZLE_RES)+
                sizeof(uint32_t) *
                drizzle->field_count))))
  {
    set_drizzle_error(drizzle, CR_OUT_OF_MEMORY, unknown_sqlstate);
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
    free((uchar*) result);
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

static DRIZZLE_RES * cli_use_result(DRIZZLE *drizzle)
{
  DRIZZLE_RES *result;

  if (!drizzle->fields)
    return(0);
  if (drizzle->status != DRIZZLE_STATUS_GET_RESULT)
  {
    set_drizzle_error(drizzle, CR_COMMANDS_OUT_OF_SYNC, unknown_sqlstate);
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
    free((uchar*) result);
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


/**************************************************************************
  Return next row of the query results
**************************************************************************/

DRIZZLE_ROW
drizzle_fetch_row(DRIZZLE_RES *res)
{
  if (!res->data)
  {            /* Unbufferred fetch */
    if (!res->eof)
    {
      DRIZZLE *drizzle= res->handle;
      if (drizzle->status != DRIZZLE_STATUS_USE_RESULT)
      {
        set_drizzle_error(drizzle,
                        res->unbuffered_fetch_cancelled ?
                        CR_FETCH_CANCELED : CR_COMMANDS_OUT_OF_SYNC,
                        unknown_sqlstate);
      }
      else if (!(read_one_row(drizzle, res->field_count, res->row, res->lengths)))
      {
  res->row_count++;
  return(res->current_row=res->row);
      }
      res->eof=1;
      drizzle->status=DRIZZLE_STATUS_READY;
      /*
        Reset only if owner points to us: there is a chance that somebody
        started new query after drizzle_stmt_close():
      */
      if (drizzle->unbuffered_fetch_owner == &res->unbuffered_fetch_cancelled)
        drizzle->unbuffered_fetch_owner= 0;
      /* Don't clear handle in drizzle_free_result */
      res->handle=0;
    }
    return((DRIZZLE_ROW) NULL);
  }
  {
    DRIZZLE_ROW tmp;
    if (!res->data_cursor)
    {
      return(res->current_row=(DRIZZLE_ROW) NULL);
    }
    tmp = res->data_cursor->data;
    res->data_cursor = res->data_cursor->next;
    return(res->current_row=tmp);
  }
}


/**************************************************************************
  Get column lengths of the current row
  If one uses drizzle_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

uint32_t *
drizzle_fetch_lengths(DRIZZLE_RES *res)
{
  DRIZZLE_ROW column;

  if (!(column=res->current_row))
    return 0;          /* Something is wrong */
  if (res->data)
    (*res->methods->fetch_lengths)(res->lengths, column, res->field_count);
  return res->lengths;
}


int
drizzle_options(DRIZZLE *drizzle,enum drizzle_option option, const void *arg)
{
  switch (option) {
  case DRIZZLE_OPT_CONNECT_TIMEOUT:
    drizzle->options.connect_timeout= *(uint*) arg;
    break;
  case DRIZZLE_OPT_READ_TIMEOUT:
    drizzle->options.read_timeout= *(uint*) arg;
    break;
  case DRIZZLE_OPT_WRITE_TIMEOUT:
    drizzle->options.write_timeout= *(uint*) arg;
    break;
  case DRIZZLE_OPT_COMPRESS:
    drizzle->options.compress= 1;      /* Remember for connect */
    drizzle->options.client_flag|= CLIENT_COMPRESS;
    break;
  case DRIZZLE_OPT_LOCAL_INFILE:      /* Allow LOAD DATA LOCAL ?*/
    if (!arg || test(*(uint*) arg))
      drizzle->options.client_flag|= CLIENT_LOCAL_FILES;
    else
      drizzle->options.client_flag&= ~CLIENT_LOCAL_FILES;
    break;
  case DRIZZLE_READ_DEFAULT_FILE:
    if (drizzle->options.my_cnf_file != NULL)
      free(drizzle->options.my_cnf_file);
    drizzle->options.my_cnf_file=strdup(arg);
    break;
  case DRIZZLE_READ_DEFAULT_GROUP:
    if (drizzle->options.my_cnf_group != NULL)
      free(drizzle->options.my_cnf_group);
    drizzle->options.my_cnf_group=strdup(arg);
    break;
  case DRIZZLE_OPT_PROTOCOL:
    break;
  case DRIZZLE_OPT_USE_REMOTE_CONNECTION:
  case DRIZZLE_OPT_GUESS_CONNECTION:
    drizzle->options.methods_to_use= option;
    break;
  case DRIZZLE_SET_CLIENT_IP:
    drizzle->options.client_ip= strdup(arg);
    break;
  case DRIZZLE_SECURE_AUTH:
    drizzle->options.secure_auth= *(const bool *) arg;
    break;
  case DRIZZLE_REPORT_DATA_TRUNCATION:
    drizzle->options.report_data_truncation= test(*(const bool *) arg);
    break;
  case DRIZZLE_OPT_RECONNECT:
    drizzle->reconnect= *(const bool *) arg;
    break;
  case DRIZZLE_OPT_SSL_VERIFY_SERVER_CERT:
    if (*(const bool*) arg)
      drizzle->options.client_flag|= CLIENT_SSL_VERIFY_SERVER_CERT;
    else
      drizzle->options.client_flag&= ~CLIENT_SSL_VERIFY_SERVER_CERT;
    break;
  default:
    return(1);
  }
  return(0);
}


/****************************************************************************
  Functions to get information from the DRIZZLE structure
  These are functions to make shared libraries more usable.
****************************************************************************/

/* DRIZZLE_RES */
uint64_t drizzle_num_rows(const DRIZZLE_RES *res)
{
  return res->row_count;
}

unsigned int drizzle_num_fields(const DRIZZLE_RES *res)
{
  return res->field_count;
}

uint drizzle_errno(const DRIZZLE *drizzle)
{
  return drizzle ? drizzle->net.last_errno : drizzle_server_last_errno;
}


const char * drizzle_error(const DRIZZLE *drizzle)
{
  return drizzle ? _(drizzle->net.last_error) : _(drizzle_server_last_error);
}


/*
  Get version number for server in a form easy to test on

  SYNOPSIS
    drizzle_get_server_version()
    drizzle Connection

  EXAMPLE
    4.1.0-alfa ->  40100
 
  NOTES
    We will ensure that a newer server always has a bigger number.

  RETURN
   Signed number > 323000
*/

uint32_t
drizzle_get_server_version(const DRIZZLE *drizzle)
{
  uint major, minor, version;
  char *pos= drizzle->server_version, *end_pos;
  major=   (uint) strtoul(pos, &end_pos, 10);  pos=end_pos+1;
  minor=   (uint) strtoul(pos, &end_pos, 10);  pos=end_pos+1;
  version= (uint) strtoul(pos, &end_pos, 10);
  return (uint32_t) major*10000L+(uint32_t) (minor*100+version);
}

