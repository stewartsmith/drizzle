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

#define DONT_MAP_VIO
#include "config.h"
#include "vio.h"
#include <drizzled/util/test.h>

#include <sys/socket.h>
#include <fcntl.h>

#include <sys/types.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <unistd.h>

#include <netdb.h>

#include <cstdio>
#include <cstring>
#include <cassert>

#include <algorithm>

using namespace std;

namespace drizzle_protocol
{


int drizzleclient_vio_errno(Vio *vio)
{
  (void)vio;
  return errno;
}


size_t drizzleclient_vio_read(Vio * vio, unsigned char* buf, size_t size)
{
  size_t r;

  /* Ensure nobody uses drizzleclient_vio_read_buff and drizzleclient_vio_read simultaneously */
  assert(vio->read_end == vio->read_pos);
  r= read(vio->sd, buf, size);

  return r;
}


/*
  Buffered read: if average read size is small it may
  reduce number of syscalls.
*/

size_t drizzleclient_vio_read_buff(Vio *vio, unsigned char* buf, size_t size)
{
  size_t rc;
#define VIO_UNBUFFERED_READ_MIN_SIZE 2048

  if (vio->read_pos < vio->read_end)
  {
    rc= min((size_t) (vio->read_end - vio->read_pos), size);
    memcpy(buf, vio->read_pos, rc);
    vio->read_pos+= rc;
    /*
      Do not try to read from the socket now even if rc < size:
      drizzleclient_vio_read can return -1 due to an error or non-blocking mode, and
      the safest way to handle it is to move to a separate branch.
    */
  }
  else if (size < VIO_UNBUFFERED_READ_MIN_SIZE)
  {
    rc= drizzleclient_vio_read(vio, (unsigned char*) vio->read_buffer, VIO_READ_BUFFER_SIZE);
    if (rc != 0 && rc != (size_t) -1)
    {
      if (rc > size)
      {
        vio->read_pos= vio->read_buffer + size;
        vio->read_end= vio->read_buffer + rc;
        rc= size;
      }
      memcpy(buf, vio->read_buffer, rc);
    }
  }
  else
    rc= drizzleclient_vio_read(vio, buf, size);

  return rc;
#undef VIO_UNBUFFERED_READ_MIN_SIZE
}


size_t drizzleclient_vio_write(Vio * vio, const unsigned char* buf, size_t size)
{
  size_t r;

  r = write(vio->sd, buf, size);

  return r;
}

int drizzleclient_vio_blocking(Vio * vio, bool set_blocking_mode, bool *old_mode)
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

bool
drizzleclient_vio_is_blocking(Vio * vio)
{
  bool r;
  r = !(vio->fcntl_mode & O_NONBLOCK);

  return r;
}


int drizzleclient_vio_fastsend(Vio * vio)
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

int32_t drizzleclient_vio_keepalive(Vio* vio, bool set_keep_alive)
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


bool
drizzleclient_vio_should_retry(Vio * vio)
{
  (void)vio;
  int en = errno;
  return (en == EAGAIN || en == EINTR ||
	  en == EWOULDBLOCK);
}


bool
drizzleclient_vio_was_interrupted(Vio *vio)
{
  (void)vio;
  int en= errno;
  return (en == EAGAIN || en == EINTR ||
	  en == EWOULDBLOCK || en == ETIMEDOUT);
}


int drizzleclient_vio_close(Vio * vio)
{
  int r=0;
 if (vio->type != VIO_CLOSED)
  {
    assert(vio->sd >= 0);
    if (shutdown(vio->sd, SHUT_RDWR))
      r= -1;
    if (close(vio->sd))
      r= -1;
  }
  vio->type= VIO_CLOSED;
  vio->sd=   -1;

  return r;
}


const char *drizzleclient_vio_description(Vio * vio)
{
  return vio->desc;
}

enum enum_vio_type drizzleclient_vio_type(Vio* vio)
{
  return vio->type;
}

int drizzleclient_vio_fd(Vio* vio)
{
  return vio->sd;
}

bool drizzleclient_vio_peer_addr(Vio *vio, char *buf, uint16_t *port, size_t buflen)
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


/* Return 0 if there is data to be read */

bool drizzleclient_vio_poll_read(Vio *vio, int32_t timeout)
{
  struct pollfd fds;
  int res;

  fds.fd=vio->sd;
  fds.events=POLLIN;
  fds.revents=0;
  if ((res=poll(&fds,1,(int) timeout*1000)) <= 0)
  {
    return res < 0 ? false : true;		/* Don't return 1 on errors */
  }
  return (fds.revents & (POLLIN | POLLERR | POLLHUP) ? false : true);
}


bool drizzleclient_vio_peek_read(Vio *vio, uint32_t *bytes)
{
  char buf[1024];
  ssize_t res= recv(vio->sd, &buf, sizeof(buf), MSG_PEEK);

  if (res < 0)
    return true;
  *bytes= (uint32_t)res;
  return false;
}

void drizzleclient_vio_timeout(Vio *vio, bool is_sndtimeo, int32_t timeout)
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

} /* namespace drizzle_protcol */
