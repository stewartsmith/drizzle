/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#include "config.h"

#define DONT_MAP_VIO
#include "vio.h"

#include <fcntl.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>

using namespace std;

namespace drizzle_protocol
{

/*
 * Helper to fill most of the Vio* with defaults.
 */

static void drizzleclient_vio_init(Vio* vio, enum enum_vio_type type,
                     int sd, uint32_t flags)
{
  memset(vio, 0, sizeof(*vio));
  vio->type	= type;
  vio->sd	= sd;
  if ((flags & VIO_BUFFERED_READ) &&
      !(vio->read_buffer= (char*)malloc(VIO_READ_BUFFER_SIZE)))
    flags&= ~VIO_BUFFERED_READ;
  {
    vio->viodelete	=drizzleclient_vio_delete;
    vio->vioerrno	=drizzleclient_vio_errno;
    vio->read= (flags & VIO_BUFFERED_READ) ? drizzleclient_vio_read_buff : drizzleclient_vio_read;
    vio->write		=drizzleclient_vio_write;
    vio->fastsend	=drizzleclient_vio_fastsend;
    vio->viokeepalive	=drizzleclient_vio_keepalive;
    vio->should_retry	=drizzleclient_vio_should_retry;
    vio->was_interrupted=drizzleclient_vio_was_interrupted;
    vio->vioclose	=drizzleclient_vio_close;
    vio->peer_addr	=drizzleclient_vio_peer_addr;
    vio->vioblocking	=drizzleclient_vio_blocking;
    vio->is_blocking	=drizzleclient_vio_is_blocking;
    vio->timeout	=drizzleclient_vio_timeout;
  }
}


/* Reset initialized VIO to use with another transport type */

void drizzleclient_vio_reset(Vio* vio, enum enum_vio_type type,
               int sd, uint32_t flags)
{
  free(vio->read_buffer);
  drizzleclient_vio_init(vio, type, sd, flags);
}


/* Open the socket or TCP/IP connection and read the fnctl() status */

Vio *drizzleclient_vio_new(int sd, enum enum_vio_type type, uint32_t flags)
{
  Vio *vio = (Vio*) malloc(sizeof(Vio));

  if (vio != NULL)
  {
    drizzleclient_vio_init(vio, type, sd, flags);
    sprintf(vio->desc, "TCP/IP (%d)", vio->sd);
    /*
      We call fcntl() to set the flags and then immediately read them back
      to make sure that we and the system are in agreement on the state of
      things.

      An example of why we need to do this is FreeBSD (and apparently some
      other BSD-derived systems, like Mac OS X), where the system sometimes
      reports that the socket is set for non-blocking when it really will
      block.
    */
    fcntl(sd, F_SETFL, 0);
    vio->fcntl_mode= fcntl(sd, F_GETFL);
  }
  return vio;
}


void drizzleclient_vio_delete(Vio* vio)
{
  if (!vio)
    return; /* It must be safe to delete null pointers. */

  if (vio->type != VIO_CLOSED)
    vio->vioclose(vio);
  free((unsigned char*) vio->read_buffer);
  free((unsigned char*) vio);
}


/*
  Cleanup memory allocated by vio or the
  components below it when application finish

*/
void drizzleclient_vio_end(void)
{ }

} /* namespace drizzle_protocol */
