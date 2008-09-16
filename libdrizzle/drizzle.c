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

#include <config.h>

#include <libdrizzle/libdrizzle.h>
#include <libdrizzle/errmsg.h>
#include <libdrizzle/drizzle.h>
#include <libdrizzle/gettext.h>
#include "libdrizzle_priv.h"

#include <vio/violite.h>

#include <drizzled/version.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <netdb.h>
#include <assert.h>

#define CONNECT_TIMEOUT 0

const char  *unknown_sqlstate= "HY000";
const char  *not_error_sqlstate= "00000";
const char  *cant_connect_sqlstate= "08001";

static bool drizzle_client_init= false;
unsigned int drizzle_server_last_errno;

/* Server error code and message */
char drizzle_server_last_error[DRIZZLE_ERRMSG_SIZE];

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
          drizzle_set_default_port((uint) ntohs((ushort) serv_ptr->s_port));
#endif
        if ((env = getenv("DRIZZLE_TCP_PORT")))
          drizzle_set_default_port((uint) atoi(env));
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
      drizzle_set_error(NULL, CR_OUT_OF_MEMORY, unknown_sqlstate);
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
   @param sqlstate  SQL standard sql state, unknown_sqlstate for the
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
