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

#if 0

class Field;
class String;
class i_string;
class my_decimal;
typedef struct st_drizzle_field DRIZZLE_FIELD;
typedef struct st_drizzle_rows DRIZZLE_ROWS;
typedef struct st_drizzle_time DRIZZLE_TIME;
#endif

class Protocol
{
protected:
  Session *session;
  String *packet;
  uint32_t field_count;

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

  /* Still need to convert below here. */

  virtual void set_read_timeout(uint32_t timeout)= 0;
  virtual void set_write_timeout(uint32_t timeout)= 0;
  virtual void set_retry_count(uint32_t count)= 0;
  virtual void set_error(char error)= 0;
  virtual bool have_error(void)= 0;
  virtual bool was_aborted(void)= 0;
  virtual bool have_compression(void)= 0;
  virtual void enable_compression(void)= 0;
  virtual bool have_more_data(void)= 0;
  virtual bool is_reading(void)= 0;
  virtual bool is_writing(void)= 0;

  virtual bool init_file_descriptor(int fd)=0;
  virtual int file_descriptor(void)=0;
  virtual void init_random(uint64_t, uint64_t) {};
  virtual bool authenticate(void)=0;
  virtual bool read_command(char **packet, uint32_t *packet_length)=0;
  virtual void sendOK()= 0;
  virtual void sendEOF()= 0;
  virtual void sendError(uint32_t sql_errno, const char *err)=0;
  virtual void sendErrorPacket(uint32_t sql_errno, const char *err)=0;
  virtual void close(void) {};

  enum { SEND_NUM_ROWS= 1, SEND_DEFAULTS= 2, SEND_EOF= 4 };
  virtual bool send_fields(List<Item> *list, uint32_t flags)= 0;

  virtual bool store(I_List<i_string> *str_list)= 0;
  virtual bool store(const char *from, const CHARSET_INFO * const cs)= 0;
  String *storage_packet() { return packet; }
  virtual void free()= 0;
  virtual bool write()= 0;
  virtual bool store(int from)
  { return store_long((int64_t) from); }
  virtual  bool store(uint32_t from)
  { return store_long((int64_t) from); }
  virtual bool store(int64_t from)
  { return store_int64_t((int64_t) from, 0); }
  virtual bool store(uint64_t from)
  { return store_int64_t((int64_t) from, 1); }
  virtual bool store(String *str)= 0;

  virtual bool prepare_for_send(List<Item> *item_list)
  {
    field_count= item_list->elements;
    return 0;
  }
  virtual bool flush()= 0;

  virtual void prepare_for_resend()=0;

  virtual bool store_null()=0;
  virtual bool store_tiny(int64_t from)=0;
  virtual bool store_short(int64_t from)=0;
  virtual bool store_long(int64_t from)=0;
  virtual bool store_int64_t(int64_t from, bool unsigned_flag)=0;
  virtual bool store_decimal(const my_decimal * dec_value)=0;
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO * const cs)=0;
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO * const fromcs,
                     const CHARSET_INFO * const tocs)=0;
  virtual bool store(float from, uint32_t decimals, String *buffer)=0;
  virtual bool store(double from, uint32_t decimals, String *buffer)=0;
  virtual bool store(DRIZZLE_TIME *time)=0;
  virtual bool store_date(DRIZZLE_TIME *time)=0;
  virtual bool store_time(DRIZZLE_TIME *time)=0;
  virtual bool store(Field *field)=0;
};

#endif /* DRIZZLED_PLUGIN_PROTOCOL_H */
