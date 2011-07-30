/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#include <drizzled/plugin/listen_tcp.h>
#include <drizzled/plugin/client.h>
#include <drizzled/atomics.h>
#include <drizzled/plugin/table_function.h>

#include "net_serv.h"

namespace drizzle_plugin {

class ProtocolCounters
{
public:
  ProtocolCounters() :
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
public:
  ListenMySQLProtocol(std::string name, const std::string &hostname) :
   drizzled::plugin::ListenTcp(name),
   _hostname(hostname)
  { }
  virtual const std::string getHost() const;
  virtual in_port_t getPort() const;
  virtual drizzled::plugin::Client *getClient(int fd);
  static ProtocolCounters mysql_counters;
  virtual ProtocolCounters& getCounters() const { return mysql_counters; }
  void addCountersToTable();
protected:
  const std::string _hostname;
};

class ClientMySQLProtocol: public drizzled::plugin::Client
{
protected:
  NET net;
  drizzled::String packet;
  uint32_t client_capabilities;
  bool _is_interactive;

  bool checkConnection();
  void netStoreData(const void*, size_t);
  void writeEOFPacket(uint32_t server_status, uint32_t total_warn_count);
  unsigned char *storeLength(unsigned char *packet, uint64_t length);
  void makeScramble(char *scramble);

public:
  ClientMySQLProtocol(int fd, ProtocolCounters&);
  virtual ~ClientMySQLProtocol();

  bool isInteractive() const
  {
    return _is_interactive;
  }

  ProtocolCounters& counters;

  virtual int getFileDescriptor();
  virtual bool isConnected();
  virtual bool flush();
  virtual void close();

  virtual bool authenticate();
  virtual bool readCommand(char **packet, uint32_t& packet_length);

  virtual void sendOK();
  virtual void sendEOF();
  virtual void sendError(const drizzled::error_t sql_errno, const char *err);

  virtual void sendFields(drizzled::List<drizzled::Item>&);

  using Client::store;
  virtual void store(drizzled::Field*);
  virtual void store();
  virtual void store(int32_t from);
  virtual void store(uint32_t from);
  virtual void store(int64_t from);
  virtual void store(uint64_t from);
  virtual void store(double from, uint32_t decimals, drizzled::String *buffer);
  virtual void store(const char*, size_t);

  virtual bool haveError();
  virtual bool wasAborted();
};

} /* namespace drizzle_plugin */

