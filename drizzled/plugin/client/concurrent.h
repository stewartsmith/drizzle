/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2011 Vijay Samuel, vjsamuel1990@gmail.com
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

#include <drizzled/plugin/client.h>
#include <drizzled/execute/context.h>
#include <drizzled/execute/parser.h>
#include <drizzled/util/string.h>
#include <vector>
#include <queue>
#include <string>
#include <cstdio>

using namespace std;

namespace drizzled {
namespace plugin {
namespace client {

/**
 * This class is an empty client implementation for internal used.
 */
class Concurrent: public Client
{
  typedef std::queue < drizzled::util::String > Queue;
  Queue to_execute;
  bool is_dead;
  drizzled::util::String packet_buffer;

public:

  Concurrent() :
    is_dead(false)
  {
  }

  virtual int getFileDescriptor(void) { return -1; }
  virtual bool isConnected(void) { return true; }
  virtual bool flush(void) { return false; }
  virtual void close(void) {}
  virtual bool authenticate(void) { return true; }

  virtual bool readCommand(char **packet, uint32_t& packet_length)
  {
    while(not to_execute.empty())
    {
      Queue::value_type next= to_execute.front();
      packet_buffer= next;

      *packet= packet_buffer.data();
      packet_length= next.size();

      to_execute.pop();

      return true;
    }

    if (not is_dead)
    {
      packet_buffer.clear();
      packet_length= 1;
      *packet= packet_buffer.data();
      is_dead= true;

      return true;
    }

    packet_length= 0;
    return false;
  }

  virtual void sendOK(void) {}
  virtual void sendEOF(void) {}
  virtual void sendError(const drizzled::error_t, const char*) {}
  virtual void sendFields(List<Item>&) {}
  virtual void store(Field*) {}
  virtual void store() {}
  virtual void store(int32_t) {}
  virtual void store(uint32_t) {}
  virtual void store(int64_t) {}
  virtual void store(uint64_t) {}
  virtual void store(double, uint32_t, String*) {}
  virtual void store(const type::Time*) {}
  virtual void store(const char*) {}
  virtual void store(const char*, size_t) {}
  virtual void store(const std::string &) {}
  virtual bool haveError(void) { return false; }
  virtual bool wasAborted(void) { return false; }

  void pushSQL(const std::string &arg)
  {
    ::drizzled::error_t err_msg;
    ::drizzled::execute::Context context(arg.data(), arg.size(), err_msg);
    std::vector<std::string> parsed_tokens= context.start();

    {
      drizzled::util::String byte;
      byte.assign(1, COM_QUERY); // Insert our COM_QUERY
      byte.append(drizzle_literal_parameter("START TRANSACTION")); // +1 for the COM_QUERY, provided by null count from sizeof()
      to_execute.push(byte);
    }
    
    BOOST_FOREACH(const string& iter, parsed_tokens)
    {
      drizzled::util::String byte;
      byte.assign(1, COM_QUERY); // Insert our COM_QUERY
      byte.append(iter.data(), iter.size());
      to_execute.push(byte);
    }

    {
      drizzled::util::String byte;
      byte.assign(1, COM_QUERY); // Insert our COM_QUERY
      byte.append(drizzle_literal_parameter("COMMIT")); // +1 for the COM_QUERY, provided by null count from sizeof()
      to_execute.push(byte);
    }
  }
};

} /* namespace client */
} /* namespace plugin */
} /* namespace drizzled */

