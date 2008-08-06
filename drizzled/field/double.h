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

#ifndef DRIZZLE_SERVER_FIELD_DOUBLE
#define DRIZZLE_SERVER_FIELD_DOUBLE

#include <drizzled/mysql_priv.h>

class Field_double :public Field_real {
public:
  Field_double(uchar *ptr_arg, uint32_t len_arg, uchar *null_ptr_arg,
	       uchar null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       uint8_t dec_arg,bool zero_arg,bool unsigned_arg)
    :Field_real(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                unireg_check_arg, field_name_arg,
                dec_arg, zero_arg, unsigned_arg)
    {}
  Field_double(uint32_t len_arg, bool maybe_null_arg, const char *field_name_arg,
	       uint8_t dec_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "" : 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {}
  Field_double(uint32_t len_arg, bool maybe_null_arg, const char *field_name_arg,
	       uint8_t dec_arg, my_bool not_fixed_arg)
    :Field_real((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "" : 0, (uint) 0,
                NONE, field_name_arg, dec_arg, 0, 0)
    {not_fixed= not_fixed_arg; }
  enum_field_types type() const { return DRIZZLE_TYPE_DOUBLE;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_DOUBLE; }
  int  store(const char *to,uint length, const CHARSET_INFO * const charset);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);
  int reset(void) { memset(ptr, 0, sizeof(double)); return 0; }
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  bool send_binary(Protocol *protocol);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  uint32_t pack_length() const { return sizeof(double); }
  uint row_pack_length() { return pack_length(); }
  void sql_type(String &str) const;
private:
  int do_save_field_metadata(uchar *first_byte);
};

#endif

