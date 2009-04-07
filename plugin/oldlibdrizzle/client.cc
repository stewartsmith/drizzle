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
  This file is included by both libdrizzleclient.c (the DRIZZLE client C API)
  and the drizzled server to connect to another DRIZZLE server.

  The differences for the two cases are:

  - Things that only works for the client:
  - Trying to automaticly determinate user name if not supplied to
    drizzleclient_connect()
  - Support for reading local file with LOAD DATA LOCAL
  - SHARED memory handling
  - Prepared statements

  - Things that only works for the server
  - Alarm handling on connect

  In all other cases, the code should be idential for the client and
  server.
*/


#include "libdrizzle_priv.h"
#include "net_serv.h"
#include "pack.h"
#include "errmsg.h"
#include "drizzle_methods.h"

#include <stdarg.h>
#include <sys/poll.h>
#include <sys/ioctl.h>

#include <netdb.h>

/* Remove client convenience wrappers */
#undef max_allowed_packet
#undef net_buffer_length


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


#include <drizzled/gettext.h>

using namespace std;

#if defined(HAVE_GETPWUID) && defined(NO_GETPWUID_DECL)
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif


/*****************************************************************************
  Read a packet from server. Give error message if socket was down
  or packet is an error message
*****************************************************************************/

/* I'm not sure if this is even used anymore, but now that libdrizzleclient is
   server only, this is safe to set here. */
extern "C" {
  bool safe_read_error_impl(NET *net)
  {
    if (net->vio)
      return drizzleclient_vio_was_interrupted(net->vio);
    return false;
  }
}

safe_read_error_hook_func safe_read_error_hook= safe_read_error_impl;

uint32_t drizzleclient_cli_safe_read(DRIZZLE *drizzle)
{
  NET *net= &drizzle->net;
  uint32_t len=0;

  if (net->vio != 0)
    len=drizzleclient_net_read(net);

  if (len == packet_error || len == 0)
  {
    if (safe_read_error_hook != NULL)
      if (safe_read_error_hook(net))
        return (packet_error);
    drizzleclient_disconnect(drizzle);
    drizzleclient_set_error(drizzle, net->last_errno == CR_NET_PACKET_TOO_LARGE ?
                      CR_NET_PACKET_TOO_LARGE : CR_SERVER_LOST,
                      drizzleclient_sqlstate_get_unknown());
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
        strncpy(net->sqlstate, pos+1, LIBDRIZZLE_SQLSTATE_LENGTH);
        pos+= LIBDRIZZLE_SQLSTATE_LENGTH+1;
      }
      else
      {
        /*
          The SQL state hasn't been received -- it should be reset to HY000
          (unknown error sql state).
        */

        strcpy(net->sqlstate, drizzleclient_sqlstate_get_unknown());
      }

      strncpy(net->last_error,(char*) pos, min((uint32_t) len,
              (uint32_t) sizeof(net->last_error)-1));
    }
    else
      drizzleclient_set_error(drizzle, CR_UNKNOWN_ERROR, drizzleclient_sqlstate_get_unknown());
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

bool
drizzleclient_cli_advanced_command(DRIZZLE *drizzle, enum enum_server_command command,
         const unsigned char *header, uint32_t header_length,
         const unsigned char *arg, uint32_t arg_length, bool skip_check)
{
  NET *net= &drizzle->net;
  bool result= 1;
  bool stmt_skip= false;

  if (drizzle->net.vio == 0)
  {            /* Do reconnect if possible */
    if (drizzleclient_reconnect(drizzle) || stmt_skip)
      return(1);
  }
  if (drizzle->status != DRIZZLE_STATUS_READY ||
      drizzle->server_status & SERVER_MORE_RESULTS_EXISTS)
  {
    drizzleclient_set_error(drizzle, CR_COMMANDS_OUT_OF_SYNC,
                      drizzleclient_sqlstate_get_unknown());
    return(1);
  }

  drizzleclient_drizzleclient_net_clear_error(net);
  drizzle->info=0;
  drizzle->affected_rows= ~(uint64_t) 0;
  /*
    We don't want to clear the protocol buffer on COM_QUIT, because if
    the previous command was a shutdown command, we may have the
    response for the COM_QUIT already in the communication buffer
  */
  drizzleclient_net_clear(&drizzle->net, (command != COM_QUIT));

  if (drizzleclient_net_write_command(net,(unsigned char) command, header, header_length,
      arg, arg_length))
  {
    if (net->last_errno == CR_NET_PACKET_TOO_LARGE)
    {
      drizzleclient_set_error(drizzle, CR_NET_PACKET_TOO_LARGE, drizzleclient_sqlstate_get_unknown());
      goto end;
    }
    drizzleclient_disconnect(drizzle);
    if (drizzleclient_reconnect(drizzle) || stmt_skip)
      goto end;
    if (drizzleclient_net_write_command(net,(unsigned char) command, header, header_length,
        arg, arg_length))
    {
      drizzleclient_set_error(drizzle, CR_SERVER_GONE_ERROR, drizzleclient_sqlstate_get_unknown());
      goto end;
    }
  }
  result=0;
  if (!skip_check)
    result= ((drizzle->packet_length=drizzleclient_cli_safe_read(drizzle)) == packet_error ?
       1 : 0);
end:
  return(result);
}

void drizzleclient_free_old_query(DRIZZLE *drizzle)
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





void
drizzleclient_free_result(DRIZZLE_RES *result)
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
    drizzleclient_free_rows(result->data);
    /* TODO: free result->fields */
    if (result->row)
      free((unsigned char*) result->row);
    free((unsigned char*) result);
  }
}




/* Read all rows (fields or data) from server */

DRIZZLE_DATA *drizzleclient_cli_read_rows(DRIZZLE *drizzle, DRIZZLE_FIELD *DRIZZLE_FIELDs, uint32_t fields)
{
  uint32_t  field;
  uint32_t pkt_len;
  uint32_t len;
  unsigned char *cp;
  char  *to, *end_to;
  DRIZZLE_DATA *result;
  DRIZZLE_ROWS **prev_ptr,*cur;
  NET *net = &drizzle->net;

  if ((pkt_len= drizzleclient_cli_safe_read(drizzle)) == packet_error)
    return(0);
  if (!(result=(DRIZZLE_DATA*) malloc(sizeof(DRIZZLE_DATA))))
  {
    drizzleclient_set_error(drizzle, CR_OUT_OF_MEMORY,
                      drizzleclient_sqlstate_get_unknown());
    return(0);
  }
  memset(result, 0, sizeof(DRIZZLE_DATA));
  prev_ptr= &result->data;
  result->rows=0;
  result->fields=fields;

  /*
    The last EOF packet is either a 254 (0xFE) character followed by 1-7 status bytes.

    This doesn't conflict with normal usage of 254 which stands for a
    string where the length of the string is 8 bytes. (see drizzleclient_net_field_length())
  */

  while (*(cp=net->read_pos) != DRIZZLE_PROTOCOL_NO_MORE_DATA || pkt_len >= 8)
  {
    result->rows++;
    if (!(cur= (DRIZZLE_ROWS*) malloc(sizeof(DRIZZLE_ROWS))) ||
        !(cur->data= ((DRIZZLE_ROW) malloc((fields+1)*sizeof(char *)+pkt_len))))
    {
      drizzleclient_free_rows(result);
      drizzleclient_set_error(drizzle, CR_OUT_OF_MEMORY, drizzleclient_sqlstate_get_unknown());
      return(0);
    }
    *prev_ptr=cur;
    prev_ptr= &cur->next;
    to= (char*) (cur->data+fields+1);
    end_to=to+pkt_len-1;
    for (field=0 ; field < fields ; field++)
    {
      if ((len= drizzleclient_net_field_length(&cp)) == NULL_LENGTH)
      {            /* null field */
        cur->data[field] = 0;
      }
      else
      {
        cur->data[field] = to;
        if (len > (uint32_t) (end_to - to))
        {
          drizzleclient_free_rows(result);
          drizzleclient_set_error(drizzle, CR_MALFORMED_PACKET,
                            drizzleclient_sqlstate_get_unknown());
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
    if ((pkt_len=drizzleclient_cli_safe_read(drizzle)) == packet_error)
    {
      drizzleclient_free_rows(result);
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
  uint32_t field;
  uint32_t pkt_len,len;
  unsigned char *pos, *prev_pos, *end_pos;
  NET *net= &drizzle->net;

  if ((pkt_len=drizzleclient_cli_safe_read(drizzle)) == packet_error)
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
    if ((len= drizzleclient_net_field_length(&pos)) == NULL_LENGTH)
    {            /* null field */
      row[field] = 0;
      *lengths++=0;
    }
    else
    {
      if (len > (uint32_t) (end_pos - pos))
      {
        drizzleclient_set_error(drizzle, CR_UNKNOWN_ERROR,
                          drizzleclient_sqlstate_get_unknown());
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


/**************************************************************************
  Return next row of the query results
**************************************************************************/

DRIZZLE_ROW
drizzleclient_fetch_row(DRIZZLE_RES *res)
{
  if (!res->data)
  {            /* Unbufferred fetch */
    if (!res->eof)
    {
      DRIZZLE *drizzle= res->handle;
      if (drizzle->status != DRIZZLE_STATUS_USE_RESULT)
      {
        drizzleclient_set_error(drizzle,
                          res->unbuffered_fetch_cancelled ?
                          CR_FETCH_CANCELED : CR_COMMANDS_OUT_OF_SYNC,
                          drizzleclient_sqlstate_get_unknown());
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
      /* Don't clear handle in drizzleclient_free_result */
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
  If one uses drizzleclient_use_result, res->lengths contains the length information,
  else the lengths are calculated from the offset between pointers.
**************************************************************************/

uint32_t *
drizzleclient_fetch_lengths(DRIZZLE_RES *res)
{
  DRIZZLE_ROW column;

  if (!(column=res->current_row))
    return 0;          /* Something is wrong */
  if (res->data)
    (*res->methods->fetch_lengths)(res->lengths, column, res->field_count);
  return res->lengths;
}


int
drizzleclient_options(DRIZZLE *drizzle,enum drizzle_option option, const void *arg)
{
  switch (option) {
  case DRIZZLE_OPT_CONNECT_TIMEOUT:
    drizzle->options.connect_timeout= *(uint32_t*) arg;
    break;
  case DRIZZLE_OPT_READ_TIMEOUT:
    drizzle->options.read_timeout= *(uint32_t*) arg;
    break;
  case DRIZZLE_OPT_WRITE_TIMEOUT:
    drizzle->options.write_timeout= *(uint32_t*) arg;
    break;
  case DRIZZLE_OPT_COMPRESS:
    drizzle->options.compress= 1;      /* Remember for connect */
    drizzle->options.client_flag|= CLIENT_COMPRESS;
    break;
  case DRIZZLE_READ_DEFAULT_FILE:
    if (drizzle->options.my_cnf_file != NULL)
      free(drizzle->options.my_cnf_file);
    drizzle->options.my_cnf_file=strdup((char *)arg);
    break;
  case DRIZZLE_READ_DEFAULT_GROUP:
    if (drizzle->options.my_cnf_group != NULL)
      free(drizzle->options.my_cnf_group);
    drizzle->options.my_cnf_group=strdup((char *)arg);
    break;
  case DRIZZLE_OPT_PROTOCOL:
    break;
  case DRIZZLE_OPT_USE_REMOTE_CONNECTION:
  case DRIZZLE_OPT_GUESS_CONNECTION:
    drizzle->options.methods_to_use= option;
    break;
  case DRIZZLE_SET_CLIENT_IP:
    drizzle->options.client_ip= strdup((char *)arg);
    break;
  case DRIZZLE_SECURE_AUTH:
    drizzle->options.secure_auth= *(const bool *) arg;
    break;
  case DRIZZLE_REPORT_DATA_TRUNCATION:
    drizzle->options.report_data_truncation= (*(const bool *) arg) ? 1 : 0;
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
uint64_t drizzleclient_num_rows(const DRIZZLE_RES *res)
{
  return res->row_count;
}

unsigned int drizzleclient_num_fields(const DRIZZLE_RES *res)
{
  return res->field_count;
}


/*
  Get version number for server in a form easy to test on

  SYNOPSIS
    drizzleclient_get_server_version()
    drizzle Connection

  EXAMPLE
    4.1.0-alfa ->  40100

  NOTES
    We will ensure that a newer server always has a bigger number.

  RETURN
   Signed number > 323000
*/

uint32_t
drizzleclient_get_server_version(const DRIZZLE *drizzle)
{
  uint32_t major, minor, version;
  char *pos= drizzle->server_version, *end_pos;
  major=   (uint32_t) strtoul(pos, &end_pos, 10);  pos=end_pos+1;
  minor=   (uint32_t) strtoul(pos, &end_pos, 10);  pos=end_pos+1;
  version= (uint32_t) strtoul(pos, &end_pos, 10);
  return (uint32_t) major*10000L+(uint32_t) (minor*100+version);
}

