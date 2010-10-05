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
#include "vio.h"
#include <string.h>
#include <drizzled/util/test.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <algorithm>
#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <fcntl.h>

using namespace std;

static void _vio_delete(Vio* vio)
{
  if (!vio)
    return; /* It must be safe to delete null pointers. */

  if (!vio->closed)
    vio->vioclose(vio);
  free((unsigned char*) vio);
}

static int _vio_errno(Vio *vio)
{
  (void)vio;
  return errno;
}

static size_t _vio_read(Vio * vio, unsigned char* buf, size_t size)
{
  size_t r;

  /* Ensure nobody uses vio_read_buff and vio_read simultaneously */
  assert(vio->read_end == vio->read_pos);
  r= read(vio->sd, buf, size);

  return r;
}

static size_t _vio_write(Vio * vio, const unsigned char* buf, size_t size)
{
  size_t r;

  r = write(vio->sd, buf, size);

  return r;
}

static int _vio_blocking(Vio * vio, bool set_blocking_mode, bool *old_mode)
{
  int r=0;

  *old_mode= drizzled::test(!(vio->fcntl_mode & O_NONBLOCK));

  if (vio->sd >= 0)
  {
    int old_fcntl=vio->fcntl_mode;
    if (set_blocking_mode)
      vio->fcntl_mode &= ~O_NONBLOCK; /* clear bit */
    else
      vio->fcntl_mode |= O_NONBLOCK; /* set bit */
    if (old_fcntl != vio->fcntl_mode)
    {
      r= fcntl(vio->sd, F_SETFL, vio->fcntl_mode);
      if (r == -1)
      {
        vio->fcntl_mode= old_fcntl;
      }
    }
  }

  return r;
}

static int _vio_fastsend(Vio * vio)
{
  (void)vio;
  int nodelay = 1;
  int error;

  error= setsockopt(vio->sd, IPPROTO_TCP, TCP_NODELAY,
                    &nodelay, sizeof(nodelay));
  if (error != 0)
  {
    perror("setsockopt");
    assert(error == 0);
  }

  return error;
}

static int32_t _vio_keepalive(Vio* vio, bool set_keep_alive)
{
  int r= 0;
  uint32_t opt= 0;

  if (set_keep_alive)
    opt= 1;

  r= setsockopt(vio->sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt, sizeof(opt));
  if (r != 0)
  {
    perror("setsockopt");
    assert(r == 0);
  }

  return r;
}

static bool _vio_should_retry(Vio * vio)
{
  (void)vio;
  int en = errno;
  return (en == EAGAIN || en == EINTR ||
	  en == EWOULDBLOCK);
}

static bool _vio_was_interrupted(Vio *vio)
{
  (void)vio;
  int en= errno;
  return (en == EAGAIN || en == EINTR ||
	  en == EWOULDBLOCK || en == ETIMEDOUT);
}

static int _vio_close(Vio * vio)
{
  int r=0;
 if (!vio->closed)
  {
    assert(vio->sd >= 0);
    if (shutdown(vio->sd, SHUT_RDWR))
      r= -1;
    if (close(vio->sd))
      r= -1;
  }
  vio->closed= true;
  vio->sd=   -1;

  return r;
}

static bool _vio_peer_addr(Vio *vio, char *buf, uint16_t *port, size_t buflen)
{
  int error;
  char port_buf[NI_MAXSERV];
  socklen_t addrLen = sizeof(vio->remote);

  if (getpeername(vio->sd, (struct sockaddr *) (&vio->remote),
                  &addrLen) != 0)
  {
    return true;
  }
  vio->addrLen= (int)addrLen;

  if ((error= getnameinfo((struct sockaddr *)(&vio->remote),
                          addrLen,
                          buf, buflen,
                          port_buf, NI_MAXSERV, NI_NUMERICHOST|NI_NUMERICSERV)))
  {
    return true;
  }

  *port= (uint16_t)strtol(port_buf, (char **)NULL, 10);

  return false;
}

static void _vio_timeout(Vio *vio, bool is_sndtimeo, int32_t timeout)
{
  int error;

  /* POSIX specifies time as struct timeval. */
  struct timeval wait_timeout;
  wait_timeout.tv_sec= timeout;
  wait_timeout.tv_usec= 0;

  assert(timeout >= 0 && timeout <= INT32_MAX);
  assert(vio->sd != -1);
  error= setsockopt(vio->sd, SOL_SOCKET, is_sndtimeo ? SO_SNDTIMEO : SO_RCVTIMEO,
                    &wait_timeout,
                    (socklen_t)sizeof(struct timeval));
  if (error == -1 && errno != ENOPROTOOPT)
  {
    perror("setsockopt");
    assert(error == 0);
  }
}

/* Open the socket or TCP/IP connection and read the fnctl() status */
Vio *mysql_protocol_vio_new(int sd)
{
  Vio *vio = (Vio*) malloc(sizeof(Vio));
  if (vio == NULL)
    return NULL;

  memset(vio, 0, sizeof(*vio));
  vio->closed= false;
  vio->sd= sd;
  vio->viodelete= _vio_delete;
  vio->vioerrno= _vio_errno;
  vio->read= _vio_read;
  vio->write= _vio_write;
  vio->fastsend= _vio_fastsend;
  vio->viokeepalive= _vio_keepalive;
  vio->should_retry= _vio_should_retry;
  vio->was_interrupted= _vio_was_interrupted;
  vio->vioclose= _vio_close;
  vio->peer_addr= _vio_peer_addr;
  vio->vioblocking= _vio_blocking;
  vio->timeout= _vio_timeout;

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

  return vio;
}
