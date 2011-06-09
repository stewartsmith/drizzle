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

namespace drizzle_plugin {

class NET
{
public:
  Vio* vio;
  unsigned char *buff,*buff_end,*write_pos,*read_pos;
  unsigned long remain_in_buf,length, buf_length, where_b;
  unsigned long max_packet,max_packet_size;
  unsigned int pkt_nr,compress_pkt_nr;
  unsigned int write_timeout_;
  unsigned int read_timeout_;
  unsigned int retry_count;
  char save_char;
  bool compress;
  unsigned int last_errno;
  unsigned char error;

  void set_write_timeout(uint32_t timeout);
  void set_read_timeout(uint32_t timeout);
};

void drizzleclient_net_end(NET*);
bool drizzleclient_net_flush(NET*);
bool drizzleclient_net_write(NET*, const void*, size_t);
bool drizzleclient_net_write_command(NET*, unsigned char command,
                                     const unsigned char *header, size_t head_len,
                                     const unsigned char *packet, size_t len);
uint32_t drizzleclient_net_read(NET*);
void drizzleclient_net_close(NET*);
void drizzleclient_net_init_sock(NET*, int sock, uint32_t buffer_length);
bool drizzleclient_net_peer_addr(NET*, char *buf, uint16_t *port, size_t buflen);
void drizzleclient_net_keepalive(NET*, bool flag);
int drizzleclient_net_get_sd(NET*);

} /* namespace drizzle_plugin */

