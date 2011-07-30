/* - mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
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

namespace drizzled
{

class Field_enum :public Field_str
{
public:

  using Field::store;
  using Field::val_int;
  using Field::val_str;
  using Field::cmp;

  static const int max_supported_elements = 0x10000;

  /** Internal storage for the string values of the ENUM */
  TYPELIB *typelib;
  Field_enum(unsigned char *ptr_arg,
             uint32_t len_arg,
             unsigned char *null_ptr_arg,
             unsigned char null_bit_arg,
             const char *field_name_arg,
             TYPELIB *typelib_arg,
             const charset_info_st * const charset_arg)
    :Field_str(ptr_arg,
               len_arg,
               null_ptr_arg,
               null_bit_arg,
	             field_name_arg,
               charset_arg),
    typelib(typelib_arg)
  {
    flags|= ENUM_FLAG;
  }
  Field *new_field(memory::Root *root, Table *new_table, bool keep_type);
  enum ha_base_keytype key_type() const { return HA_KEYTYPE_ULONG_INT; }
  int  store(const char *to, uint32_t length, const charset_info_st * const);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);
  double val_real(void) const;
  int64_t val_int(void) const;
  String *val_str(String*, String *) const;
  int cmp(const unsigned char *, const unsigned char *);
  void sort_string(unsigned char *buff, uint32_t length);
  void store_type(uint64_t value);
  bool eq_def(Field *field);
  enum_field_types type() const
  {
    return DRIZZLE_TYPE_ENUM;
  }
  enum Item_result cmp_type () const
  {
    return INT_RESULT;
  }
  enum Item_result cast_to_int_type () const
  {
    return INT_RESULT;
  }
  uint32_t pack_length() const { return 4; }
  uint32_t size_of() const
  {
    return sizeof(*this);
  }
  enum_field_types real_type() const
  {
    return DRIZZLE_TYPE_ENUM;
  }
  virtual bool zero_pack() const
  {
    return false;
  }
  bool optimize_range(uint32_t, uint32_t)
  {
    return false;
  }
  bool has_charset(void) const
  {
    return true;
  }
  /* enum and set are sorted as integers */
  const charset_info_st *sort_charset(void) const { return &my_charset_bin; }
};

} /* namespace drizzled */

