/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
  @file

  Low level functions for storing data to be send to the MySQL client.
  The actual communction is handled by the net_xxx functions in net_serv.cc
*/

#include <drizzled/server_includes.h>
#include <drizzled/error.h>
#include <drizzled/sql_state.h>
#include <drizzled/protocol.h>
#include <drizzled/session.h>
#include <drizzled/data_home.h>
#include "pack.h"
#include "errmsg.h"
#include "oldlibdrizzle.h"

/*
  Function called by drizzleclient_net_init() to set some check variables
*/

static const unsigned int PACKET_BUFFER_EXTRA_ALLOC= 1024;

static void write_eof_packet(Session *session, NET *net,
                             uint32_t server_status, uint32_t total_warn_count);

bool ProtocolOldLibdrizzle::isConnected()
{
  return net.vio != 0;
}

void ProtocolOldLibdrizzle::setReadTimeout(uint32_t timeout)
{
  drizzleclient_net_set_read_timeout(&net, timeout);
}

void ProtocolOldLibdrizzle::setWriteTimeout(uint32_t timeout)
{
  drizzleclient_net_set_write_timeout(&net, timeout);
}

void ProtocolOldLibdrizzle::setRetryCount(uint32_t count)
{
  net.retry_count=count;
}

void ProtocolOldLibdrizzle::setError(char error)
{
  net.error= error;
}

bool ProtocolOldLibdrizzle::haveError(void)
{
  return net.error || net.vio == 0;
}

bool ProtocolOldLibdrizzle::wasAborted(void)
{
  return net.error && net.vio != 0;
}

bool ProtocolOldLibdrizzle::haveMoreData(void)
{
  return drizzleclient_net_more_data(&net);
}

void ProtocolOldLibdrizzle::enableCompression(void)
{
  net.compress= true;
}

bool ProtocolOldLibdrizzle::isReading(void)
{
  return net.reading_or_writing == 1;
}

bool ProtocolOldLibdrizzle::isWriting(void)
{
  return net.reading_or_writing == 2;
}

bool ProtocolOldLibdrizzle::netStoreData(const unsigned char *from, size_t length)
{
  size_t packet_length= packet->length();
  /*
     The +9 comes from that strings of length longer than 16M require
     9 bytes to be stored (see drizzleclient_net_store_length).
  */
  if (packet_length+9+length > packet->alloced_length() &&
      packet->realloc(packet_length+9+length))
    return 1;
  unsigned char *to= drizzleclient_net_store_length((unsigned char*) packet->ptr()+packet_length, length);
  memcpy(to,from,length);
  packet->length((size_t) (to+length-(unsigned char*) packet->ptr()));
  return 0;
}


/*
  netStoreData() - extended version with character set conversion.

  It is optimized for short strings whose length after
  conversion is garanteed to be less than 251, which accupies
  exactly one byte to store length. It allows not to use
  the "convert" member as a temporary buffer, conversion
  is done directly to the "packet" member.
  The limit 251 is good enough to optimize send_fields()
  because column, table, database names fit into this limit.
*/

bool ProtocolOldLibdrizzle::netStoreData(const unsigned char *from, size_t length,
                              const CHARSET_INFO * const from_cs,
                              const CHARSET_INFO * const to_cs)
{
  uint32_t dummy_errors;
  /* Calculate maxumum possible result length */
  uint32_t conv_length= to_cs->mbmaxlen * length / from_cs->mbminlen;
  if (conv_length > 250)
  {
    /*
      For strings with conv_length greater than 250 bytes
      we don't know how many bytes we will need to store length: one or two,
      because we don't know result length until conversion is done.
      For example, when converting from utf8 (mbmaxlen=3) to latin1,
      conv_length=300 means that the result length can vary between 100 to 300.
      length=100 needs one byte, length=300 needs to bytes.

      Thus conversion directly to "packet" is not worthy.
      Let's use "convert" as a temporary buffer.
    */
    return (convert->copy((const char*) from, length, from_cs,
                          to_cs, &dummy_errors) ||
            netStoreData((const unsigned char*) convert->ptr(), convert->length()));
  }

  size_t packet_length= packet->length();
  size_t new_length= packet_length + conv_length + 1;

  if (new_length > packet->alloced_length() && packet->realloc(new_length))
    return 1;

  char *length_pos= (char*) packet->ptr() + packet_length;
  char *to= length_pos + 1;

  to+= copy_and_convert(to, conv_length, to_cs,
                        (const char*) from, length, from_cs, &dummy_errors);

  drizzleclient_net_store_length((unsigned char*) length_pos, to - length_pos - 1);
  packet->length((uint32_t) (to - packet->ptr()));
  return 0;
}


/**
  Return ok to the client.

  The ok packet has the following structure:

  - 0               : Marker (1 byte)
  - affected_rows    : Stored in 1-9 bytes
  - id        : Stored in 1-9 bytes
  - server_status    : Copy of session->server_status;  Can be used by client
  to check if we are inside an transaction.
  New in 4.0 protocol
  - warning_count    : Stored in 2 bytes; New in 4.1 protocol
  - message        : Stored as packed length (1-9 bytes) + message.
  Is not stored if no message.

  @param session           Thread handler
  @param affected_rows       Number of rows changed by statement
  @param id           Auto_increment id for first row (if used)
  @param message       Message to send to the client (Used by mysql_status)
*/

void ProtocolOldLibdrizzle::sendOK()
{
  unsigned char buff[DRIZZLE_ERRMSG_SIZE+10],*pos;
  const char *message= NULL;
  uint32_t tmp;

  if (!net.vio)    // hack for re-parsing queries
  {
    return;
  }

  buff[0]=0;                    // No fields
  if (session->main_da.status() == Diagnostics_area::DA_OK)
  {
    pos=drizzleclient_net_store_length(buff+1,session->main_da.affected_rows());
    pos=drizzleclient_net_store_length(pos, session->main_da.last_insert_id());
    int2store(pos, session->main_da.server_status());
    pos+=2;
    tmp= cmin(session->main_da.total_warn_count(), (uint32_t)65535);
    message= session->main_da.message();
  }
  else
  {
    pos=drizzleclient_net_store_length(buff+1,0);
    pos=drizzleclient_net_store_length(pos, 0);
    int2store(pos, session->server_status);
    pos+=2;
    tmp= cmin(session->total_warn_count, (uint32_t)65535);
  }

  /* We can only return up to 65535 warnings in two bytes */
  int2store(pos, tmp);
  pos+= 2;

  session->main_da.can_overwrite_status= true;

  if (message && message[0])
  {
    size_t length= strlen(message);
    pos=drizzleclient_net_store_length(pos,length);
    memcpy(pos,(unsigned char*) message,length);
    pos+=length;
  }
  drizzleclient_net_write(&net, buff, (size_t) (pos-buff));
  drizzleclient_net_flush(&net);

  session->main_da.can_overwrite_status= false;
}

/**
  Send eof (= end of result set) to the client.

  The eof packet has the following structure:

  - 254    (DRIZZLE_PROTOCOL_NO_MORE_DATA)    : Marker (1 byte)
  - warning_count    : Stored in 2 bytes; New in 4.1 protocol
  - status_flag    : Stored in 2 bytes;
  For flags like SERVER_MORE_RESULTS_EXISTS.

  Note that the warning count will not be sent if 'no_flush' is set as
  we don't want to report the warning count until all data is sent to the
  client.
*/

void ProtocolOldLibdrizzle::sendEOF()
{
  /* Set to true if no active vio, to work well in case of --init-file */
  if (net.vio != 0)
  {
    session->main_da.can_overwrite_status= true;
    write_eof_packet(session, &net, session->main_da.server_status(),
                     session->main_da.total_warn_count());
    drizzleclient_net_flush(&net);
    session->main_da.can_overwrite_status= false;
  }
}


/**
  Format EOF packet according to the current protocol and
  write it to the network output buffer.
*/

static void write_eof_packet(Session *session, NET *net,
                             uint32_t server_status,
                             uint32_t total_warn_count)
{
  unsigned char buff[5];
  /*
    Don't send warn count during SP execution, as the warn_list
    is cleared between substatements, and mysqltest gets confused
  */
  uint32_t tmp= cmin(total_warn_count, (uint32_t)65535);
  buff[0]= DRIZZLE_PROTOCOL_NO_MORE_DATA;
  int2store(buff+1, tmp);
  /*
    The following test should never be true, but it's better to do it
    because if 'is_fatal_error' is set the server is not going to execute
    other queries (see the if test in dispatch_command / COM_QUERY)
  */
  if (session->is_fatal_error)
    server_status&= ~SERVER_MORE_RESULTS_EXISTS;
  int2store(buff + 3, server_status);
  drizzleclient_net_write(net, buff, 5);
}

void ProtocolOldLibdrizzle::sendError(uint32_t sql_errno, const char *err)
{
  uint32_t length;
  /*
    buff[]: sql_errno:2 + ('#':1 + SQLSTATE_LENGTH:5) + DRIZZLE_ERRMSG_SIZE:512
  */
  unsigned char buff[2+1+SQLSTATE_LENGTH+DRIZZLE_ERRMSG_SIZE], *pos;

  assert(sql_errno);
  assert(err && err[0]);

  /*
    It's one case when we can push an error even though there
    is an OK or EOF already.
  */
  session->main_da.can_overwrite_status= true;

  /* Abort multi-result sets */
  session->server_status&= ~SERVER_MORE_RESULTS_EXISTS;

  /**
    Send a error string to client.

    For SIGNAL/RESIGNAL and GET DIAGNOSTICS functionality it's
    critical that every error that can be intercepted is issued in one
    place only, my_message_sql.
  */

  if (net.vio == 0)
  {
    return;
  }

  int2store(buff,sql_errno);
  pos= buff+2;

  /* The first # is to make the protocol backward compatible */
  buff[2]= '#';
  pos= (unsigned char*) strcpy((char*) buff+3, drizzle_errno_to_sqlstate(sql_errno));
  pos+= strlen(drizzle_errno_to_sqlstate(sql_errno));

  char *tmp= strncpy((char*)pos, err, DRIZZLE_ERRMSG_SIZE-1);
  tmp+= strlen((char*)pos);
  tmp[0]= '\0';
  length= (uint32_t)(tmp-(char*)buff);
  err= (char*) buff;

  drizzleclient_net_write_command(&net,(unsigned char) 255, (unsigned char*) "", 0, (unsigned char*) err, length);

  session->main_da.can_overwrite_status= false;
}


ProtocolOldLibdrizzle::ProtocolOldLibdrizzle()
{
  scramble[0]= 0;
  net.vio= 0;
}

void ProtocolOldLibdrizzle::setSession(Session *session_arg)
{
  session= session_arg;
  packet= &session->packet;
  convert= &session->convert_buffer;
}


/**
  Send name and type of result to client.

  Sum fields has table name empty and field_name.

  @param Session        Thread data object
  @param list            List of items to send to client
  @param flag            Bit mask with the following functions:
                        - 1 send number of rows
                        - 2 send default values
                        - 4 don't write eof packet

  @retval
    0    ok
  @retval
    1    Error  (Note that in this case the error is not sent to the
    client)
*/
bool ProtocolOldLibdrizzle::sendFields(List<Item> *list, uint32_t flags)
{
  List_iterator_fast<Item> it(*list);
  Item *item;
  unsigned char buff[80];
  String tmp((char*) buff,sizeof(buff),&my_charset_bin);

  if (flags & SEND_NUM_ROWS)
  {                // Packet with number of elements
    unsigned char *pos= drizzleclient_net_store_length(buff, list->elements);
    (void) drizzleclient_net_write(&net, buff, (size_t) (pos-buff));
  }

  while ((item=it++))
  {
    char *pos;
    const CHARSET_INFO * const cs= system_charset_info;
    Send_field field;
    item->make_field(&field);

    prepareForResend();

    if (store(STRING_WITH_LEN("def"), cs) ||
        store(field.db_name, cs) ||
        store(field.table_name, cs) ||
        store(field.org_table_name, cs) ||
        store(field.col_name, cs) ||
        store(field.org_col_name, cs) ||
        packet->realloc(packet->length()+12))
      goto err;

    /* Store fixed length fields */
    pos= (char*) packet->ptr()+packet->length();
    *pos++= 12;                // Length of packed fields
    if (item->collation.collation == &my_charset_bin)
    {
      /* No conversion */
      int2store(pos, field.charsetnr);
      int4store(pos+2, field.length);
    }
    else
    {
      /* With conversion */
      uint32_t max_char_len;
      int2store(pos, cs->number);
      /*
        For TEXT/BLOB columns, field_length describes the maximum data
        length in bytes. There is no limit to the number of characters
        that a TEXT column can store, as long as the data fits into
        the designated space.
        For the rest of textual columns, field_length is evaluated as
        char_count * mbmaxlen, where character count is taken from the
        definition of the column. In other words, the maximum number
        of characters here is limited by the column definition.
      */
      max_char_len= field.length / item->collation.collation->mbmaxlen;
      int4store(pos+2, max_char_len * cs->mbmaxlen);
    }
    pos[6]= field.type;
    int2store(pos+7,field.flags);
    pos[9]= (char) field.decimals;
    pos[10]= 0;                // For the future
    pos[11]= 0;                // For the future
    pos+= 12;

    packet->length((uint32_t) (pos - packet->ptr()));
    if (flags & SEND_DEFAULTS)
      item->send(this, &tmp);            // Send default value
    if (write())
      break;                    /* purecov: inspected */
  }

  if (flags & SEND_EOF)
  {
    /*
      Mark the end of meta-data result set, and store session->server_status,
      to show that there is no cursor.
      Send no warning information, as it will be sent at statement end.
    */
    write_eof_packet(session, &net, session->server_status, session->total_warn_count);
  }

  field_count= list->elements;
  return 0;

err:
  my_message(ER_OUT_OF_RESOURCES, ER(ER_OUT_OF_RESOURCES),
             MYF(0));    /* purecov: inspected */
  return 1;                /* purecov: inspected */
}


bool ProtocolOldLibdrizzle::write()
{
  return(drizzleclient_net_write(&net, (unsigned char*) packet->ptr(),
                           packet->length()));
}

void ProtocolOldLibdrizzle::free()
{
  packet->free();
}


void ProtocolOldLibdrizzle::setRandom(uint64_t seed1, uint64_t seed2)
{
  drizzleclient_randominit(&rand, seed1, seed2);
}

bool ProtocolOldLibdrizzle::setFileDescriptor(int fd)
{
  if (drizzleclient_net_init_sock(&net, fd, 0))
    return true;
  return false;
}

int ProtocolOldLibdrizzle::fileDescriptor(void)
{
  return drizzleclient_net_get_sd(&net);
}

bool ProtocolOldLibdrizzle::authenticate()
{
  bool connection_is_valid;

  /* Use "connect_timeout" value during connection phase */
  drizzleclient_net_set_read_timeout(&net, connect_timeout);
  drizzleclient_net_set_write_timeout(&net, connect_timeout);

  connection_is_valid= checkConnection();

  if (connection_is_valid)
    sendOK();
  else
  {
    sendError(session->main_da.sql_errno(), session->main_da.message());
    return false;
  }

  /* Connect completed, set read/write timeouts back to default */
  drizzleclient_net_set_read_timeout(&net,
                                     session->variables.net_read_timeout);
  drizzleclient_net_set_write_timeout(&net,
                                      session->variables.net_write_timeout);
  return true;
}

bool ProtocolOldLibdrizzle::readCommand(char **l_packet, uint32_t *packet_length)
{
  /*
    This thread will do a blocking read from the client which
    will be interrupted when the next command is received from
    the client, the connection is closed or "net_wait_timeout"
    number of seconds has passed
  */
#ifdef NEVER
  /* We can do this much more efficiently with poll timeouts or watcher thread,
     disabling for now, which means net_wait_timeout == read_timeout. */
  drizzleclient_net_set_read_timeout(&net,
                                     session->variables.net_wait_timeout);
#endif

  session->clear_error();
  session->main_da.reset_diagnostics_area();

  net.pkt_nr=0;

  *packet_length= drizzleclient_net_read(&net);
  if (*packet_length == packet_error)
  {
    /* Check if we can continue without closing the connection */

    if(net.last_errno== CR_NET_PACKET_TOO_LARGE)
      my_error(ER_NET_PACKET_TOO_LARGE, MYF(0));
    if (session->main_da.status() == Diagnostics_area::DA_ERROR)
      sendError(session->main_da.sql_errno(), session->main_da.message());
    else
      session->protocol->sendOK();

    if (net.error != 3)
      return false;                       // We have to close it.

    net.error= 0;
    *packet_length= 0;
    return true;
  }

  *l_packet= (char*) net.read_pos;

  /*
    'packet_length' contains length of data, as it was stored in packet
    header. In case of malformed header, drizzleclient_net_read returns zero.
    If packet_length is not zero, drizzleclient_net_read ensures that the returned
    number of bytes was actually read from network.
    There is also an extra safety measure in drizzleclient_net_read:
    it sets packet[packet_length]= 0, but only for non-zero packets.
  */

  if (*packet_length == 0)                       /* safety */
  {
    /* Initialize with COM_SLEEP packet */
    (*l_packet)[0]= (unsigned char) COM_SLEEP;
    *packet_length= 1;
  }
  /* Do not rely on drizzleclient_net_read, extra safety against programming errors. */
  (*l_packet)[*packet_length]= '\0';                  /* safety */

#ifdef NEVER
  /* See comment above. */
  /* Restore read timeout value */
  drizzleclient_net_set_read_timeout(&net,
                                     session->variables.net_read_timeout);
#endif

  return true;
}

void ProtocolOldLibdrizzle::close(void)
{
  if (net.vio)
  { 
    drizzleclient_net_close(&net);
    drizzleclient_net_end(&net);
  }
}

void ProtocolOldLibdrizzle::forceClose(void)
{
  if (net.vio)
    drizzleclient_vio_close(net.vio);
}

void ProtocolOldLibdrizzle::prepareForResend()
{
  packet->length(0);
}

bool ProtocolOldLibdrizzle::store(void)
{
  char buff[1];
  buff[0]= (char)251;
  return packet->append(buff, sizeof(buff), PACKET_BUFFER_EXTRA_ALLOC);
}


/**
  Auxilary function to convert string to the given character set
  and store in network buffer.
*/

bool ProtocolOldLibdrizzle::storeString(const char *from, size_t length,
                                        const CHARSET_INFO * const fromcs,
                                        const CHARSET_INFO * const tocs)
{
  /* 'tocs' is set 0 when client issues SET character_set_results=NULL */
  if (tocs && !my_charset_same(fromcs, tocs) &&
      fromcs != &my_charset_bin &&
      tocs != &my_charset_bin)
  {
    /* Store with conversion */
    return netStoreData((unsigned char*) from, length, fromcs, tocs);
  }
  /* Store without conversion */
  return netStoreData((unsigned char*) from, length);
}


bool ProtocolOldLibdrizzle::store(const char *from, size_t length,
                          const CHARSET_INFO * const fromcs)
{
  const CHARSET_INFO * const tocs= default_charset_info;
  return storeString(from, length, fromcs, tocs);
}


bool ProtocolOldLibdrizzle::store(int32_t from)
{
  char buff[12];
  return netStoreData((unsigned char*) buff,
                      (size_t) (int10_to_str(from, buff, -10) - buff));
}

bool ProtocolOldLibdrizzle::store(uint32_t from)
{
  char buff[11];
  return netStoreData((unsigned char*) buff,
                      (size_t) (int10_to_str(from, buff, 10) - buff));
}

bool ProtocolOldLibdrizzle::store(int64_t from)
{
  char buff[22];
  return netStoreData((unsigned char*) buff,
                      (size_t) (int64_t10_to_str(from, buff, -10) - buff));
}

bool ProtocolOldLibdrizzle::store(uint64_t from)
{
  char buff[21];
  return netStoreData((unsigned char*) buff,
                      (size_t) (int64_t10_to_str(from, buff, 10) - buff));
}


bool ProtocolOldLibdrizzle::store(double from, uint32_t decimals, String *buffer)
{
  buffer->set_real(from, decimals, session->charset());
  return netStoreData((unsigned char*) buffer->ptr(), buffer->length());
}


bool ProtocolOldLibdrizzle::store(Field *from)
{
  if (from->is_null())
    return store();
  char buff[MAX_FIELD_WIDTH];
  String str(buff,sizeof(buff), &my_charset_bin);
  const CHARSET_INFO * const tocs= default_charset_info;

  from->val_str(&str);

  return storeString(str.ptr(), str.length(), str.charset(), tocs);
}


/**
  @todo
    Second_part format ("%06") needs to change when
    we support 0-6 decimals for time.
*/

bool ProtocolOldLibdrizzle::store(const DRIZZLE_TIME *tm)
{
  char buff[40];
  uint32_t length;
  uint32_t day;

  switch (tm->time_type)
  {
  case DRIZZLE_TIMESTAMP_DATETIME:
    length= sprintf(buff, "%04d-%02d-%02d %02d:%02d:%02d",
                    (int) tm->year,
                    (int) tm->month,
                    (int) tm->day,
                    (int) tm->hour,
                    (int) tm->minute,
                    (int) tm->second);
    if (tm->second_part)
      length+= sprintf(buff+length, ".%06d", (int)tm->second_part);
    break;

  case DRIZZLE_TIMESTAMP_DATE:
    length= sprintf(buff, "%04d-%02d-%02d",
                    (int) tm->year,
                    (int) tm->month,
                    (int) tm->day);
    break;

  case DRIZZLE_TIMESTAMP_TIME:
    day= (tm->year || tm->month) ? 0 : tm->day;
    length= sprintf(buff, "%s%02ld:%02d:%02d", tm->neg ? "-" : "",
                    (long) day*24L+(long) tm->hour, (int) tm->minute,
                    (int) tm->second);
    if (tm->second_part)
      length+= sprintf(buff+length, ".%06d", (int)tm->second_part);
    break;

  case DRIZZLE_TIMESTAMP_NONE:
  case DRIZZLE_TIMESTAMP_ERROR:
  default:
    assert(0);
    return false;
  }

  return netStoreData((unsigned char*) buff, length);
}

bool ProtocolOldLibdrizzle::checkConnection(void)
{
  uint32_t pkt_len= 0;
  char *end;

  // TCP/IP connection
  {
    char ip[NI_MAXHOST];

    if (drizzleclient_net_peer_addr(&net, ip, &session->peer_port, NI_MAXHOST))
    {
      my_error(ER_BAD_HOST_ERROR, MYF(0), session->security_ctx.ip.c_str());
      return false;
    }

    session->security_ctx.ip.assign(ip);
  }
  drizzleclient_net_keepalive(&net, true);

  uint32_t server_capabilites;
  {
    /* buff[] needs to big enough to hold the server_version variable */
    char buff[SERVER_VERSION_LENGTH + SCRAMBLE_LENGTH + 64];

    server_capabilites= CLIENT_BASIC_FLAGS;

#ifdef HAVE_COMPRESS
    server_capabilites|= CLIENT_COMPRESS;
#endif /* HAVE_COMPRESS */

    end= buff + strlen(server_version);
    if ((end - buff) >= SERVER_VERSION_LENGTH)
      end= buff + (SERVER_VERSION_LENGTH - 1);
    memcpy(buff, server_version, end - buff);
    *end= 0;
    end++;

    int4store((unsigned char*) end, thread_id);
    end+= 4;
    /*
      So as _checkConnection is the only entry point to authorization
      procedure, scramble is set here. This gives us new scramble for
      each handshake.
    */
    drizzleclient_create_random_string(scramble, SCRAMBLE_LENGTH, &rand);
    /*
      Old clients does not understand long scrambles, but can ignore packet
      tail: that's why first part of the scramble is placed here, and second
      part at the end of packet.
    */
    end= strncpy(end, scramble, SCRAMBLE_LENGTH_323);
    end+= SCRAMBLE_LENGTH_323;

    *end++= 0; /* an empty byte for some reason */

    int2store(end, server_capabilites);
    /* write server characteristics: up to 16 bytes allowed */
    end[2]=(char) default_charset_info->number;
    int2store(end+3, session->server_status);
    memset(end+5, 0, 13);
    end+= 18;
    /* write scramble tail */
    size_t scramble_len= SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323;
    end= strncpy(end, scramble + SCRAMBLE_LENGTH_323, scramble_len);
    end+= scramble_len;

    *end++= 0; /* an empty byte for some reason */

    /* At this point we write connection message and read reply */
    if (drizzleclient_net_write_command(&net
          , (unsigned char) protocol_version
          , (unsigned char*) ""
          , 0
          , (unsigned char*) buff
          , (size_t) (end-buff)) 
        ||    (pkt_len= drizzleclient_net_read(&net)) == packet_error 
        || pkt_len < MIN_HANDSHAKE_SIZE)
    {
      my_error(ER_HANDSHAKE_ERROR, MYF(0), session->security_ctx.ip.c_str());
      return false;
    }
  }
  if (session->packet.alloc(session->variables.net_buffer_length))
    return false; /* The error is set by alloc(). */

  session->client_capabilities= uint2korr(net.read_pos);


  session->client_capabilities|= ((uint32_t) uint2korr(net.read_pos + 2)) << 16;
  session->max_client_packet_length= uint4korr(net.read_pos + 4);
  session->update_charset();
  end= (char*) net.read_pos + 32;

  /*
    Disable those bits which are not supported by the server.
    This is a precautionary measure, if the client lies. See Bug#27944.
  */
  session->client_capabilities&= server_capabilites;

  if (end >= (char*) net.read_pos + pkt_len + 2)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), session->security_ctx.ip.c_str());
    return false;
  }

  net.return_status= &session->server_status;

  char *user= end;
  char *passwd= strchr(user, '\0')+1;
  uint32_t user_len= passwd - user - 1;
  char *l_db= passwd;
  char db_buff[NAME_LEN + 1];           // buffer to store db in utf8
  char user_buff[USERNAME_LENGTH + 1];    // buffer to store user in utf8
  uint32_t dummy_errors;

  /*
    Old clients send null-terminated string as password; new clients send
    the size (1 byte) + string (not null-terminated). Hence in case of empty
    password both send '\0'.

    This strlen() can't be easily deleted without changing protocol.

    Cast *passwd to an unsigned char, so that it doesn't extend the sign for
    *passwd > 127 and become 2**32-127+ after casting to uint.
  */
  uint32_t passwd_len= session->client_capabilities & CLIENT_SECURE_CONNECTION ?
    (unsigned char)(*passwd++) : strlen(passwd);
  l_db= session->client_capabilities & CLIENT_CONNECT_WITH_DB ? l_db + passwd_len + 1 : 0;

  /* strlen() can't be easily deleted without changing protocol */
  uint32_t db_len= l_db ? strlen(l_db) : 0;

  if (passwd + passwd_len + db_len > (char *) net.read_pos + pkt_len)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), session->security_ctx.ip.c_str());
    return false;
  }

  /* Since 4.1 all database names are stored in utf8 */
  if (l_db)
  {
    db_buff[copy_and_convert(db_buff, sizeof(db_buff)-1,
                             system_charset_info,
                             l_db, db_len,
                             session->charset(), &dummy_errors)]= 0;
    l_db= db_buff;
  }

  user_buff[user_len= copy_and_convert(user_buff, sizeof(user_buff)-1,
                                       system_charset_info, user, user_len,
                                       session->charset(), &dummy_errors)]= '\0';
  user= user_buff;

  /* If username starts and ends in "'", chop them off */
  if (user_len > 1 && user[0] == '\'' && user[user_len - 1] == '\'')
  {
    user[user_len-1]= 0;
    user++;
    user_len-= 2;
  }

  session->security_ctx.user.assign(user);

  return session->checkUser(passwd, passwd_len, l_db);
}

static ProtocolFactoryOldLibdrizzle *factory= NULL;

static int init(PluginRegistry &registry)
{
  factory= new ProtocolFactoryOldLibdrizzle;
  registry.add(factory); 
  return 0;
}

static int deinit(PluginRegistry &registry)
{
  if (factory)
  {
    registry.remove(factory);
    delete factory;
  }
  return 0;
}

drizzle_declare_plugin(oldlibdrizzle)
{
  DRIZZLE_PROTOCOL_PLUGIN,
  "oldlibdrizzle",
  "0.1",
  "Eric Day",
  "Old libdrizzle Protocol",
  PLUGIN_LICENSE_GPL,
  init,   /* Plugin Init */
  deinit, /* Plugin Deinit */
  NULL,   /* status variables */
  NULL,   /* system variables */
  NULL    /* config options */
}
drizzle_declare_plugin_end;
