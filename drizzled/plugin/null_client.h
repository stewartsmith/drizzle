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

namespace drizzled
{
namespace plugin
{

/**
 * This class is an empty client implementation for internal used.
 */
class NullClient: public Client
{
public:
  virtual int getFileDescriptor(void) { return -1; }
  virtual bool isConnected(void) { return true; }
  virtual bool isReading(void) { return false; }
  virtual bool isWriting(void) { return false; }
  virtual bool flush(void) { return false; }
  virtual void close(void) {}
  virtual bool authenticate(void) { return true; }
  virtual bool readCommand(char**, uint32_t*) { return false; }
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
  virtual bool haveMoreData(void) { return false; }
  virtual bool haveError(void) { return false; }
  virtual bool wasAborted(void) { return false; }
};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_NULL_CLIENT_H */
