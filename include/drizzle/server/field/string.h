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

#ifndef DRIZZLE_SERVER_FIELD_STRING
#define DRIZZLE_SERVER_FIELD_STRING

#include "mysql_priv.h"

class Field_string :public Field_longstr {
public:
  bool can_alter_field_type;
  Field_string(uchar *ptr_arg, uint32_t len_arg,uchar *null_ptr_arg,
	       uchar null_bit_arg,
	       enum utype unireg_check_arg, const char *field_name_arg,
	       CHARSET_INFO *cs)
    :Field_longstr(ptr_arg, len_arg, null_ptr_arg, null_bit_arg,
                   unireg_check_arg, field_name_arg, cs),
     can_alter_field_type(1) {};
  Field_string(uint32_t len_arg,bool maybe_null_arg, const char *field_name_arg,
               CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
     can_alter_field_type(1) {};

  enum_field_types type() const
  {
    return  FIELD_TYPE_STRING;
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
  int store(const char *to,uint length,CHARSET_INFO *charset);
  int store(int64_t nr, bool unsigned_val);
  int store(double nr) { return Field_str::store(nr); } /* QQ: To be deleted */
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp(const uchar *,const uchar *);
  void sort_string(uchar *buff,uint length);
  void sql_type(String &str) const;
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar* to, const uchar *from,
                              uint param_data, bool low_byte_first);
  uint pack_length_from_metadata(uint field_metadata)
  { return (field_metadata & 0x00ff); }
  uint row_pack_length() { return (field_length + 1); }
  int pack_cmp(const uchar *a,const uchar *b,uint key_length,
               my_bool insert_or_update);
  int pack_cmp(const uchar *b,uint key_length,my_bool insert_or_update);
  uint packed_col_length(const uchar *to, uint length);
  uint max_packed_col_length(uint max_length);
  uint size_of() const { return sizeof(*this); }
  enum_field_types real_type() const { return FIELD_TYPE_STRING; }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? false : true; }
  Field *new_field(MEM_ROOT *root, struct st_table *new_table, bool keep_type);
  virtual uint get_key_image(uchar *buff,uint length, imagetype type);
private:
  int do_save_field_metadata(uchar *first_byte);
};

#endif
