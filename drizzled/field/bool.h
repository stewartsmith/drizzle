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

#ifndef DRIZZLED_FIELD_BOOL_H
#define DRIZZLED_FIELD_BOOL_H

#include <drizzled/field.h>
#include <string>

namespace drizzled
{
namespace field
{

class Bool :public Field {
  bool ansi_display;

public:
  Bool(unsigned char *ptr_arg,
       uint32_t len_arg,
       unsigned char *null_ptr_arg,
       unsigned char null_bit_arg,
       const char *field_name_arg,
       bool ansi_display= false);

  enum_field_types type() const { return DRIZZLE_TYPE_BOOL; }
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_BINARY; }
  bool zero_pack() const { return 0; }
  int  reset(void) { ptr[0]= 0; return 0; }
  uint32_t pack_length() const { return sizeof(unsigned char); }
  uint32_t key_length() const { return sizeof(unsigned char); }

  int store(const char *to, uint32_t length, const CHARSET_INFO * const charset);
  int store(double );
  int store(int64_t nr, bool unsigned_val);
  int store_decimal(const drizzled::my_decimal*);

  String *val_str(String*,String *);
  double val_real();
  int64_t val_int();
  void sql_type(drizzled::String&) const;

  Item_result result_type () const { return INT_RESULT; }
  int cmp(const unsigned char*, const unsigned char*);
  void sort_string(unsigned char*, uint32_t);
  uint32_t max_display_length() { return 5; } // longest string is "false"

  inline String *val_str(String *str) { return val_str(str, str); }
  uint32_t size_of() const { return sizeof(*this); }

  static size_t max_string_length()
  {
    return sizeof(unsigned char);
  }

private:
  void setTrue();

  void setFalse()
  {
    ptr[0]= 0;
  }

  bool isTrue() const
  {
    return ptr[0] ? true : false;
  }
};

} /* namespace field */
} /* namespace drizzled */

#endif /* DRIZZLED_FIELD_BOOL_H */

