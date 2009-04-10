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

#ifndef DRIZZLED_PLUGIN_PROTOCOL_H
#define DRIZZLED_PLUGIN_PROTOCOL_H

#include <drizzled/sql_list.h>
#include <drizzled/item.h>

class Session;
class String;

class Protocol
{
protected:
  Session *session;

public:
  Protocol() {}
  virtual ~Protocol() {}

  virtual void setSession(Session *session_arg)
  {
    session= session_arg;
  }

  virtual Session *getSession(void)
  {
    return session;
  }

  virtual bool isConnected()= 0;
  virtual void setReadTimeout(uint32_t timeout)= 0;
  virtual void setWriteTimeout(uint32_t timeout)= 0;
  virtual void setRetryCount(uint32_t count)= 0;
  virtual void setError(char error)= 0;
  virtual bool haveError(void)= 0;
  virtual bool wasAborted(void)= 0;
  virtual void enableCompression(void)= 0;
  virtual bool haveMoreData(void)= 0;
  virtual bool isReading(void)= 0;
  virtual bool isWriting(void)= 0;
  virtual bool setFileDescriptor(int fd)=0;
  virtual int fileDescriptor(void)=0;
  virtual void setRandom(uint64_t, uint64_t) {};
  virtual bool authenticate(void)=0;
  virtual bool readCommand(char **packet, uint32_t *packet_length)=0;
  virtual void sendOK()= 0;
  virtual void sendEOF()= 0;
  virtual void sendError(uint32_t sql_errno, const char *err)=0;
  virtual void close()= 0;
  virtual void forceClose()= 0;
  virtual void prepareForResend()= 0;
  virtual void free()= 0;
  virtual bool write()= 0;

  enum { SEND_NUM_ROWS= 1, SEND_DEFAULTS= 2, SEND_EOF= 4 };
  virtual bool sendFields(List<Item> *list, uint32_t flags)= 0;

  virtual bool store(Field *from)= 0;
  virtual bool store(void)= 0;
  virtual bool store(int32_t from)= 0;
  virtual bool store(uint32_t from)= 0;
  virtual bool store(int64_t from)= 0;
  virtual bool store(uint64_t from)= 0;
  virtual bool store(double from, uint32_t decimals, String *buffer)= 0;
  virtual bool store(const DRIZZLE_TIME *from)= 0;
  virtual bool store(const char *from, const CHARSET_INFO * const cs)
  {
    if (from == NULL)
      store();
    return store(from, strlen(from), cs);
  }
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO * const cs)= 0;
};

class ProtocolFactory
{
  std::string name;
public:
  ProtocolFactory(std::string name_arg): name(name_arg) {}
  ProtocolFactory(const char *name_arg): name(name_arg) {}
  virtual ~ProtocolFactory() {}
  virtual Protocol *operator()(void)= 0;
  std::string getName() {return name;}
};

#endif /* DRIZZLED_PLUGIN_PROTOCOL_H */
