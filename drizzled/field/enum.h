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

#ifndef DRIZZLE_SERVER_FIELD_ENUM
#define DRIZZLE_SERVER_FIELD_ENUM

class Field_enum :public Field_str {
protected:
  uint32_t packlength;
public:
  TYPELIB *typelib;
  Field_enum(unsigned char *ptr_arg, uint32_t len_arg, unsigned char *null_ptr_arg,
             unsigned char null_bit_arg,
             enum utype unireg_check_arg, const char *field_name_arg,
             uint32_t packlength_arg,
             TYPELIB *typelib_arg,
             const CHARSET_INFO * const charset_arg)
    :Field_str(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
	       unireg_check_arg, field_name_arg, charset_arg),
    packlength(packlength_arg),typelib(typelib_arg)
  {
      flags|=ENUM_FLAG;
  }
  Field *new_field(MEM_ROOT *root, Table *new_table, bool keep_type);
  enum_field_types type() const { return DRIZZLE_TYPE_ENUM; }
  enum Item_result cmp_type () const { return INT_RESULT; }
  enum Item_result cast_to_int_type () const { return INT_RESULT; }
  enum ha_base_keytype key_type() const;
  int  store(const char *to,uint32_t length, const CHARSET_INFO * const charset);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  int cmp(const unsigned char *,const unsigned char *);
  void sort_string(unsigned char *buff,uint32_t length);
  uint32_t pack_length() const { return (uint32_t) packlength; }
  void store_type(uint64_t value);
  void sql_type(String &str) const;
  uint32_t size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return DRIZZLE_TYPE_ENUM; }
  uint32_t pack_length_from_metadata(uint32_t field_metadata)
  { return (field_metadata & 0x00ff); }
  uint32_t row_pack_length() { return pack_length(); }
  virtual bool zero_pack() const { return 0; }
  bool optimize_range(uint32_t idx __attribute__((unused)),
                      uint32_t part __attribute__((unused)))
  { return 0; }
  bool eq_def(Field *field);
  bool has_charset(void) const { return true; }
  /* enum and set are sorted as integers */
  const CHARSET_INFO *sort_charset(void) const { return &my_charset_bin; }
private:
  int do_save_field_metadata(unsigned char *first_byte);
};

#endif
