/* - mode: c++ c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2010 Brian Aker
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

#include <drizzled/field.h>
#include <string>

#include <drizzled/type/uuid.h>

namespace drizzled
{
namespace field
{

class Uuid :public Field {
  const charset_info_st *field_charset;
  bool is_set;

public:
  Uuid(unsigned char *ptr_arg,
       uint32_t len_arg,
       unsigned char *null_ptr_arg,
       unsigned char null_bit_arg,
       const char *field_name_arg);

  enum_field_types type() const { return DRIZZLE_TYPE_UUID; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  bool zero_pack() const { return 0; }
  int  reset(void) { memset(ptr, 0, type::Uuid::LENGTH); return 0; }
  uint32_t pack_length() const { return type::Uuid::LENGTH; }
  uint32_t key_length() const { return type::Uuid::LENGTH; }

  int store(const char *to, uint32_t length, const charset_info_st * const charset);
  int store(int64_t nr, bool unsigned_val);
  double val_real() const;
  int64_t val_int() const;
  String *val_str(String*,String *) const;
  int store_decimal(const drizzled::type::Decimal*);

  Item_result result_type () const { return STRING_RESULT; }
  int cmp(const unsigned char*, const unsigned char*);
  void sort_string(unsigned char*, uint32_t);
  uint32_t max_display_length() { return type::Uuid::DISPLAY_LENGTH; }

  int  store(double ) { return 0; }
  inline String *val_str(String *str) { return val_str(str, str); }
  uint32_t size_of() const { return sizeof(*this); }

  bool get_date(type::Time &ltime, uint32_t) const;
  bool get_time(type::Time &ltime) const;

#ifdef NOT_YET
  void generate();
  void set(const unsigned char *arg);
#endif

  static size_t max_string_length()
  {
    return type::Uuid::LENGTH;
  }
};

} /* namespace field */
} /* namespace drizzled */


