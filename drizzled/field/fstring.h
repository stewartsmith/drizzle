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

#ifndef DRIZZLE_SERVER_FIELD_STRING
#define DRIZZLE_SERVER_FIELD_STRING

class Field_string :public Field_longstr {
public:
  bool can_alter_field_type;
  Field_string(unsigned char *ptr_arg, uint32_t len_arg,unsigned char *null_ptr_arg,
	       unsigned char null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       const CHARSET_INFO * const cs)
    :Field_longstr(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                   unireg_check_arg, field_name_arg, cs),
     can_alter_field_type(1) {};
  Field_string(uint32_t len_arg,bool maybe_null_arg, const char *field_name_arg,
               const CHARSET_INFO * const cs)
    :Field_longstr((unsigned char*) 0, len_arg, maybe_null_arg ? (unsigned char*) "": 0, 0,
                   NONE, field_name_arg, cs),
     can_alter_field_type(1) {};

  enum_field_types type() const
  {
    return  DRIZZLE_TYPE_VARCHAR;
  }
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_BINARY : HA_KEYTYPE_TEXT; }
  bool zero_pack() const { return 0; }
  int reset(void)
  {
    charset()->cset->fill(charset(),(char*) ptr, field_length,
                          (has_charset() ? ' ' : 0));
    return 0;
  }
  int store(const char *to,uint32_t length, const CHARSET_INFO * const charset);
  int store(int64_t nr, bool unsigned_val);
  int store(double nr) { return Field_str::store(nr); } /* QQ: To be deleted */
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp(const unsigned char *,const unsigned char *);
  void sort_string(unsigned char *buff,uint32_t length);
  void sql_type(String &str) const;
  virtual unsigned char *pack(unsigned char *to, const unsigned char *from,
                      uint32_t max_length, bool low_byte_first);
  virtual const unsigned char *unpack(unsigned char* to, const unsigned char *from,
                              uint32_t param_data, bool low_byte_first);
  uint32_t pack_length_from_metadata(uint32_t field_metadata)
  { return (field_metadata & 0x00ff); }
  uint32_t row_pack_length() { return (field_length + 1); }
  int pack_cmp(const unsigned char *a,const unsigned char *b,uint32_t key_length,
               bool insert_or_update);
  int pack_cmp(const unsigned char *b,uint32_t key_length,bool insert_or_update);
  uint32_t packed_col_length(const unsigned char *to, uint32_t length);
  uint32_t max_packed_col_length(uint32_t max_length);
  uint32_t size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return DRIZZLE_TYPE_VARCHAR; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? false : true; }
  Field *new_field(MEM_ROOT *root, Table *new_table, bool keep_type);
  virtual uint32_t get_key_image(unsigned char *buff,uint32_t length, imagetype type);
private:
  int do_save_field_metadata(unsigned char *first_byte);
};

#endif /* DRIZZLE_SERVER_FIELD_STRING */ 
