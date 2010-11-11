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

#ifndef DRIZZLED_PLUGIN_NULL_CLIENT_H
#define DRIZZLED_PLUGIN_NULL_CLIENT_H

#include <drizzled/plugin/client.h>
#include <vector>
#include <queue>
#include <string>

namespace drizzled
{
namespace plugin
{

/**
 * This class is an empty client implementation for internal used.
 */
class NullClient: public Client
{
  typedef std::vector<char> Bytes;
  typedef std::queue <Bytes> Queue;
  Queue to_execute;
  bool is_dead;
  Bytes packet_buffer;

public:

  NullClient() :
    is_dead(false)
  {
  }

  virtual int getFileDescriptor(void) { return -1; }
  virtual bool isConnected(void) { return true; }
  virtual bool isReading(void) { return false; }
  virtual bool isWriting(void) { return false; }
  virtual bool flush(void) { return false; }
  virtual void close(void) {}
  virtual bool authenticate(void) { return true; }

  virtual bool readCommand(char **packet, uint32_t *packet_length)
  {
    while(not to_execute.empty())
    {
      Queue::value_type next= to_execute.front();
      packet_buffer.resize(next.size());
      memcpy(&packet_buffer[0], &next[0], next.size());

      *packet= &packet_buffer[0];

      *packet_length= next.size();

      to_execute.pop();

      return true;
    }

    if (not is_dead)
    {
      packet_buffer.resize(1);
      *packet_length= 1;
      *packet= &packet_buffer[0];
      is_dead= true;

      return true;
    }

    *packet_length= 0;
    return false;
  }

  virtual void sendOK(void) {}
  virtual void sendEOF(void) {}
  virtual void sendError(uint32_t, const char*) {}
  virtual bool sendFields(List<Item>*) { return false; }
  virtual bool store(Field *) { return false; }
  virtual bool store(void) { return false; }
  virtual bool store(int32_t) { return false; }
  virtual bool store(uint32_t) { return false; }
  virtual bool store(int64_t) { return false; }
  virtual bool store(uint64_t) { return false; }
  virtual bool store(double, uint32_t, String*) { return false; }
  virtual bool store(const DRIZZLE_TIME*) { return false; }
  virtual bool store(const char*) { return false; }
  virtual bool store(const char*, size_t) { return false; }
  virtual bool store(const std::string &) { return false; }
  virtual bool haveMoreData(void) { return false;}
  virtual bool haveError(void) { return false; }
  virtual bool wasAborted(void) { return false; }

  void pushSQL(const std::string &arg)
  {
    Bytes byte;
    byte.resize(arg.size()+2); // +1 for the COM_QUERY
    byte[0]= COM_QUERY;
    memcpy(&byte[1], arg.c_str(), arg.size());
    byte.push_back(';');
    to_execute.push(byte);
  }
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_NULL_CLIENT_H */
