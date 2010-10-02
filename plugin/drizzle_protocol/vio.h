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
 * Vio Lite.
 * Purpose: include file for Vio that will work with C and C++
 */

#ifndef PLUGIN_DRIZZLE_PROTOCOL_VIO_H
#define	PLUGIN_DRIZZLE_PROTOCOL_VIO_H

#include <sys/socket.h>
#include <errno.h>

namespace drizzle_protocol
{

/* Simple vio interface in C;  The functions are implemented in violite.c */

enum enum_vio_type
{
  VIO_CLOSED, VIO_TYPE_TCPIP, VIO_TYPE_SOCKET, VIO_TYPE_NAMEDPIPE,
  VIO_TYPE_SSL, VIO_TYPE_SHARED_MEMORY
};

typedef struct st_vio Vio;

#define VIO_BUFFERED_READ 2                     /* use buffered read */
#define VIO_READ_BUFFER_SIZE 16384              /* size of read buffer */

Vio*	drizzleclient_vio_new(int sd, enum enum_vio_type type, unsigned int flags);

void	drizzleclient_vio_delete(Vio* vio);
int	drizzleclient_vio_close(Vio* vio);
void    drizzleclient_vio_reset(Vio* vio, enum enum_vio_type type, int sd, uint32_t flags);
size_t	drizzleclient_vio_read(Vio *vio, unsigned char *	buf, size_t size);
size_t  drizzleclient_vio_read_buff(Vio *vio, unsigned char * buf, size_t size);
size_t	drizzleclient_vio_write(Vio *vio, const unsigned char * buf, size_t size);
int	drizzleclient_vio_blocking(Vio *vio, bool onoff, bool *old_mode);
bool	drizzleclient_vio_is_blocking(Vio *vio);
/* setsockopt TCP_NODELAY at IPPROTO_TCP level, when possible */
int	drizzleclient_vio_fastsend(Vio *vio);
/* setsockopt SO_KEEPALIVE at SOL_SOCKET level, when possible */
int32_t	drizzleclient_vio_keepalive(Vio *vio, bool	onoff);
/* Whenever we should retry the last read/write operation. */
bool	drizzleclient_vio_should_retry(Vio *vio);
/* Check that operation was timed out */
bool	drizzleclient_vio_was_interrupted(Vio *vio);
/* Short text description of the socket for those, who are curious.. */
const char* drizzleclient_vio_description(Vio *vio);
/* Return the type of the connection */
enum enum_vio_type drizzleclient_vio_type(Vio* vio);
/* Return last error number */
int	drizzleclient_vio_errno(Vio*vio);
/* Get socket number */
int drizzleclient_vio_fd(Vio*vio);
/* Remote peer's address and name in text form */
bool drizzleclient_vio_peer_addr(Vio *vio, char *buf, uint16_t *port, size_t buflen);
bool drizzleclient_vio_poll_read(Vio *vio, int timeout);
bool drizzleclient_vio_peek_read(Vio *vio, unsigned int *bytes);

void drizzleclient_vio_end(void);

void drizzleclient_vio_ignore_timeout(Vio *vio, bool is_sndtimeo, int32_t timeout);
void drizzleclient_vio_timeout(Vio *vio, bool is_sndtimeo, int32_t timeout);

#if !defined(DONT_MAP_VIO)
#define drizzleclient_vio_delete(vio) 			(vio)->viodelete(vio)
#define drizzleclient_vio_errno(vio)	 			(vio)->vioerrno(vio)
#define drizzleclient_vio_read(vio, buf, size)                ((vio)->read)(vio,buf,size)
#define drizzleclient_vio_write(vio, buf, size)               ((vio)->write)(vio, buf, size)
#define drizzleclient_vio_blocking(vio, set_blocking_mode, old_mode)\
 	(vio)->vioblocking(vio, set_blocking_mode, old_mode)
#define drizzleclient_vio_is_blocking(vio) 			(vio)->is_blocking(vio)
#define drizzleclient_vio_fastsend(vio)			(vio)->fastsend(vio)
#define drizzleclient_vio_keepalive(vio, set_keep_alive)	(vio)->viokeepalive(vio, set_keep_alive)
#define drizzleclient_vio_should_retry(vio) 			(vio)->should_retry(vio)
#define drizzleclient_vio_was_interrupted(vio) 		(vio)->was_interrupted(vio)
#define drizzleclient_vio_close(vio)				((vio)->vioclose)(vio)
#define drizzleclient_vio_peer_addr(vio, buf, prt, buflen)	(vio)->peer_addr(vio, buf, prt, buflen)
#define drizzleclient_vio_timeout(vio, which, seconds)	(vio)->timeout(vio, which, seconds)
#endif /* !defined(DONT_MAP_VIO) */

/* This enumerator is used in parser - should be always visible */
enum SSL_type
{
  SSL_TYPE_NOT_SPECIFIED= -1,
  SSL_TYPE_NONE,
  SSL_TYPE_ANY,
  SSL_TYPE_X509,
  SSL_TYPE_SPECIFIED
};

/* HFTODO - hide this if we don't want client in embedded server */
/* This structure is for every connection on both sides */
struct st_vio
{
  int		sd;		/* int - real or imaginary */
  int			fcntl_mode;	/* Buffered fcntl(sd,F_GETFL) */
  struct sockaddr_storage	local;		/* Local internet address */
  struct sockaddr_storage	remote;		/* Remote internet address */
  int addrLen;                          /* Length of remote address */
  enum enum_vio_type	type;		/* Type of connection */
  char			desc[30];	/* String description */
  char                  *read_pos;      /* start of unfetched data in the
                                           read buffer */
  char                  *read_end;      /* end of unfetched data */

  /* function pointers. They are similar for socket/SSL/whatever */
  void    (*viodelete)(Vio*);
  int32_t     (*vioerrno)(Vio*);
  size_t  (*read)(Vio*, unsigned char *, size_t);
  size_t  (*write)(Vio*, const unsigned char *, size_t);
  int32_t     (*vioblocking)(Vio*, bool, bool *);
  bool (*is_blocking)(Vio*);
  int32_t     (*viokeepalive)(Vio*, bool);
  int32_t     (*fastsend)(Vio*);
  bool (*peer_addr)(Vio*, char *, uint16_t *, size_t);
  void    (*in_addr)(Vio*, struct sockaddr_storage*);
  bool (*should_retry)(Vio*);
  bool (*was_interrupted)(Vio*);
  int32_t     (*vioclose)(Vio*);
  void	  (*timeout)(Vio*, bool is_sndtimeo, int32_t timeout);
  char                  *read_buffer;   /* buffer for drizzleclient_vio_read_buff */
};

} /* namespace drizzle_procotol */

#endif /* PLUGIN_DRIZZLE_PROTOCOL_VIO_H */
