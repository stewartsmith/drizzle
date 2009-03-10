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
#include "libdrizzle_priv.h"

#include "libdrizzle.h"
#include "errmsg.h"
#include "pack.h"

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
#include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_POLL
#include <sys/poll.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifndef INADDR_NONE
#define INADDR_NONE  -1
#endif

#include <stdlib.h>
#include <string.h>
#include <mystrings/utf8.h>

uint32_t net_buffer_length= 8192;
uint32_t max_allowed_packet= 1024L*1024L*1024L;

unsigned int drizzle_port=0;

#include <errno.h>


static DRIZZLE_PARAMETERS drizzle_internal_parameters=
{&max_allowed_packet, &net_buffer_length, 0};

const DRIZZLE_PARAMETERS * drizzleclient_get_parameters(void)
{
  return &drizzle_internal_parameters;
}

unsigned int drizzleclient_get_default_port(void)
{
  return drizzle_port;
}

void drizzleclient_set_default_port(unsigned int port)
{
  drizzle_port= port;
}

/*
  Expand wildcard to a sql string
*/

static void
append_wild(char *to, char *end, const char *wild)
{
  end-=5;          /* Some extra */
  if (wild && wild[0])
  {
    to= strcpy(to," like '");
    to+= 7; /* strlen(" like '"); */

    while (*wild && to < end)
    {
      if (*wild == '\\' || *wild == '\'')
  *to++='\\';
      *to++= *wild++;
    }
    if (*wild)          /* Too small buffer */
      *to++='%';        /* Nicer this way */
    to[0]='\'';
    to[1]=0;
  }
}

/**************************************************************************
  Change user and database
**************************************************************************/

int drizzleclient_cli_read_change_user_result(DRIZZLE *drizzle)
{
  uint32_t pkt_length;

  pkt_length= drizzleclient_cli_safe_read(drizzle);

  if (pkt_length == packet_error)
    return 1;

  return 0;
}

bool drizzleclient_change_user(DRIZZLE *drizzle, const char *user,
                                 const char *passwd, const char *db)
{
  char buff[USERNAME_LENGTH+SCRAMBLED_PASSWORD_CHAR_LENGTH+NAME_LEN+2];
  char *end= buff;
  int rc;

  /* Use an empty string instead of NULL. */

  if (!user)
    user="";
  if (!passwd)
    passwd="";

  /* Store user into the buffer */
  end= strncpy(end, user, USERNAME_LENGTH) + USERNAME_LENGTH + 1;

  /* write scrambled password according to server capabilities */
  if (passwd[0])
  {
    {
      *end++= SCRAMBLE_LENGTH;
      end+= SCRAMBLE_LENGTH;
    }
  }
  else
    *end++= '\0';                               /* empty password */
  /* Add database if needed */
  end= strncpy(end, db ? db : "", NAME_LEN) + NAME_LEN + 1;

  /* Add character set number. */
  if (drizzle->server_capabilities & CLIENT_SECURE_CONNECTION)
  {
    int2store(end, (uint16_t) 45); // utf8mb4 number from mystrings/ctype-utf8.c
    end+= 2;
  }

  /* Write authentication package */
  (void)simple_command(drizzle,COM_CHANGE_USER, (unsigned char*) buff, (uint32_t) (end-buff), 1);

  rc= (*drizzle->methods->read_change_user_result)(drizzle);

  if (rc == 0)
  {
    /* Free old connect information */
    if(drizzle->user)
      free(drizzle->user);
    if(drizzle->passwd)
      free(drizzle->passwd);
    if(drizzle->db)
      free(drizzle->db);

    /* alloc new connect information */
    drizzle->user= strdup(user);
    drizzle->passwd= strdup(passwd);
    drizzle->db= db ? strdup(db) : 0;
  }

  return(rc);
}

#if defined(HAVE_GETPWUID) && defined(NO_GETPWUID_DECL)
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif

/**************************************************************************
  Do a query. If query returned rows, free old rows.
  Read data by drizzleclient_store_result or by repeat call of drizzleclient_fetch_row
**************************************************************************/

int
drizzleclient_query(DRIZZLE *drizzle, const char *query)
{
  return drizzleclient_real_query(drizzle,query, (uint32_t) strlen(query));
}


/**************************************************************************
  Return next field of the query results
**************************************************************************/

DRIZZLE_FIELD *
drizzleclient_fetch_field(DRIZZLE_RES *result)
{
  if (result->current_field >= result->field_count)
    return(NULL);
  return &result->fields[result->current_field++];
}


/**************************************************************************
  Move to a specific row and column
**************************************************************************/

void
drizzleclient_data_seek(DRIZZLE_RES *result, uint64_t row)
{
  DRIZZLE_ROWS  *tmp=0;
  if (result->data)
    for (tmp=result->data->data; row-- && tmp ; tmp = tmp->next) ;
  result->current_row=0;
  result->data_cursor = tmp;
}


/*************************************************************************
  put the row or field cursor one a position one got from DRIZZLE_ROW_tell()
  This doesn't restore any data. The next drizzleclient_fetch_row or
  drizzleclient_fetch_field will return the next row or field after the last used
*************************************************************************/

DRIZZLE_ROW_OFFSET
drizzleclient_row_seek(DRIZZLE_RES *result, DRIZZLE_ROW_OFFSET row)
{
  DRIZZLE_ROW_OFFSET return_value=result->data_cursor;
  result->current_row= 0;
  result->data_cursor= row;
  return return_value;
}


DRIZZLE_FIELD_OFFSET
drizzleclient_field_seek(DRIZZLE_RES *result, DRIZZLE_FIELD_OFFSET field_offset)
{
  DRIZZLE_FIELD_OFFSET return_value=result->current_field;
  result->current_field=field_offset;
  return return_value;
}


/*****************************************************************************
  List all tables in a database
  If wild is given then only the tables matching wild is returned
*****************************************************************************/

DRIZZLE_RES *
drizzleclient_list_tables(DRIZZLE *drizzle, const char *wild)
{
  char buff[255];
  char *ptr= strcpy(buff, "show tables");
  ptr+= 11; /* strlen("show tables"); */

  append_wild(ptr,buff+sizeof(buff),wild);
  if (drizzleclient_query(drizzle,buff))
    return(0);
  return (drizzleclient_store_result(drizzle));
}


DRIZZLE_FIELD *drizzleclient_cli_list_fields(DRIZZLE *drizzle)
{
  DRIZZLE_DATA *query;
  if (!(query= drizzleclient_cli_read_rows(drizzle,(DRIZZLE_FIELD*) 0, 8)))
    return NULL;

  drizzle->field_count= (uint32_t) query->rows;
  return drizzleclient_unpack_fields(query, drizzle->field_count, 1);
}


/**************************************************************************
  List all fields in a table
  If wild is given then only the fields matching wild is returned
  Instead of this use query:
  show fields in 'table' like "wild"
**************************************************************************/

DRIZZLE_RES *
drizzleclient_list_fields(DRIZZLE *drizzle, const char *table, const char *wild)
{
  DRIZZLE_RES   *result;
  DRIZZLE_FIELD *fields;
  char buff[257], *end;

  end= strncpy(buff, table, 128) + 128;
  end= strncpy(end+1, wild ? wild : "", 128) + 128;

  drizzleclient_free_old_query(drizzle);
  if (simple_command(drizzle, COM_FIELD_LIST, (unsigned char*) buff,
                     (uint32_t) (end-buff), 1) ||
      !(fields= (*drizzle->methods->list_fields)(drizzle)))
    return(NULL);

  if (!(result = (DRIZZLE_RES *) malloc(sizeof(DRIZZLE_RES))))
    return(NULL);

  memset(result, 0, sizeof(DRIZZLE_RES));

  result->methods= drizzle->methods;
  drizzle->fields=0;
  result->field_count = drizzle->field_count;
  result->fields= fields;
  result->eof=1;
  return(result);
}

int
drizzleclient_shutdown(DRIZZLE *drizzle)
{
  return(simple_command(drizzle, COM_SHUTDOWN, 0, 0, 0));
}


int
drizzleclient_refresh(DRIZZLE *drizzle, uint32_t options)
{
  unsigned char bits[1];
  bits[0]= (unsigned char) options;
  return(simple_command(drizzle, COM_REFRESH, bits, 1, 0));
}


const char *drizzleclient_cli_read_statistics(DRIZZLE *drizzle)
{
  drizzle->net.read_pos[drizzle->packet_length]=0;  /* End of stat string */
  if (!drizzle->net.read_pos[0])
  {
    drizzleclient_set_error(drizzle, CR_WRONG_HOST_INFO, drizzleclient_sqlstate_get_unknown());
    return drizzle->net.last_error;
  }
  return (char*) drizzle->net.read_pos;
}


int
drizzleclient_ping(DRIZZLE *drizzle)
{
  int res;
  res= simple_command(drizzle,COM_PING,0,0,0);
  if (res == CR_SERVER_LOST && drizzle->reconnect)
    res= simple_command(drizzle,COM_PING,0,0,0);
  return(res);
}


const char *
drizzleclient_get_server_info(const DRIZZLE *drizzle)
{
  return((char*) drizzle->server_version);
}


const char *
drizzleclient_get_host_info(const DRIZZLE *drizzle)
{
  return(drizzle->host_info);
}


uint32_t
drizzleclient_get_proto_info(const DRIZZLE *drizzle)
{
  return (drizzle->protocol_version);
}

const char *
drizzleclient_get_client_info(void)
{
  return (char*) VERSION;
}

uint32_t drizzleclient_get_client_version(void)
{
  return DRIZZLE_VERSION_ID;
}

bool drizzleclient_eof(const DRIZZLE_RES *res)
{
  return res->eof;
}

const DRIZZLE_FIELD * drizzleclient_fetch_field_direct(const DRIZZLE_RES *res, unsigned int fieldnr)
{
  return &(res)->fields[fieldnr];
}

const DRIZZLE_FIELD * drizzleclient_fetch_fields(const DRIZZLE_RES *res)
{
  return res->fields;
}

DRIZZLE_ROW_OFFSET drizzleclient_row_tell(const DRIZZLE_RES *res)
{
  return res->data_cursor;
}

DRIZZLE_FIELD_OFFSET drizzleclient_field_tell(const DRIZZLE_RES *res)
{
  return res->current_field;
}

/* DRIZZLE */

unsigned int drizzleclient_field_count(const DRIZZLE *drizzle)
{
  return drizzle->field_count;
}

uint64_t drizzleclient_affected_rows(const DRIZZLE *drizzle)
{
  return drizzle->affected_rows;
}

uint64_t drizzleclient_insert_id(const DRIZZLE *drizzle)
{
  return drizzle->insert_id;
}

const char * drizzleclient_sqlstate(const DRIZZLE *drizzle)
{
  return drizzle ? drizzle->net.sqlstate : drizzleclient_sqlstate_get_cant_connect();
}

uint32_t drizzleclient_warning_count(const DRIZZLE *drizzle)
{
  return drizzle->warning_count;
}

const char * drizzleclient_info(const DRIZZLE *drizzle)
{
  return drizzle->info;
}

uint32_t drizzleclient_thread_id(const DRIZZLE *drizzle)
{
  return drizzle->thread_id;
}

/****************************************************************************
  Some support functions
****************************************************************************/

/*
  Functions called my drizzleclient_net_init() to set some application specific variables
*/

void drizzleclient_net_local_init(NET *net)
{
  net->max_packet=   (uint32_t) net_buffer_length;
  drizzleclient_net_set_read_timeout(net, CLIENT_NET_READ_TIMEOUT);
  drizzleclient_net_set_write_timeout(net, CLIENT_NET_WRITE_TIMEOUT);
  net->retry_count=  1;
  net->max_packet_size= (net_buffer_length > max_allowed_packet) ?
    net_buffer_length : max_allowed_packet;
}

/*
  Add escape characters to a string (blob?) to make it suitable for a insert
  to should at least have place for length*2+1 chars
  Returns the length of the to string
*/

uint32_t
drizzleclient_escape_string(char *to,const char *from, uint32_t length)
{
  const char *to_start= to;
  const char *end, *to_end=to_start + 2*length;
  bool overflow= false;
  for (end= from + length; from < end; from++)
  {
    uint32_t tmp_length;
    char escape= 0;
    if (!U8_IS_SINGLE(*from))
    {
      tmp_length= U8_LENGTH(*(uint32_t*)from);
      if (to + tmp_length > to_end)
      {
        overflow= true;
        break;
      }
      while (tmp_length--)
        *to++= *from++;
      from--;
      continue;
    }
    switch (*from) {
    case 0:                             /* Must be escaped for 'mysql' */
      escape= '0';
      break;
    case '\n':                          /* Must be escaped for logs */
      escape= 'n';
      break;
    case '\r':
      escape= 'r';
      break;
    case '\\':
      escape= '\\';
      break;
    case '\'':
      escape= '\'';
      break;
    case '"':                           /* Better safe than sorry */
      escape= '"';
      break;
    case '\032':                        /* This gives problems on Win32 */
      escape= 'Z';
      break;
    }
    if (escape)
    {
      if (to + 2 > to_end)
      {
        overflow= true;
        break;
      }
      *to++= '\\';
      *to++= escape;
    }
    else
    {
      if (to + 1 > to_end)
      {
        overflow= true;
        break;
      }
      *to++= *from;
    }
  }
  *to= 0;
  return overflow ? (size_t) -1 : (size_t) (to - to_start);
}

int drizzleclient_cli_unbuffered_fetch(DRIZZLE *drizzle, char **row)
{
  if (packet_error == drizzleclient_cli_safe_read(drizzle))
    return 1;

  *row= ((drizzle->net.read_pos[0] == DRIZZLE_PROTOCOL_NO_MORE_DATA) ? NULL :
   (char*) (drizzle->net.read_pos+1));
  return 0;
}

/********************************************************************
 Transactional APIs
*********************************************************************/

/*
  Commit the current transaction
*/

bool drizzleclient_commit(DRIZZLE *drizzle)
{
  return((bool) drizzleclient_real_query(drizzle, "commit", 6));
}

/*
  Rollback the current transaction
*/

bool drizzleclient_rollback(DRIZZLE *drizzle)
{
  return((bool) drizzleclient_real_query(drizzle, "rollback", 8));
}


/*
  Set autocommit to either true or false
*/

bool drizzleclient_autocommit(DRIZZLE *drizzle, bool auto_mode)
{
  return((bool) drizzleclient_real_query(drizzle, auto_mode ?
                                         "set autocommit=1":"set autocommit=0",
                                         16));
}


/********************************************************************
 Multi query execution + SPs APIs
*********************************************************************/

/*
  Returns true/false to indicate whether any more query results exist
  to be read using drizzleclient_next_result()
*/

bool drizzleclient_more_results(const DRIZZLE *drizzle)
{
  return (drizzle->server_status & SERVER_MORE_RESULTS_EXISTS) ? true:false;
}


/*
  Reads and returns the next query results
*/
int drizzleclient_next_result(DRIZZLE *drizzle)
{
  if (drizzle->status != DRIZZLE_STATUS_READY)
  {
    drizzleclient_set_error(drizzle, CR_COMMANDS_OUT_OF_SYNC, drizzleclient_sqlstate_get_unknown());
    return(1);
  }

  drizzleclient_drizzleclient_net_clear_error(&drizzle->net);
  drizzle->affected_rows= ~(uint64_t) 0;

  if (drizzle->server_status & SERVER_MORE_RESULTS_EXISTS)
    return((*drizzle->methods->next_result)(drizzle));

  return(-1);        /* No more results */
}


DRIZZLE_RES * drizzleclient_use_result(DRIZZLE *drizzle)
{
  return (*drizzle->methods->use_result)(drizzle);
}

bool drizzleclient_read_query_result(DRIZZLE *drizzle)
{
  return (*drizzle->methods->read_query_result)(drizzle);
}

