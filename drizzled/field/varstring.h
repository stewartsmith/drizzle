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
#include <string>

namespace drizzled
{

class Field_varstring :public Field_str {
public:

  using Field::store;
  using Field::pack;
  using Field::unpack;
  using Field::val_int;
  using Field::val_str;

  /*
    The maximum space available in a Field_varstring, in bytes. See
    length_bytes.
  */
  static const uint32_t MAX_SIZE;
private:
  /* Store number of bytes used to store length (1 or 2) */
  uint32_t length_bytes;
public:
  Field_varstring(unsigned char *ptr_arg,
                  uint32_t len_arg,
                  uint32_t length_bytes_arg,
                  unsigned char *null_ptr_arg,
                  unsigned char null_bit_arg,
                  const char *field_name_arg,
                  const charset_info_st * const cs);
  Field_varstring(uint32_t len_arg,
                  bool maybe_null_arg,
                  const char *field_name_arg,
                  const charset_info_st * const cs);

  enum_field_types type() const { return DRIZZLE_TYPE_VARCHAR; }
  enum ha_base_keytype key_type() const;
  bool zero_pack() const { return 0; }
  int  reset(void) { memset(ptr, 0, field_length+length_bytes); return 0; }
  uint32_t pack_length() const { return (uint32_t) field_length+length_bytes; }
  uint32_t pack_length_no_ptr() const { return length_bytes; }
  uint32_t key_length() const { return (uint32_t) field_length; }
  uint32_t sort_length() const
  {
    return (uint32_t) field_length + (field_charset == &my_charset_bin ?
                                      length_bytes : 0);
  }
  int  store(const char *to,uint32_t length, const charset_info_st * const charset);


  int  store(int64_t nr, bool unsigned_val);
  int  store(double nr) { return Field_str::store(nr); } /* QQ: To be deleted */
  double val_real(void) const;
  int64_t val_int(void) const;
  String *val_str(String*,String *) const;
  inline String *val_str(String *str) { return val_str(str, str); }
  type::Decimal *val_decimal(type::Decimal *) const;
  int cmp_max(const unsigned char *, const unsigned char *, uint32_t max_length);
  inline  int cmp(const unsigned char *str) { return cmp(ptr,str); }
  int cmp(const unsigned char *a,const unsigned char *b)
  {
    return cmp_max(a, b, UINT32_MAX);
  }
  void sort_string(unsigned char *buff,uint32_t length);
  uint32_t get_key_image(unsigned char *buff,uint32_t length);
  uint32_t get_key_image(std::basic_string <unsigned char> &buff, uint32_t length);
  void set_key_image(const unsigned char *buff,uint32_t length);
  virtual unsigned char *pack(unsigned char *to,
                              const unsigned char *from,
                              uint32_t max_length,
                              bool low_byte_first);

  virtual const unsigned char *unpack(unsigned char* to,
                                      const unsigned char *from,
                                      uint32_t param_data,
                                      bool low_byte_first);

  int cmp_binary(const unsigned char *a,const unsigned char *b, uint32_t max_length=UINT32_MAX);
  int key_cmp(const unsigned char *,const unsigned char*);
  int key_cmp(const unsigned char *str, uint32_t length);
  uint32_t max_packed_col_length(uint32_t max_length);
  uint32_t used_length();
  uint32_t size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return DRIZZLE_TYPE_VARCHAR; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? false : true; }
  Field *new_field(memory::Root *root, Table *new_table, bool keep_type);
  Field *new_key_field(memory::Root *root, Table *new_table,
                       unsigned char *new_ptr, unsigned char *new_null_ptr,
                       uint32_t new_null_bit);
};

} /* namespace drizzled */


