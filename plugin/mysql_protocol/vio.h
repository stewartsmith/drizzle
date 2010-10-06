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
 * Virtual I/O layer, only used with TCP/IP sockets at the moment.
 */

#ifndef PLUGIN_MYSQL_PROTOCOL_VIO_H
#define PLUGIN_MYSQL_PROTOCOL_VIO_H

#include <sys/socket.h>
#include <errno.h>

typedef struct st_vio Vio;

struct st_vio
{
  bool closed;
  int sd;
  int fcntl_mode; /* Buffered fcntl(sd,F_GETFL) */
  struct sockaddr_storage local; /* Local internet address */
  struct sockaddr_storage remote; /* Remote internet address */
  int addrLen; /* Length of remote address */
  char *read_pos; /* start of unfetched data in the read buffer */
  char *read_end; /* end of unfetched data */

  /* function pointers. They are similar for socket/SSL/whatever */
  void (*viodelete)(Vio*);
  int32_t (*vioerrno)(Vio*);
  size_t (*read)(Vio*, unsigned char *, size_t);
  size_t (*write)(Vio*, const unsigned char *, size_t);
  int32_t (*vioblocking)(Vio*, bool, bool *);
  int32_t (*viokeepalive)(Vio*, bool);
  int32_t (*fastsend)(Vio*);
  bool (*peer_addr)(Vio*, char *, uint16_t *, size_t);
  void (*in_addr)(Vio*, struct sockaddr_storage*);
  bool (*should_retry)(Vio*);
  bool (*was_interrupted)(Vio*);
  int32_t (*vioclose)(Vio*);
  void (*timeout)(Vio*, bool is_sndtimeo, int32_t timeout);
};

Vio* mysql_protocol_vio_new(int sd);

#define vio_fd(vio) (vio)->sd
#define vio_delete(vio) (vio)->viodelete(vio)
#define vio_errno(vio) (vio)->vioerrno(vio)
#define vio_read(vio, buf, size) ((vio)->read)(vio,buf,size)
#define vio_write(vio, buf, size) ((vio)->write)(vio, buf, size)
#define vio_blocking(vio, set_blocking_mode, old_mode) (vio)->vioblocking(vio, set_blocking_mode, old_mode)
#define vio_fastsend(vio) (vio)->fastsend(vio)
#define vio_keepalive(vio, set_keep_alive) (vio)->viokeepalive(vio, set_keep_alive)
#define vio_should_retry(vio) (vio)->should_retry(vio)
#define vio_was_interrupted(vio) (vio)->was_interrupted(vio)
#define vio_close(vio) ((vio)->vioclose)(vio)
#define vio_peer_addr(vio, buf, prt, buflen) (vio)->peer_addr(vio, buf, prt, buflen)
#define vio_timeout(vio, which, seconds) (vio)->timeout(vio, which, seconds)

#endif /* PLUGIN_MYSQL_PROTOCOL_VIO_H */
