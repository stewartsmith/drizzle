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

#ifndef DRIZZLED_PLUGIN_CLIENT_H
#define DRIZZLED_PLUGIN_CLIENT_H

#include <drizzled/sql_list.h>
#include <drizzled/item.h>

namespace drizzled
{
class Session;
class String;

namespace plugin
{

/**
 * This class allows new client sources to be written. This could be through
 * network protocols, in-process threads, or any other client source that can
 * provide commands and handle result sets. The current implementation is
 * file-descriptor based, so for non-fd client sources (like from another
 * thread), derived classes will need to use a pipe() for event notifications.
 */
class Client
{
protected:
  Session *session;

public:
  virtual ~Client() {}

  /**
   * Get attached session from the client object.
   * @retval Session object that is attached, NULL if none.
   */
  virtual Session *getSession(void)
  {
    return session;
  }

  /**
   * Attach session to the client object.
   * @param[in] session_arg Session object to attach, or NULL to clear.
   */
  virtual void setSession(Session *session_arg)
  {
    session= session_arg;
  }

  /**
   * Get file descriptor associated with client object.
   * @retval File descriptor that is attached, -1 if none.
   */
  virtual int getFileDescriptor(void)= 0;

  /**
   * Check to see if the client is currently connected.
   * @retval Boolean value representing connected state.
   */
  virtual bool isConnected(void)= 0;

  /**
   * Check to see if the client is actively reading.
   * @retval Boolean value representing reading state.
   */
  virtual bool isReading(void)= 0;

  /**
   * Check to see if the client is actively writing.
   * @retval Boolean value representing writing state.
   */
  virtual bool isWriting(void)= 0;

  /**
   * Flush all data that has been buffered with store() methods.
   * @retval Boolean indicating success or failure.
   */
  virtual bool flush(void)= 0;

  /**
   * Close the client object.
   */
  virtual void close(void)= 0;

  /**
   * Perform handshake and authorize client if needed.
   */
  virtual bool authenticate(void)= 0;

  /**
   * Read command from client.
   */
  virtual bool readCommand(char **packet, uint32_t *packet_length)= 0;

  /* Send responses. */
  virtual void sendOK(void)= 0;
  virtual void sendEOF(void)= 0;
  virtual void sendError(uint32_t sql_errno, const char *err)= 0;

  /**
   * Send field list for result set.
   */
  virtual bool sendFields(List<Item> *list)= 0;

  /* Send result fields in various forms. */
  virtual bool store(Field *from)= 0;
  virtual bool store(void)= 0;
  virtual bool store(int32_t from)= 0;
  virtual bool store(uint32_t from)= 0;
  virtual bool store(int64_t from)= 0;
  virtual bool store(uint64_t from)= 0;
  virtual bool store(double from, uint32_t decimals, String *buffer)= 0;
  virtual bool store(const type::Time *from);
  virtual bool store(const char *from);
  virtual bool store(const char *from, size_t length)= 0;
  virtual bool store(const std::string &from)
  {
    return store(from.c_str(), from.size());
  }

  /* Try to remove these. */
  virtual bool haveMoreData(void)= 0;
  virtual bool haveError(void)= 0;
  virtual bool wasAborted(void)= 0;

};

} /* namespace plugin */
} /* namespace drizzled */

#endif /* DRIZZLED_PLUGIN_CLIENT_H */
