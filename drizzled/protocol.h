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

#ifndef DRIZZLED_PROTOCOL_H
#define DRIZZLED_PROTOCOL_H

#include <drizzled/plugin/protocol.h>
#include <drizzled/sql_list.h>
#include <drizzled/item.h>
#include <libdrizzleclient/net_serv.h>
#include <libdrizzleclient/password.h>

class Field;
class String;
class i_string;
class my_decimal;
typedef struct st_vio Vio;
typedef struct st_drizzle_field DRIZZLE_FIELD;
typedef struct st_drizzle_rows DRIZZLE_ROWS;
typedef struct st_drizzle_time DRIZZLE_TIME;

class Protocol_libdrizzleclient :public Protocol
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
  Protocol_libdrizzleclient();

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
  virtual void sendErrorPacket(uint32_t sql_errno, const char *err);
  virtual void close(void);
  virtual void prepare_for_resend();
  virtual void free();
  virtual bool write();
  virtual bool flush();
  virtual bool send_fields(List<Item> *list, uint32_t flags);
  virtual bool store(I_List<i_string> *str_list);
  virtual bool store(const char *from, const CHARSET_INFO * const cs);
  virtual bool store_null();
  virtual bool store_tiny(int64_t from);
  virtual bool store_short(int64_t from);
  virtual bool store_long(int64_t from);
  virtual bool store_int64_t(int64_t from, bool unsigned_flag);
  virtual bool store_decimal(const my_decimal *);
  virtual bool store(int from)
  { return store_long((int64_t) from); }
  virtual  bool store(uint32_t from)
  { return store_long((int64_t) from); }
  virtual bool store(int64_t from)
  { return store_int64_t((int64_t) from, 0); }
  virtual bool store(uint64_t from)
  { return store_int64_t((int64_t) from, 1); }
  virtual bool store(String *str);
  virtual bool store(const char *from, size_t length, const CHARSET_INFO * const cs);
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO * const fromcs,  const CHARSET_INFO * const tocs);
  virtual bool store(DRIZZLE_TIME *time);
  virtual bool store_date(DRIZZLE_TIME *time);
  virtual bool store_time(DRIZZLE_TIME *time);
  virtual bool store(float nr, uint32_t decimals, String *buffer);
  virtual bool store(double from, uint32_t decimals, String *buffer);
  virtual bool store(Field *field);
};

#endif /* DRIZZLED_PROTOCOL_H */
