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
  HFTODO this must be hidden if we don't want client capabilities in 
  embedded library
 */
#include <my_global.h>
#include <drizzle.h>
#include <drizzle_com.h>
#include <mysqld_error.h>
#include <my_sys.h>
#include <m_string.h>
#include <my_net.h>
#include <violite.h>
#include <signal.h>
#include <errno.h>

/*
  The following handles the differences when this is linked between the
  client and the server.

  This gives an error if a too big packet is found
  The server can change this with the -O switch, but because the client
  can't normally do this the client should have a bigger max_allowed_packet.
*/


#define DONT_USE_THR_ALARM

#include "thr_alarm.h"


#define update_statistics(A)
#define thd_increment_bytes_sent(N)

#define TEST_BLOCKING		8
#define MAX_PACKET_LENGTH (256L*256L*256L-1)

static bool net_write_buff(NET *net, const unsigned char *packet, uint32_t len);


/** Init with packet info. */

bool my_net_init(NET *net, Vio* vio)
{
  net->vio = vio;
  my_net_local_init(net);			/* Set some limits */
  if (!(net->buff=(uchar*) my_malloc((size_t) net->max_packet+
				     NET_HEADER_SIZE + COMP_HEADER_SIZE,
				     MYF(MY_WME))))
    return(1);
  net->buff_end=net->buff+net->max_packet;
  net->error=0; net->return_status=0;
  net->pkt_nr=net->compress_pkt_nr=0;
  net->write_pos=net->read_pos = net->buff;
  net->last_error[0]=0;
  net->compress=0; net->reading_or_writing=0;
  net->where_b = net->remain_in_buf=0;
  net->last_errno=0;
  net->unused= 0;

  if (vio != 0)					/* If real connection */
  {
    net->fd  = vio_fd(vio);			/* For perl DBI/DBD */
    vio_fastsend(vio);
  }
  return(0);
}


void net_end(NET *net)
{
  my_free(net->buff,MYF(MY_ALLOW_ZERO_PTR));
  net->buff=0;
  return;
}


/** Realloc the packet buffer. */

bool net_realloc(NET *net, size_t length)
{
  uchar *buff;
  size_t pkt_length;

  if (length >= net->max_packet_size)
  {
    /* @todo: 1 and 2 codes are identical. */
    net->error= 1;
    net->last_errno= ER_NET_PACKET_TOO_LARGE;
    return(1);
  }
  pkt_length = (length+IO_SIZE-1) & ~(IO_SIZE-1); 
  /*
    We must allocate some extra bytes for the end 0 and to be able to
    read big compressed blocks
  */
  if (!(buff= (uchar*) my_realloc((char*) net->buff, pkt_length +
                                  NET_HEADER_SIZE + COMP_HEADER_SIZE,
                                  MYF(MY_WME))))
  {
    /* @todo: 1 and 2 codes are identical. */
    net->error= 1;
    net->last_errno= ER_OUT_OF_RESOURCES;
    /* In the server the error is reported by MY_WME flag. */
    return(1);
  }
  net->buff=net->write_pos=buff;
  net->buff_end=buff+(net->max_packet= (ulong) pkt_length);
  return(0);
}


/**
  Check if there is any data to be read from the socket.

  @param sd   socket descriptor

  @retval
    0  No data to read
  @retval
    1  Data or EOF to read
  @retval
    -1   Don't know if data is ready or not
*/

static int net_data_is_ready(my_socket sd)
{
  struct pollfd ufds;
  int res;

  ufds.fd= sd;
  ufds.events= POLLIN | POLLPRI;
  if (!(res= poll(&ufds, 1, 0)))
    return 0;
  if (res < 0 || !(ufds.revents & (POLLIN | POLLPRI)))
    return 0;
  return 1;
}

/**
  Remove unwanted characters from connection
  and check if disconnected.

    Read from socket until there is nothing more to read. Discard
    what is read.

    If there is anything when to read 'net_clear' is called this
    normally indicates an error in the protocol.

    When connection is properly closed (for TCP it means with
    a FIN packet), then select() considers a socket "ready to read",
    in the sense that there's EOF to read, but read() returns 0.

  @param net			NET handler
  @param clear_buffer           if <> 0, then clear all data from comm buff
*/

void net_clear(NET *net, bool clear_buffer)
{
  size_t count;
  int32_t ready;

  if (clear_buffer)
  {
    while ((ready= net_data_is_ready(net->vio->sd)) > 0)
    {
      /* The socket is ready */
      if ((long) (count= vio_read(net->vio, net->buff,
                                  (size_t) net->max_packet)) <= 0)
      {
        net->error= 2;
        break;
      }
    }
  }
  net->pkt_nr=net->compress_pkt_nr=0;		/* Ready for new command */
  net->write_pos=net->buff;
  return;
}


/** Flush write_buffer if not empty. */

bool net_flush(NET *net)
{
  my_bool error= 0;
  if (net->buff != net->write_pos)
  {
    error=test(net_real_write(net, net->buff,
			      (size_t) (net->write_pos - net->buff)));
    net->write_pos=net->buff;
  }
  /* Sync packet number if using compression */
  if (net->compress)
    net->pkt_nr=net->compress_pkt_nr;
  return(error);
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

bool
my_net_write(NET *net,const uchar *packet,size_t len)
{
  uchar buff[NET_HEADER_SIZE];
  if (unlikely(!net->vio)) /* nowhere to write */
    return 0;
  /*
    Big packets are handled by splitting them in packets of MAX_PACKET_LENGTH
    length. The last packet is always a packet that is < MAX_PACKET_LENGTH.
    (The last packet may even have a length of 0)
  */
  while (len >= MAX_PACKET_LENGTH)
  {
    const ulong z_size = MAX_PACKET_LENGTH;
    int3store(buff, z_size);
    buff[3]= (uchar) net->pkt_nr++;
    if (net_write_buff(net, buff, NET_HEADER_SIZE) ||
	net_write_buff(net, packet, z_size))
      return 1;
    packet += z_size;
    len-=     z_size;
  }
  /* Write last packet */
  int3store(buff,len);
  buff[3]= (uchar) net->pkt_nr++;
  if (net_write_buff(net, buff, NET_HEADER_SIZE))
    return 1;
  return test(net_write_buff(net,packet,len));
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

  @param net		NET handler
  @param command	Command in MySQL server (enum enum_server_command)
  @param header	Header to write after command
  @param head_len	Length of header
  @param packet	Query or parameter to query
  @param len		Length of packet

  @retval
    0	ok
  @retval
    1	error
*/

bool
net_write_command(NET *net,uchar command,
		  const uchar *header, size_t head_len,
		  const uchar *packet, size_t len)
{
  ulong length=len+1+head_len;			/* 1 extra byte for command */
  uchar buff[NET_HEADER_SIZE+1];
  uint header_size=NET_HEADER_SIZE+1;

  buff[4]=command;				/* For first packet */

  if (length >= MAX_PACKET_LENGTH)
  {
    /* Take into account that we have the command in the first header */
    len= MAX_PACKET_LENGTH - 1 - head_len;
    do
    {
      int3store(buff, MAX_PACKET_LENGTH);
      buff[3]= (uchar) net->pkt_nr++;
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
    len=length;					/* Data left to be written */
  }
  int3store(buff,length);
  buff[3]= (uchar) net->pkt_nr++;
  return(test(net_write_buff(net, buff, header_size) ||
                   (head_len && net_write_buff(net, header, head_len)) ||
                   net_write_buff(net, packet, len) || net_flush(net)));
}

/**
  Caching the data in a local buffer before sending it.

   Fill up net->buffer and send it to the client when full.

    If the rest of the to-be-sent-packet is bigger than buffer,
    send it in one big block (to avoid copying to internal buffer).
    If not, copy the rest of the data to the buffer and return without
    sending data.

  @param net		Network handler
  @param packet	Packet to send
  @param len		Length of packet

  @note
    The cached buffer can be sent as it is with 'net_flush()'.
    In this code we have to be careful to not send a packet longer than
    MAX_PACKET_LENGTH to net_real_write() if we are using the compressed
    protocol as we store the length of the compressed packet in 3 bytes.

  @retval
    0	ok
  @retval
    1
*/

static bool
net_write_buff(NET *net, const unsigned char *packet, uint32_t len)
{
  ulong left_length;
  if (net->compress && net->max_packet > MAX_PACKET_LENGTH)
    left_length= MAX_PACKET_LENGTH - (net->write_pos - net->buff);
  else
    left_length= (ulong) (net->buff_end - net->write_pos);

  if (len > left_length)
  {
    if (net->write_pos != net->buff)
    {
      /* Fill up already used packet and write it */
      memcpy((char*) net->write_pos,packet,left_length);
      if (net_real_write(net, net->buff, 
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
	if (net_real_write(net, packet, left_length))
	  return 1;
	packet+= left_length;
	len-= left_length;
      }
    }
    if (len > net->max_packet)
      return net_real_write(net, packet, len) ? 1 : 0;
    /* Send out rest of the blocks as full sized blocks */
  }
  memcpy((char*) net->write_pos,packet,len);
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
int
net_real_write(NET *net,const uchar *packet, size_t len)
{
  size_t length;
  const uchar *pos,*end;
  uint retry_count= 0;

  /* Backup of the original SO_RCVTIMEO timeout */
  struct timeval backtime;
  int error;

  if (net->error == 2)
    return(-1);				/* socket can't be used */

  net->reading_or_writing=2;
  if (net->compress)
  {
    size_t complen;
    uchar *b;
    uint header_length=NET_HEADER_SIZE+COMP_HEADER_SIZE;
    if (!(b= (uchar*) my_malloc(len + NET_HEADER_SIZE +
                                COMP_HEADER_SIZE, MYF(MY_WME))))
    {
      net->error= 2;
      net->last_errno= ER_OUT_OF_RESOURCES;
      /* In the server, the error is reported by MY_WME flag. */
      net->reading_or_writing= 0;
      return(1);
    }
    memcpy(b+header_length,packet,len);

    if (my_compress(b+header_length, &len, &complen))
      complen=0;
    int3store(&b[NET_HEADER_SIZE],complen);
    int3store(b,len);
    b[3]=(uchar) (net->compress_pkt_nr++);
    len+= header_length;
    packet= b;
  }

  /* Check for error, currently assert */
  if (net->write_timeout)
  {
    struct timeval waittime;
    socklen_t length;

    waittime.tv_sec= net->write_timeout;
    waittime.tv_usec= 0;

    memset(&backtime, 0, sizeof(struct timeval));
    length= sizeof(struct timeval);
    error= getsockopt(net->vio->sd, SOL_SOCKET, SO_RCVTIMEO, 
                      &backtime, &length);
    if (error != 0)
    {
      perror("getsockopt");
      assert(error == 0);
    }
    error= setsockopt(net->vio->sd, SOL_SOCKET, SO_RCVTIMEO, 
                      &waittime, (socklen_t)sizeof(struct timeval));
    assert(error == 0);
  }
  pos= packet;
  end=pos+len;
  /* Loop until we have read everything */
  while (pos != end)
  {
    if ((long) (length= vio_write(net->vio,pos,(size_t) (end-pos))) <= 0)
    {
      my_bool interrupted= vio_should_retry(net->vio);
      /* 
        If we read 0, or we were interrupted this means that 
        we need to switch to blocking mode and wait until the timeout 
        on the socket kicks in.
      */
      if ((interrupted || length == 0))
      {
        bool old_mode;

        while (vio_blocking(net->vio, true, &old_mode) < 0)
        {
          if (vio_should_retry(net->vio) && retry_count++ < net->retry_count)
            continue;
          net->error= 2;                     /* Close socket */
          net->last_errno= ER_NET_PACKET_TOO_LARGE;
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
      
      if (vio_errno(net->vio) == SOCKET_EINTR)
      {
        continue;
      }
      net->error= 2;				/* Close socket */
      net->last_errno= (interrupted ? ER_NET_WRITE_INTERRUPTED :
                        ER_NET_ERROR_ON_WRITE);
      break;
    }
    pos+=length;
    update_statistics(thd_increment_bytes_sent(length));
  }
 end:
  if (net->compress)
    my_free((char*) packet,MYF(0));
  net->reading_or_writing=0;

  if (net->write_timeout)
    error= setsockopt(net->vio->sd, SOL_SOCKET, SO_RCVTIMEO, 
                      &backtime, (socklen_t)sizeof(struct timeval));

  return(((int) (pos != end)));
}


/**
  Reads one packet to net->buff + net->where_b.
  Long packets are handled by my_net_read().
  This function reallocates the net->buff buffer if necessary.

  @return
    Returns length of packet.
*/

static ulong
my_real_read(NET *net, size_t *complen)
{
  uchar *pos;
  size_t length;
  uint i,retry_count=0;
  ulong len=packet_error;
  uint32_t remain= (net->compress ? NET_HEADER_SIZE+COMP_HEADER_SIZE :
                  NET_HEADER_SIZE);
  /* Backup of the original SO_RCVTIMEO timeout */
  struct timeval backtime;
  int error;

  *complen = 0;

  net->reading_or_writing= 1;
  /* Read timeout is set in my_net_set_read_timeout */

  pos = net->buff + net->where_b;		/* net->packet -4 */


  /* Check for error, currently assert */
  if (net->read_timeout)
  {
    struct timeval waittime;
    socklen_t length;

    waittime.tv_sec= net->read_timeout;
    waittime.tv_usec= 0;

    memset(&backtime, 0, sizeof(struct timeval));
    length= sizeof(struct timeval);
    error= getsockopt(net->vio->sd, SOL_SOCKET, SO_RCVTIMEO, 
                      &backtime, &length);
    if (error != 0)
    {
      perror("getsockopt");
      assert(error == 0);
    }
    error= setsockopt(net->vio->sd, SOL_SOCKET, SO_RCVTIMEO, 
                      &waittime, (socklen_t)sizeof(struct timeval));
    assert(error == 0);
  }

  for (i= 0; i < 2 ; i++)
  {
    while (remain > 0)
    {
      /* First read is done with non blocking mode */
      if ((long) (length= vio_read(net->vio, pos, remain)) <= 0L)
      {
        bool interrupted = vio_should_retry(net->vio);

        if (interrupted)
        {					/* Probably in MIT threads */
          if (retry_count++ < net->retry_count)
            continue;
        }
        if (vio_errno(net->vio) == SOCKET_EINTR)
        {
          continue;
        }
        len= packet_error;
        net->error= 2;				/* Close socket */
        net->last_errno= (vio_was_interrupted(net->vio) ?
                          ER_NET_READ_INTERRUPTED :
                          ER_NET_READ_ERROR);
        goto end;
      }
      remain -= (uint32_t) length;
      pos+= length;
      update_statistics(thd_increment_bytes_received(length));
    }
    if (i == 0)
    {					/* First parts is packet length */
      ulong helping;

      if (net->buff[net->where_b + 3] != (uchar) net->pkt_nr)
      {
        len= packet_error;
        /* Not a NET error on the client. XXX: why? */
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
      if (!len)				/* End of big multi-packet */
        goto end;
      helping = max(len,*complen) + net->where_b;
      /* The necessary size of net->buff */
      if (helping >= net->max_packet)
      {
        if (net_realloc(net,helping))
        {
          len= packet_error;          /* Return error and close connection */
          goto end;
        }
      }
      pos=net->buff + net->where_b;
      remain = (uint32_t) len;
    }
  }

end:
  if  (net->read_timeout)
    error= setsockopt(net->vio->sd, SOL_SOCKET, SO_RCVTIMEO, 
                      &backtime, (socklen_t)sizeof(struct timeval));
  assert(error == 0);
  net->reading_or_writing= 0;

  return(len);
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

uint32_t
my_net_read(NET *net)
{
  size_t len, complen;

  if (!net->compress)
  {
    len = my_real_read(net,&complen);
    if (len == MAX_PACKET_LENGTH)
    {
      /* First packet of a multi-packet.  Concatenate the packets */
      ulong save_pos = net->where_b;
      size_t total_length= 0;
      do
      {
	net->where_b += len;
	total_length += len;
	len = my_real_read(net,&complen);
      } while (len == MAX_PACKET_LENGTH);
      if (len != packet_error)
	len+= total_length;
      net->where_b = save_pos;
    }
    net->read_pos = net->buff + net->where_b;
    if (len != packet_error)
      net->read_pos[len]=0;		/* Safeguard for drizzle_use_result */
    return len;
  }
  else
  {
    /* We are using the compressed protocol */

    ulong buf_length;
    ulong start_of_packet;
    ulong first_packet_offset;
    uint read_length, multi_byte_packet=0;

    if (net->remain_in_buf)
    {
      buf_length= net->buf_length;		/* Data left in old packet */
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
      ulong packet_len;

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

	  if (read_length != MAX_PACKET_LENGTH)	/* last package */
	  {
	    multi_byte_packet= 0;		/* No last zero len packet */
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
      if (my_uncompress(net->buff + net->where_b, packet_len,
			&complen))
      {
	net->error= 2;			/* caller will close socket */
        net->last_errno= ER_NET_UNCOMPRESS_ERROR;
	return packet_error;
      }
      buf_length+= complen;
    }

    net->read_pos=      net->buff+ first_packet_offset + NET_HEADER_SIZE;
    net->buf_length=    buf_length;
    net->remain_in_buf= (ulong) (buf_length - start_of_packet);
    len = ((ulong) (start_of_packet - first_packet_offset) - NET_HEADER_SIZE -
           multi_byte_packet);
    net->save_char= net->read_pos[len];	/* Must be saved */
    net->read_pos[len]=0;		/* Safeguard for drizzle_use_result */
  }
  return len;
}


void my_net_set_read_timeout(NET *net, uint timeout)
{
  net->read_timeout= timeout;
  if (net->vio)
    vio_timeout(net->vio, 0, timeout);
  return;
}


void my_net_set_write_timeout(NET *net, uint timeout)
{
  net->write_timeout= timeout;
  if (net->vio)
    vio_timeout(net->vio, 1, timeout);
  return;
}
