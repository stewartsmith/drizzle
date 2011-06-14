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
  unsigned char error_;

  void init(int sock, uint32_t buffer_length);
  bool flush();
  void end();
  void close();
  bool peer_addr(char *buf, size_t buflen, uint16_t&);
  void keepalive(bool flag);
  int get_sd() const;
  void set_write_timeout(uint32_t timeout);
  void set_read_timeout(uint32_t timeout);
  bool write(const void*, size_t);
  bool write_command(unsigned char command,
    const unsigned char *header, size_t head_len,
    const unsigned char *packet, size_t len);
  uint32_t read();
};


} /* namespace drizzle_plugin */

