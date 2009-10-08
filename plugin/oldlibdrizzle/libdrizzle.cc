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

#if defined(HAVE_GETPWUID) && defined(NO_GETPWUID_DECL)
struct passwd *getpwuid(uid_t);
char* getlogin(void);
#endif

/*************************************************************************
  put the row or field cursor one a position one got from DRIZZLE_ROW_tell()
  This doesn't restore any data. The next drizzleclient_fetch_row or
  drizzleclient_fetch_field will return the next row or field after the last used
*************************************************************************/

DRIZZLE_FIELD *drizzleclient_cli_list_fields(DRIZZLE *drizzle)
{
  DRIZZLE_DATA *query;
  if (!(query= drizzleclient_cli_read_rows(drizzle,(DRIZZLE_FIELD*) 0, 8)))
    return NULL;

  drizzle->field_count= (uint32_t) query->rows;
  return drizzleclient_unpack_fields(query, drizzle->field_count, 1);
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
 Multi query execution + SPs APIs
*********************************************************************/

DRIZZLE_RES * drizzleclient_use_result(DRIZZLE *drizzle)
{
  return (*drizzle->methods->use_result)(drizzle);
}

