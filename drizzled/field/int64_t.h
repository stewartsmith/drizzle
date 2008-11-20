/* - mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

#ifndef DRIZZLE_SERVER_FIELD_INT_64_T
#define DRIZZLE_SERVER_FIELD_INT_64_T

#include <drizzled/field/num.h>

class Field_int64_t :public Field_num {
public:
  Field_int64_t(unsigned char *ptr_arg, uint32_t len_arg,
                unsigned char *null_ptr_arg,
                unsigned char null_bit_arg,
                enum utype unireg_check_arg, const char *field_name_arg,
                bool zero_arg, bool unsigned_arg)
    :Field_num(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
               unireg_check_arg, field_name_arg,
               0, zero_arg,unsigned_arg)
    {}
  Field_int64_t(uint32_t len_arg,bool maybe_null_arg,
                const char *field_name_arg,
                bool unsigned_arg)
    :Field_num((unsigned char*) 0, len_arg, maybe_null_arg ? (unsigned char*) "": 0,0,
               NONE, field_name_arg,0,0,unsigned_arg)
    {}
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types type() const { return DRIZZLE_TYPE_LONGLONG;}
  enum ha_base_keytype key_type() const
    { return unsigned_flag ? HA_KEYTYPE_ULONGLONG : HA_KEYTYPE_LONGLONG; }
  int store(const char *to,uint32_t length, const CHARSET_INFO * const charset);
  int store(double nr);
  int store(int64_t nr, bool unsigned_val);
  int reset(void)
  {
    ptr[0]=ptr[1]=ptr[2]=ptr[3]=ptr[4]=ptr[5]=ptr[6]=ptr[7]=0;
    return 0;
  }
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const unsigned char *,const unsigned char *);
  void sort_string(unsigned char *buff,uint32_t length);
  uint32_t pack_length() const { return 8; }
  void sql_type(String &str) const;
  bool can_be_compared_as_int64_t() const { return true; }
  uint32_t max_display_length() { return 20; }
  virtual unsigned char *pack(unsigned char* to, const unsigned char *from,
                      uint32_t max_length __attribute__((unused)),
                      bool low_byte_first __attribute__((unused)))
  {
    int64_t val;
#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      val = sint8korr(from);
    else
#endif
      int64_tget(val, from);

#ifdef WORDS_BIGENDIAN
    if (low_byte_first)
      int8store(to, val);
    else
#endif
      int64_tstore(to, val);
    return to + sizeof(val);
  }

  virtual const unsigned char *unpack(unsigned char* to, const unsigned char *from,
                              uint32_t param_data __attribute__((unused)),
                              bool low_byte_first __attribute__((unused)))
  {
    int64_t val;
#ifdef WORDS_BIGENDIAN
    if (low_byte_first)
      val = sint8korr(from);
    else
#endif
      int64_tget(val, from);

#ifdef WORDS_BIGENDIAN
    if (table->s->db_low_byte_first)
      int8store(to, val);
    else
#endif
      int64_tstore(to, val);
    return from + sizeof(val);
  }
};

#endif
