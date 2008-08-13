/* Copyright (C) 2002-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif


class i_string;
class THD;
typedef struct st_drizzle_field DRIZZLE_FIELD;
typedef struct st_drizzle_rows DRIZZLE_ROWS;

class Protocol
{
protected:
  THD	 *thd;
  String *packet;
  String *convert;
  uint field_pos;
  uint field_count;
  bool net_store_data(const uchar *from, size_t length);
  bool net_store_data(const uchar *from, size_t length,
                      const CHARSET_INFO * const fromcs, const CHARSET_INFO * const tocs);
  bool store_string_aux(const char *from, size_t length,
                        const CHARSET_INFO * const fromcs, const CHARSET_INFO * const tocs);
public:
  Protocol() {}
  Protocol(THD *thd_arg) { init(thd_arg); }
  virtual ~Protocol() {}
  void init(THD* thd_arg);

  enum { SEND_NUM_ROWS= 1, SEND_DEFAULTS= 2, SEND_EOF= 4 };
  virtual bool send_fields(List<Item> *list, uint flags);

  bool store(I_List<i_string> *str_list);
  bool store(const char *from, const CHARSET_INFO * const cs);
  String *storage_packet() { return packet; }
  inline void free() { packet->free(); }
  virtual bool write();
  inline  bool store(int from)
  { return store_long((int64_t) from); }
  inline  bool store(uint32_t from)
  { return store_long((int64_t) from); }
  inline  bool store(int64_t from)
  { return store_int64_t((int64_t) from, 0); }
  inline  bool store(uint64_t from)
  { return store_int64_t((int64_t) from, 1); }
  inline bool store(String *str)
  { return store((char*) str->ptr(), str->length(), str->charset()); }

  virtual bool prepare_for_send(List<Item> *item_list) 
  {
    field_count=item_list->elements;
    return 0;
  }
  virtual bool flush();
  virtual void end_partial_result_set(THD *thd);
  virtual void prepare_for_resend()=0;

  virtual bool store_null()=0;
  virtual bool store_tiny(int64_t from)=0;
  virtual bool store_short(int64_t from)=0;
  virtual bool store_long(int64_t from)=0;
  virtual bool store_int64_t(int64_t from, bool unsigned_flag)=0;
  virtual bool store_decimal(const my_decimal *)=0;
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
  Protocol_text(THD *thd_arg) :Protocol(thd_arg) {}
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

void send_warning(THD *thd, uint sql_errno, const char *err=0);
void net_send_error(THD *thd, uint sql_errno=0, const char *err=0);
void net_end_statement(THD *thd);
uchar *net_store_data(uchar *to,const uchar *from, size_t length);
uchar *net_store_data(uchar *to,int32_t from);
uchar *net_store_data(uchar *to,int64_t from);

