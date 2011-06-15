/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Pawel Blokus
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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


#ifndef UNITTESTS_STUB_PLUGIN_STUBS_H
#define UNITTESTS_STUB_PLUGIN_STUBS_H
 
#include <config.h>

#include <cstring>
#include <drizzled/plugin/authentication.h>
#include <drizzled/plugin/client.h>
#include <drizzled/plugin/error_message.h>
#include <string>
 
class ClientStub : public drizzled::plugin::Client
{
protected:
  bool store_ret_val;
  char *last_call_char_ptr;

public:

  ClientStub() :
  store_ret_val(false),
  last_call_char_ptr(NULL)
  {}

  inline void set_store_ret_val(bool value)
  {
    store_ret_val= value;
  }

  inline void set_last_call_char_ptr(char *ptr)
  {
    last_call_char_ptr= ptr;
  }

  virtual ~ClientStub() {}

  /**
  * Get attached session from the client object.
  * @retval Session object that is attached, NULL if none.
  */
  virtual drizzled::Session *getSession(void)
  {
    return Client::getSession();
  }

  /**
  * Attach session to the client object.
  * @param[in] session_arg Session object to attach, or NULL to clear.
  */
  virtual void setSession(drizzled::Session *session_arg)
  {
    Client::setSession(session_arg);
  }

  /**
  * Get file descriptor associated with client object.
  * @retval File descriptor that is attached, -1 if none.
  */
  virtual int getFileDescriptor(void) { return 0; };

  /**
  * Check to see if the client is currently connected.
  * @retval Boolean value representing connected state.
  */
  virtual bool isConnected(void) { return false; };

  /**
  * Flush all data that has been buffered with store() methods.
  * @retval Boolean indicating success or failure.
  */
  virtual bool flush(void)  { return false; };

  /**
  * Close the client object.
  */
  virtual void close(void) {};

  /**
  * Perform handshake and authorize client if needed.
  */
  virtual bool authenticate(void) { return false; };

  /**
  * Read command from client.
  */
  virtual bool readCommand(char **packet, uint32_t *packet_length)
  {
    (void)packet;
    (void)packet_length;
    return false;
  };

  /* Send responses. */
  virtual void sendOK(void) {};
  virtual void sendEOF(void) {};
  virtual void sendError(uint32_t sql_errno, const char *err)
  {
    (void)sql_errno;
    (void)err;
  };

  /**
  * Send field list for result set.
  */
  virtual bool sendFields(drizzled::List<drizzled::Item> *list)
  {
    (void)list;
    return false;
  };

  /* Send result fields in various forms. */
  virtual bool store(drizzled::Field *from)
  {
    (void)from;
    return store_ret_val;
  };
  virtual bool store(void) { return store_ret_val; };
  virtual bool store(int32_t from)
  {
    (void)from;
    return store_ret_val;
  };
  virtual bool store(uint32_t from)
  {
    (void)from;
    return store_ret_val;
  };
  virtual bool store(int64_t from)
  {
    (void)from;
    return store_ret_val;
  };
  virtual bool store(uint64_t from)
  {
    (void)from;
    return store_ret_val;
  };
  virtual bool store(double from, uint32_t decimals, drizzled::String *buffer)
  {
    (void)from;
    (void)decimals;
    (void)buffer;
    return store_ret_val;
  };
  virtual bool store(const drizzled::type::Time *from)
  {
    return Client::store(from);
  }
  virtual bool store(const char *from)
  {
    return Client::store(from);
  }
  virtual bool store(const char *from, size_t length)
  {
    strncpy(last_call_char_ptr, from, length);
    return store_ret_val;
  };
  virtual bool store(const std::string &from)
  {
    return Client::store(from);
  }

  /* Try to remove these. */
  virtual bool haveError(void) { return false; };
  virtual bool wasAborted(void) { return false;};
};

class ErrorMessageStub : public drizzled::plugin::ErrorMessage
{

public:
  ErrorMessageStub() : ErrorMessage("ErrorMessageStub") {}

  virtual bool errmsg(drizzled::Session *session, int priority, const char *format, va_list ap)
  {
    (void)session;
    (void)priority;
    (void)format;
    (void)ap;
    return false;
  }
};

class AuthenticationStub : public drizzled::plugin::Authentication
{
private:
  bool authenticate_return;

public:
  AuthenticationStub(std::string name_arg)
  : Authentication(name_arg),
    authenticate_return(false)
  {}

  void set_authenticate_return(bool value)
  {
    authenticate_return = value;
  }

  virtual bool authenticate(const drizzled::identifier::User &sctx, const std::string &passwd)
  {
    (void)sctx;
    (void)passwd;
    return authenticate_return;
  };
};

#endif /* UNITTESTS_STUB_PLUGIN_STUBS_H */
