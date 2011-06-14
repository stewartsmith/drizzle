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


#pragma once

#include "vio.h"

#define LIBDRIZZLE_ERRMSG_SIZE 512
#define LIBDRIZZLE_SQLSTATE_LENGTH 5

namespace drizzle_plugin
{

typedef struct st_net {
  Vio *vio;
  unsigned char *buff,*buff_end,*write_pos,*read_pos;
  int fd;					/* For Perl DBI/dbd */
  /*
    The following variable is set if we are doing several queries in one
    command ( as in LOAD TABLE ... FROM MASTER ),
    and do not want to confuse the client with OK at the wrong time
  */
  unsigned long remain_in_buf,length, buf_length, where_b;
  unsigned long max_packet,max_packet_size;
  unsigned int pkt_nr,compress_pkt_nr;
  unsigned int write_timeout;
  unsigned int read_timeout;
  unsigned int retry_count;
  int fcntl;
  unsigned int *return_status;
  unsigned char reading_or_writing;
  char save_char;
  bool compress;
  /*
    Pointer to query object in query cache, do not equal NULL (0) for
    queries in cache that have not stored its results yet
  */
  /*
    Unused, please remove with the next incompatible ABI change.
  */
  unsigned char *unused;
  unsigned int last_errno;
  unsigned char error;
  /** Client library error message buffer. Actually belongs to struct MYSQL. */
  char last_error[LIBDRIZZLE_ERRMSG_SIZE];
  /** Client library sqlstate buffer. Set along with the error message. */
  char sqlstate[LIBDRIZZLE_SQLSTATE_LENGTH+1];
  void *extension;
} NET;

bool drizzleclient_net_init(NET *net, Vio* vio, uint32_t buffer_length);
void drizzleclient_net_end(NET *net);
void drizzleclient_net_clear(NET *net, bool clear_buffer);
bool drizzleclient_net_flush(NET *net);
bool drizzleclient_net_write(NET *net,const unsigned char *packet, size_t len);
bool drizzleclient_net_write_command(NET *net,unsigned char command,
                                     const unsigned char *header, size_t head_len,
                                     const unsigned char *packet, size_t len);
uint32_t drizzleclient_net_read(NET *net);
void drizzleclient_net_close(NET *net);
bool drizzleclient_net_init_sock(NET * net, int sock, uint32_t buffer_length);
bool drizzleclient_net_peer_addr(NET *net, char *buf, uint16_t *port, size_t buflen);
void drizzleclient_net_keepalive(NET *net, bool flag);
int drizzleclient_net_get_sd(NET *net);
bool drizzleclient_net_more_data(NET *net);

void drizzleclient_net_set_write_timeout(NET *net, uint32_t timeout);
void drizzleclient_net_set_read_timeout(NET *net, uint32_t timeout);
void drizzleclient_drizzleclient_net_clear_error(NET *net);

} /* namespace drizzle_plugin */

