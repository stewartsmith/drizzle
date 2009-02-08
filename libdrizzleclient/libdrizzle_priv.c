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

/* Various helper functions not intended to be part of a public API */

#include <drizzled/global.h>
#include "libdrizzle_priv.h"
#include <poll.h>
#include <fcntl.h>

const char  *unknown_sqlstate= "HY000";
const char  *not_error_sqlstate= "00000";
const char  *cant_connect_sqlstate= "08001";

const char * drizzleclient_sqlstate_get_unknown(void)
{
  return unknown_sqlstate;
}

const char * drizzleclient_sqlstate_get_not_error(void)
{
  return not_error_sqlstate;
}

const char * drizzleclient_sqlstate_get_cant_connect(void)
{
  return cant_connect_sqlstate;
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
/****************************************************************************
  A modified version of connect().  drizzleclient_connect_with_timeout() allows you to specify
  a timeout value, in seconds, that we should wait until we
  derermine we can't connect to a particular host.  If timeout is 0,
  drizzleclient_connect_with_timeout() will behave exactly like connect().

  Base version coded by Steve Bernacki, Jr. <steve@navinet.net>
*****************************************************************************/

int drizzleclient_connect_with_timeout(int fd, const struct sockaddr *name, uint32_t namelen, int32_t timeout)
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

