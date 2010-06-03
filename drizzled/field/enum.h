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

#ifndef DRIZZLED_FIELD_ENUM_H
#define DRIZZLED_FIELD_ENUM_H

#include "drizzled/field/str.h"

namespace drizzled
{

class Field_enum :public Field_str
{
protected:
  uint32_t packlength;
public:

  using Field::store;
  using Field::val_int;
  using Field::val_str;
  using Field::cmp;

  /** Internal storage for the string values of the ENUM */
  TYPELIB *typelib;
  Field_enum(unsigned char *ptr_arg,
             uint32_t len_arg,
             unsigned char *null_ptr_arg,
             unsigned char null_bit_arg,
             const char *field_name_arg,
             uint32_t packlength_arg,
             TYPELIB *typelib_arg,
             const CHARSET_INFO * const charset_arg)
    :Field_str(ptr_arg,
               len_arg,
               null_ptr_arg,
               null_bit_arg,
	             field_name_arg,
               charset_arg),
    packlength(packlength_arg),
    typelib(typelib_arg)
  {
    flags|= ENUM_FLAG;
  }
  Field *new_field(memory::Root *root, Table *new_table, bool keep_type);
  enum ha_base_keytype key_type() const;
  int  store(const char *to, uint32_t length, const CHARSET_INFO * const);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*, String *);
  int cmp(const unsigned char *, const unsigned char *);
  void sort_string(unsigned char *buff, uint32_t length);
  void store_type(uint64_t value);
  void sql_type(String &str) const;
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
  uint32_t pack_length() const
  {
    return (uint32_t) packlength;
  }
  uint32_t size_of() const
  {
    return sizeof(*this);
  }
  enum_field_types real_type() const
  {
    return DRIZZLE_TYPE_ENUM;
  }
  uint32_t pack_length_from_metadata(uint32_t field_metadata)
  {
    return (field_metadata & 0x00ff);
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
  const CHARSET_INFO *sort_charset(void) const { return &my_charset_bin; }
};

} /* namespace drizzled */

#endif /* DRIZZLED_FIELD_ENUM_H */
