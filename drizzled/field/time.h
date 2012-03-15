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

#pragma once

#include <drizzled/field/str.h>

namespace drizzled {
namespace field {

class Time :public Field_str {
public:
  Time(unsigned char *ptr_arg,
                  uint32_t len_arg,
                  unsigned char *null_ptr_arg,
                  unsigned char null_bit_arg,
                  const char *field_name_arg);

  Time(bool maybe_null_arg,
                  const char *field_name_arg);

  enum_field_types type() const { return DRIZZLE_TYPE_TIMESTAMP;}
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_LONG_INT; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  int  store(const char *to,uint32_t length,
             const charset_info_st * const charset);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);
  int  reset(void) { ptr[0]= ptr[1]= ptr[2]= ptr[3]= 0; return 0; }
  double val_real(void) const;
  int64_t val_int(void) const;
  String *val_str(String*,String *) const;
  int cmp(const unsigned char *,const unsigned char *);
  void sort_string(unsigned char *buff,uint32_t length);
  uint32_t pack_length() const { return 4; }
  bool can_be_compared_as_int64_t() const { return true; }
  bool zero_pack() const { return 0; }

  /* Get TIME field value as seconds since begging of Unix Epoch */
  long get_timestamp(bool *null_value) const;
private:
  bool get_date(type::Time &ltime, uint32_t fuzzydate) const;
  bool get_time(type::Time &ltime) const;

public:
  timestamp_auto_set_type get_auto_set_type() const;
  static size_t max_string_length();
  void pack_time(drizzled::Time &arg);
  void unpack_time(drizzled::Time &arg) const;
  void unpack_time(int32_t &destination, const unsigned char *source) const;
};

} /* namespace field */
} /* namespace drizzled */


