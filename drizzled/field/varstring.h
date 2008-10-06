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

#ifndef DRIZZLE_SERVER_FIELD_VARSTRING
#define DRIZZLE_SERVER_FIELD_VARSTRING

class Field_varstring :public Field_longstr {
public:
  /*
    The maximum space available in a Field_varstring, in bytes. See
    length_bytes.
  */
  static const uint MAX_SIZE;
  /* Store number of bytes used to store length (1 or 2) */
  uint32_t length_bytes;
  Field_varstring(unsigned char *ptr_arg,
                  uint32_t len_arg, uint length_bytes_arg,
                  unsigned char *null_ptr_arg, unsigned char null_bit_arg,
		  enum utype unireg_check_arg, const char *field_name_arg,
		  TABLE_SHARE *share, const CHARSET_INFO * const cs)
    :Field_longstr(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                   unireg_check_arg, field_name_arg, cs),
     length_bytes(length_bytes_arg)
  {
    share->varchar_fields++;
  }
  Field_varstring(uint32_t len_arg,bool maybe_null_arg,
                  const char *field_name_arg,
                  TABLE_SHARE *share, const CHARSET_INFO * const cs)
    :Field_longstr((unsigned char*) 0,len_arg, maybe_null_arg ? (unsigned char*) "": 0, 0,
                   NONE, field_name_arg, cs),
     length_bytes(len_arg < 256 ? 1 :2)
  {
    share->varchar_fields++;
  }

  enum_field_types type() const { return DRIZZLE_TYPE_VARCHAR; }
  enum ha_base_keytype key_type() const;
  uint row_pack_length() { return field_length; }
  bool zero_pack() const { return 0; }
  int  reset(void) { memset(ptr, 0, field_length+length_bytes); return 0; }
  uint32_t pack_length() const { return (uint32_t) field_length+length_bytes; }
  uint32_t key_length() const { return (uint32_t) field_length; }
  uint32_t sort_length() const
  {
    return (uint32_t) field_length + (field_charset == &my_charset_bin ?
                                    length_bytes : 0);
  }
  int  store(const char *to,uint length, const CHARSET_INFO * const charset);
  int  store(int64_t nr, bool unsigned_val);
  int  store(double nr) { return Field_str::store(nr); } /* QQ: To be deleted */
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp_max(const unsigned char *, const unsigned char *, uint32_t max_length);
  int cmp(const unsigned char *a,const unsigned char *b)
  {
    return cmp_max(a, b, UINT32_MAX);
  }
  void sort_string(unsigned char *buff,uint length);
  uint get_key_image(unsigned char *buff,uint length, imagetype type);
  void set_key_image(const unsigned char *buff,uint length);
  void sql_type(String &str) const;
  virtual unsigned char *pack(unsigned char *to, const unsigned char *from,
                      uint max_length, bool low_byte_first);
  unsigned char *pack_key(unsigned char *to, const unsigned char *from, uint max_length, bool low_byte_first);
  unsigned char *pack_key_from_key_image(unsigned char* to, const unsigned char *from,
                                 uint max_length, bool low_byte_first);
  virtual const unsigned char *unpack(unsigned char* to, const unsigned char *from,
                              uint param_data, bool low_byte_first);
  const unsigned char *unpack_key(unsigned char* to, const unsigned char *from,
                          uint max_length, bool low_byte_first);
  int pack_cmp(const unsigned char *a, const unsigned char *b, uint key_length,
               bool insert_or_update);
  int pack_cmp(const unsigned char *b, uint key_length,bool insert_or_update);
  int cmp_binary(const unsigned char *a,const unsigned char *b, uint32_t max_length=UINT32_MAX);
  int key_cmp(const unsigned char *,const unsigned char*);
  int key_cmp(const unsigned char *str, uint length);
  uint packed_col_length(const unsigned char *to, uint length);
  uint max_packed_col_length(uint max_length);
  uint32_t data_length();
  uint32_t used_length();
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return DRIZZLE_TYPE_VARCHAR; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? false : true; }
  Field *new_field(MEM_ROOT *root, Table *new_table, bool keep_type);
  Field *new_key_field(MEM_ROOT *root, Table *new_table,
                       unsigned char *new_ptr, unsigned char *new_null_ptr,
                       uint new_null_bit);
  uint is_equal(Create_field *new_field);
  void hash(uint32_t *nr, uint32_t *nr2);
private:
  int do_save_field_metadata(unsigned char *first_byte);
};

#endif

