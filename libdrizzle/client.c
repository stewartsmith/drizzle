/* Copyright (C) 2008 Drizzle Open Source Project

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
  This file is included by both libdrizzle.c (the DRIZZLE client C API)
  and the drizzled server to connect to another DRIZZLE server.

  The differences for the two cases are:

  - Things that only works for the client:
  - Trying to automaticly determinate user name if not supplied to
    drizzle_connect()
  - Support for reading local file with LOAD DATA LOCAL
  - SHARED memory handling
  - Protection against sigpipe
  - Prepared statements
 
  - Things that only works for the server
  - Alarm handling on connect
 
  In all other cases, the code should be idential for the client and
  server.
*/

#include <drizzled/global.h>

#include "drizzle.h"

#include <sys/poll.h>
#include <sys/ioctl.h>

#include <netdb.h>

/* Remove client convenience wrappers */
#undef max_allowed_packet
#undef net_buffer_length

#define CLI_DRIZZLE_CONNECT STDCALL drizzle_connect

#include <mysys/my_sys.h>
#include <mysys/mysys_err.h>
#include <mystrings/m_string.h>
#include <mystrings/m_ctype.h>
#include <drizzled/error.h>
#include "errmsg.h"
#include <vio/violite.h>
#include <mysys/my_pthread.h>        /* because of signal()  */

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

#include "client_settings.h"
#include <drizzled/version.h>
#include <libdrizzle/sql_common.h>

uint    drizzle_port=0;
char    *drizzle_unix_port= 0;
const char  *unknown_sqlstate= "HY000";
const char  *not_error_sqlstate= "00000";
const char  *cant_connect_sqlstate= "08001";

static bool drizzle_client_init= false;
static bool org_my_init_done= false;

static void drizzle_close_free_options(DRIZZLE *drizzle);
static void drizzle_close_free(DRIZZLE *drizzle);

static int wait_for_data(int fd, int32_t timeout);
int connect_with_timeout(int fd, const struct sockaddr *name, uint namelen, int32_t timeout);

CHARSET_INFO *default_client_charset_info = &my_charset_latin1;

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
  ulong len=0;
  init_sigpipe_variables

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(drizzle);
  if (net->vio != 0)
    len=my_net_read(net);
  reset_sigpipe(drizzle);

  if (len == packet_error || len == 0)
  {
#ifdef MYSQL_SERVER
    if (net->vio && vio_was_interrupted(net->vio))
      return (packet_error);
#endif /*DRIZZLE_SERVER*/
    end_server(drizzle);
    set_drizzle_error(drizzle, net->last_errno == ER_NET_PACKET_TOO_LARGE ?
                    CR_NET_PACKET_TOO_LARGE: CR_SERVER_LOST, unknown_sqlstate);
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
    free_root(&cur->alloc,MYF(0));
    my_free((uchar*) cur,MYF(0));
  }
}

bool
cli_advanced_command(DRIZZLE *drizzle, enum enum_server_command command,
         const unsigned char *header, uint32_t header_length,
         const unsigned char *arg, uint32_t arg_length, bool skip_check)
{
  NET *net= &drizzle->net;
  bool result= 1;
  init_sigpipe_variables
  bool stmt_skip= false;

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(drizzle);

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
    if (net->last_errno == ER_NET_PACKET_TOO_LARGE)
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
  reset_sigpipe(drizzle);
  return(result);
}

void free_old_query(DRIZZLE *drizzle)
{
  if (drizzle->fields)
    free_root(&drizzle->field_alloc,MYF(0));
  init_alloc_root(&drizzle->field_alloc,8192,0); /* Assume rowlength < 8192 */
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
    ulong pkt_len;
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
    init_sigpipe_variables
    set_sigpipe(drizzle);
    vio_delete(drizzle->net.vio);
    reset_sigpipe(drizzle);
    drizzle->net.vio= 0;          /* Marker */
  }
  net_end(&drizzle->net);
  free_old_query(drizzle);
  errno= save_errno;
}


void STDCALL
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
    if (result->fields)
      free_root(&result->field_alloc,MYF(0));
    if (result->row)
      my_free((uchar*) result->row,MYF(0));
    my_free((uchar*) result,MYF(0));
  }
}

/****************************************************************************
  Get options from my.cnf
****************************************************************************/

static const char *default_options[]=
{
  "port","socket","compress","password","pipe", "timeout", "user",
  "init-command", "host", "database", "return-found-rows",
  "ssl-key" ,"ssl-cert" ,"ssl-ca" ,"ssl-capath",
  "character-sets-dir", "default-character-set", "interactive-timeout",
  "connect-timeout", "local-infile", "disable-local-infile",
  "ssl-cipher", "max-allowed-packet", "protocol", "shared-memory-base-name",
  "multi-results", "multi-statements", "multi-queries", "secure-auth",
  "report-data-truncation",
  NullS
};

static TYPELIB option_types={array_elements(default_options)-1,
           "options",default_options, NULL};

const char *sql_protocol_names_lib[] =
{ "TCP", "SOCKET", "PIPE", "MEMORY", NullS };
TYPELIB sql_protocol_typelib = {array_elements(sql_protocol_names_lib)-1,"",
        sql_protocol_names_lib, NULL};

static int add_init_command(struct st_drizzle_options *options, const char *cmd)
{
  char *tmp;

  if (!options->init_commands)
  {
    options->init_commands= (DYNAMIC_ARRAY*)my_malloc(sizeof(DYNAMIC_ARRAY),
                  MYF(MY_WME));
    init_dynamic_array(options->init_commands,sizeof(char*),0,5 CALLER_INFO);
  }

  if (!(tmp= my_strdup(cmd,MYF(MY_WME))) ||
      insert_dynamic(options->init_commands, (uchar*)&tmp))
  {
    my_free(tmp, MYF(MY_ALLOW_ZERO_PTR));
    return 1;
  }

  return 0;
}

void drizzle_read_default_options(struct st_drizzle_options *options,
        const char *filename,const char *group)
{
  int argc;
  char *argv_buff[1],**argv;
  const char *groups[3];

  argc=1; argv=argv_buff; argv_buff[0]= (char*) "client";
  groups[0]= (char*) "client"; groups[1]= (char*) group; groups[2]=0;

  load_defaults(filename, groups, &argc, &argv);
  if (argc != 1)        /* If some default option */
  {
    char **option=argv;
    while (*++option)
    {
      if (option[0][0] == '-' && option[0][1] == '-')
      {
        char *end=strrchr(*option,'=');
        char *opt_arg=0;
        if (end != NULL)
        {
          opt_arg=end+1;
          *end=0;        /* Remove '=' */
        }
        /* Change all '_' in variable name to '-' */
        for (end= *option ; *(end= strrchr(end,'_')) ; )
          *end= '-';
        switch (find_type(*option+2,&option_types,2)) {
        case 1:        /* port */
          if (opt_arg)
            options->port=atoi(opt_arg);
          break;
        case 2:        /* socket */
          if (opt_arg)
          {
            my_free(options->unix_socket,MYF(MY_ALLOW_ZERO_PTR));
            options->unix_socket=my_strdup(opt_arg,MYF(MY_WME));
          }
          break;
        case 3:        /* compress */
          options->compress=1;
          options->client_flag|= CLIENT_COMPRESS;
          break;
        case 4:        /* password */
          if (opt_arg)
          {
            my_free(options->password,MYF(MY_ALLOW_ZERO_PTR));
            options->password=my_strdup(opt_arg,MYF(MY_WME));
          }
          break;
        case 20:      /* connect_timeout */
        case 6:        /* timeout */
          if (opt_arg)
            options->connect_timeout=atoi(opt_arg);
          break;
        case 7:        /* user */
          if (opt_arg)
          {
            my_free(options->user,MYF(MY_ALLOW_ZERO_PTR));
            options->user=my_strdup(opt_arg,MYF(MY_WME));
          }
          break;
        case 8:        /* init-command */
          add_init_command(options,opt_arg);
          break;
        case 9:        /* host */
          if (opt_arg)
          {
            my_free(options->host,MYF(MY_ALLOW_ZERO_PTR));
            options->host=my_strdup(opt_arg,MYF(MY_WME));
          }
          break;
        case 10:      /* database */
          if (opt_arg)
          {
            my_free(options->db,MYF(MY_ALLOW_ZERO_PTR));
            options->db=my_strdup(opt_arg,MYF(MY_WME));
          }
          break;
        case 12:      /* return-found-rows */
          options->client_flag|=CLIENT_FOUND_ROWS;
          break;
        case 13:        /* Ignore SSL options */
        case 14:
        case 15:
        case 16:
        case 23:
          break;
        case 17:      /* charset-lib */
          my_free(options->charset_dir,MYF(MY_ALLOW_ZERO_PTR));
          options->charset_dir = my_strdup(opt_arg, MYF(MY_WME));
          break;
        case 18:
          my_free(options->charset_name,MYF(MY_ALLOW_ZERO_PTR));
          options->charset_name = my_strdup(opt_arg, MYF(MY_WME));
          break;
        case 19:        /* Interactive-timeout */
          options->client_flag|= CLIENT_INTERACTIVE;
          break;
        case 21:
          if (!opt_arg || atoi(opt_arg) != 0)
            options->client_flag|= CLIENT_LOCAL_FILES;
          else
            options->client_flag&= ~CLIENT_LOCAL_FILES;
          break;
        case 22:
          options->client_flag&= ~CLIENT_LOCAL_FILES;
          break;
        case 24: /* max-allowed-packet */
          if (opt_arg)
            options->max_allowed_packet= atoi(opt_arg);
          break;
        case 25: /* protocol */
          if ((options->protocol= find_type(opt_arg,
                                            &sql_protocol_typelib,0)) <= 0)
          {
            fprintf(stderr, "Unknown option to protocol: %s\n", opt_arg);
            exit(1);
          }
          break;
        case 27: /* multi-results */
          options->client_flag|= CLIENT_MULTI_RESULTS;
          break;
        case 28: /* multi-statements */
        case 29: /* multi-queries */
          options->client_flag|= CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS;
          break;
        case 30: /* secure-auth */
          options->secure_auth= true;
          break;
        case 31: /* report-data-truncation */
          options->report_data_truncation= opt_arg ? test(atoi(opt_arg)) : 1;
          break;
        default:
          break;
        }
      }
    }
  }
  free_defaults(argv);
  return;
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
      *prev_length= (ulong) (*column-start-1);
    start= *column;
    prev_length= to;
  }
}

/***************************************************************************
  Change field rows to field structs
***************************************************************************/

DRIZZLE_FIELD *
unpack_fields(DRIZZLE_DATA *data, MEM_ROOT *alloc,uint fields,
              bool default_value)
{
  DRIZZLE_ROWS  *row;
  DRIZZLE_FIELD  *field,*result;
  uint32_t lengths[9];        /* Max of fields */

  field= result= (DRIZZLE_FIELD*) alloc_root(alloc,
             (uint) sizeof(*field)*fields);
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
    field->catalog=   strmake_root(alloc,(char*) row->data[0], lengths[0]);
    field->db=        strmake_root(alloc,(char*) row->data[1], lengths[1]);
    field->table=     strmake_root(alloc,(char*) row->data[2], lengths[2]);
    field->org_table= strmake_root(alloc,(char*) row->data[3], lengths[3]);
    field->name=      strmake_root(alloc,(char*) row->data[4], lengths[4]);
    field->org_name=  strmake_root(alloc,(char*) row->data[5], lengths[5]);

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

    if (INTERNAL_NUM_FIELD(field))
      field->flags|= NUM_FLAG;
    if (default_value && row->data[7])
    {
      field->def=strmake_root(alloc,(char*) row->data[7], lengths[7]);
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

DRIZZLE_DATA *cli_read_rows(DRIZZLE *drizzle,DRIZZLE_FIELD *DRIZZLE_FIELDs,
        unsigned int fields)
{
  uint  field;
  ulong pkt_len;
  ulong len;
  uchar *cp;
  char  *to, *end_to;
  DRIZZLE_DATA *result;
  DRIZZLE_ROWS **prev_ptr,*cur;
  NET *net = &drizzle->net;

  if ((pkt_len= cli_safe_read(drizzle)) == packet_error)
    return(0);
  if (!(result=(DRIZZLE_DATA*) my_malloc(sizeof(DRIZZLE_DATA),
               MYF(MY_WME | MY_ZEROFILL))))
  {
    set_drizzle_error(drizzle, CR_OUT_OF_MEMORY, unknown_sqlstate);
    return(0);
  }
  init_alloc_root(&result->alloc,8192,0);  /* Assume rowlength < 8192 */
  result->alloc.min_malloc=sizeof(DRIZZLE_ROWS);
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
    if (!(cur= (DRIZZLE_ROWS*) alloc_root(&result->alloc,
          sizeof(DRIZZLE_ROWS))) ||
  !(cur->data= ((DRIZZLE_ROW)
          alloc_root(&result->alloc,
         (fields+1)*sizeof(char *)+pkt_len))))
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
      if ((len=(ulong) net_field_length(&cp)) == NULL_LENGTH)
      {            /* null field */
  cur->data[field] = 0;
      }
      else
      {
  cur->data[field] = to;
        if (len > (ulong) (end_to - to))
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
  ulong pkt_len,len;
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
    if ((len=(ulong) net_field_length(&pos)) == NULL_LENGTH)
    {            /* null field */
      row[field] = 0;
      *lengths++=0;
    }
    else
    {
      if (len > (ulong) (end_pos - pos))
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
    org_my_init_done=my_init_done;

    /* Will init threads */
    if (my_init())
      return NULL;

    init_client_errs();

    if (!drizzle_port)
    {
      drizzle_port = MYSQL_PORT;
      {
        struct servent *serv_ptr;
        char *env;

        /*
          if builder specifically requested a default port, use that
          (even if it coincides with our factory default).
          only if they didn't do we check /etc/services (and, failing
          on that, fall back to the factory default of 4427).
          either default can be overridden by the environment variable
          MYSQL_TCP_PORT, which in turn can be overridden with command
          line options.
        */

#if MYSQL_PORT_DEFAULT == 0
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
  else
    /* Init if new thread */
    if (my_thread_init())
      return NULL;

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
  ptr->charset=default_client_charset_info;
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


void STDCALL drizzle_server_end()
{
  if (!drizzle_client_init)
    return;

  finish_client_errs();
  vio_end();

  /* If library called my_init(), free memory allocated by it */
  if (!org_my_init_done)
  {
    my_end(0);
  }
  else
  {
    free_charsets();
    drizzle_thread_end();
  }

  drizzle_client_init= org_my_init_done= 0;
#ifdef EMBEDDED_SERVER
  if (stderror_file)
  {
    fclose(stderror_file);
    stderror_file= 0;
  }
#endif
}


/*
  Fill in SSL part of DRIZZLE structure and set 'use_ssl' flag.
  NB! Errors are not reported until you do drizzle_connect.
*/

#define strdup_if_not_null(A) (A) == 0 ? 0 : my_strdup((A),MYF(MY_WME))

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
  cli_flush_use_result,                         /* flush_use_result */
#ifndef MYSQL_SERVER
  cli_list_fields,                            /* list_fields */
  cli_unbuffered_fetch,                        /* unbuffered_fetch */
  cli_read_statistics,                         /* read_statistics */
  cli_read_query_result,                       /* next_result */
  cli_read_change_user_result,                 /* read_change_user_result */
#else
  0,0,0,0,0
#endif
};

C_MODE_START
int drizzle_init_character_set(DRIZZLE *drizzle)
{
  const char *default_collation_name;
 
  /* Set character set */
  if (!drizzle->options.charset_name)
  {
    default_collation_name= MYSQL_DEFAULT_COLLATION_NAME;
    if (!(drizzle->options.charset_name=
       my_strdup(MYSQL_DEFAULT_CHARSET_NAME,MYF(MY_WME))))
    return 1;
  }
  else
    default_collation_name= NULL;
 
  {
    const char *save= charsets_dir;
    if (drizzle->options.charset_dir)
      charsets_dir=drizzle->options.charset_dir;
    drizzle->charset=get_charset_by_csname(drizzle->options.charset_name,
                                         MY_CS_PRIMARY, MYF(MY_WME));
    if (drizzle->charset && default_collation_name)
    {
      const CHARSET_INFO *collation;
      if ((collation=
           get_charset_by_name(default_collation_name, MYF(MY_WME))))
      {
        if (!my_charset_same(drizzle->charset, collation))
        {
          my_printf_error(ER_UNKNOWN_ERROR,
                         "COLLATION %s is not valid for CHARACTER SET %s",
                         MYF(0),
                         default_collation_name, drizzle->options.charset_name);
          drizzle->charset= NULL;
        }
        else
        {
          drizzle->charset= collation;
        }
      }
      else
        drizzle->charset= NULL;
    }
    charsets_dir= save;
  }

  if (!drizzle->charset)
  {
    if (drizzle->options.charset_dir)
      set_drizzle_extended_error(drizzle, CR_CANT_READ_CHARSET, unknown_sqlstate,
                               ER(CR_CANT_READ_CHARSET),
                               drizzle->options.charset_name,
                               drizzle->options.charset_dir);
    else
    {
      char cs_dir_name[FN_REFLEN];
      get_charsets_dir(cs_dir_name);
      set_drizzle_extended_error(drizzle, CR_CANT_READ_CHARSET, unknown_sqlstate,
                               ER(CR_CANT_READ_CHARSET),
                               drizzle->options.charset_name,
                               cs_dir_name);
    }
    return 1;
  }
  return 0;
}
C_MODE_END


DRIZZLE * STDCALL
CLI_DRIZZLE_CONNECT(DRIZZLE *drizzle,const char *host, const char *user,
                       const char *passwd, const char *db,
                       uint32_t port, const char *unix_socket, uint32_t client_flag)
{
  char          buff[NAME_LEN+USERNAME_LENGTH+100];
  char          *end,*host_info=NULL;
  uint32_t         pkt_length;
  NET           *net= &drizzle->net;
  init_sigpipe_variables

  /* Don't give sigpipe errors if the client doesn't want them */
  set_sigpipe(drizzle);
  drizzle->methods= &client_methods;
  net->vio = 0;        /* If something goes wrong */
  drizzle->client_flag=0;      /* For handshake */

  /* use default options */
  if (drizzle->options.my_cnf_file || drizzle->options.my_cnf_group)
  {
    drizzle_read_default_options(&drizzle->options,
             (drizzle->options.my_cnf_file ?
        drizzle->options.my_cnf_file : "my"),
             drizzle->options.my_cnf_group);
    my_free(drizzle->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
    my_free(drizzle->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
    drizzle->options.my_cnf_file=drizzle->options.my_cnf_group=0;
  }

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
  if (!unix_socket)
    unix_socket=drizzle->options.unix_socket;

  drizzle->server_status=SERVER_STATUS_AUTOCOMMIT;

  /*
    Part 0: Grab a socket and connect it to the server
  */
  if (!net->vio &&
      (!drizzle->options.protocol ||
       drizzle->options.protocol == DRIZZLE_PROTOCOL_TCP))
  {
    struct addrinfo *res_lst, hints, *t_res;
    int gai_errno;
    char port_buf[NI_MAXSERV];

    unix_socket=0;        /* This is not used */

    if (!port)
      port= drizzle_port;

    if (!host)
      host= LOCAL_HOST;

    snprintf(host_info=buff, sizeof(buff)-1, ER(CR_TCP_CONNECTION), host);

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype= SOCK_STREAM;
    /* OSX 10.5 was failing unless defined only as support IPV4 */
#ifdef TARGET_OS_OSX
    hints.ai_family= AF_INET;
#endif


    snprintf(port_buf, NI_MAXSERV, "%d", port);
    gai_errno= getaddrinfo(host, port_buf, &hints, &res_lst);

    if (gai_errno != 0)
    {
      set_drizzle_extended_error(drizzle, CR_UNKNOWN_HOST, unknown_sqlstate,
                               ER(CR_UNKNOWN_HOST), host, errno);

      goto error;
    }

    /* We only look at the first item (something to think about changing in the future) */
    t_res= res_lst;
    {
      int sock= socket(t_res->ai_family, t_res->ai_socktype,
                             t_res->ai_protocol);
      if (sock == SOCKET_ERROR)
      {
        set_drizzle_extended_error(drizzle, CR_IPSOCK_ERROR, unknown_sqlstate,
                                 ER(CR_IPSOCK_ERROR), socket_errno);
        freeaddrinfo(res_lst);
        goto error;
      }

      net->vio= vio_new(sock, VIO_TYPE_TCPIP, VIO_BUFFERED_READ);
      if (! net->vio )
      {
        set_drizzle_error(drizzle, CR_CONN_UNKNOW_PROTOCOL, unknown_sqlstate);
        closesocket(sock);
        freeaddrinfo(res_lst);
        goto error;
      }

      if (connect_with_timeout(sock, t_res->ai_addr, t_res->ai_addrlen, drizzle->options.connect_timeout))
      {
        set_drizzle_extended_error(drizzle, CR_CONN_HOST_ERROR, unknown_sqlstate,
                                 ER(CR_CONN_HOST_ERROR), host, socket_errno);
        vio_delete(net->vio);
        net->vio= 0;
        freeaddrinfo(res_lst);
        goto error;
      }
    }

    freeaddrinfo(res_lst);
  }

  if (!net->vio)
  {
    set_drizzle_error(drizzle, CR_CONN_UNKNOW_PROTOCOL, unknown_sqlstate);
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
                             ER(CR_SERVER_LOST_EXTENDED),
                             "waiting for initial communication packet",
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
                               ER(CR_SERVER_LOST_EXTENDED),
                               "reading initial communication packet",
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
  end=strend((char*) net->read_pos+1);
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

  if (drizzle_init_character_set(drizzle))
    goto error;

  /* Save connection information */
  if (!my_multi_malloc(MYF(0),
           &drizzle->host_info, (uint) strlen(host_info)+1,
           &drizzle->host,      (uint) strlen(host)+1,
           &drizzle->unix_socket,unix_socket ?
           (uint) strlen(unix_socket)+1 : (uint) 1,
           &drizzle->server_version,
           (uint) (end - (char*) net->read_pos),
           NullS) ||
      !(drizzle->user=my_strdup(user,MYF(0))) ||
      !(drizzle->passwd=my_strdup(passwd,MYF(0))))
  {
    set_drizzle_error(drizzle, CR_OUT_OF_MEMORY, unknown_sqlstate);
    goto error;
  }
  strcpy(drizzle->host_info,host_info);
  strcpy(drizzle->host,host);
  if (unix_socket)
    strcpy(drizzle->unix_socket,unix_socket);
  else
    drizzle->unix_socket=0;
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
  buff[8]= (char) drizzle->charset->number;
  memset(buff+9, 0, 32-9);
  end= buff+32;

  drizzle->client_flag=client_flag;

  /* This needs to be changed as it's not useful with big packets */
  if (user && user[0])
    strncpy(end,user,USERNAME_LENGTH);          /* Max user name */
  else
    read_user_name((char*) end);

  /* We have to handle different version of handshake here */
  end= strend(end) + 1;
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
    drizzle->db= my_strdup(db,MYF(MY_WME));
    db= 0;
  }
  /* Write authentication package */
  if (my_net_write(net, (uchar*) buff, (size_t) (end-buff)) || net_flush(net))
  {
    set_drizzle_extended_error(drizzle, CR_SERVER_LOST, unknown_sqlstate,
                             ER(CR_SERVER_LOST_EXTENDED),
                             "sending authentication information",
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
                               ER(CR_SERVER_LOST_EXTENDED),
                               "reading authorization packet",
                               errno);
    goto error;
  }

  if (client_flag & CLIENT_COMPRESS)    /* We will use compression */
    net->compress=1;


  if (db && drizzle_select_db(drizzle, db))
  {
    if (drizzle->net.last_errno == CR_SERVER_LOST)
        set_drizzle_extended_error(drizzle, CR_SERVER_LOST, unknown_sqlstate,
                                 ER(CR_SERVER_LOST_EXTENDED),
                                 "Setting intital database",
                                 errno);
    goto error;
  }

  if (drizzle->options.init_commands)
  {
    DYNAMIC_ARRAY *init_commands= drizzle->options.init_commands;
    char **ptr= (char**)init_commands->buffer;
    char **end_command= ptr + init_commands->elements;

    bool reconnect=drizzle->reconnect;
    drizzle->reconnect=0;

    for (; ptr < end_command; ptr++)
    {
      DRIZZLE_RES *res;
      if (drizzle_real_query(drizzle,*ptr, (ulong) strlen(*ptr)))
  goto error;
      if (drizzle->fields)
      {
  if (!(res= cli_use_result(drizzle)))
    goto error;
  drizzle_free_result(res);
      }
    }
    drizzle->reconnect=reconnect;
  }

  reset_sigpipe(drizzle);
  return(drizzle);

error:
  reset_sigpipe(drizzle);
  {
    /* Free alloced memory */
    end_server(drizzle);
    drizzle_close_free(drizzle);
    if (!(((ulong) client_flag) & CLIENT_REMEMBER_OPTIONS))
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
        drizzle->db, drizzle->port, drizzle->unix_socket,
        drizzle->client_flag | CLIENT_REMEMBER_OPTIONS))
  {
    drizzle->net.last_errno= tmp_drizzle.net.last_errno;
    strcpy(drizzle->net.last_error, tmp_drizzle.net.last_error);
    strcpy(drizzle->net.sqlstate, tmp_drizzle.net.sqlstate);
    return(1);
  }
  if (drizzle_set_character_set(&tmp_drizzle, drizzle->charset->csname))
  {
    memset(&tmp_drizzle.options, 0, sizeof(tmp_drizzle.options));
    drizzle_close(&tmp_drizzle);
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

int STDCALL
drizzle_select_db(DRIZZLE *drizzle, const char *db)
{
  int error;

  if ((error=simple_command(drizzle,COM_INIT_DB, (const uchar*) db,
                            (ulong) strlen(db),0)))
    return(error);
  my_free(drizzle->db,MYF(MY_ALLOW_ZERO_PTR));
  drizzle->db=my_strdup(db,MYF(MY_WME));
  return(0);
}


/*************************************************************************
  Send a QUIT to the server and close the connection
  If handle is alloced by DRIZZLE connect free it.
*************************************************************************/

static void drizzle_close_free_options(DRIZZLE *drizzle)
{
  my_free(drizzle->options.user,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->options.host,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->options.password,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->options.unix_socket,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->options.db,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->options.charset_dir,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->options.charset_name,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->options.client_ip,MYF(MY_ALLOW_ZERO_PTR));
  if (drizzle->options.init_commands)
  {
    DYNAMIC_ARRAY *init_commands= drizzle->options.init_commands;
    char **ptr= (char**)init_commands->buffer;
    char **end= ptr + init_commands->elements;
    for (; ptr<end; ptr++)
      my_free(*ptr,MYF(MY_WME));
    delete_dynamic(init_commands);
    my_free((char*)init_commands,MYF(MY_WME));
  }
  memset(&drizzle->options, 0, sizeof(drizzle->options));
  return;
}


static void drizzle_close_free(DRIZZLE *drizzle)
{
  my_free((uchar*) drizzle->host_info,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->user,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->passwd,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->db,MYF(MY_ALLOW_ZERO_PTR));
  my_free(drizzle->info_buffer,MYF(MY_ALLOW_ZERO_PTR));
  drizzle->info_buffer= 0;

  /* Clear pointers for better safety */
  drizzle->host_info= drizzle->user= drizzle->passwd= drizzle->db= 0;
}


void STDCALL drizzle_close(DRIZZLE *drizzle)
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
      my_free((uchar*) drizzle,MYF(0));
  }
  return;
}


static bool cli_read_query_result(DRIZZLE *drizzle)
{
  uchar *pos;
  ulong field_count;
  DRIZZLE_DATA *fields;
  ulong length;

  if ((length = cli_safe_read(drizzle)) == packet_error)
    return(1);
  free_old_query(drizzle);    /* Free old result */
#ifdef MYSQL_CLIENT      /* Avoid warn of unused labels*/
get_info:
#endif
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
#ifdef MYSQL_CLIENT
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
#endif
  if (!(drizzle->server_status & SERVER_STATUS_AUTOCOMMIT))
    drizzle->server_status|= SERVER_STATUS_IN_TRANS;

  if (!(fields=cli_read_rows(drizzle,(DRIZZLE_FIELD*)0, 7)))
    return(1);
  if (!(drizzle->fields= unpack_fields(fields,&drizzle->field_alloc,
            (uint) field_count, 0)))
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

int32_t STDCALL
drizzle_send_query(DRIZZLE *drizzle, const char* query, uint32_t length)
{
  return(simple_command(drizzle, COM_QUERY, (uchar*) query, length, 1));
}


int32_t STDCALL
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

DRIZZLE_RES * STDCALL drizzle_store_result(DRIZZLE *drizzle)
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
  if (!(result=(DRIZZLE_RES*) my_malloc((uint) (sizeof(DRIZZLE_RES)+
                sizeof(ulong) *
                drizzle->field_count),
              MYF(MY_WME | MY_ZEROFILL))))
  {
    set_drizzle_error(drizzle, CR_OUT_OF_MEMORY, unknown_sqlstate);
    return(0);
  }
  result->methods= drizzle->methods;
  result->eof= 1;        /* Marker for buffered */
  result->lengths= (uint32_t*) (result+1);
  if (!(result->data=
  (*drizzle->methods->read_rows)(drizzle,drizzle->fields,drizzle->field_count)))
  {
    my_free((uchar*) result,MYF(0));
    return(0);
  }
  drizzle->affected_rows= result->row_count= result->data->rows;
  result->data_cursor=  result->data->data;
  result->fields=  drizzle->fields;
  result->field_alloc=  drizzle->field_alloc;
  result->field_count=  drizzle->field_count;
  /* The rest of result members is zeroed in malloc */
  drizzle->fields=0;        /* fields is now in result */
  clear_alloc_root(&drizzle->field_alloc);
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
  if (!(result=(DRIZZLE_RES*) my_malloc(sizeof(*result)+
              sizeof(ulong)*drizzle->field_count,
              MYF(MY_WME | MY_ZEROFILL))))
    return(0);
  result->lengths=(uint32_t*) (result+1);
  result->methods= drizzle->methods;
  if (!(result->row=(DRIZZLE_ROW)
  my_malloc(sizeof(result->row[0])*(drizzle->field_count+1), MYF(MY_WME))))
  {          /* Ptrs: to one row */
    my_free((uchar*) result,MYF(0));
    return(0);
  }
  result->fields=  drizzle->fields;
  result->field_alloc=  drizzle->field_alloc;
  result->field_count=  drizzle->field_count;
  result->current_field=0;
  result->handle=  drizzle;
  result->current_row=  0;
  drizzle->fields=0;      /* fields is now in result */
  clear_alloc_root(&drizzle->field_alloc);
  drizzle->status=DRIZZLE_STATUS_USE_RESULT;
  drizzle->unbuffered_fetch_owner= &result->unbuffered_fetch_cancelled;
  return(result);      /* Data is read to be fetched */
}


/**************************************************************************
  Return next row of the query results
**************************************************************************/

DRIZZLE_ROW STDCALL
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

uint32_t * STDCALL
drizzle_fetch_lengths(DRIZZLE_RES *res)
{
  DRIZZLE_ROW column;

  if (!(column=res->current_row))
    return 0;          /* Something is wrong */
  if (res->data)
    (*res->methods->fetch_lengths)(res->lengths, column, res->field_count);
  return res->lengths;
}


int STDCALL
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
  case DRIZZLE_INIT_COMMAND:
    add_init_command(&drizzle->options,arg);
    break;
  case DRIZZLE_READ_DEFAULT_FILE:
    my_free(drizzle->options.my_cnf_file,MYF(MY_ALLOW_ZERO_PTR));
    drizzle->options.my_cnf_file=my_strdup(arg,MYF(MY_WME));
    break;
  case DRIZZLE_READ_DEFAULT_GROUP:
    my_free(drizzle->options.my_cnf_group,MYF(MY_ALLOW_ZERO_PTR));
    drizzle->options.my_cnf_group=my_strdup(arg,MYF(MY_WME));
    break;
  case DRIZZLE_SET_CHARSET_DIR:
    my_free(drizzle->options.charset_dir,MYF(MY_ALLOW_ZERO_PTR));
    drizzle->options.charset_dir=my_strdup(arg,MYF(MY_WME));
    break;
  case DRIZZLE_SET_CHARSET_NAME:
    my_free(drizzle->options.charset_name,MYF(MY_ALLOW_ZERO_PTR));
    drizzle->options.charset_name=my_strdup(arg,MYF(MY_WME));
    break;
  case DRIZZLE_OPT_PROTOCOL:
    drizzle->options.protocol= *(const uint*) arg;
    break;
  case DRIZZLE_OPT_USE_REMOTE_CONNECTION:
  case DRIZZLE_OPT_GUESS_CONNECTION:
    drizzle->options.methods_to_use= option;
    break;
  case DRIZZLE_SET_CLIENT_IP:
    drizzle->options.client_ip= my_strdup(arg, MYF(MY_WME));
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
uint64_t STDCALL drizzle_num_rows(const DRIZZLE_RES *res)
{
  return res->row_count;
}

unsigned int STDCALL drizzle_num_fields(const DRIZZLE_RES *res)
{
  return res->field_count;
}

uint STDCALL drizzle_errno(const DRIZZLE *drizzle)
{
  return drizzle ? drizzle->net.last_errno : drizzle_server_last_errno;
}


const char * STDCALL drizzle_error(const DRIZZLE *drizzle)
{
  return drizzle ? drizzle->net.last_error : drizzle_server_last_error;
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

uint32_t STDCALL
drizzle_get_server_version(const DRIZZLE *drizzle)
{
  uint major, minor, version;
  char *pos= drizzle->server_version, *end_pos;
  major=   (uint) strtoul(pos, &end_pos, 10);  pos=end_pos+1;
  minor=   (uint) strtoul(pos, &end_pos, 10);  pos=end_pos+1;
  version= (uint) strtoul(pos, &end_pos, 10);
  return (ulong) major*10000L+(ulong) (minor*100+version);
}


/*
   drizzle_set_character_set function sends SET NAMES cs_name to
   the server (which changes character_set_client, character_set_result
   and character_set_connection) and updates drizzle->charset so other
   functions like drizzle_real_escape will work correctly.
*/
int STDCALL drizzle_set_character_set(DRIZZLE *drizzle, const char *cs_name)
{
  const CHARSET_INFO *cs;
  const char *save_csdir= charsets_dir;

  if (drizzle->options.charset_dir)
    charsets_dir= drizzle->options.charset_dir;

  if (strlen(cs_name) < MY_CS_NAME_SIZE &&
      (cs= get_charset_by_csname(cs_name, MY_CS_PRIMARY, MYF(0))))
  {
    char buff[MY_CS_NAME_SIZE + 10];
    charsets_dir= save_csdir;
    sprintf(buff, "SET NAMES %s", cs_name);
    if (!drizzle_real_query(drizzle, buff, strlen(buff)))
    {
      drizzle->charset= cs;
    }
  }
  else
  {
    char cs_dir_name[FN_REFLEN];
    get_charsets_dir(cs_dir_name);
    set_drizzle_extended_error(drizzle, CR_CANT_READ_CHARSET, unknown_sqlstate,
                             ER(CR_CANT_READ_CHARSET), cs_name, cs_dir_name);
  }
  charsets_dir= save_csdir;
  return drizzle->net.last_errno;
}


