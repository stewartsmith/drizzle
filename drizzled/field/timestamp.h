/* - mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL
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

#ifndef DRIZZLE_SERVER_FIELD_TIMESTAMP
#define DRIZZLE_SERVER_FIELD_TIMESTAMP

class Field_timestamp :public Field_str {
public:
  Field_timestamp(uchar *ptr_arg, uint32_t len_arg,
                  uchar *null_ptr_arg, uchar null_bit_arg,
		  enum utype unireg_check_arg, const char *field_name_arg,
		  TABLE_SHARE *share, CHARSET_INFO *cs);
  Field_timestamp(bool maybe_null_arg, const char *field_name_arg,
		  CHARSET_INFO *cs);
  enum_field_types type() const { return DRIZZLE_TYPE_TIMESTAMP;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);
  int  reset(void) { ptr[0]=ptr[1]=ptr[2]=ptr[3]=0; return 0; }
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32_t pack_length() const { return 4; }
  void sql_type(String &str) const;
  bool can_be_compared_as_int64_t() const { return true; }
  bool zero_pack() const { return 0; }
  void set_time();
  virtual void set_default()
  {
    if (table->timestamp_field == this &&
        unireg_check != TIMESTAMP_UN_FIELD)
      set_time();
    else
      Field::set_default();
  }
  /* Get TIMESTAMP field value as seconds since begging of Unix Epoch */
  inline long get_timestamp(my_bool *null_value)
  {
    if ((*null_value= is_null()))
      return 0;
#ifdef WORDS_BIGENDIAN
    if (table && table->s->db_low_byte_first)
      return sint4korr(ptr);
#endif
    long tmp;
    longget(tmp,ptr);
    return tmp;
  }
  inline void store_timestamp(my_time_t timestamp)
  {
#ifdef WORDS_BIGENDIAN
    if (table && table->s->db_low_byte_first)
    {
      int4store(ptr,timestamp);
    }
    else
#endif
      longstore(ptr,(uint32_t) timestamp);
  }
  bool get_date(DRIZZLE_TIME *ltime,uint fuzzydate);
  bool get_time(DRIZZLE_TIME *ltime);
  timestamp_auto_set_type get_auto_set_type() const;
};

#endif

