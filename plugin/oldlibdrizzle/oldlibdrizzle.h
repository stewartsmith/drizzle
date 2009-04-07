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

#include <drizzled/plugin/protocol.h>

#include "net_serv.h"
#include "password.h"

class ProtocolOldLibdrizzle: public Protocol
{
private:
  NET net;
  Vio* save_vio;
  struct rand_struct rand;
  char scramble[SCRAMBLE_LENGTH+1];
  String *convert;
  uint32_t field_pos;
  uint32_t field_count;
  bool netStoreData(const unsigned char *from, size_t length);
  bool netStoreData(const unsigned char *from, size_t length,
                    const CHARSET_INFO * const fromcs,
                    const CHARSET_INFO * const tocs);
  bool storeStringAux(const char *from, size_t length,
                      const CHARSET_INFO * const fromcs,
                      const CHARSET_INFO * const tocs);

  /**
   * Performs handshake with client and authorizes user.
   *
   * Returns true is the connection is valid and the
   * user is authorized, otherwise false.
   */  
  bool checkConnection(void);

public:
  ProtocolOldLibdrizzle();
  virtual void setSession(Session *session_arg);
  virtual bool isConnected();

  virtual void set_read_timeout(uint32_t timeout);
  virtual void set_write_timeout(uint32_t timeout);
  virtual void set_retry_count(uint32_t count);
  virtual void set_error(char error);
  virtual bool have_error(void);
  virtual bool was_aborted(void);
  virtual bool have_compression(void); 
  virtual void enable_compression(void);
  virtual bool have_more_data(void);
  virtual bool is_reading(void);
  virtual bool is_writing(void);
  virtual bool init_file_descriptor(int fd);
  virtual int file_descriptor(void);
  virtual void init_random(uint64_t seed1, uint64_t seed2);
  virtual bool authenticate(void);
  virtual bool read_command(char **packet, uint32_t *packet_length);
  virtual void sendOK();
  virtual void sendEOF();
  virtual void sendError(uint32_t sql_errno, const char *err);
  virtual void close(void);
  virtual void prepare_for_resend();
  virtual void free();
  virtual bool write();
  virtual bool flush();
  virtual bool send_fields(List<Item> *list, uint32_t flags);

  using Protocol::store;
  virtual bool store(Field *from);
  virtual bool store(void);
  virtual bool store(int32_t from);
  virtual bool store(uint32_t from);
  virtual bool store(int64_t from);
  virtual bool store(uint64_t from);
  virtual bool store(double from, uint32_t decimals, String *buffer);
  virtual bool store(const DRIZZLE_TIME *from);
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO * const cs);
};

class ProtocolFactoryOldLibdrizzle: public ProtocolFactory
{
public:
  Protocol *operator()(void)
  {
    return new ProtocolOldLibdrizzle;
  }
};

#endif /* DRIZZLED_PLUGIN_OLDLIBDRIZZLE_H */
