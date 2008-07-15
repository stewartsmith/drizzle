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

int vio_errno(Vio *vio __attribute__((unused)))
{
  return socket_errno;		/* On Win32 this mapped to WSAGetLastError() */
}


size_t vio_read(Vio * vio, uchar* buf, size_t size)
{
  size_t r;

  /* Ensure nobody uses vio_read_buff and vio_read simultaneously */
  assert(vio->read_end == vio->read_pos);
  errno=0;					/* For linux */
  r = read(vio->sd, buf, size);
  return r;
}


/*
  Buffered read: if average read size is small it may
  reduce number of syscalls.
*/

size_t vio_read_buff(Vio *vio, uchar* buf, size_t size)
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
      vio_read can return -1 due to an error or non-blocking mode, and
      the safest way to handle it is to move to a separate branch.
    */
  }
  else if (size < VIO_UNBUFFERED_READ_MIN_SIZE)
  {
    rc= vio_read(vio, (uchar*) vio->read_buffer, VIO_READ_BUFFER_SIZE);
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
    rc= vio_read(vio, buf, size);

  return rc;
#undef VIO_UNBUFFERED_READ_MIN_SIZE
}


size_t vio_write(Vio * vio, const uchar* buf, size_t size)
{
  size_t r;

  r = write(vio->sd, buf, size);

  return r;
}

int vio_blocking(Vio * vio __attribute__((unused)), my_bool set_blocking_mode,
		 my_bool *old_mode)
{
  int r=0;

  *old_mode= test(!(vio->fcntl_mode & O_NONBLOCK));

#if !defined(NO_FCNTL_NONBLOCK)
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
#else
  r= set_blocking_mode ? 0 : 1;
#endif /* !defined(NO_FCNTL_NONBLOCK) */
  return r;
}

my_bool
vio_is_blocking(Vio * vio)
{
  my_bool r;
  r = !(vio->fcntl_mode & O_NONBLOCK);

  return r;
}


int vio_fastsend(Vio * vio __attribute__((unused)))
{
  int r=0;

#if defined(IPTOS_THROUGHPUT)
  {
    int tos = IPTOS_THROUGHPUT;
    r= setsockopt(vio->sd, IPPROTO_IP, IP_TOS, (void *) &tos, sizeof(tos));
  }
#endif                                    /* IPTOS_THROUGHPUT */
  if (!r)
  {
    int nodelay = 1;

    r= setsockopt(vio->sd, IPPROTO_TCP, TCP_NODELAY,
                  IF_WIN(const char*, void*) &nodelay,
                  sizeof(nodelay));

  }
  if (r)
  {
    r= -1;
  }

  return r;
}

int vio_keepalive(Vio* vio, my_bool set_keep_alive)
{
  int r= 0;
  uint opt= 0;

  if (vio->type != VIO_TYPE_NAMEDPIPE)
  {
    if (set_keep_alive)
      opt= 1;
    r= setsockopt(vio->sd, SOL_SOCKET, SO_KEEPALIVE, (char *) &opt,
                  sizeof(opt));
  }

  return r;
}


my_bool
vio_should_retry(Vio * vio __attribute__((unused)))
{
  int en = socket_errno;
  return (en == SOCKET_EAGAIN || en == SOCKET_EINTR ||
	  en == SOCKET_EWOULDBLOCK);
}


my_bool
vio_was_interrupted(Vio *vio __attribute__((unused)))
{
  int en= socket_errno;
  return (en == SOCKET_EAGAIN || en == SOCKET_EINTR ||
	  en == SOCKET_EWOULDBLOCK || en == SOCKET_ETIMEDOUT);
}


int vio_close(Vio * vio)
{
  int r=0;
 if (vio->type != VIO_CLOSED)
  {
    assert(vio->sd >= 0);
    if (shutdown(vio->sd, SHUT_RDWR))
      r= -1;
    if (closesocket(vio->sd))
      r= -1;
  }
  vio->type= VIO_CLOSED;
  vio->sd=   -1;

  return r;
}


const char *vio_description(Vio * vio)
{
  return vio->desc;
}

enum enum_vio_type vio_type(Vio* vio)
{
  return vio->type;
}

my_socket vio_fd(Vio* vio)
{
  return vio->sd;
}

my_bool vio_peer_addr(Vio *vio, char *buf, uint16 *port, size_t buflen)
{
  if (vio->localhost)
  {
    strmov(buf, "127.0.0.1");
    *port= 0;
  }
  else
  {
    int error;
    char port_buf[NI_MAXSERV];
    size_socket addrLen = sizeof(vio->remote);
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

    *port= (uint16)strtol(port_buf, (char **)NULL, 10);

    /*
      A lot of users do not have IPv6 loopback resolving to localhost
      correctly setup. Should this exist? No. If we do not do it though
      we will be getting a lot of support questions from users who
      have bad setups. This code should be removed by say... 2012.
        -Brian
    */
    if (!memcmp(buf, "::ffff:127.0.0.1", sizeof("::ffff:127.0.0.1")))
      strmov(buf, "127.0.0.1");
  }

  return false;
}


/* Return 0 if there is data to be read */

my_bool vio_poll_read(Vio *vio,uint timeout)
{
#if defined(HAVE_POLL)
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
#else
  return 0;
#endif
}


my_bool vio_peek_read(Vio *vio, uint *bytes)
{
#if FIONREAD_IN_SYS_IOCTL
  int len;
  if (ioctl(vio->sd, FIONREAD, &len) < 0)
    return true;
  *bytes= len;
  return false;
#else
  char buf[1024];
  ssize_t res= recv(vio->sd, &buf, sizeof(buf), MSG_PEEK);
  if (res < 0)
    return true;
  *bytes= res;
  return false;
#endif
}

void vio_timeout(Vio *vio, uint which, uint timeout)
{
#if defined(SO_SNDTIMEO) && defined(SO_RCVTIMEO)
  int r;

  {
  /* POSIX specifies time as struct timeval. */
  struct timeval wait_timeout;
  wait_timeout.tv_sec= timeout;
  wait_timeout.tv_usec= 0;

  r= setsockopt(vio->sd, SOL_SOCKET, which ? SO_SNDTIMEO : SO_RCVTIMEO,
                IF_WIN(const char*, const void*)&wait_timeout,
                sizeof(wait_timeout));

  }
#else
/*
  Platforms not suporting setting of socket timeout should either use
  thr_alarm or just run without read/write timeout(s)
*/
#endif
}
