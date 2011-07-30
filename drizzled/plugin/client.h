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

#include <drizzled/catalog/instance.h>
#include <drizzled/catalog/local.h>
#include <drizzled/error_t.h>
#include <drizzled/item.h>
#include <drizzled/visibility.h>

namespace drizzled {
namespace plugin {

/**
 * This class allows new client sources to be written. This could be through
 * network protocols, in-process threads, or any other client source that can
 * provide commands and handle result sets. The current implementation is
 * file-descriptor based, so for non-fd client sources (like from another
 * thread), derived classes will need to use a pipe() for event notifications.
 */
class DRIZZLED_API Client
{
protected:
  Session *session;

public:
  virtual ~Client() {}

  /**
   * Get attached session from the client object.
   * @retval Session object that is attached, NULL if none.
   */
  virtual Session *getSession()
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
  virtual int getFileDescriptor()= 0;

  /**
   * Check to see if the client is currently connected.
   * @retval Boolean value representing connected state.
   */
  virtual bool isConnected()= 0;

  /**
   * Flush all data that has been buffered with store() methods.
   * @retval Boolean indicating success or failure.
   */
  virtual bool flush()= 0;

  /**
   * Close the client object.
   */
  virtual void close()= 0;

  /**
   * Perform handshake and authorize client if needed.
   */
  virtual bool authenticate()= 0;

  virtual bool isConsole() const
  {
    return false;
  }

  virtual bool isInteractive() const
  {
    return false;
  }

  virtual catalog::Instance::shared_ptr catalog()
  {
    return catalog::local();
  }

  /**
   * Read command from client.
   */
  virtual bool readCommand(char **packet, uint32_t& packet_length)= 0;

  /* Send responses. */
  virtual void sendOK()= 0;
  virtual void sendEOF()= 0;
  virtual void sendError(const drizzled::error_t sql_errno, const char *err)= 0;

  /**
   * Send field list for result set.
   */
  virtual void sendFields(List<Item>&)= 0;

  /* Send result fields in various forms. */
  virtual void store(Field *from)= 0;
  virtual void store()= 0;
  virtual void store(int32_t from)= 0;
  virtual void store(uint32_t from)= 0;
  virtual void store(int64_t from)= 0;
  virtual void store(uint64_t from)= 0;
  virtual void store(double from, uint32_t decimals, String *buffer)= 0;
  virtual void store(const type::Time *from);
  virtual void store(const char *from);
  virtual void store(const char *from, size_t length)= 0;
  virtual void store(const std::string &from)
  {
    store(from.c_str(), from.size());
  }

  /* Try to remove these. */
  virtual bool haveError()= 0;
  virtual bool wasAborted()= 0;

};

} /* namespace plugin */
} /* namespace drizzled */
