/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>
#include <drizzled/gettext.h>
#include <drizzled/error.h>
#include <drizzled/query_id.h>
#include <drizzled/error/sql_state.h>
#include <drizzled/session.h>
#include <drizzled/internal/m_string.h>
#include <algorithm>
#include <boost/program_options.hpp>
#include <drizzled/module/option_map.h>
#include <drizzled/util/tokenize.h>
#include "errmsg.h"
#include "mysql_protocol.h"
#include "mysql_password.h"
#include "options.h"
#include <drizzled/identifier.h>
#include <drizzled/plugin/function.h>
#include <drizzled/diagnostics_area.h>
#include <drizzled/system_variables.h>
#include <libdrizzle/constants.h>

#define MIN_HANDSHAKE_SIZE 6
#define PROTOCOL_VERSION 10

namespace po= boost::program_options;
using namespace std;
using namespace drizzled;

namespace drizzle_plugin {

static const unsigned int PACKET_BUFFER_EXTRA_ALLOC= 1024;

static port_constraint port;
static timeout_constraint connect_timeout;
static timeout_constraint read_timeout;
static timeout_constraint write_timeout;
static retry_constraint retry_count;
static buffer_constraint buffer_length;

static uint32_t random_seed1;
static uint32_t random_seed2;
static const uint32_t random_max= 0x3FFFFFFF;
static const double random_max_double= (double)0x3FFFFFFF;

ProtocolCounters ListenMySQLProtocol::mysql_counters;

void ListenMySQLProtocol::addCountersToTable()
{
  counters.push_back(new drizzled::plugin::ListenCounter(new std::string("connection_count"), &getCounters().connectionCount));
  counters.push_back(new drizzled::plugin::ListenCounter(new std::string("connected"), &getCounters().connected));
  counters.push_back(new drizzled::plugin::ListenCounter(new std::string("failed_connections"), &getCounters().failedConnections));
}

const std::string ListenMySQLProtocol::getHost(void) const
{
  return _hostname;
}

in_port_t ListenMySQLProtocol::getPort(void) const
{
  return port.get();
}

plugin::Client *ListenMySQLProtocol::getClient(int fd)
{
  int new_fd= acceptTcp(fd);
  return new_fd == -1 ? NULL : new ClientMySQLProtocol(new_fd, getCounters());
}

ClientMySQLProtocol::ClientMySQLProtocol(int fd, ProtocolCounters& set_counters) :
  _is_interactive(false),
  counters(set_counters)
{
  net.vio= 0;

  if (fd == -1)
    return;

  net.init(fd, buffer_length.get());
  net.set_read_timeout(read_timeout.get());
  net.set_write_timeout(write_timeout.get());
  net.retry_count=retry_count.get();
}

ClientMySQLProtocol::~ClientMySQLProtocol()
{
  if (net.vio)
    net.vio->close();
}

int ClientMySQLProtocol::getFileDescriptor()
{
  return net.get_sd();
}

bool ClientMySQLProtocol::isConnected()
{
  return net.vio != 0;
}

bool ClientMySQLProtocol::flush()
{
  if (net.vio == NULL)
    return false;
  bool ret= net.write(packet.ptr(), packet.length());
  packet.length(0);
  return ret;
}

void ClientMySQLProtocol::close()
{
  if (net.vio)
  { 
    net.close();
    net.end();
    counters.connected.decrement();
  }
}

bool ClientMySQLProtocol::authenticate()
{
  counters.connectionCount.increment();
  counters.connected.increment();

  /* Use "connect_timeout" value during connection phase */
  net.set_read_timeout(connect_timeout.get());
  net.set_write_timeout(connect_timeout.get());

  if (checkConnection())
  {
    if (counters.connected > counters.max_connections)
    {
      std::string errmsg(ER(ER_CON_COUNT_ERROR));
      sendError(ER_CON_COUNT_ERROR, errmsg.c_str());
      counters.failedConnections.increment();
    }
    else
    {
      sendOK();
    }
  }
  else
  {
    sendError(session->main_da().sql_errno(), session->main_da().message());
    counters.failedConnections.increment();
    return false;
  }

  /* Connect completed, set read/write timeouts back to default */
  net.set_read_timeout(read_timeout.get());
  net.set_write_timeout(write_timeout.get());
  return true;
}

bool ClientMySQLProtocol::readCommand(char **l_packet, uint32_t& packet_length)
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

  net.pkt_nr=0;
  packet_length= net.read();
  if (packet_length == packet_error)
  {
    /* Check if we can continue without closing the connection */

    if(net.last_errno== ER_NET_PACKET_TOO_LARGE)
      my_error(ER_NET_PACKET_TOO_LARGE, MYF(0));
    if (session->main_da().status() == Diagnostics_area::DA_ERROR)
      sendError(session->main_da().sql_errno(), session->main_da().message());
    else
      sendOK();

    if (net.error_ != 3)
      return false;                       // We have to close it.

    net.error_= 0;
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

  if (packet_length == 0)                       /* safety */
  {
    /* Initialize with COM_SLEEP packet */
    (*l_packet)[0]= (unsigned char) COM_SLEEP;
    packet_length= 1;
  }
  else
  {
    /* Map from MySQL commands to Drizzle commands. */
    switch ((*l_packet)[0])
    {
    case 0: /* SLEEP */
    case 1: /* QUIT */
    case 2: /* INIT_DB */
    case 3: /* QUERY */
      break;

    case 8: /* SHUTDOWN */
      (*l_packet)[0]= COM_SHUTDOWN;
      break;

    case 12: /* KILL */
      (*l_packet)[0]= COM_KILL;
      break;

    case 14: /* PING */
      (*l_packet)[0]= COM_PING;
      break;

    default:
      /* Respond with unknown command for MySQL commands we don't support. */
      (*l_packet)[0]= COM_END;
      packet_length= 1;
    }
  }

  /* Do not rely on drizzleclient_net_read, extra safety against programming errors. */
  (*l_packet)[packet_length]= '\0';                  /* safety */

#ifdef NEVER
  /* See comment above. */
  /* Restore read timeout value */
  drizzleclient_net_set_read_timeout(&net,
                                     session->variables.net_read_timeout);
#endif

  return true;
}

/**
  Return ok to the client.

  The ok packet has the following structure:

  - 0               : Marker (1 byte)
  - affected_rows    : Stored in 1-9 bytes
  - id        : Stored in 1-9 bytes
  - server_status    : Copy of session->server_status;  Can be used by client
  to check if we are inside an transaction.
  New in 4.0 client
  - warning_count    : Stored in 2 bytes; New in 4.1 client
  - message        : Stored as packed length (1-9 bytes) + message.
  Is not stored if no message.

  @param session           Thread handler
  @param affected_rows       Number of rows changed by statement
  @param id           Auto_increment id for first row (if used)
  @param message       Message to send to the client (Used by mysql_status)
*/

void ClientMySQLProtocol::sendOK()
{
  unsigned char buff[DRIZZLE_ERRMSG_SIZE+10],*pos;
  const char *message= NULL;
  uint32_t tmp;

  if (!net.vio)    // hack for re-parsing queries
  {
    return;
  }

  buff[0]=0;                    // No fields
  if (session->main_da().status() == Diagnostics_area::DA_OK)
  {
    if (client_capabilities & CLIENT_FOUND_ROWS && session->main_da().found_rows())
      pos=storeLength(buff+1,session->main_da().found_rows());
    else
      pos=storeLength(buff+1,session->main_da().affected_rows());
    pos=storeLength(pos, session->main_da().last_insert_id());
    int2store(pos, session->main_da().server_status());
    pos+=2;
    tmp= min(session->main_da().total_warn_count(), (uint32_t)65535);
    message= session->main_da().message();
  }
  else
  {
    pos=storeLength(buff+1,0);
    pos=storeLength(pos, 0);
    int2store(pos, session->server_status);
    pos+=2;
    tmp= min(session->total_warn_count, (uint32_t)65535);
  }

  /* We can only return up to 65535 warnings in two bytes */
  int2store(pos, tmp);
  pos+= 2;

  session->main_da().can_overwrite_status= true;

  if (message && message[0])
  {
    size_t length= strlen(message);
    pos=storeLength(pos,length);
    memcpy(pos,(unsigned char*) message,length);
    pos+=length;
  }
  net.write(buff, pos - buff);
  net.flush();

  session->main_da().can_overwrite_status= false;
}

/**
  Send eof (= end of result set) to the client.

  The eof packet has the following structure:

  - 254    (DRIZZLE_PROTOCOL_NO_MORE_DATA)    : Marker (1 byte)
  - warning_count    : Stored in 2 bytes; New in 4.1 client
  - status_flag    : Stored in 2 bytes;
  For flags like SERVER_MORE_RESULTS_EXISTS.

  Note that the warning count will not be sent if 'no_flush' is set as
  we don't want to report the warning count until all data is sent to the
  client.
*/

void ClientMySQLProtocol::sendEOF()
{
  /* Set to true if no active vio, to work well in case of --init-file */
  if (net.vio)
  {
    session->main_da().can_overwrite_status= true;
    writeEOFPacket(session->main_da().server_status(), session->main_da().total_warn_count());
    net.flush();
    session->main_da().can_overwrite_status= false;
  }
  packet.shrink(buffer_length.get());
}


void ClientMySQLProtocol::sendError(drizzled::error_t sql_errno, const char *err)
{
  uint32_t length;
  /*
    buff[]: sql_errno:2 + ('#':1 + SQLSTATE_LENGTH:5) + DRIZZLE_ERRMSG_SIZE:512
  */
  unsigned char buff[2+1+SQLSTATE_LENGTH+DRIZZLE_ERRMSG_SIZE], *pos;

  assert(sql_errno != EE_OK);
  assert(err && err[0]);

  /*
    It's one case when we can push an error even though there
    is an OK or EOF already.
  */
  session->main_da().can_overwrite_status= true;

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

  int2store(buff, static_cast<uint16_t>(sql_errno));
  pos= buff+2;

  /* The first # is to make the client backward compatible */
  buff[2]= '#';
  pos= (unsigned char*) strcpy((char*) buff+3, error::convert_to_sqlstate(sql_errno));
  pos+= strlen(error::convert_to_sqlstate(sql_errno));

  char *tmp= strncpy((char*)pos, err, DRIZZLE_ERRMSG_SIZE-1);
  tmp+= strlen((char*)pos);
  tmp[0]= '\0';
  length= (uint32_t)(tmp-(char*)buff);
  err= (char*) buff;
  net.write_command((unsigned char) 255, (unsigned char*) "", 0, (unsigned char*) err, length);
  net.flush();
  session->main_da().can_overwrite_status= false;
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
void ClientMySQLProtocol::sendFields(List<Item>& list)
{
  List<Item>::iterator it(list.begin());
  unsigned char buff[80];
  String tmp((char*) buff,sizeof(buff),&my_charset_bin);

  unsigned char *row_pos= storeLength(buff, list.size());
  (void) net.write(buff, row_pos - buff);

  while (Item* item=it++)
  {
    SendField field;
    item->make_field(&field);

    packet.length(0);

    store(STRING_WITH_LEN("def"));
    store(field.db_name);
    store(field.table_name);
    store(field.org_table_name);
    store(field.col_name);
    store(field.org_col_name);
    packet.realloc(packet.length()+12);

    /* Store fixed length fields */
    char* pos= (char*) packet.ptr()+packet.length();
    *pos++= 12;                // Length of packed fields
    /* No conversion */
    int2store(pos, field.charsetnr);
    int4store(pos+2, field.length);

    if (true) // _using_mysql41_protocol)
    {
      /* Switch to MySQL field numbering. */
      switch (field.type)
      {
      case DRIZZLE_TYPE_LONG:
        pos[6]= DRIZZLE_COLUMN_TYPE_LONG;
        break;

      case DRIZZLE_TYPE_DOUBLE:
        pos[6]= DRIZZLE_COLUMN_TYPE_DOUBLE;
        break;

      case DRIZZLE_TYPE_NULL:
        pos[6]= DRIZZLE_COLUMN_TYPE_NULL;
        break;

      case DRIZZLE_TYPE_TIMESTAMP:
        pos[6]= DRIZZLE_COLUMN_TYPE_TIMESTAMP;
        break;

      case DRIZZLE_TYPE_LONGLONG:
        pos[6]= DRIZZLE_COLUMN_TYPE_LONGLONG;
        break;

      case DRIZZLE_TYPE_DATETIME:
        pos[6]= DRIZZLE_COLUMN_TYPE_DATETIME;
        break;

      case DRIZZLE_TYPE_TIME:
        pos[6]= DRIZZLE_COLUMN_TYPE_TIME;
        break;

      case DRIZZLE_TYPE_DATE:
        pos[6]= DRIZZLE_COLUMN_TYPE_DATE;
        break;

      case DRIZZLE_TYPE_VARCHAR:
        pos[6]= DRIZZLE_COLUMN_TYPE_VARCHAR;
        break;

      case DRIZZLE_TYPE_MICROTIME:
        pos[6]= DRIZZLE_COLUMN_TYPE_VARCHAR;
        break;

      case DRIZZLE_TYPE_UUID:
        pos[6]= DRIZZLE_COLUMN_TYPE_VARCHAR;
        break;

      case DRIZZLE_TYPE_BOOLEAN:
        pos[6]= DRIZZLE_COLUMN_TYPE_TINY;
        break;

      case DRIZZLE_TYPE_DECIMAL:
        pos[6]= (char)DRIZZLE_COLUMN_TYPE_NEWDECIMAL;
        break;

      case DRIZZLE_TYPE_ENUM:
        pos[6]= (char)DRIZZLE_COLUMN_TYPE_ENUM;
        break;

      case DRIZZLE_TYPE_BLOB:
        pos[6]= (char)DRIZZLE_COLUMN_TYPE_BLOB;
        break;
      }
    }
    else
    {
      /* Add one to compensate for tinyint removal from enum. */
      pos[6]= field.type + 1;
    }

    int2store(pos+7,field.flags);
    pos[9]= (char) field.decimals;
    pos[10]= 0;                // For the future
    pos[11]= 0;                // For the future
    pos+= 12;

    packet.length((uint32_t) (pos - packet.ptr()));
    if (flush())
      break;
  }

  /*
    Mark the end of meta-data result set, and store session->server_status,
    to show that there is no cursor.
    Send no warning information, as it will be sent at statement end.
  */
  writeEOFPacket(session->server_status, session->total_warn_count);
}

void ClientMySQLProtocol::store(Field *from)
{
  if (from->is_null())
    return store();
  if (from->type() == DRIZZLE_TYPE_BOOLEAN)
  {
    return store(from->val_int());
  }

  char buff[MAX_FIELD_WIDTH];
  String str(buff,sizeof(buff), &my_charset_bin);

  from->val_str_internal(&str);

  netStoreData(str.ptr(), str.length());
}

void ClientMySQLProtocol::store()
{
  char buff[1];
  buff[0]= (char)251;
  packet.append(buff, sizeof(buff), PACKET_BUFFER_EXTRA_ALLOC);
}

void ClientMySQLProtocol::store(int32_t from)
{
  char buff[12];
  netStoreData(buff, (internal::int10_to_str(from, buff, -10) - buff));
}

void ClientMySQLProtocol::store(uint32_t from)
{
  char buff[11];
  netStoreData(buff, (size_t) (internal::int10_to_str(from, buff, 10) - buff));
}

void ClientMySQLProtocol::store(int64_t from)
{
  char buff[22];
  netStoreData(buff, (size_t) (internal::int64_t10_to_str(from, buff, -10) - buff));
}

void ClientMySQLProtocol::store(uint64_t from)
{
  char buff[21];
  netStoreData(buff, (size_t) (internal::int64_t10_to_str(from, buff, 10) - buff));
}

void ClientMySQLProtocol::store(double from, uint32_t decimals, String *buffer)
{
  buffer->set_real(from, decimals, session->charset());
  netStoreData(buffer->ptr(), buffer->length());
}

void ClientMySQLProtocol::store(const char *from, size_t length)
{
  netStoreData(from, length);
}

bool ClientMySQLProtocol::wasAborted()
{
  return net.error_ && net.vio != 0;
}

bool ClientMySQLProtocol::haveError()
{
  return net.error_ || net.vio == 0;
}

bool ClientMySQLProtocol::checkConnection()
{
  char scramble[SCRAMBLE_LENGTH];
  identifier::user::mptr user_identifier= identifier::User::make_shared();

  makeScramble(scramble);

  // TCP/IP connection
  {
    char ip[NI_MAXHOST];
    uint16_t peer_port;

    if (net.peer_addr(ip, NI_MAXHOST, peer_port))
    {
      my_error(ER_BAD_HOST_ERROR, MYF(0), ip);
      return false;
    }

    user_identifier->setAddress(ip);
  }
  net.keepalive(true);

  char* end;
  uint32_t pkt_len= 0;
  uint32_t server_capabilites;
  {
    /* buff[] needs to big enough to hold the server_version variable */
    char buff[SERVER_VERSION_LENGTH + SCRAMBLE_LENGTH + 64];

    server_capabilites= CLIENT_BASIC_FLAGS | CLIENT_PROTOCOL_MYSQL41;

#ifdef HAVE_COMPRESS
    server_capabilites|= CLIENT_COMPRESS;
#endif /* HAVE_COMPRESS */

    end= buff + strlen(PANDORA_RELEASE_VERSION);
    if ((end - buff) >= SERVER_VERSION_LENGTH)
      end= buff + (SERVER_VERSION_LENGTH - 1);
    memcpy(buff, PANDORA_RELEASE_VERSION, end - buff);
    *end= 0;
    end++;

    int4store((unsigned char*) end, session->variables.pseudo_thread_id);
    end+= 4;

    /* We don't use scramble anymore. */
    memcpy(end, scramble, SCRAMBLE_LENGTH_323);
    end+= SCRAMBLE_LENGTH_323;
    *end++= 0; /* an empty byte for some reason */

    int2store(end, server_capabilites);
    /* write server characteristics: up to 16 bytes allowed */
    end[2]=(char) default_charset_info->number;
    int2store(end+3, session->server_status);
    memset(end+5, 0, 13);
    end+= 18;

    /* Write scramble tail. */
    memcpy(end, scramble + SCRAMBLE_LENGTH_323, SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323);
    end+= (SCRAMBLE_LENGTH - SCRAMBLE_LENGTH_323);
    *end++= 0; /* an empty byte for some reason */

    /* At this point we write connection message and read reply */
    if (net.write_command(
            (unsigned char) PROTOCOL_VERSION
          , (unsigned char*) ""
          , 0
          , (unsigned char*) buff
          , (size_t) (end-buff)) 
        ||    (pkt_len= net.read()) == packet_error 
        || pkt_len < MIN_HANDSHAKE_SIZE)
    {
      my_error(ER_HANDSHAKE_ERROR, MYF(0), user_identifier->address().c_str());
      return false;
    }
  }
  packet.alloc(buffer_length.get());

  client_capabilities= uint2korr(net.read_pos);
  if (!(client_capabilities & CLIENT_PROTOCOL_MYSQL41))
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), user_identifier->address().c_str());
    return false;
  }

  client_capabilities|= ((uint32_t) uint2korr(net.read_pos + 2)) << 16;
  // session->max_client_packet_length= uint4korr(net.read_pos + 4);
  end= (char*) net.read_pos + 32;

  /*
    Disable those bits which are not supported by the server.
    This is a precautionary measure, if the client lies. See Bug#27944.
  */
  client_capabilities&= server_capabilites;

  if (end >= (char*) net.read_pos + pkt_len + 2)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), user_identifier->address().c_str());
    return false;
  }

  char *user= end;
  char *passwd= strchr(user, '\0')+1;
  uint32_t user_len= passwd - user - 1;
  char *l_db= passwd;

  /*
    Only support new password format.

    Cast *passwd to an unsigned char, so that it doesn't extend the sign for
    *passwd > 127 and become 2**32-127+ after casting to uint.
  */
  uint32_t passwd_len;
  if (client_capabilities & CLIENT_SECURE_CONNECTION &&
      passwd < (char *) net.read_pos + pkt_len)
  {
    passwd_len= (unsigned char)(*passwd++);
    if (passwd_len > 0 and client_capabilities & CLIENT_CAPABILITIES_PLUGIN_AUTH)
    {
      user_identifier->setPasswordType(identifier::User::PLAIN_TEXT);
    }
    else if (passwd_len > 0)
    {
      user_identifier->setPasswordType(identifier::User::MYSQL_HASH);
      user_identifier->setPasswordContext(scramble, SCRAMBLE_LENGTH);
    }
  }
  else
  {
    passwd_len= 0;
  }

  if (client_capabilities & CLIENT_CONNECT_WITH_DB &&
      passwd < (char *) net.read_pos + pkt_len)
  {
    l_db= l_db + passwd_len + 1;
  }
  else
  {
    l_db= NULL;
  }

  /* strlen() can't be easily deleted without changing client */
  uint32_t db_len= l_db ? strlen(l_db) : 0;

  if (passwd + passwd_len + db_len > (char *) net.read_pos + pkt_len)
  {
    my_error(ER_HANDSHAKE_ERROR, MYF(0), user_identifier->address().c_str());
    return false;
  }

  /* If username starts and ends in "'", chop them off */
  if (user_len > 1 && user[0] == '\'' && user[user_len - 1] == '\'')
  {
    user[user_len-1]= 0;
    user++;
    user_len-= 2;
  }

  if (client_capabilities & CLIENT_INTERACTIVE)
  {
    _is_interactive= true;
  }

  if (client_capabilities & CLIENT_CAPABILITIES_PLUGIN_AUTH)
  {
    passwd_len= strlen(passwd);
  }

  user_identifier->setUser(user);
  session->setUser(user_identifier);

  return session->checkUser(string(passwd, passwd_len), string(l_db ? l_db : ""));

}

void ClientMySQLProtocol::netStoreData(const void* from, size_t length)
{
  size_t packet_length= packet.length();
  /*
     The +9 comes from that strings of length longer than 16M require
     9 bytes to be stored (see storeLength).
  */
  if (packet_length+9+length > packet.alloced_length())
      packet.realloc(packet_length+9+length);
  unsigned char *to= storeLength((unsigned char*) packet.ptr()+packet_length, length);
  memcpy(to,from,length);
  packet.length((size_t) (to+length-(unsigned char*) packet.ptr()));
}

/**
  Format EOF packet according to the current client and
  write it to the network output buffer.
*/

void ClientMySQLProtocol::writeEOFPacket(uint32_t server_status,
                                         uint32_t total_warn_count)
{
  unsigned char buff[5];
  /*
    Don't send warn count during SP execution, as the warn_list
    is cleared between substatements, and mysqltest gets confused
  */
  uint32_t tmp= min(total_warn_count, (uint32_t)65535);
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
  net.write(buff, 5);
}

/*
  Store an integer with simple packing into a output package

  buffer      Store the packed integer here
  length    integers to store

  This is mostly used to store lengths of strings.  We have to cast
  the result for the LL() becasue of a bug in Forte CC compiler.

  RETURN
  Position in 'buffer' after the packed length
*/

unsigned char *ClientMySQLProtocol::storeLength(unsigned char *buffer, uint64_t length)
{
  if (length < (uint64_t) 251LL)
  {
    *buffer=(unsigned char) length;
    return buffer+1;
  }
  /* 251 is reserved for NULL */
  if (length < (uint64_t) 65536LL)
  {
    *buffer++=252;
    int2store(buffer,(uint32_t) length);
    return buffer+2;
  }
  if (length < (uint64_t) 16777216LL)
  {
    *buffer++=253;
    int3store(buffer,(uint32_t) length);
    return buffer+3;
  }
  *buffer++=254;
  int8store(buffer,length);
  return buffer+8;
}

void ClientMySQLProtocol::makeScramble(char *scramble)
{
  /* This is the MySQL algorithm with minimal changes. */
  random_seed1= (random_seed1 * 3 + random_seed2) % random_max;
  random_seed2= (random_seed1 + random_seed2 + 33) % random_max;
  uint32_t seed= static_cast<uint32_t>((static_cast<double>(random_seed1) / random_max_double) * 0xffffffff);

  void *pointer= this;
  uint32_t pointer_seed;
  memcpy(&pointer_seed, &pointer, 4);
  uint32_t random1= (seed + pointer_seed) % random_max;
  uint32_t random2= (seed + session->variables.pseudo_thread_id + net.vio->get_fd()) % random_max;

  for (char *end= scramble + SCRAMBLE_LENGTH; scramble != end; scramble++)
  {
    random1= (random1 * 3 + random2) % random_max;
    random2= (random1 + random2 + 33) % random_max;
    *scramble= static_cast<char>((static_cast<double>(random1) / random_max_double) * 94 + 33);
  }
}

static int init(drizzled::module::Context &context)
{  
  /* Initialize random seeds for the MySQL algorithm with minimal changes. */
  time_t seed_time= time(NULL);
  random_seed1= seed_time % random_max;
  random_seed2= (seed_time / 2) % random_max;

  const module::option_map &vm= context.getOptions();

  context.add(new plugin::Create_function<MySQLPassword>(MySQLPasswordName));

  ListenMySQLProtocol* listen_obj= new ListenMySQLProtocol("mysql_protocol", vm["bind-address"].as<std::string>());
  listen_obj->addCountersToTable();
  context.add(listen_obj); 
  context.registerVariable(new sys_var_constrained_value_readonly<in_port_t>("port", port));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("connect_timeout", connect_timeout));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("read_timeout", read_timeout));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("write_timeout", write_timeout));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("retry_count", retry_count));
  context.registerVariable(new sys_var_constrained_value<uint32_t>("buffer_length", buffer_length));
  context.registerVariable(new sys_var_const_string_val("bind_address", vm["bind-address"].as<std::string>()));
  context.registerVariable(new sys_var_uint32_t_ptr("max-connections", &ListenMySQLProtocol::mysql_counters.max_connections));

  return 0;
}

static void init_options(drizzled::module::option_context &context)
{
  context("port",
          po::value<port_constraint>(&port)->default_value(3306),
          _("Port number to use for connection or 0 for default to with MySQL "
                              "protocol."));
  context("connect-timeout",
          po::value<timeout_constraint>(&connect_timeout)->default_value(10),
          _("Connect Timeout."));
  context("read-timeout",
          po::value<timeout_constraint>(&read_timeout)->default_value(30),
          _("Read Timeout."));
  context("write-timeout",
          po::value<timeout_constraint>(&write_timeout)->default_value(60),
          _("Write Timeout."));
  context("retry-count",
          po::value<retry_constraint>(&retry_count)->default_value(10),
          _("Retry Count."));
  context("buffer-length",
          po::value<buffer_constraint>(&buffer_length)->default_value(16384),
          _("Buffer length."));
  context("bind-address",
          po::value<string>()->default_value("localhost"),
          _("Address to bind to."));
  context("max-connections",
          po::value<uint32_t>(&ListenMySQLProtocol::mysql_counters.max_connections)->default_value(1000),
          _("Maximum simultaneous connections."));
}

} /* namespace drizzle_plugin */

DRIZZLE_DECLARE_PLUGIN
{
  DRIZZLE_VERSION_ID,
  "mysql-protocol",
  "0.1",
  "Eric Day",
  "MySQL Protocol Module",
  PLUGIN_LICENSE_GPL,
  drizzle_plugin::init,             /* Plugin Init */
  NULL, /* depends */
  drizzle_plugin::init_options    /* config options */
}
DRIZZLE_DECLARE_PLUGIN_END;
