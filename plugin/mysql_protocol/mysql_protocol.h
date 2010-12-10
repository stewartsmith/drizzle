/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems
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

#ifndef PLUGIN_MYSQL_PROTOCOL_MYSQL_PROTOCOL_H
#define PLUGIN_MYSQL_PROTOCOL_MYSQL_PROTOCOL_H

#include <drizzled/plugin/listen_tcp.h>
#include <drizzled/plugin/client.h>
#include <drizzled/atomics.h>
#include "drizzled/plugin/table_function.h"

#include "net_serv.h"

namespace drizzle_plugin
{
class ProtocolCounters
{
  public:
    ProtocolCounters():
      max_connections(1000)
    { }
    drizzled::atomic<uint64_t> connectionCount;
    drizzled::atomic<uint64_t> failedConnections;
    drizzled::atomic<uint64_t> connected;
    uint32_t max_connections;
};

typedef drizzled::constrained_check<uint32_t, 300, 1> timeout_constraint;
typedef drizzled::constrained_check<uint32_t, 300, 1> retry_constraint;
typedef drizzled::constrained_check<uint32_t, 1048576, 1024, 1024> buffer_constraint;

class ListenMySQLProtocol: public drizzled::plugin::ListenTcp
{
private:
  const std::string _hostname;
  bool _using_mysql41_protocol;

public:
  ListenMySQLProtocol(std::string name,
                      const std::string &hostname,
                      bool using_mysql41_protocol):
   drizzled::plugin::ListenTcp(name),
   _hostname(hostname),
   _using_mysql41_protocol(using_mysql41_protocol)
  { }
  virtual ~ListenMySQLProtocol();
  virtual const std::string getHost(void) const;
  virtual in_port_t getPort(void) const;
  virtual drizzled::plugin::Client *getClient(int fd);
  static ProtocolCounters *mysql_counters;
  virtual ProtocolCounters *getCounters(void) const { return mysql_counters; }
};

class ClientMySQLProtocol: public drizzled::plugin::Client
{
private:
  NET net;
  drizzled::String packet;
  uint32_t client_capabilities;
  bool is_admin_connection;
  bool _using_mysql41_protocol;

  bool checkConnection(void);
  bool netStoreData(const unsigned char *from, size_t length);
  void writeEOFPacket(uint32_t server_status, uint32_t total_warn_count);
  unsigned char *storeLength(unsigned char *packet, uint64_t length);
  void makeScramble(char *scramble);

public:
  ClientMySQLProtocol(int fd, bool _using_mysql41_protocol, ProtocolCounters *set_counters);
  virtual ~ClientMySQLProtocol();

  ProtocolCounters *counters;

  virtual int getFileDescriptor(void);
  virtual bool isConnected();
  virtual bool isReading(void);
  virtual bool isWriting(void);
  virtual bool flush(void);
  virtual void close(void);

  virtual bool authenticate(void);
  virtual bool readCommand(char **packet, uint32_t *packet_length);

  virtual void sendOK(void);
  virtual void sendEOF(void);
  virtual void sendError(uint32_t sql_errno, const char *err);

  virtual bool sendFields(drizzled::List<drizzled::Item> *list);

  using Client::store;
  virtual bool store(drizzled::Field *from);
  virtual bool store(void);
  virtual bool store(int32_t from);
  virtual bool store(uint32_t from);
  virtual bool store(int64_t from);
  virtual bool store(uint64_t from);
  virtual bool store(double from, uint32_t decimals, drizzled::String *buffer);
  virtual bool store(const char *from, size_t length);

  virtual bool haveError(void);
  virtual bool haveMoreData(void);
  virtual bool wasAborted(void);
};

} /* namespace drizzle_plugin */

#endif /* PLUGIN_MYSQL_PROTOCOL_MYSQL_PROTOCOL_H */
