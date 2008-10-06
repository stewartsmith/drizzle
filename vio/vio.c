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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Note that we can't have assertion on file descriptors;  The reason for
  this is that during mysql shutdown, another thread can close a file
  we are working on.  In this case we should just return read errors from
  the file descriptior.
*/

#include "vio_priv.h"

/*
 * Helper to fill most of the Vio* with defaults.
 */

static void vio_init(Vio* vio, enum enum_vio_type type,
                     int sd, uint32_t flags)
{
#ifndef HAVE_VIO_READ_BUFF
  flags&= ~VIO_BUFFERED_READ;
#endif
  memset(vio, 0, sizeof(*vio));
  vio->type	= type;
  vio->sd	= sd;
  if ((flags & VIO_BUFFERED_READ) &&
      !(vio->read_buffer= (char*)my_malloc(VIO_READ_BUFFER_SIZE, MYF(MY_WME))))
    flags&= ~VIO_BUFFERED_READ;
  {
    vio->viodelete	=vio_delete;
    vio->vioerrno	=vio_errno;
    vio->read= (flags & VIO_BUFFERED_READ) ? vio_read_buff : vio_read;
    vio->write		=vio_write;
    vio->fastsend	=vio_fastsend;
    vio->viokeepalive	=vio_keepalive;
    vio->should_retry	=vio_should_retry;
    vio->was_interrupted=vio_was_interrupted;
    vio->vioclose	=vio_close;
    vio->peer_addr	=vio_peer_addr;
    vio->vioblocking	=vio_blocking;
    vio->is_blocking	=vio_is_blocking;
    vio->timeout	=vio_timeout;
  }
}


/* Reset initialized VIO to use with another transport type */

void vio_reset(Vio* vio, enum enum_vio_type type,
               int sd, uint flags)
{
  free(vio->read_buffer);
  vio_init(vio, type, sd, flags);
}


/* Open the socket or TCP/IP connection and read the fnctl() status */

Vio *vio_new(int sd, enum enum_vio_type type, uint flags)
{
  Vio *vio;

  if ((vio = (Vio*) my_malloc(sizeof(*vio),MYF(MY_WME))))
  {
    vio_init(vio, type, sd, flags);
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


void vio_delete(Vio* vio)
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
void vio_end(void)
{
}
