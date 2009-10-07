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

#ifndef DRIZZLED_PLUGIN_OLDLIBDRIZZLE_H
#define DRIZZLED_PLUGIN_OLDLIBDRIZZLE_H

#include <drizzled/plugin/listen_tcp.h>
#include <drizzled/plugin/client.h>

#include "net_serv.h"

class ListenOldLibdrizzle: public drizzled::plugin::ListenTcp
{
public:
  ListenOldLibdrizzle(std::string name_arg)
   : drizzled::plugin::ListenTcp(name_arg)
  { }
  virtual const char* getHost(void) const;
  virtual in_port_t getPort(void) const;
  virtual drizzled::plugin::Client *getClient(int fd);
};

class ClientOldLibdrizzle: public drizzled::plugin::Client
{
private:
  NET net;
  String packet;
  uint32_t client_capabilities;

  bool checkConnection(void);
  bool netStoreData(const unsigned char *from, size_t length);
  void writeEOFPacket(uint32_t server_status, uint32_t total_warn_count);

public:
  ClientOldLibdrizzle(int fd);
  virtual ~ClientOldLibdrizzle();

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

  virtual bool sendFields(List<Item> *list);

  using Client::store;
  virtual bool store(Field *from);
  virtual bool store(void);
  virtual bool store(int32_t from);
  virtual bool store(uint32_t from);
  virtual bool store(int64_t from);
  virtual bool store(uint64_t from);
  virtual bool store(double from, uint32_t decimals, String *buffer);
  virtual bool store(const char *from, size_t length);

  virtual bool haveError(void);
  virtual bool haveMoreData(void);
  virtual bool wasAborted(void);
};

#endif /* DRIZZLED_PLUGIN_OLDLIBDRIZZLE_H */
