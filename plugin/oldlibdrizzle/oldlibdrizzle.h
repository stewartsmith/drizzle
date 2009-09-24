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

#ifndef PLUGIN_OLDLIBDRIZZLE_OLDLIBDRIZZLE_H
#define PLUGIN_OLDLIBDRIZZLE_OLDLIBDRIZZLE_H

#include <drizzled/plugin/listen_tcp.h>
#include <drizzled/plugin/client.h>

#include "net_serv.h"
#include "password.h"

class ListenOldLibdrizzle: public drizzled::plugin::ListenTcp
{
public:
  ListenOldLibdrizzle() {}
  virtual in_port_t getPort(void) const;
  virtual drizzled::plugin::Client *getClient(int fd);
};

class ClientOldLibdrizzle: public drizzled::plugin::Client
{
private:
  NET net;
  Vio* save_vio;
  char scramble[SCRAMBLE_LENGTH+1];
  String *packet;
  String *convert;
  uint32_t field_pos;
  uint32_t field_count;
  uint32_t client_capabilities;
  bool netStoreData(const unsigned char *from, size_t length);

  /**
   * Performs handshake with client and authorizes user.
   *
   * Returns true is the connection is valid and the
   * user is authorized, otherwise false.
   */  
  bool checkConnection(void);

public:
  ClientOldLibdrizzle(int fd);
  ~ClientOldLibdrizzle();
  virtual void setSession(Session *session_arg);
  virtual int getFileDescriptor(void);
  virtual bool isConnected();
  virtual bool isReading(void);
  virtual bool isWriting(void);
  virtual bool flush();

  virtual bool haveError(void);
  virtual bool haveMoreData(void);

  virtual void setError(char error);
  virtual bool wasAborted(void);
  virtual bool authenticate(void);
  virtual bool readCommand(char **packet, uint32_t *packet_length);
  virtual void sendOK();
  virtual void sendEOF();
  virtual void sendError(uint32_t sql_errno, const char *err);
  virtual void close();
  virtual void forceClose();
  virtual void prepareForResend();
  virtual void free();

  virtual bool sendFields(List<Item> *list);

  using Client::store;
  virtual bool store(Field *from);
  virtual bool store(void);
  virtual bool store(int32_t from);
  virtual bool store(uint32_t from);
  virtual bool store(int64_t from);
  virtual bool store(uint64_t from);
  virtual bool store(double from, uint32_t decimals, String *buffer);
  virtual bool store(const DRIZZLE_TIME *from);
  virtual bool store(const char *from, size_t length);
};

#endif /* PLUGIN_OLDLIBDRIZZLE_OLDLIBDRIZZLE_H */
