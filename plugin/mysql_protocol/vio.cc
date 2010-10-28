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

Vio::Vio(int nsd)
: closed(false),
sd(nsd),
fcntl_mode(0),
read_pos(NULL),
read_end(NULL)
{
  closed= false;
  sd= nsd;

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
  fcntl_mode= fcntl(sd, F_GETFL);

  memset(&local, 0, sizeof(local));
  memset(&remote, 0, sizeof(remote));
}

Vio::~Vio()
{
 if (!closed)
    close();
}

int Vio::close()
{
  int r=0;
  if (!closed)
  {
    assert(sd >= 0);
    if (shutdown(sd, SHUT_RDWR))
      r= -1;
    if (::close(sd))
      r= -1;
  }
  closed= true;
  sd=   -1;

  return r;
}

size_t Vio::read(unsigned char* buf, size_t size)
{
  size_t r;

  /* Ensure nobody uses vio_read_buff and vio_read simultaneously */
  assert(read_end == read_pos);
  r= ::read(sd, buf, size);

  return r;
}

size_t Vio::write(const unsigned char* buf, size_t size)
{
  size_t r;

  r = ::write(sd, buf, size);

  return r;
}

int Vio::blocking(bool set_blocking_mode, bool *old_mode)
{
  int r=0;

  // make sure ptr is not NULL:
  if (NULL != old_mode)
    *old_mode= drizzled::test(!(fcntl_mode & O_NONBLOCK));

  if (sd >= 0)
  {
    int old_fcntl=fcntl_mode;
    if (set_blocking_mode)
      fcntl_mode &= ~O_NONBLOCK; /* clear bit */
    else
      fcntl_mode |= O_NONBLOCK; /* set bit */
    if (old_fcntl != fcntl_mode)
    {
      r= fcntl(sd, F_SETFL, fcntl_mode);
      if (r == -1)
      {
        fcntl_mode= old_fcntl;
      }
    }
  }

  return r;
}

int Vio::fastsend()
{
  int nodelay = 1;
  int error;

  error= setsockopt(sd, IPPROTO_TCP, TCP_NODELAY,
                    &nodelay, sizeof(nodelay));
  if (error != 0)
  {
    perror("setsockopt");
    assert(error == 0);
  }

  return error;
}

int32_t Vio::keepalive(bool set_keep_alive)
{
  int r= 0;
  uint32_t opt= 0;

  if (set_keep_alive)
    opt= 1;

  r= setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt, sizeof(opt));
  if (r != 0)
  {
    perror("setsockopt");
    assert(r == 0);
  }

  return r;
}

bool Vio::should_retry() const
{
  int en = errno;
  return (en == EAGAIN || en == EINTR ||
          en == EWOULDBLOCK);
}

bool Vio::was_interrupted() const
{
  int en= errno;
  return (en == EAGAIN || en == EINTR ||
          en == EWOULDBLOCK || en == ETIMEDOUT);
}

bool Vio::peer_addr(char *buf, uint16_t *port, size_t buflen) const
{
  int error;
  char port_buf[NI_MAXSERV];
  socklen_t al = sizeof(remote);

  if (getpeername(sd, (struct sockaddr *) (&remote),
                  &al) != 0)
  {
    return true;
  }

  if ((error= getnameinfo((struct sockaddr *)(&remote),
                          al,
                          buf, buflen,
                          port_buf, NI_MAXSERV, NI_NUMERICHOST|NI_NUMERICSERV)))
  {
    return true;
  }

  *port= (uint16_t)strtol(port_buf, (char **)NULL, 10);

  return false;
}

void Vio::timeout(bool is_sndtimeo, int32_t t)
{
  int error;

  /* POSIX specifies time as struct timeval. */
  struct timeval wait_timeout;
  wait_timeout.tv_sec= t;
  wait_timeout.tv_usec= 0;

  assert(t >= 0 && t <= INT32_MAX);
  assert(sd != -1);
  error= setsockopt(sd, SOL_SOCKET, is_sndtimeo ? SO_SNDTIMEO : SO_RCVTIMEO,
                    &wait_timeout,
                    (socklen_t)sizeof(struct timeval));
  if (error == -1 && errno != ENOPROTOOPT)
  {
    perror("setsockopt");
    assert(error == 0);
  }
}

int Vio::get_errno() const
{
  return errno;
}

int Vio::get_fd() const
{
  return sd;
}


char *Vio::get_read_pos() const
{
  return read_pos;
}

char *Vio::get_read_end() const
{
  return read_end;
}

