/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <config.h>

#include <drizzled/current_session.h>
#include <drizzled/error.h>
#include <drizzled/session.h>
#include <drizzled/statistics_variables.h>
#include <drizzled/system_variables.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <zlib.h>
#include <algorithm>

#include "errmsg.h"
#include "vio.h"
#include "net_serv.h"

namespace drizzle_plugin {

using namespace std;
using namespace drizzled;

/*
  The following handles the differences when this is linked between the
  client and the server.

  This gives an error if a too big packet is found
  The server can change this with the -O switch, but because the client
  can't normally do this the client should have a bigger max_allowed_packet.
*/

  /* Constants when using compression */
#define NET_HEADER_SIZE 4		/* standard header size */
#define COMP_HEADER_SIZE 3		/* compression header extra size */

#define MAX_PACKET_LENGTH (256L*256L*256L-1)

static bool net_write_buff(NET*, const void*, uint32_t len);
static int drizzleclient_net_real_write(NET *net, const unsigned char *packet, size_t len);

/** Init with packet info. */

void NET::init(int sock, uint32_t buffer_length)
{
  vio= new Vio(sock);
  max_packet= (uint32_t) buffer_length;
  max_packet_size= max(buffer_length, drizzled::global_system_variables.max_allowed_packet);

  buff= (unsigned char*) malloc((size_t) max_packet + NET_HEADER_SIZE + COMP_HEADER_SIZE);
  buff_end= buff + max_packet;
  error_= 0;
  pkt_nr= compress_pkt_nr= 0;
  write_pos= read_pos= buff;
  compress= 0; 
  where_b= remain_in_buf= 0;
  last_errno= 0;
  vio->fastsend();
}

void NET::end()
{
  free(buff);
  buff= NULL;
}

void NET::close()
{
  drizzled::safe_delete(vio);
}

bool NET::peer_addr(char *buf, size_t buflen, uint16_t& port)
{
  return vio->peer_addr(buf, buflen, port);
}

void NET::keepalive(bool flag)
{
  vio->keepalive(flag);
}

int NET::get_sd() const
{
  return vio->get_fd();
}

/** Realloc the packet buffer. */

static bool drizzleclient_net_realloc(NET *net, size_t length)
{
  if (length >= net->max_packet_size)
  {
    /* @todo: 1 and 2 codes are identical. */
    net->error_= 3;
    net->last_errno= ER_NET_PACKET_TOO_LARGE;
    my_error(ER_NET_PACKET_TOO_LARGE, MYF(0));
    return 1;
  }
  size_t pkt_length = (length + IO_SIZE - 1) & ~(IO_SIZE - 1);
  /*
    We must allocate some extra bytes for the end 0 and to be able to
    read big compressed blocks
  */
  unsigned char* buff= (unsigned char*)realloc((char*) net->buff, pkt_length + NET_HEADER_SIZE + COMP_HEADER_SIZE);
  net->buff=net->write_pos= buff;
  net->buff_end= buff + (net->max_packet= (uint32_t) pkt_length);
  return 0;
}

bool NET::flush()
{
  bool error= false;
  if (buff != write_pos)
  {
    error= drizzleclient_net_real_write(this, buff, write_pos - buff) ? true : false;
    write_pos= buff;
  }
  /* Sync packet number if using compression */
  if (compress)
    pkt_nr= compress_pkt_nr;
  return error;
}


/*****************************************************************************
 ** Write something to server/client buffer
 *****************************************************************************/

/**
   Write a logical packet with packet header.

   Format: Packet length (3 bytes), packet number(1 byte)
   When compression is used a 3 byte compression length is added

   @note
   If compression is used the original package is modified!
*/

static bool
drizzleclient_net_write(NET* net, const void* packet0, size_t len)
{
  const unsigned char* packet= reinterpret_cast<const unsigned char*>(packet0);
  unsigned char buff[NET_HEADER_SIZE];
  if (unlikely(!net->vio)) /* nowhere to write */
    return 0;
  /*
    Big packets are handled by splitting them in packets of MAX_PACKET_LENGTH
    length. The last packet is always a packet that is < MAX_PACKET_LENGTH.
    (The last packet may even have a length of 0)
  */
  while (len >= MAX_PACKET_LENGTH)
  {
    const uint32_t z_size = MAX_PACKET_LENGTH;
    int3store(buff, z_size);
    buff[3]= (unsigned char) net->pkt_nr++;
    if (net_write_buff(net, buff, NET_HEADER_SIZE) || net_write_buff(net, packet, z_size))
      return 1;
    packet += z_size;
    len-=     z_size;
  }
  /* Write last packet */
  int3store(buff,len);
  buff[3]= (unsigned char) net->pkt_nr++;
  return net_write_buff(net, buff, NET_HEADER_SIZE) || net_write_buff(net, packet, len);
}

/**
   Send a command to the server.

   The reason for having both header and packet is so that libdrizzle
   can easy add a header to a special command (like prepared statements)
   without having to re-alloc the string.

   As the command is part of the first data packet, we have to do some data
   juggling to put the command in there, without having to create a new
   packet.

   This function will split big packets into sub-packets if needed.
   (Each sub packet can only be 2^24 bytes)

   @param net        NET handler
   @param command    Command in MySQL server (enum enum_server_command)
   @param header    Header to write after command
   @param head_len    Length of header
   @param packet    Query or parameter to query
   @param len        Length of packet

   @retval
   0    ok
   @retval
   1    error
*/

static bool
drizzleclient_net_write_command(NET *net,unsigned char command,
                  const unsigned char *header, size_t head_len,
                  const unsigned char *packet, size_t len)
{
  uint32_t length=len+1+head_len;            /* 1 extra byte for command */
  unsigned char buff[NET_HEADER_SIZE+1];
  uint32_t header_size=NET_HEADER_SIZE+1;

  buff[4]=command;                /* For first packet */

  if (length >= MAX_PACKET_LENGTH)
  {
    /* Take into account that we have the command in the first header */
    len= MAX_PACKET_LENGTH - 1 - head_len;
    do
    {
      int3store(buff, MAX_PACKET_LENGTH);
      buff[3]= (unsigned char) net->pkt_nr++;
      if (net_write_buff(net, buff, header_size) ||
          net_write_buff(net, header, head_len) ||
          net_write_buff(net, packet, len))
        return(1);
      packet+= len;
      length-= MAX_PACKET_LENGTH;
      len= MAX_PACKET_LENGTH;
      head_len= 0;
      header_size= NET_HEADER_SIZE;
    } while (length >= MAX_PACKET_LENGTH);
    len=length;                    /* Data left to be written */
  }
  int3store(buff,length);
  buff[3]= (unsigned char) net->pkt_nr++;
  return (net_write_buff(net, buff, header_size) ||
          (head_len && net_write_buff(net, header, head_len)) ||
          net_write_buff(net, packet, len) || net->flush());
}

/**
   Caching the data in a local buffer before sending it.

   Fill up net->buffer and send it to the client when full.

   If the rest of the to-be-sent-packet is bigger than buffer,
   send it in one big block (to avoid copying to internal buffer).
   If not, copy the rest of the data to the buffer and return without
   sending data.

   @param net        Network handler
   @param packet    Packet to send
   @param len        Length of packet

   @note
   The cached buffer can be sent as it is with 'drizzleclient_net_flush()'.
   In this code we have to be careful to not send a packet longer than
   MAX_PACKET_LENGTH to drizzleclient_net_real_write() if we are using the compressed
   protocol as we store the length of the compressed packet in 3 bytes.

   @retval
   0    ok
   @retval
   1
*/

static bool
net_write_buff(NET* net, const void* packet0, uint32_t len)
{
  const unsigned char* packet= reinterpret_cast<const unsigned char*>(packet0);
  uint32_t left_length;
  if (net->compress && net->max_packet > MAX_PACKET_LENGTH)
    left_length= MAX_PACKET_LENGTH - (net->write_pos - net->buff);
  else
    left_length= (uint32_t) (net->buff_end - net->write_pos);

  if (len > left_length)
  {
    if (net->write_pos != net->buff)
    {
      /* Fill up already used packet and write it */
      memcpy(net->write_pos,packet,left_length);
      if (drizzleclient_net_real_write(net, net->buff,
                         (size_t) (net->write_pos - net->buff) + left_length))
        return 1;
      net->write_pos= net->buff;
      packet+= left_length;
      len-= left_length;
    }
    if (net->compress)
    {
      /*
        We can't have bigger packets than 16M with compression
        Because the uncompressed length is stored in 3 bytes
      */
      left_length= MAX_PACKET_LENGTH;
      while (len > left_length)
      {
        if (drizzleclient_net_real_write(net, packet, left_length))
          return 1;
        packet+= left_length;
        len-= left_length;
      }
    }
    if (len > net->max_packet)
      return drizzleclient_net_real_write(net, packet, len) ? 1 : 0;
    /* Send out rest of the blocks as full sized blocks */
  }
  memcpy(net->write_pos,packet,len);
  net->write_pos+= len;
  return 0;
}


/**
   Read and write one packet using timeouts.
   If needed, the packet is compressed before sending.

   @todo
   - TODO is it needed to set this variable if we have no socket
*/

/*
  TODO: rewrite this in a manner to do non-block writes. If a write can not be made, and we are
  in the server, yield to another process and come back later.
*/
static int
drizzleclient_net_real_write(NET *net, const unsigned char *packet, size_t len)
{
  /* Backup of the original SO_RCVTIMEO timeout */

  if (net->error_ == 2)
    return(-1);                /* socket can't be used */

  if (net->compress)
  {
    const uint32_t header_length=NET_HEADER_SIZE+COMP_HEADER_SIZE;
    unsigned char* b= (unsigned char*) malloc(len + NET_HEADER_SIZE + COMP_HEADER_SIZE);
    memcpy(b+header_length,packet,len);

    size_t complen= len * 120 / 100 + 12;
    unsigned char* compbuf= new unsigned char[complen];
    uLongf tmp_complen= complen;
    int res= compress((Bytef*) compbuf, &tmp_complen,
      (Bytef*) (b+header_length),
      len);
    complen= tmp_complen;

    delete[] compbuf;

    if (res != Z_OK || complen >= len)
      complen= 0;
    else
    {
      size_t tmplen= complen;
      complen= len;
      len= tmplen;
    }
    int3store(&b[NET_HEADER_SIZE],complen);
    int3store(b,len);
    b[3]=(unsigned char) (net->compress_pkt_nr++);
    len+= header_length;
    packet= b;
  }

  uint32_t retry_count= 0;
  const unsigned char* pos= packet;
  const unsigned char* end= pos + len;
  /* Loop until we have read everything */
  while (pos != end)
  {
    assert(pos);
    // TODO - see bug comment below - will we crash now?
    size_t length;
    if ((long) (length= net->vio->write( pos, (size_t) (end-pos))) <= 0)
    {
     /*
      * We could end up here with net->vio == NULL
      * See LP bug#436685
      * If that is the case, we exit the while loop
      */
      if (net->vio == NULL)
        break;
      
      const bool interrupted= net->vio->should_retry();
      /*
        If we read 0, or we were interrupted this means that
        we need to switch to blocking mode and wait until the timeout
        on the socket kicks in.
      */
      if (interrupted || length == 0)
      {
        bool old_mode;
        while (net->vio->blocking(true, &old_mode) < 0)
        {
          if (net->vio->should_retry() && retry_count++ < net->retry_count)
            continue;
          net->error_= 2;                     /* Close socket */
          net->last_errno= ER_NET_PACKET_TOO_LARGE;
          my_error(ER_NET_PACKET_TOO_LARGE, MYF(0));
          goto end;
        }
        retry_count=0;
        continue;
      }
      else
      {
        if (retry_count++ < net->retry_count)
          continue;
      }

      if (net->vio->get_errno() == EINTR)
      {
        continue;
      }
      net->error_= 2;                /* Close socket */
      net->last_errno= interrupted ? CR_NET_WRITE_INTERRUPTED : CR_NET_ERROR_ON_WRITE;
      break;
    }
    pos+= length;

    /* If this is an error we may not have a current_session any more */
    if (current_session)
      current_session->status_var.bytes_sent+= length;
  }
end:
  if (net->compress)
    free((char*)packet);

  return (int) (pos != end);
}


/**
   Reads one packet to net->buff + net->where_b.
   Long packets are handled by drizzleclient_net_read().
   This function reallocates the net->buff buffer if necessary.

   @return
   Returns length of packet.
*/

static uint32_t
my_real_read(NET *net, size_t *complen)
{
  size_t length= 0;
  uint32_t retry_count=0;
  size_t len=packet_error;
  uint32_t remain= net->compress ? NET_HEADER_SIZE+COMP_HEADER_SIZE : NET_HEADER_SIZE;

  *complen = 0;

  /* Read timeout is set in drizzleclient_net_set_read_timeout */

  unsigned char* pos = net->buff + net->where_b;        /* net->packet -4 */

  for (uint32_t i= 0; i < 2 ; i++)
  {
    while (remain > 0)
    {
      /* First read is done with non blocking mode */
      if ((long) (length= net->vio->read(pos, remain)) <= 0L)
      {
        if (net->vio == NULL)
          goto end;

        const bool interrupted = net->vio->should_retry();

        if (interrupted)
        {                    /* Probably in MIT threads */
          if (retry_count++ < net->retry_count)
            continue;
        }
        if (net->vio->get_errno() == EINTR)
        {
          continue;
        }
        len= packet_error;
        net->error_= 2;                /* Close socket */
        net->last_errno= net->vio->was_interrupted() ? CR_NET_READ_INTERRUPTED : CR_NET_READ_ERROR;
        goto end;
      }
      remain -= (uint32_t) length;
      pos+= length;
      current_session->status_var.bytes_received+= length;
    }
    if (i == 0)
    {                    /* First parts is packet length */
      uint32_t helping;

      if (net->buff[net->where_b + 3] != (unsigned char) net->pkt_nr)
      {
        len= packet_error;
        /* Not a NET error on the client. XXX: why? */
        my_error(ER_NET_PACKETS_OUT_OF_ORDER, MYF(0));
        goto end;
      }
      net->compress_pkt_nr= ++net->pkt_nr;
      if (net->compress)
      {
        /*
          If the packet is compressed then complen > 0 and contains the
          number of bytes in the uncompressed packet
        */
        *complen=uint3korr(&(net->buff[net->where_b + NET_HEADER_SIZE]));
      }

      len=uint3korr(net->buff+net->where_b);
      if (!len)                /* End of big multi-packet */
        goto end;
      helping = max(len,*complen) + net->where_b;
      /* The necessary size of net->buff */
      if (helping >= net->max_packet)
      {
        if (drizzleclient_net_realloc(net,helping))
        {
          /* Clear the buffer so libdrizzle doesn't keep retrying */
          while (len > 0)
          {
            length= read(net->vio->get_fd(), net->buff, min((size_t)net->max_packet, len));
            assert((long)length > 0L);
            len-= length;
          }
            
          len= packet_error;          /* Return error and close connection */
          goto end;
        }
      }
      pos=net->buff + net->where_b;
      remain = (uint32_t) len;
    }
  }

end:
  return len;
}


/**
   Read a packet from the client/server and return it without the internal
   package header.

   If the packet is the first packet of a multi-packet packet
   (which is indicated by the length of the packet = 0xffffff) then
   all sub packets are read and concatenated.

   If the packet was compressed, its uncompressed and the length of the
   uncompressed packet is returned.

   @return
   The function returns the length of the found packet or packet_error.
   net->read_pos points to the read data.
*/

static uint32_t
drizzleclient_net_read(NET *net)
{
  size_t len, complen;

  if (not net->compress)
  {
    len = my_real_read(net,&complen);
    if (len == MAX_PACKET_LENGTH)
    {
      /* First packet of a multi-packet.  Concatenate the packets */
      uint32_t save_pos = net->where_b;
      size_t total_length= 0;

      do
      {
        net->where_b += len;
        total_length += len;
        len = my_real_read(net,&complen);
      } while (len == MAX_PACKET_LENGTH);

      if (len != packet_error)
      {
        len+= total_length;
      }
      net->where_b = save_pos;
    }
    net->read_pos = net->buff + net->where_b;

    if (len != packet_error)
      net->read_pos[len]=0;        /* Safeguard for drizzleclient_use_result */

    return len;
  }
  else
  {
    /* We are using the compressed protocol */

    uint32_t buf_length;
    uint32_t start_of_packet;
    uint32_t first_packet_offset;
    uint32_t read_length, multi_byte_packet=0;

    if (net->remain_in_buf)
    {
      buf_length= net->buf_length;        /* Data left in old packet */
      first_packet_offset= start_of_packet= (net->buf_length -
                                             net->remain_in_buf);
      /* Restore the character that was overwritten by the end 0 */
      net->buff[start_of_packet]= net->save_char;
    }
    else
    {
      /* reuse buffer, as there is nothing in it that we need */
      buf_length= start_of_packet= first_packet_offset= 0;
    }
    for (;;)
    {
      uint32_t packet_len;

      if (buf_length - start_of_packet >= NET_HEADER_SIZE)
      {
        read_length = uint3korr(net->buff+start_of_packet);
        if (!read_length)
        {
          /* End of multi-byte packet */
          start_of_packet += NET_HEADER_SIZE;
          break;
        }
        if (read_length + NET_HEADER_SIZE <= buf_length - start_of_packet)
        {
          if (multi_byte_packet)
          {
            /* Remove packet header for second packet */
            memmove(net->buff + first_packet_offset + start_of_packet,
                    net->buff + first_packet_offset + start_of_packet +
                    NET_HEADER_SIZE,
                    buf_length - start_of_packet);
            start_of_packet += read_length;
            buf_length -= NET_HEADER_SIZE;
          }
          else
            start_of_packet+= read_length + NET_HEADER_SIZE;

          if (read_length != MAX_PACKET_LENGTH)    /* last package */
          {
            multi_byte_packet= 0;        /* No last zero len packet */
            break;
          }
          multi_byte_packet= NET_HEADER_SIZE;
          /* Move data down to read next data packet after current one */
          if (first_packet_offset)
          {
            memmove(net->buff,net->buff+first_packet_offset,
                    buf_length-first_packet_offset);
            buf_length-=first_packet_offset;
            start_of_packet -= first_packet_offset;
            first_packet_offset=0;
          }
          continue;
        }
      }
      /* Move data down to read next data packet after current one */
      if (first_packet_offset)
      {
        memmove(net->buff,net->buff+first_packet_offset,
                buf_length-first_packet_offset);
        buf_length-=first_packet_offset;
        start_of_packet -= first_packet_offset;
        first_packet_offset=0;
      }

      net->where_b=buf_length;
      if ((packet_len = my_real_read(net,&complen)) == packet_error)
        return packet_error;

      if (complen)
      {
        unsigned char * compbuf= (unsigned char *) malloc(complen);
        if (compbuf != NULL)
        {
          uLongf tmp_complen= complen;
          int error= uncompress((Bytef*) compbuf, &tmp_complen,
                                (Bytef*) (net->buff + net->where_b),
                                (uLong)packet_len);
          complen= tmp_complen;

          if (error != Z_OK)
          {
            net->error_= 2;            /* caller will close socket */
            net->last_errno= CR_NET_UNCOMPRESS_ERROR;
          }
          else
          {
            memcpy((net->buff + net->where_b), compbuf, complen);
          }
          free(compbuf);
        }
      }
      else
      {
        complen= packet_len;
      }

    }
    buf_length+= complen;

    net->read_pos=      net->buff+ first_packet_offset + NET_HEADER_SIZE;
    net->buf_length=    buf_length;
    net->remain_in_buf= (uint32_t) (buf_length - start_of_packet);
    len = ((uint32_t) (start_of_packet - first_packet_offset) - NET_HEADER_SIZE -
           multi_byte_packet);
    net->save_char= net->read_pos[len];    /* Must be saved */
    net->read_pos[len]=0;        /* Safeguard for drizzleclient_use_result */
  }
  return len;
}

void NET::set_read_timeout(uint32_t timeout)
{
  read_timeout_= timeout;
#ifndef __sun
  if (vio)
    vio->timeout(0, timeout);
#endif
  return;
}

void NET::set_write_timeout(uint32_t timeout)
{
  write_timeout_= timeout;
#ifndef __sun
  if (vio)
    vio->timeout(1, timeout);
#endif
  return;
}

bool NET::write(const void* data, size_t size)
{
  return drizzleclient_net_write(this, data, size);
}

bool NET::write_command(unsigned char command,
  const unsigned char *header, size_t head_len,
  const unsigned char *packet, size_t len)
{
  return drizzleclient_net_write_command(this, command, header, head_len, packet, len);
}

uint32_t NET::read()
{
  return drizzleclient_net_read(this);
}

} /* namespace drizzle_plugin */
