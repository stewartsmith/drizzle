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

unsigned int drizzle_port=0;

#include <errno.h>

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

int
drizzleclient_shutdown(DRIZZLE *drizzle)
{
  return(simple_command(drizzle, COM_SHUTDOWN, 0, 0, 0));
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

DRIZZLE_ROW_OFFSET drizzleclient_row_tell(const DRIZZLE_RES *res)
{
  return res->data_cursor;
}

/* DRIZZLE */

uint64_t drizzleclient_insert_id(const DRIZZLE *drizzle)
{
  return drizzle->insert_id;
}

const char * drizzleclient_sqlstate(const DRIZZLE *drizzle)
{
  return drizzle ? drizzle->net.sqlstate : drizzleclient_sqlstate_get_cant_connect();
}

uint32_t drizzleclient_thread_id(const DRIZZLE *drizzle)
{
  return drizzle->thread_id;
}

/****************************************************************************
  Some support functions
****************************************************************************/

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
  Rollback the current transaction
*/

bool drizzleclient_rollback(DRIZZLE *drizzle)
{
  return((bool) drizzleclient_real_query(drizzle, "rollback", 8));
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

