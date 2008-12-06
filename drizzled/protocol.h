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

#include <drizzled/sql_list.h>
#include <drizzled/item.h>

class Field;
class String;
class i_string;
class Session;
class my_decimal;
typedef struct st_drizzle_field DRIZZLE_FIELD;
typedef struct st_drizzle_rows DRIZZLE_ROWS;
typedef struct st_drizzle_time DRIZZLE_TIME;

class Protocol
{
protected:
  Session	 *session;
  String *packet;
  String *convert;
  uint32_t field_pos;
  uint32_t field_count;
  bool net_store_data(const unsigned char *from, size_t length);
  bool net_store_data(const unsigned char *from, size_t length,
                      const CHARSET_INFO * const fromcs, const CHARSET_INFO * const tocs);
  bool store_string_aux(const char *from, size_t length,
                        const CHARSET_INFO * const fromcs, const CHARSET_INFO * const tocs);
public:
  Protocol() {}
  Protocol(Session *session_arg) { init(session_arg); }
  virtual ~Protocol() {}
  void init(Session* session_arg);

  enum { SEND_NUM_ROWS= 1, SEND_DEFAULTS= 2, SEND_EOF= 4 };
  virtual bool send_fields(List<Item> *list, uint32_t flags);

  bool store(I_List<i_string> *str_list);
  bool store(const char *from, const CHARSET_INFO * const cs);
  String *storage_packet() { return packet; }
  void free();
  virtual bool write();
  inline  bool store(int from)
  { return store_long((int64_t) from); }
  inline  bool store(uint32_t from)
  { return store_long((int64_t) from); }
  inline  bool store(int64_t from)
  { return store_int64_t((int64_t) from, 0); }
  inline  bool store(uint64_t from)
  { return store_int64_t((int64_t) from, 1); }
  bool store(String *str);

  virtual bool prepare_for_send(List<Item> *item_list)
  {
    field_count=item_list->elements;
    return 0;
  }
  virtual bool flush();
  virtual void end_partial_result_set(Session *session);
  virtual void prepare_for_resend()=0;

  virtual bool store_null()=0;
  virtual bool store_tiny(int64_t from)=0;
  virtual bool store_short(int64_t from)=0;
  virtual bool store_long(int64_t from)=0;
  virtual bool store_int64_t(int64_t from, bool unsigned_flag)=0;
  virtual bool store_decimal(const my_decimal * dec_value)=0;
  virtual bool store(const char *from, size_t length, const CHARSET_INFO * const cs)=0;
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO * const fromcs, const CHARSET_INFO * const tocs)=0;
  virtual bool store(float from, uint32_t decimals, String *buffer)=0;
  virtual bool store(double from, uint32_t decimals, String *buffer)=0;
  virtual bool store(DRIZZLE_TIME *time)=0;
  virtual bool store_date(DRIZZLE_TIME *time)=0;
  virtual bool store_time(DRIZZLE_TIME *time)=0;
  virtual bool store(Field *field)=0;
  void remove_last_row() {}
  enum enum_protocol_type
  {
    PROTOCOL_TEXT= 0, PROTOCOL_BINARY= 1
    /*
      before adding here or change the values, consider that it is cast to a
      bit in sql_cache.cc.
    */
  };
  virtual enum enum_protocol_type type()= 0;
};


/** Class used for the old (MySQL 4.0 protocol). */

class Protocol_text :public Protocol
{
public:
  Protocol_text() {}
  Protocol_text(Session *session_arg) :Protocol(session_arg) {}
  virtual void prepare_for_resend();
  virtual bool store_null();
  virtual bool store_tiny(int64_t from);
  virtual bool store_short(int64_t from);
  virtual bool store_long(int64_t from);
  virtual bool store_int64_t(int64_t from, bool unsigned_flag);
  virtual bool store_decimal(const my_decimal *);
  virtual bool store(const char *from, size_t length, const CHARSET_INFO * const cs);
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO * const fromcs,  const CHARSET_INFO * const tocs);
  virtual bool store(DRIZZLE_TIME *time);
  virtual bool store_date(DRIZZLE_TIME *time);
  virtual bool store_time(DRIZZLE_TIME *time);
  virtual bool store(float nr, uint32_t decimals, String *buffer);
  virtual bool store(double from, uint32_t decimals, String *buffer);
  virtual bool store(Field *field);
  virtual enum enum_protocol_type type() { return PROTOCOL_TEXT; };
};

void send_warning(Session *session, uint32_t sql_errno, const char *err=0);
void net_send_error(Session *session, uint32_t sql_errno=0, const char *err=0);
void net_end_statement(Session *session);
unsigned char *net_store_data(unsigned char *to,const unsigned char *from, size_t length);
unsigned char *net_store_data(unsigned char *to,int32_t from);
unsigned char *net_store_data(unsigned char *to,int64_t from);

#endif /* DRIZZLED_PROTOCOL_H */
