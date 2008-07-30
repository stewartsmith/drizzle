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

#ifndef DRIZZLE_SERVER_FIELD_BLOB
#define DRIZZLE_SERVER_FIELD_BLOB

#include "mysql_priv.h"

class Field_blob :public Field_longstr {
protected:
  uint packlength;
  String value;				// For temporaries
public:
  Field_blob(uchar *ptr_arg, uchar *null_ptr_arg, uchar null_bit_arg,
	     enum utype unireg_check_arg, const char *field_name_arg,
	     TABLE_SHARE *share, uint blob_pack_length, CHARSET_INFO *cs);
  Field_blob(uint32_t len_arg,bool maybe_null_arg, const char *field_name_arg,
             CHARSET_INFO *cs)
    :Field_longstr((uchar*) 0, len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs),
    packlength(4)
  {
    flags|= BLOB_FLAG;
  }
  Field_blob(uint32_t len_arg,bool maybe_null_arg, const char *field_name_arg,
	     CHARSET_INFO *cs, bool set_packlength)
    :Field_longstr((uchar*) 0,len_arg, maybe_null_arg ? (uchar*) "": 0, 0,
                   NONE, field_name_arg, cs)
  {
    flags|= BLOB_FLAG;
    packlength= 4;
    if (set_packlength)
    {
      uint32_t l_char_length= len_arg/cs->mbmaxlen;
      packlength= l_char_length <= 255 ? 1 :
                  l_char_length <= 65535 ? 2 :
                  l_char_length <= 16777215 ? 3 : 4;
    }
  }
  Field_blob(uint32_t packlength_arg)
    :Field_longstr((uchar*) 0, 0, (uchar*) "", 0, NONE, "temp", system_charset_info),
    packlength(packlength_arg) {}
  enum_field_types type() const { return MYSQL_TYPE_BLOB;}
  enum ha_base_keytype key_type() const
    { return binary() ? HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2; }
  int  store(const char *to,uint length,CHARSET_INFO *charset);
  int  store(double nr);
  int  store(int64_t nr, bool unsigned_val);
  double val_real(void);
  int64_t val_int(void);
  String *val_str(String*,String *);
  my_decimal *val_decimal(my_decimal *);
  int cmp_max(const uchar *, const uchar *, uint max_length);
  int cmp(const uchar *a,const uchar *b)
    { return cmp_max(a, b, ~0L); }
  int cmp(const uchar *a, uint32_t a_length, const uchar *b, uint32_t b_length);
  int cmp_binary(const uchar *a,const uchar *b, uint32_t max_length=~0L);
  int key_cmp(const uchar *,const uchar*);
  int key_cmp(const uchar *str, uint length);
  uint32_t key_length() const { return 0; }
  void sort_string(uchar *buff,uint length);
  uint32_t pack_length() const
  { return (uint32_t) (packlength+table->s->blob_ptr_size); }

  /**
     Return the packed length without the pointer size added. 

     This is used to determine the size of the actual data in the row
     buffer.

     @returns The length of the raw data itself without the pointer.
  */
  uint32_t pack_length_no_ptr() const
  { return (uint32_t) (packlength); }
  uint row_pack_length() { return pack_length_no_ptr(); }
  uint32_t sort_length() const;
  virtual uint32_t max_data_length() const
  {
    return (uint32_t) (((uint64_t) 1 << (packlength*8)) -1);
  }
  int reset(void) { memset(ptr, 0, packlength+sizeof(uchar*)); return 0; }
  void reset_fields() { memset((uchar*) &value, 0, sizeof(value)); }
#ifndef WORDS_BIGENDIAN
  static
#endif
  void store_length(uchar *i_ptr, uint i_packlength, uint32_t i_number, bool low_byte_first);
  void store_length(uchar *i_ptr, uint i_packlength, uint32_t i_number)
  {
    store_length(i_ptr, i_packlength, i_number, table->s->db_low_byte_first);
  }
  inline void store_length(uint32_t number)
  {
    store_length(ptr, packlength, number);
  }

  /**
     Return the packed length plus the length of the data. 

     This is used to determine the size of the data plus the 
     packed length portion in the row data.

     @returns The length in the row plus the size of the data.
  */
  uint32_t get_packed_size(const uchar *ptr_arg, bool low_byte_first)
    {return packlength + get_length(ptr_arg, packlength, low_byte_first);}

  inline uint32_t get_length(uint row_offset= 0)
  { return get_length(ptr+row_offset, this->packlength, table->s->db_low_byte_first); }
  uint32_t get_length(const uchar *ptr, uint packlength, bool low_byte_first);
  uint32_t get_length(const uchar *ptr_arg)
  { return get_length(ptr_arg, this->packlength, table->s->db_low_byte_first); }
  void put_length(uchar *pos, uint32_t length);
  inline void get_ptr(uchar **str)
    {
      memcpy((uchar*) str,ptr+packlength,sizeof(uchar*));
    }
  inline void get_ptr(uchar **str, uint row_offset)
    {
      memcpy((uchar*) str,ptr+packlength+row_offset,sizeof(char*));
    }
  inline void set_ptr(uchar *length, uchar *data)
    {
      memcpy(ptr,length,packlength);
      memcpy(ptr+packlength,&data,sizeof(char*));
    }
  void set_ptr_offset(my_ptrdiff_t ptr_diff, uint32_t length, uchar *data)
    {
      uchar *ptr_ofs= ADD_TO_PTR(ptr,ptr_diff,uchar*);
      store_length(ptr_ofs, packlength, length);
      memcpy(ptr_ofs+packlength,&data,sizeof(char*));
    }
  inline void set_ptr(uint32_t length, uchar *data)
    {
      set_ptr_offset(0, length, data);
    }
  uint get_key_image(uchar *buff,uint length, imagetype type);
  void set_key_image(const uchar *buff,uint length);
  void sql_type(String &str) const;
  inline bool copy()
  {
    uchar *tmp;
    get_ptr(&tmp);
    if (value.copy((char*) tmp, get_length(), charset()))
    {
      Field_blob::reset();
      return 1;
    }
    tmp=(uchar*) value.ptr();
    memcpy(ptr+packlength,&tmp,sizeof(char*));
    return 0;
  }
  virtual uchar *pack(uchar *to, const uchar *from,
                      uint max_length, bool low_byte_first);
  uchar *pack_key(uchar *to, const uchar *from,
                  uint max_length, bool low_byte_first);
  uchar *pack_key_from_key_image(uchar* to, const uchar *from,
                                 uint max_length, bool low_byte_first);
  virtual const uchar *unpack(uchar *to, const uchar *from,
                              uint param_data, bool low_byte_first);
  const uchar *unpack_key(uchar* to, const uchar *from,
                          uint max_length, bool low_byte_first);
  int pack_cmp(const uchar *a, const uchar *b, uint key_length,
               my_bool insert_or_update);
  int pack_cmp(const uchar *b, uint key_length,my_bool insert_or_update);
  uint packed_col_length(const uchar *col_ptr, uint length);
  uint max_packed_col_length(uint max_length);
  void free() { value.free(); }
  inline void clear_temporary() { memset((uchar*) &value, 0, sizeof(value)); }
  friend int field_conv(Field *to,Field *from);
  uint size_of() const { return sizeof(*this); }
  bool has_charset(void) const
  { return charset() == &my_charset_bin ? false : true; }
  uint32_t max_display_length();
  uint is_equal(Create_field *new_field);
  inline bool in_read_set() { return bitmap_is_set(table->read_set, field_index); }
  inline bool in_write_set() { return bitmap_is_set(table->write_set, field_index); }
private:
  int do_save_field_metadata(uchar *first_byte);
};

#endif

